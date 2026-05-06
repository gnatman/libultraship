#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <d3d11.h>
#include "VRSession.h"
#include <openxr/openxr_platform.h>
#include <spdlog/spdlog.h>

namespace LUS {

VRSession::VRSession(ID3D11Device* d3dDevice) : mD3DDevice(d3dDevice) {
}

VRSession::~VRSession() {
    Shutdown();
}

bool VRSession::Init() {
    // 1. Create Instance
    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy(createInfo.applicationInfo.applicationName, "VRmicelliKart");
    createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;

    const char* extensions[] = { XR_KHR_D3D11_ENABLE_EXTENSION_NAME };
    createInfo.enabledExtensionCount = 1;
    createInfo.enabledExtensionNames = extensions;

    XrResult res = xrCreateInstance(&createInfo, &mInstance);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("Failed to create OpenXR instance");
        return false;
    }

    // 2. Get System
    XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    res = xrGetSystem(mInstance, &systemInfo, &mSystemId);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("Failed to get OpenXR system");
        return false;
    }

    // 3. Check D3D11 requirements
    PFN_xrGetD3D11GraphicsRequirementsKHR pfnGetD3D11GraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(mInstance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetD3D11GraphicsRequirementsKHR);
    if (pfnGetD3D11GraphicsRequirementsKHR) {
        XrGraphicsRequirementsD3D11KHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
        pfnGetD3D11GraphicsRequirementsKHR(mInstance, mSystemId, &graphicsRequirements);
    }

    // 4. Create Session
    XrGraphicsBindingD3D11KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    graphicsBinding.device = mD3DDevice;

    XrSessionCreateInfo sessionCreateInfo = { XR_TYPE_SESSION_CREATE_INFO };
    sessionCreateInfo.next = &graphicsBinding;
    sessionCreateInfo.systemId = mSystemId;

    res = xrCreateSession(mInstance, &sessionCreateInfo, &mSession);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("Failed to create OpenXR session");
        return false;
    }

    // 5. Create Space
    XrReferenceSpaceCreateInfo spaceCreateInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
    res = xrCreateReferenceSpace(mSession, &spaceCreateInfo, &mAppSpace);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("Failed to create OpenXR reference space");
        return false;
    }

    // 6. Setup Views and Swapchains
    uint32_t viewCount;
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    mConfigViews.resize(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, mConfigViews.data());

    mViews.resize(viewCount, { XR_TYPE_VIEW });

    for (uint32_t i = 0; i < viewCount && i < 2; ++i) {
        mSwapchains[i] = std::make_unique<VRSwapchain>(mSession, mD3DDevice, mConfigViews[i].recommendedImageRectWidth, mConfigViews[i].recommendedImageRectHeight, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    return true;
}

void VRSession::Shutdown() {
    mSwapchains[0].reset();
    mSwapchains[1].reset();

    if (mAppSpace != XR_NULL_HANDLE) {
        xrDestroySpace(mAppSpace);
        mAppSpace = XR_NULL_HANDLE;
    }
    if (mSession != XR_NULL_HANDLE) {
        xrDestroySession(mSession);
        mSession = XR_NULL_HANDLE;
    }
    if (mInstance != XR_NULL_HANDLE) {
        xrDestroyInstance(mInstance);
        mInstance = XR_NULL_HANDLE;
    }
}

bool VRSession::WaitFrame(VRPose& outPose) {
    if (!mSession) return false;

    // Pump events
    XrEventDataBuffer eventData = { XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(mInstance, &eventData) == XR_SUCCESS) {
        if (eventData.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            auto* stateChanged = (XrEventDataSessionStateChanged*)&eventData;
            mSessionState = stateChanged->state;
            if (mSessionState == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo sessionBeginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
                sessionBeginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                xrBeginSession(mSession, &sessionBeginInfo);
            } else if (mSessionState == XR_SESSION_STATE_STOPPING) {
                xrEndSession(mSession);
            }
        }
        eventData = { XR_TYPE_EVENT_DATA_BUFFER };
    }

    if (mSessionState != XR_SESSION_STATE_FOCUSED && mSessionState != XR_SESSION_STATE_VISIBLE && mSessionState != XR_SESSION_STATE_SYNCHRONIZED) {
        return false;
    }

    XrFrameWaitInfo frameWaitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    XrFrameState frameState = { XR_TYPE_FRAME_STATE };
    xrWaitFrame(mSession, &frameWaitInfo, &frameState);

    outPose.shouldRender = frameState.shouldRender;
    outPose.displayTime = frameState.predictedDisplayTime;

    if (outPose.shouldRender) {
        XrViewLocateInfo viewLocateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = frameState.predictedDisplayTime;
        viewLocateInfo.space = mAppSpace;

        XrViewState viewState = { XR_TYPE_VIEW_STATE };
        uint32_t viewCount;
        xrLocateViews(mSession, &viewLocateInfo, &viewState, (uint32_t)mViews.size(), &viewCount, mViews.data());

        for (uint32_t i = 0; i < 2; ++i) {
            outPose.eyes[i] = mViews[i].pose;
            outPose.fov[i] = mViews[i].fov;
        }
    }

    return true;
}

bool VRSession::BeginFrame() {
    if (!mSession) return false;
    XrFrameBeginInfo frameBeginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    XrResult res = xrBeginFrame(mSession, &frameBeginInfo);
    return XR_SUCCEEDED(res);
}

void VRSession::EndFrame(XrEnvironmentBlendMode blendMode) {
    if (!mSession) return;
    
    // In a real application, you'd construct the composition layers based on whether you rendered anything
    XrCompositionLayerProjection projectionLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    projectionLayer.space = mAppSpace;
    
    // Simplification for scaffolding:
    XrFrameEndInfo frameEndInfo = { XR_TYPE_FRAME_END_INFO };
    frameEndInfo.environmentBlendMode = blendMode;
    // frameEndInfo.layerCount = 1;
    // frameEndInfo.layers = (const XrCompositionLayerBaseHeader* const*)&projectionLayerPtr;
    
    xrEndFrame(mSession, &frameEndInfo);
}

} // namespace LUS
