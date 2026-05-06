#pragma once

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

namespace LUS {

struct VRSwapchainImageContext {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;
};

class VRSwapchain {
  public:
    VRSwapchain(XrSession session, ID3D11Device* device, int32_t width, int32_t height, int64_t format);
    ~VRSwapchain();

    bool AcquireImage(uint32_t& imageIndex);
    bool WaitImage(uint32_t imageIndex);
    bool ReleaseImage(uint32_t imageIndex);

    XrSwapchain GetHandle() const { return mSwapchain; }
    int32_t GetWidth() const { return mWidth; }
    int32_t GetHeight() const { return mHeight; }

    ID3D11RenderTargetView* GetRTV(uint32_t imageIndex) const;
    ID3D11DepthStencilView* GetDSV(uint32_t imageIndex) const;

  private:
    XrSwapchain mSwapchain = XR_NULL_HANDLE;
    int32_t mWidth = 0;
    int32_t mHeight = 0;
    std::vector<XrSwapchainImageD3D11KHR> mImages;
    std::vector<VRSwapchainImageContext> mContexts;
};

} // namespace LUS
