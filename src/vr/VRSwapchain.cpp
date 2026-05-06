#include "VRSwapchain.h"
#include <spdlog/spdlog.h>

namespace LUS {

VRSwapchain::VRSwapchain(XrSession session, ID3D11Device* device, int32_t width, int32_t height, int64_t format)
    : mWidth(width), mHeight(height) {

    XrSwapchainCreateInfo swapchainCreateInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
    swapchainCreateInfo.arraySize = 1;
    swapchainCreateInfo.format = format;
    swapchainCreateInfo.width = width;
    swapchainCreateInfo.height = height;
    swapchainCreateInfo.mipCount = 1;
    swapchainCreateInfo.faceCount = 1;
    swapchainCreateInfo.sampleCount = 1;
    swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

    XrResult res = xrCreateSwapchain(session, &swapchainCreateInfo, &mSwapchain);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("Failed to create OpenXR swapchain");
        return;
    }

    uint32_t imageCount;
    xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr);
    mImages.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
    xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)mImages.data());

    mContexts.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; ++i) {
        mContexts[i].texture = mImages[i].texture;

        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = (DXGI_FORMAT)format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        
        device->CreateRenderTargetView(mContexts[i].texture.Get(), &rtvDesc, mContexts[i].rtv.GetAddressOf());

        // Create Depth Stencil for this image
        D3D11_TEXTURE2D_DESC depthDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

        Microsoft::WRL::ComPtr<ID3D11Texture2D> depthTexture;
        device->CreateTexture2D(&depthDesc, nullptr, depthTexture.GetAddressOf());

        D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        
        device->CreateDepthStencilView(depthTexture.Get(), &dsvDesc, mContexts[i].dsv.GetAddressOf());
    }
}

VRSwapchain::~VRSwapchain() {
    if (mSwapchain != XR_NULL_HANDLE) {
        xrDestroySwapchain(mSwapchain);
    }
}

bool VRSwapchain::AcquireImage(uint32_t& imageIndex) {
    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrResult res = xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &imageIndex);
    return XR_SUCCEEDED(res);
}

bool VRSwapchain::WaitImage(uint32_t imageIndex) {
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    XrResult res = xrWaitSwapchainImage(mSwapchain, &waitInfo);
    return XR_SUCCEEDED(res);
}

bool VRSwapchain::ReleaseImage(uint32_t imageIndex) {
    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    XrResult res = xrReleaseSwapchainImage(mSwapchain, &releaseInfo);
    return XR_SUCCEEDED(res);
}

ID3D11RenderTargetView* VRSwapchain::GetRTV(uint32_t imageIndex) const {
    return mContexts[imageIndex].rtv.Get();
}

ID3D11DepthStencilView* VRSwapchain::GetDSV(uint32_t imageIndex) const {
    return mContexts[imageIndex].dsv.Get();
}

} // namespace LUS
