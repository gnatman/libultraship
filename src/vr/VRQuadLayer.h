#pragma once

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <vector>
#include <memory>

struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;

namespace Ship {

class VRQuadLayer {
public:
    VRQuadLayer(XrSession session, int32_t width, int32_t height);
    ~VRQuadLayer();

    void* GetRTV(uint32_t index) const { return mRtvs[index]; }
    void* GetDSV(uint32_t index) const { return mDsvs[index]; }
    void* GetSRV(uint32_t index) const { return mSrvs[index]; }
    void GetDimensions(int32_t* w, int32_t* h) const { *w = mWidth; *h = mHeight; }
    XrSwapchain GetSwapchain() const { return mSwapchain; }
    bool IsValid() const { return mSwapchain != XR_NULL_HANDLE; }

    uint32_t AcquireImage();
    void ReleaseImage();

    void SetPose(XrPosef pose) { mPose = pose; }
    void SetSize(XrExtent2Df size) { mSize = size; }
    
    XrCompositionLayerQuad GetCompositionLayer(XrSpace space) const;

private:
    XrSession mSession;
    XrSwapchain mSwapchain = XR_NULL_HANDLE;
    int32_t mWidth;
    int32_t mHeight;
    std::vector<ID3D11Texture2D*> mImages;
    std::vector<ID3D11RenderTargetView*> mRtvs;
    std::vector<ID3D11DepthStencilView*> mDsvs;
    std::vector<ID3D11ShaderResourceView*> mSrvs;

    XrPosef mPose;
    XrExtent2Df mSize;
};

} // namespace Ship
