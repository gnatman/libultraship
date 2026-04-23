#include "VRSession.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <spdlog/spdlog.h>

namespace Fast {

VRSession::VRSession() {
}

VRSession::~VRSession() {
    Shutdown();
}

bool VRSession::Init(ID3D11Device* device) {
    SPDLOG_INFO("Initializing VRSession");
    if (!CreateInstance()) { SPDLOG_ERROR("Failed to create OpenXR Instance"); return false; }
    if (!CreateSession(device)) { SPDLOG_ERROR("Failed to create OpenXR Session"); return false; }
    if (!CreateReferenceSpaces()) { SPDLOG_ERROR("Failed to create OpenXR Reference Spaces"); return false; }
    if (!CreateSwapchain()) { SPDLOG_ERROR("Failed to create OpenXR Swapchain"); return false; }
    SPDLOG_INFO("VRSession initialized successfully");
    return true;
}

void VRSession::Shutdown() {
    SPDLOG_INFO("Shutting down VRSession");
    for (auto& bundle : mSwapchains) {
        for (auto rtv : bundle.rtvs) {
            rtv->Release();
        }
        if (bundle.dsv) {
            bundle.dsv->Release();
        }
        xrDestroySwapchain(bundle.handle);
    }
    mSwapchains.clear();

    if (mAppSpace != XR_NULL_HANDLE) xrDestroySpace(mAppSpace);
    if (mViewSpace != XR_NULL_HANDLE) xrDestroySpace(mViewSpace);
    if (mSession != XR_NULL_HANDLE) xrDestroySession(mSession);

    if (mDebugMessenger != XR_NULL_HANDLE) {
        PFN_xrDestroyDebugUtilsMessengerEXT pfnDestroyDebugUtilsMessengerEXT;
        xrGetInstanceProcAddr(mInstance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&pfnDestroyDebugUtilsMessengerEXT);
        if (pfnDestroyDebugUtilsMessengerEXT) {
            pfnDestroyDebugUtilsMessengerEXT(mDebugMessenger);
        }
    }

    if (mInstance != XR_NULL_HANDLE) xrDestroyInstance(mInstance);
    
    mAppSpace = XR_NULL_HANDLE;
    mViewSpace = XR_NULL_HANDLE;
    mSession = XR_NULL_HANDLE;
    mInstance = XR_NULL_HANDLE;
    mDebugMessenger = XR_NULL_HANDLE;
}

static XRAPI_ATTR XrBool32 XRAPI_CALL xrDebugCallback(XrDebugUtilsMessageSeverityFlagsEXT severity, XrDebugUtilsMessageTypeFlagsEXT type, const XrDebugUtilsMessengerCallbackDataEXT* callbackData, void* userData) {
    if (severity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        SPDLOG_ERROR("OpenXR Debug: {}", callbackData->message);
    } else if (severity >= XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        SPDLOG_WARN("OpenXR Debug: {}", callbackData->message);
    } else {
        SPDLOG_INFO("OpenXR Debug: {}", callbackData->message);
    }
    return XR_FALSE;
}

bool VRSession::CreateInstance() {
    // Query supported extensions
    uint32_t extensionCount = 0;
    xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr);
    std::vector<XrExtensionProperties> supportedExtensions(extensionCount, { XR_TYPE_EXTENSION_PROPERTIES });
    xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, supportedExtensions.data());

    SPDLOG_INFO("OpenXR Supported Extensions:");
    for (const auto& ext : supportedExtensions) {
        SPDLOG_INFO("  - {}", ext.extensionName);
    }

    auto isExtensionSupported = [&](const char* name) {
        for (const auto& ext : supportedExtensions) {
            if (strcmp(ext.extensionName, name) == 0) return true;
        }
        return false;
    };

    std::vector<const char*> requestedExtensions;
    if (isExtensionSupported(XR_KHR_D3D11_ENABLE_EXTENSION_NAME)) {
        requestedExtensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
    } else {
        SPDLOG_ERROR("OpenXR: Runtime does not support D3D11 extension");
        return false;
    }

    // Depth extension is optional for now
    if (isExtensionSupported(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME)) {
        requestedExtensions.push_back(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
    }

    bool debugSupported = isExtensionSupported(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (debugSupported) {
        requestedExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    strcpy(createInfo.applicationInfo.applicationName, "VRmicelliKart");
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0; // Use 1.0 for max compatibility
    createInfo.enabledExtensionCount = (uint32_t)requestedExtensions.size();
    createInfo.enabledExtensionNames = requestedExtensions.data();

    XrDebugUtilsMessengerCreateInfoEXT debugInfo = { XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT };
    if (debugSupported) {
        debugInfo.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                      XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                      XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                      XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageTypes = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                 XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                 XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                 XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
        debugInfo.userCallback = xrDebugCallback;
        createInfo.next = &debugInfo;
    }

    XrResult res = xrCreateInstance(&createInfo, &mInstance);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrCreateInstance failed: {}", (int)res);
        return false;
    }

    if (debugSupported) {
        PFN_xrCreateDebugUtilsMessengerEXT pfnCreateDebugUtilsMessengerEXT;
        xrGetInstanceProcAddr(mInstance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&pfnCreateDebugUtilsMessengerEXT);
        pfnCreateDebugUtilsMessengerEXT(mInstance, &debugInfo, &mDebugMessenger);
    }

    XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    res = xrGetSystem(mInstance, &systemInfo, &mSystemId);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrGetSystem failed: {}", (int)res);
        return false;
    }

    return true;
}

