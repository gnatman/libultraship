#pragma once

#include <openxr/openxr.h>
#include <string>
#include <vector>
#include <memory>
#include "ship/window/VRPose.h"

struct ID3D11Texture2D;

namespace Ship {

class VRRuntime {
public:
    static std::shared_ptr<VRRuntime> GetInstance();
    
    VRRuntime();
    ~VRRuntime();

    bool Init();
    void Shutdown();
    void Update();

    bool IsInitialized() const { return mInitialized; }
    XrInstance GetInstanceHandle() const { return mInstance; }
    XrSession GetSessionHandle() const { return mSession; }

    struct Swapchain {
        XrSwapchain handle;
        int32_t width;
        int32_t height;
        std::vector<ID3D11Texture2D*> images;
    };

    const VRPose& GetPose() const { return mCurrentPose; }

    void GetProjectionMatrix(int eye, float* m, float nearZ, float farZ) const;
    void GetViewMatrix(int eye, float* m) const;

private:
    bool CreateInstance();
    bool CreateSession();
    bool CreateSwapchains();
    void HandleSessionState(XrSessionState state);
    void UpdatePose(XrTime predictedTime);

    bool mInitialized = false;
    XrInstance mInstance = XR_NULL_HANDLE;
    XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
    XrSession mSession = XR_NULL_HANDLE;
    XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
    XrSpace mStageSpace = XR_NULL_HANDLE;

    VRPose mCurrentPose;
    Swapchain mSwapchains[2];
    
    static std::shared_ptr<VRRuntime> mInstancePtr;
};

} // namespace Ship
