#pragma once
#include <d3d11.h>
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>
#include <vector>
#include <stdint.h>

class VRSwapchain {
public:
    VRSwapchain(XrSession session, ID3D11Device* device, int32_t width, int32_t height, int64_t format);
    ~VRSwapchain();
    
    XrSwapchain GetHandle() const { return mSwapchain; }
    ID3D11RenderTargetView* GetRenderTargetView(uint32_t index) const;

private:
    XrSwapchain mSwapchain{XR_NULL_HANDLE};
    std::vector<XrSwapchainImageD3D11KHR> mImages;
    std::vector<ID3D11RenderTargetView*> mRTVs;
};
