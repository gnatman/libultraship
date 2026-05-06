#pragma once

#include "VRPose.h"
#include "VRSwapchain.h"
#include <openxr/openxr.h>
#include <d3d11.h>
#include <memory>
#include <vector>

namespace LUS {

class VRSession {
  public:
    VRSession(ID3D11Device* d3dDevice);
    ~VRSession();

    bool Init();
    void Shutdown();

    bool WaitFrame(VRPose& outPose);
    bool BeginFrame();
    void EndFrame(XrEnvironmentBlendMode blendMode);

    bool IsSessionActive() const { return mSessionState != XR_SESSION_STATE_LOSS_PENDING; }
    
    VRSwapchain* GetSwapchain(int eye) { return mSwapchains[eye].get(); }

  private:
    ID3D11Device* mD3DDevice = nullptr;

    XrInstance mInstance = XR_NULL_HANDLE;
    XrSession mSession = XR_NULL_HANDLE;
    XrSpace mAppSpace = XR_NULL_HANDLE;
    XrSystemId mSystemId = XR_NULL_SYSTEM_ID;

    XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;

    std::vector<XrViewConfigurationView> mConfigViews;
    std::vector<XrView> mViews;

    std::unique_ptr<VRSwapchain> mSwapchains[2];
};

} // namespace LUS