bool VRSession::CreateSession(ID3D11Device* device) {
    // We MUST query graphics requirements before creating a session using that graphics API
    PFN_xrGetD3D11GraphicsRequirementsKHR pfnGetD3D11GraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(mInstance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetD3D11GraphicsRequirementsKHR);
    
    if (pfnGetD3D11GraphicsRequirementsKHR) {
        XrGraphicsRequirementsD3D11KHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
        pfnGetD3D11GraphicsRequirementsKHR(mInstance, mSystemId, &graphicsRequirements);
        SPDLOG_INFO("OpenXR: D3D11 Graphics Requirements met");
    }

    XrGraphicsBindingD3D11KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    graphicsBinding.device = device;

    XrSessionCreateInfo createInfo = { XR_TYPE_SESSION_CREATE_INFO };
    createInfo.next = &graphicsBinding;
    createInfo.systemId = mSystemId;

    XrResult res = xrCreateSession(mInstance, &createInfo, &mSession);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrCreateSession failed: {}", (int)res);
        return false;
    }

    return true;
}

bool VRSession::CreateReferenceSpaces() {
    XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    
    XrResult res = xrCreateReferenceSpace(mSession, &spaceInfo, &mAppSpace);
    if (XR_FAILED(res)) {
        // Fallback to local if stage is not available
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        res = xrCreateReferenceSpace(mSession, &spaceInfo, &mAppSpace);
        if (XR_FAILED(res)) return false;
    }

    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    res = xrCreateReferenceSpace(mSession, &spaceInfo, &mViewSpace);
    if (XR_FAILED(res)) return false;

    return true;
}

bool VRSession::CreateSwapchain() {
    uint32_t viewCount = 0;
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &viewCount, nullptr);
    mConfigViews.resize(viewCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, viewCount, &viewCount, mConfigViews.data());

    for (uint32_t i = 0; i < viewCount; i++) {
        XrSwapchainCreateInfo swapchainInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainInfo.arraySize = 1;
        swapchainInfo.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        swapchainInfo.width = mConfigViews[i].recommendedImageRectWidth;
        swapchainInfo.height = mConfigViews[i].recommendedImageRectHeight;
        swapchainInfo.mipCount = 1;
        swapchainInfo.faceCount = 1;
        swapchainInfo.sampleCount = 1;
        swapchainInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        if (i == 0) {
            mWidth = swapchainInfo.width;
            mHeight = swapchainInfo.height;
        }

        SwapchainBundle bundle;
        bundle.width = swapchainInfo.width;
        bundle.height = swapchainInfo.height;
        bundle.dsv = nullptr;
        xrCreateSwapchain(mSession, &swapchainInfo, &bundle.handle);

        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(bundle.handle, 0, &imageCount, nullptr);
        bundle.images.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
        xrEnumerateSwapchainImages(bundle.handle, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)bundle.images.data());

        ID3D11Device* device = nullptr;
        bundle.images[0].texture->GetDevice(&device);

        for (uint32_t j = 0; j < imageCount; j++) {
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            
            ID3D11RenderTargetView* rtv = nullptr;
            device->CreateRenderTargetView(bundle.images[j].texture, &rtvDesc, &rtv);
            bundle.rtvs.push_back(rtv);
        }

        // Create Depth Buffer for this swapchain
        D3D11_TEXTURE2D_DESC depthDesc = {};
        depthDesc.Width = bundle.width;
        depthDesc.Height = bundle.height;
        depthDesc.MipLevels = 1;
        depthDesc.ArraySize = 1;
        depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDesc.SampleDesc.Count = 1;
        depthDesc.SampleDesc.Quality = 0;
        depthDesc.Usage = D3D11_USAGE_DEFAULT;
        depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        
        ID3D11Texture2D* depthTex = nullptr;
        device->CreateTexture2D(&depthDesc, nullptr, &depthTex);
        device->CreateDepthStencilView(depthTex, nullptr, &bundle.dsv);
        depthTex->Release();

        device->Release();

        mSwapchains.push_back(bundle);
        SPDLOG_INFO("Created Swapchain {} ({}x{})", i, bundle.width, bundle.height);
    }

    return true;
}

