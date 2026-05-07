#include "gfx_direct3d11_vr.h"

namespace Fast {

GfxDirect3D11VR::GfxDirect3D11VR() : GfxDirect3D11() {
}

void GfxDirect3D11VR::BindExternalRenderTarget(ID3D11RenderTargetView* rtv) {
#ifdef _WIN32
    if (rtv) {
        mDeviceContext->OMSetRenderTargets(1, &rtv, mDepthStencilView.Get());
    } else {
        // Fallback to default backbuffer if needed
        ID3D11RenderTargetView* rtvDefault = mRenderTargetView.Get();
        mDeviceContext->OMSetRenderTargets(1, &rtvDefault, mDepthStencilView.Get());
    }
#endif
}

}
