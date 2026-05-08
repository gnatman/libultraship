#pragma once

#include <openxr/openxr.h>
#include <string>
#include <vector>
#include <memory>

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

private:
    bool CreateInstance();
    bool CreateSession();
    void HandleSessionState(XrSessionState state);

    bool mInitialized = false;
    XrInstance mInstance = XR_NULL_HANDLE;
    XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
    XrSession mSession = XR_NULL_HANDLE;
    XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
    
    static std::shared_ptr<VRRuntime> mInstancePtr;
};

} // namespace Ship
