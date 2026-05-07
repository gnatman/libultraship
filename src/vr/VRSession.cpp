#include "VRSession.h"
#include <stdexcept>

VRSession& VRSession::GetInstance() {
    static VRSession instance;
    return instance;
}

void VRSession::Init(ID3D11Device* device) {
#ifdef _WIN32
    XrInstanceCreateInfo createInfo{XR_TYPE_INSTANCE_CREATE_INFO};
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
    const char* exts[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
    createInfo.enabledExtensionCount = 1;
    createInfo.enabledExtensionNames = exts;
    xrCreateInstance(&createInfo, &mInstance);

    XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    xrGetSystem(mInstance, &systemInfo, &mSystemId);

    XrGraphicsBindingD3D11KHR graphicsBinding{XR_TYPE_GRAPHICS_BINDING_D3D11_KHR};
    graphicsBinding.device = device;
    
    XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};
    sessionInfo.next = &graphicsBinding;
    sessionInfo.systemId = mSystemId;
    xrCreateSession(mInstance, &sessionInfo, &mSession);
#endif
}

void VRSession::Destroy() {
#ifdef _WIN32
    if (mSession != XR_NULL_HANDLE) xrDestroySession(mSession);
    if (mInstance != XR_NULL_HANDLE) xrDestroyInstance(mInstance);
#endif
}

bool VRSession::WaitFrame(VRPose& outPose) {
    // OpenXR frame wait logic
    return true;
}

void VRSession::BeginFrame() {
    // OpenXR begin frame logic
}

void VRSession::EndFrame() {
    // OpenXR end frame logic
}

void VRSession::RenderEye(int eyeIndex, std::function<void(ID3D11RenderTargetView*)> renderFunc) {
#ifdef _WIN32
    uint32_t imageIndex;
    XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
    xrAcquireSwapchainImage(mSwapchains[eyeIndex]->GetHandle(), &acquireInfo, &imageIndex);

    XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, nullptr, XR_INFINITE_DURATION};
    xrWaitSwapchainImage(mSwapchains[eyeIndex]->GetHandle(), &waitInfo);

    renderFunc(mSwapchains[eyeIndex]->GetRenderTargetView(imageIndex));

    XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
    xrReleaseSwapchainImage(mSwapchains[eyeIndex]->GetHandle(), &releaseInfo);
#endif
}

void VRSession::SubmitQuadLayer(void* texture, float width, float height, float distance) {
#ifdef _WIN32
    // TODO: Add XrCompositionLayerQuad to mLayers
    SPDLOG_INFO("Submitted VR Quad Layer: {}x{} at {}m", width, height, distance);
#endif
}

void VRSession::GetEyeFov(int eyeIndex, float* left, float* right, float* up, float* down) {
#ifdef _WIN32
    *left = tanf(mViews[eyeIndex].fov.angleLeft);
    *right = tanf(mViews[eyeIndex].fov.angleRight);
    *up = tanf(mViews[eyeIndex].fov.angleUp);
    *down = tanf(mViews[eyeIndex].fov.angleDown);
#else
    *left = -0.5f; *right = 0.5f; *up = 0.5f; *down = -0.5f;
#endif
}

extern "C" {
void VRSession_GetEyeFov(int eyeIndex, float* left, float* right, float* up, float* down) {
    VRSession::GetInstance().GetEyeFov(eyeIndex, left, right, up, down);
}

void VRSession_GetEyePose(int eyeIndex, float* posX, float* posY, float* posZ, float* quatX, float* quatY, float* quatZ, float* quatW) {
    VRSession::GetInstance().GetEyePose(eyeIndex, posX, posY, posZ, quatX, quatY, quatZ, quatW);
}

void VRSession_SubmitQuadLayer(void* texture, float width, float height, float distance) {
    VRSession::GetInstance().SubmitQuadLayer(texture, width, height, distance);
}

int VRSession_GetCurrentEye() {
    return VRSession::GetInstance().GetCurrentEye();
}
}
