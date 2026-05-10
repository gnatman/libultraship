#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>

#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32
#endif
#ifndef XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D11
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "VRQuadLayer.h"
#include "ship/Context.h"
#include "ship/window/Window.h"
#include "fast/Fast3dWindow.h"
#include "fast/backends/gfx_rendering_api.h"
#include <d3d11.h>
#include <vector>
#include <spdlog/spdlog.h>

namespace Ship {

VRQuadLayer::VRQuadLayer(XrSession session, int32_t width, int32_t height, XrEyeVisibility visibility)
    : mSession(session), mWidth(width), mHeight(height), mVisibility(visibility) {
    
    auto window = Context::GetInstance()->GetWindow();
    auto fastWindow = std::dynamic_pointer_cast<Fast::Fast3dWindow>(window);
    auto rapi = fastWindow->GetRenderingApi();
    ID3D11Device* device = (ID3D11Device*)rapi->GetDevice();

    // Use SRGB format as it is explicitly supported by the runtime
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    
    XrSwapchainCreateInfo createInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
    createInfo.arraySize = 1;
    createInfo.format = format;
    createInfo.width = width;
    createInfo.height = height;
    createInfo.mipCount = 1;
    createInfo.faceCount = 1;
    createInfo.sampleCount = 1;
    createInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    XrResult res = xrCreateSwapchain(mSession, &createInfo, &mSwapchain);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrCreateSwapchain failed for Quad Layer: {}", (int)res);
        return;
    }
    
    spdlog::critical("Quad Layer Created - Visibility: {}, Handle: 0x{:X}", (int)mVisibility, (uintptr_t)mSwapchain);

    uint32_t imageCount = 0;
    xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr);
    std::vector<XrSwapchainImageD3D11KHR> images(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
    xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());

    for (const auto& img : images) {
        if (!img.texture) {
            SPDLOG_ERROR("Acquired null texture from quad swapchain");
            continue;
        }
        mImages.push_back(img.texture);

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

        ID3D11RenderTargetView* rtv = nullptr;
        HRESULT hr = device->CreateRenderTargetView(img.texture, &rtvDesc, &rtv);
        if (FAILED(hr)) {
            SPDLOG_ERROR("Failed to create RTV for quad layer: 0x{:X}", (uint32_t)hr);
            mRtvs.push_back(nullptr);
        } else {
            mRtvs.push_back(rtv);
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        hr = device->CreateShaderResourceView(img.texture, &srvDesc, &srv);
        if (SUCCEEDED(hr)) {
            mSrvs.push_back(srv);
        } else {
            mSrvs.push_back(nullptr);
        }

        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        ID3D11Texture2D* depthTex = nullptr;
        hr = device->CreateTexture2D(&depthDesc, nullptr, &depthTex);
        if (SUCCEEDED(hr)) {
            ID3D11DepthStencilView* dsv = nullptr;
            hr = device->CreateDepthStencilView(depthTex, nullptr, &dsv);
            if (SUCCEEDED(hr)) {
                mDsvs.push_back(dsv);
            } else {
                mDsvs.push_back(nullptr);
            }
            depthTex->Release();
        } else {
            mDsvs.push_back(nullptr);
        }
    }

    mPose = { {0, 0, 0, 1}, {0, 0, 0} };
    mSize = { 1.0f, 1.0f };
}

VRQuadLayer::~VRQuadLayer() {
    for (auto rtv : mRtvs) if (rtv) rtv->Release();
    for (auto dsv : mDsvs) if (dsv) dsv->Release();
    for (auto srv : mSrvs) if (srv) srv->Release();
    if (mSwapchain != XR_NULL_HANDLE) {
        xrDestroySwapchain(mSwapchain);
    }
}

uint32_t VRQuadLayer::AcquireImage() {
    if (mSwapchain == XR_NULL_HANDLE) return 0;

    uint32_t index = 0;
    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &index);
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(mSwapchain, &waitInfo);
    return index;
}

void VRQuadLayer::ReleaseImage() {
    if (mSwapchain == XR_NULL_HANDLE) return;

    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    xrReleaseSwapchainImage(mSwapchain, &releaseInfo);
}

XrCompositionLayerQuad VRQuadLayer::GetCompositionLayer(XrSpace space) const {
    XrCompositionLayerQuad layer = { XR_TYPE_COMPOSITION_LAYER_QUAD };
    layer.layerFlags = 0; // Disable alpha for persistence test
    layer.space = space;
    layer.eyeVisibility = mVisibility;
    layer.pose = mPose;
    layer.size = mSize;
    layer.subImage.swapchain = mSwapchain;
    layer.subImage.imageRect.offset = { 0, 0 };
    layer.subImage.imageRect.extent = { mWidth, mHeight };
    layer.subImage.imageArrayIndex = 0;
    return layer;
}

} // namespace Ship