bool VRSession::BeginFrame() {
    XrEventDataBuffer event = { XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(mInstance, &event) == XR_SUCCESS) {
        if (event.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged* stateChanged = (XrEventDataSessionStateChanged*)&event;
            mSessionState = stateChanged->state;
            SPDLOG_INFO("OpenXR Session State changed to {}", (int)mSessionState);
            if (mSessionState == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
                beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                xrBeginSession(mSession, &beginInfo);
                mSessionRunning = true;
                SPDLOG_INFO("OpenXR Session Started");
            } else if (mSessionState == XR_SESSION_STATE_STOPPING) {
                xrEndSession(mSession);
                mSessionRunning = false;
                SPDLOG_INFO("OpenXR Session Stopped");
            } else if (mSessionState == XR_SESSION_STATE_LOSS_PENDING || mSessionState == XR_SESSION_STATE_EXITING) {
                mSessionRunning = false;
            }
        }
        event = { XR_TYPE_EVENT_DATA_BUFFER };
    }

    if (!mSessionRunning) {
        return false;
    }

    XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    XrResult res = xrWaitFrame(mSession, &waitInfo, &mFrameState);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrWaitFrame failed: {}", (int)res);
        return false;
    }

    XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    res = xrBeginFrame(mSession, &beginInfo);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrBeginFrame failed: {}", (int)res);
        return false;
    }

    mFrameStarted = true;

    if (!mFrameState.shouldRender) {
        return false;
    }

    // Locate views for the current frame
    XrViewLocateInfo viewLocateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    viewLocateInfo.displayTime = mFrameState.predictedDisplayTime;
    viewLocateInfo.space = mAppSpace;

    uint32_t viewCount = 0;
    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    mViews.resize(mSwapchains.size(), { XR_TYPE_VIEW });
    res = xrLocateViews(mSession, &viewLocateInfo, &viewState, (uint32_t)mViews.size(), &viewCount, mViews.data());
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrLocateViews failed: {}", (int)res);
    }

    return true;
}

void VRSession::GetProjectionMatrix(int eye, float nearZ, float farZ, float mat[4][4]) const {
    if (eye >= mViews.size()) return;

    const XrFovf& fov = mViews[eye].fov;
    float tanLeft = tanf(fov.angleLeft);
    float tanRight = tanf(fov.angleRight);
    float tanUp = tanf(fov.angleUp);
    float tanDown = tanf(fov.angleDown);

    float tanWidth = tanRight - tanLeft;
    float tanHeight = tanUp - tanDown;

    mat[0][0] = 2.0f / tanWidth;
    mat[0][1] = 0.0f;
    mat[0][2] = 0.0f;
    mat[0][3] = 0.0f;

    mat[1][0] = 0.0f;
    mat[1][1] = 2.0f / tanHeight;
    mat[1][2] = 0.0f;
    mat[1][3] = 0.0f;

    mat[2][0] = (tanRight + tanLeft) / tanWidth;
    mat[2][1] = (tanUp + tanDown) / tanHeight;
    mat[2][2] = -(farZ + nearZ) / (farZ - nearZ);
    mat[2][3] = -1.0f;

    mat[3][0] = 0.0f;
    mat[3][1] = 0.0f;
    mat[3][2] = -(2.0f * farZ * nearZ) / (farZ - nearZ);
    mat[3][3] = 0.0f;
}

