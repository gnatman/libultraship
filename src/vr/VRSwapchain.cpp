#include "VRSwapchain.h"
#include <stdexcept>

VRSwapchain::VRSwapchain(XrSession session, ID3D11Device* device, int32_t width, int32_t height, int64_t format) {
    XrSwapchainCreateInfo swapchainInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.format = format;
    swapchainInfo.sampleCount = 1;
    swapchainInfo.width = width;
    swapchainInfo.height = height;
    swapchainInfo.faceCount = 1;
    swapchainInfo.arraySize = 1;
    swapchainInfo.mipCount = 1;
    
    if (XR_FAILED(xrCreateSwapchain(session, &swapchainInfo, &mSwapchain))) {
        throw std::runtime_error("Failed to create OpenXR swapchain");
    }
    
    uint32_t imageCount;
    xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr);
    mImages.resize(imageCount, {XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
    xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)mImages.data());
    
    for (uint32_t i = 0; i < imageCount; i++) {
        ID3D11RenderTargetView* rtv;
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format = (DXGI_FORMAT)format;
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(mImages[i].texture, &rtvDesc, &rtv);
        mRTVs.push_back(rtv);
    }
}

VRSwapchain::~VRSwapchain() {
    for (auto rtv : mRTVs) if (rtv) rtv->Release();
    if (mSwapchain != XR_NULL_HANDLE) xrDestroySwapchain(mSwapchain);
}

ID3D11RenderTargetView* VRSwapchain::GetRenderTargetView(uint32_t index) const {
    return mRTVs[index];
}
