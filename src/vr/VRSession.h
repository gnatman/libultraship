#pragma once
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>
#include <memory>
#include <functional>
#include "VRSwapchain.h"

struct VRPose {
    XrPosef head;
    XrPosef eyes[2];
    XrFovf fov[2];
    XrTime displayTime;
};

class VRSession {
public:
    static VRSession& GetInstance();
    void Init(ID3D11Device* device);
    void Destroy();
    bool WaitFrame(VRPose& outPose);
    void BeginFrame();
    void EndFrame();
    void RenderEye(int eyeIndex, std::function<void(ID3D11RenderTargetView*)> renderFunc);
    void SubmitQuadLayer(void* texture, float width, float height, float distance);

    void GetEyeFov(int eyeIndex, float* left, float* right, float* up, float* down);
    int GetCurrentEye() const { return mCurrentEye; }
    void SetCurrentEye(int eye) { mCurrentEye = eye; }

private:
    XrInstance mInstance{XR_NULL_HANDLE};
    XrSession mSession{XR_NULL_HANDLE};
    XrSpace mSpace{XR_NULL_HANDLE};
    XrSystemId mSystemId{XR_NULL_SYSTEM_ID};
    std::unique_ptr<VRSwapchain> mSwapchains[2];
    XrFrameState mFrameState{XR_TYPE_FRAME_STATE};
    int mCurrentEye = 0;
    XrView mViews[2];
};

#ifdef __cplusplus
extern "C" {
#endif

void VRSession_GetEyeFov(int eyeIndex, float* left, float* right, float* up, float* down);
void VRSession_GetEyePose(int eyeIndex, float* posX, float* posY, float* posZ, float* quatX, float* quatY, float* quatZ, float* quatW);
void VRSession_SubmitQuadLayer(void* texture, float width, float height, float distance);
int VRSession_GetCurrentEye();

#ifdef __cplusplus
}
#endif