void VRSession::GetViewMatrix(int eye, float mat[4][4]) const {
    if (eye >= mViews.size()) return;

    const XrPosef& pose = mViews[eye].pose;
    
    // Convert quaternion to rotation matrix
    float x = pose.orientation.x, y = pose.orientation.y, z = pose.orientation.z, w = pose.orientation.w;
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    mat[0][0] = 1.0f - (yy + zz);
    mat[0][1] = xy + wz;
    mat[0][2] = xz - wy;
    mat[0][3] = 0.0f;

    mat[1][0] = xy - wz;
    mat[1][1] = 1.0f - (xx + zz);
    mat[1][2] = yz + wx;
    mat[1][3] = 0.0f;

    mat[2][0] = xz + wy;
    mat[2][1] = yz - wx;
    mat[2][2] = 1.0f - (xx + yy);
    mat[2][3] = 0.0f;

    // Translation
    mat[3][0] = -(pose.position.x * mat[0][0] + pose.position.y * mat[1][0] + pose.position.z * mat[2][0]);
    mat[3][1] = -(pose.position.x * mat[0][1] + pose.position.y * mat[1][1] + pose.position.z * mat[2][1]);
    mat[3][2] = -(pose.position.x * mat[0][2] + pose.position.y * mat[1][2] + pose.position.z * mat[2][2]);
    mat[3][3] = 1.0f;
}

void VRSession::GetHeadPose(float pos[3], float rot[3]) const {
    pos[0] = 0; pos[1] = 0; pos[2] = 0;
    rot[0] = 0; rot[1] = 0; rot[2] = 0;

    if (!mSessionRunning) return;

    XrSpaceLocation location = { XR_TYPE_SPACE_LOCATION };
    xrLocateSpace(mViewSpace, mAppSpace, mFrameState.predictedDisplayTime, &location);

    if (location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
        pos[0] = location.pose.position.x;
        pos[1] = location.pose.position.y;
        pos[2] = location.pose.position.z;
    }

    if (location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
        XrQuaternionf q = location.pose.orientation;
        // Convert quaternion to Euler angles (YXZ intrinsic / ZXY extrinsic)
        // Yaw (Y)
        rot[1] = atan2f(2.0f * (q.w * q.y + q.z * q.x), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));
        // Pitch (X)
        rot[0] = asinf(std::clamp(2.0f * (q.w * q.x - q.y * q.z), -1.0f, 1.0f));
        // Roll (Z)
        rot[2] = atan2f(2.0f * (q.w * q.z + q.x * q.y), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));
    }
}

ID3D11RenderTargetView* VRSession::BeginEye(int eye) {
    if (!mSessionRunning || eye >= (int)mSwapchains.size()) return nullptr;

    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    xrAcquireSwapchainImage(mSwapchains[eye].handle, &acquireInfo, &mCurrentImageIndices[eye]);

    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = 100000000; // 100ms timeout to prevent hard lockups
    XrResult res = xrWaitSwapchainImage(mSwapchains[eye].handle, &waitInfo);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrWaitSwapchainImage failed: {}", (int)res);
        return nullptr;
    }

    return mSwapchains[eye].rtvs[mCurrentImageIndices[eye]];
}

void VRSession::EndEye(int eye) {
    if (!mSessionRunning || eye >= (int)mSwapchains.size()) return;

    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    xrReleaseSwapchainImage(mSwapchains[eye].handle, &releaseInfo);
}

ID3D11DepthStencilView* VRSession::GetEyeDSV(int eye) const {
    if (eye >= (int)mSwapchains.size()) return nullptr;
    return mSwapchains[eye].dsv;
}

void VRSession::EndFrame(ID3D11DeviceContext* context, ID3D11RenderTargetView* leftEyeRtv, ID3D11RenderTargetView* rightEyeRtv) {
    if (!mSessionRunning || !mFrameStarted) return;

    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    endInfo.displayTime = mFrameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;

    if (mFrameState.shouldRender) {
        std::vector<XrCompositionLayerProjectionView> projectionViews;
        projectionViews.reserve(mSwapchains.size());

        for (uint32_t i = 0; i < mSwapchains.size(); i++) {
            XrCompositionLayerProjectionView projectionView = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
            projectionView.pose = mViews[i].pose;
            projectionView.fov = mViews[i].fov;
            projectionView.subImage.swapchain = mSwapchains[i].handle;
            projectionView.subImage.imageRect.offset = { 0, 0 };
            projectionView.subImage.imageRect.extent = { (int32_t)mSwapchains[i].width, (int32_t)mSwapchains[i].height };
            projectionViews.push_back(projectionView);
        }

        XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        layer.space = mAppSpace;
        layer.viewCount = (uint32_t)projectionViews.size();
        layer.views = projectionViews.data();

        XrCompositionLayerBaseHeader* layers[] = { (XrCompositionLayerBaseHeader*)&layer };
        endInfo.layerCount = 1;
        endInfo.layers = layers;

        xrEndFrame(mSession, &endInfo);
    } else {
        endInfo.layerCount = 0;
        endInfo.layers = nullptr;
        xrEndFrame(mSession, &endInfo);
    }

    mFrameStarted = false;
}

} // namespace Fast
