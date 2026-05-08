#include "vr/VRRuntime.h"
#ifdef _WIN32
#include <windows.h>
#include <d3d11.h>
#endif
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
#include <openxr/openxr_platform.h>
#include <spdlog/spdlog.h>
#include <iostream>
#include <cmath>

#include "ship/Context.h"
#include "fast/Fast3dWindow.h"
#include "fast/backends/gfx_rendering_api.h"

namespace Ship {

std::shared_ptr<VRRuntime> VRRuntime::mInstancePtr = nullptr;

std::shared_ptr<VRRuntime> VRRuntime::GetInstance() {
    if (mInstancePtr == nullptr) {
        mInstancePtr = std::make_shared<VRRuntime>();
    }
    return mInstancePtr;
}

VRRuntime::VRRuntime() : mCurrentPose{} {
    for (int i = 0; i < 2; i++) {
        mSwapchains[i].handle = XR_NULL_HANDLE;
        mSwapchains[i].width = 0;
        mSwapchains[i].height = 0;
        mSwapchains[i].images.clear();
    }
    mCurrentPose.head.orientation[3] = 1.0f;
    mCurrentPose.eyes[0].orientation[3] = 1.0f;
    mCurrentPose.eyes[1].orientation[3] = 1.0f;
}

VRRuntime::~VRRuntime() {
    Shutdown();
}

bool VRRuntime::Init() {
    if (mInitialized) return true;

    SPDLOG_INFO("Initializing OpenXR...");

    // Enumerate Extensions
    uint32_t extCount = 0;
    XrResult res = xrEnumerateInstanceExtensionProperties(nullptr, 0, &extCount, nullptr);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("Failed to enumerate OpenXR extensions: {}", (int)res);
        return false;
    }

    std::vector<XrExtensionProperties> props(extCount, { XR_TYPE_EXTENSION_PROPERTIES });
    xrEnumerateInstanceExtensionProperties(nullptr, extCount, &extCount, props.data());
    
    SPDLOG_INFO("Available OpenXR Extensions:");
    bool hasD3D11 = false;
    for (const auto& p : props) {
        SPDLOG_INFO("  - {}", p.extensionName);
        if (strcmp(p.extensionName, XR_KHR_D3D11_ENABLE_EXTENSION_NAME) == 0) {
            hasD3D11 = true;
        }
    }

    if (!hasD3D11) {
        SPDLOG_ERROR("Required extension {} not supported by runtime", XR_KHR_D3D11_ENABLE_EXTENSION_NAME);
        return false;
    }

    if (!CreateInstance()) {
        return false;
    }

    // Get System
    XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
    systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrResult result = xrGetSystem(mInstance, &systemInfo, &mSystemId);
    if (XR_FAILED(result)) {
        SPDLOG_ERROR("Failed to get OpenXR system");
        return false;
    }

    SPDLOG_INFO("OpenXR System found!");

    if (!CreateSession()) {
        SPDLOG_ERROR("Failed to create OpenXR session");
        Shutdown();
        return false;
    }

    if (!CreateSwapchains()) {
        SPDLOG_ERROR("Failed to create OpenXR swapchains");
        Shutdown();
        return false;
    }

    mInitialized = true;
    return true;
}

void VRRuntime::Shutdown() {
    for (int i = 0; i < 2; i++) {
        if (mSwapchains[i].handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(mSwapchains[i].handle);
            mSwapchains[i].handle = XR_NULL_HANDLE;
        }
        mSwapchains[i].images.clear();
    }

    if (mSession != XR_NULL_HANDLE) {
        xrDestroySession(mSession);
        mSession = XR_NULL_HANDLE;
    }

    if (mInstance != XR_NULL_HANDLE) {
        xrDestroyInstance(mInstance);
        mInstance = XR_NULL_HANDLE;
    }

    mInitialized = false;
    SPDLOG_INFO("OpenXR Shutdown complete");
}

void VRRuntime::Update() {
    if (!mInitialized) return;

    XrEventDataBuffer event = { XR_TYPE_EVENT_DATA_BUFFER };
    while (xrPollEvent(mInstance, &event) == XR_SUCCESS) {
        switch (event.type) {
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                SPDLOG_WARN("OpenXR Instance loss pending");
                Shutdown();
                return;
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                auto stateEvent = reinterpret_cast<XrEventDataSessionStateChanged*>(&event);
                HandleSessionState(stateEvent->state);
                break;
            }
            default:
                break;
        }
        event = { XR_TYPE_EVENT_DATA_BUFFER };
    }

    // Basic frame loop
    if (mSessionState == XR_SESSION_STATE_READY || mSessionState == XR_SESSION_STATE_SYNCHRONIZED || 
        mSessionState == XR_SESSION_STATE_VISIBLE || mSessionState == XR_SESSION_STATE_FOCUSED) {
        
        XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState = { XR_TYPE_FRAME_STATE };
        xrWaitFrame(mSession, &waitInfo, &frameState);

        UpdatePose(frameState.predictedDisplayTime);

        XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
        xrBeginFrame(mSession, &beginInfo);

        XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
        endInfo.displayTime = frameState.predictedDisplayTime;
        endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endInfo.layerCount = 0;
        endInfo.layers = nullptr;
        xrEndFrame(mSession, &endInfo);
    }
}

static void CreateProjectionMatrix(float* m, const XrFovf fov, float nearZ, float farZ) {
    const float tanLeft = tanf(fov.angleLeft);
    const float tanRight = tanf(fov.angleRight);
    const float tanDown = tanf(fov.angleDown);
    const float tanUp = tanf(fov.angleUp);

    const float tanWidth = tanRight - tanLeft;
    const float tanHeight = tanUp - tanDown;

    // Matrix is intended for RowVector * Matrix multiplication
    // Column 0
    m[0] = 2.0f / tanWidth;
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;

    // Column 1
    m[4] = 0.0f;
    m[5] = 2.0f / tanHeight;
    m[6] = 0.0f;
    m[7] = 0.0f;

    // Column 2
    m[8] = (tanRight + tanLeft) / tanWidth;
    m[9] = (tanUp + tanDown) / tanHeight;
    m[10] = -(farZ + nearZ) / (farZ - nearZ);
    m[11] = -1.0f;

    // Column 3
    m[12] = 0.0f;
    m[13] = 0.0f;
    m[14] = -2.0f * farZ * nearZ / (farZ - nearZ);
    m[15] = 0.0f;
}

void VRRuntime::GetProjectionMatrix(int eye, float* m, float nearZ, float farZ) const {
    CreateProjectionMatrix(m, { mCurrentPose.fov[eye].angleLeft, mCurrentPose.fov[eye].angleRight, mCurrentPose.fov[eye].angleUp, mCurrentPose.fov[eye].angleDown }, nearZ, farZ);
}

static void QuaternionToMatrix(const float* q, float* m) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

    // Row-major matrix
    m[0] = 1.0f - (yy + zz); m[1] = xy + wz;          m[2] = xz - wy;          m[3] = 0.0f;
    m[4] = xy - wz;          m[5] = 1.0f - (xx + zz); m[6] = yz + wx;          m[7] = 0.0f;
    m[8] = xz + wy;          m[9] = yz - wx;          m[10] = 1.0f - (xx + yy); m[11] = 0.0f;
    m[12] = 0.0f;             m[13] = 0.0f;             m[14] = 0.0f;             m[15] = 1.0f;
}

void VRRuntime::GetViewMatrix(int eye, float* m) const {
    float rot[16];
    QuaternionToMatrix(mCurrentPose.eyes[eye].orientation, rot);

    float x = mCurrentPose.eyes[eye].position[0];
    float y = mCurrentPose.eyes[eye].position[1];
    float z = mCurrentPose.eyes[eye].position[2];

    // Transpose of rotation matrix (which is inverse for pure rotation)
    // [ m0 m1 m2  0 ]
    // [ m4 m5 m6  0 ]
    // [ m8 m9 m10 0 ]
    // [ m12 m13 m14 1 ]
    m[0] = rot[0]; m[1] = rot[4]; m[2] = rot[8];  m[3] = 0.0f;
    m[4] = rot[1]; m[5] = rot[5]; m[6] = rot[9];  m[7] = 0.0f;
    m[8] = rot[2]; m[9] = rot[6]; m[10] = rot[10]; m[11] = 0.0f;
    
    // Translation inverse: -dot(pos, col)
    m[12] = -(m[0] * x + m[4] * y + m[8] * z);
    m[13] = -(m[1] * x + m[5] * y + m[9] * z);
    m[14] = -(m[2] * x + m[6] * y + m[10] * z);
    m[15] = 1.0f;
}

void VRRuntime::UpdatePose(XrTime predictedTime) {
    XrViewLocateInfo locateInfo = { XR_TYPE_VIEW_LOCATE_INFO };
    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locateInfo.displayTime = predictedTime;
    locateInfo.space = mStageSpace;

    uint32_t viewCount = 0;
    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    xrLocateViews(mSession, &locateInfo, &viewState, 0, &viewCount, nullptr);
    
    std::vector<XrView> views(viewCount, { XR_TYPE_VIEW });
    xrLocateViews(mSession, &locateInfo, &viewState, viewCount, &viewCount, views.data());

    if (viewCount >= 2) {
        // Average eye positions for head pose
        mCurrentPose.head.position[0] = (views[0].pose.position.x + views[1].pose.position.x) * 0.5f;
        mCurrentPose.head.position[1] = (views[0].pose.position.y + views[1].pose.position.y) * 0.5f;
        mCurrentPose.head.position[2] = (views[0].pose.position.z + views[1].pose.position.z) * 0.5f;
        
        // Just use left eye orientation for head for now
        mCurrentPose.head.orientation[0] = views[0].pose.orientation.x;
        mCurrentPose.head.orientation[1] = views[0].pose.orientation.y;
        mCurrentPose.head.orientation[2] = views[0].pose.orientation.z;
        mCurrentPose.head.orientation[3] = views[0].pose.orientation.w;

        for (int i = 0; i < 2; i++) {
            mCurrentPose.eyes[i].position[0] = views[i].pose.position.x;
            mCurrentPose.eyes[i].position[1] = views[i].pose.position.y;
            mCurrentPose.eyes[i].position[2] = views[i].pose.position.z;
            mCurrentPose.eyes[i].orientation[0] = views[i].pose.orientation.x;
            mCurrentPose.eyes[i].orientation[1] = views[i].pose.orientation.y;
            mCurrentPose.eyes[i].orientation[2] = views[i].pose.orientation.z;
            mCurrentPose.eyes[i].orientation[3] = views[i].pose.orientation.w;

            mCurrentPose.fov[i].angleLeft = views[i].fov.angleLeft;
            mCurrentPose.fov[i].angleRight = views[i].fov.angleRight;
            mCurrentPose.fov[i].angleUp = views[i].fov.angleUp;
            mCurrentPose.fov[i].angleDown = views[i].fov.angleDown;
        }
    }

    mCurrentPose.displayTime = predictedTime;
}

bool VRRuntime::CreateInstance() {
    std::vector<const char*> extensions;
    extensions.push_back(XR_KHR_D3D11_ENABLE_EXTENSION_NAME);

    XrInstanceCreateInfo createInfo = { XR_TYPE_INSTANCE_CREATE_INFO };
    createInfo.next = nullptr;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.enabledExtensionNames = extensions.data();
    
    strcpy_s(createInfo.applicationInfo.applicationName, "libultraship VR");
    createInfo.applicationInfo.applicationVersion = 1;
    strcpy_s(createInfo.applicationInfo.engineName, "libultraship");
    createInfo.applicationInfo.engineVersion = 1;
    createInfo.applicationInfo.apiVersion = XR_API_VERSION_1_0;

    XrResult result = xrCreateInstance(&createInfo, &mInstance);
    if (XR_FAILED(result)) {
        SPDLOG_ERROR("xrCreateInstance failed with error: {}", (int)result);
        return false;
    }
    return true;
}

bool VRRuntime::CreateSession() {
    auto window = Context::GetInstance()->GetWindow();
    if (!window) return false;

    auto fastWindow = std::dynamic_pointer_cast<Fast::Fast3dWindow>(window);
    if (!fastWindow) return false;

    auto renderingApi = fastWindow->GetRenderingApi();
    if (!renderingApi) return false;

    ID3D11Device* device = (ID3D11Device*)renderingApi->GetDevice();
    if (!device) {
        SPDLOG_ERROR("Failed to retrieve D3D11 device from rendering API");
        return false;
    }

    // Mandatory: Get Graphics Requirements
    PFN_xrGetD3D11GraphicsRequirementsKHR pfnGetD3D11GraphicsRequirementsKHR = nullptr;
    xrGetInstanceProcAddr(mInstance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&pfnGetD3D11GraphicsRequirementsKHR);
    
    if (pfnGetD3D11GraphicsRequirementsKHR) {
        XrGraphicsRequirementsD3D11KHR graphicsRequirements = { XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR };
        XrResult res = pfnGetD3D11GraphicsRequirementsKHR(mInstance, mSystemId, &graphicsRequirements);
        if (XR_FAILED(res)) {
            SPDLOG_ERROR("xrGetD3D11GraphicsRequirementsKHR failed with error: {}", (int)res);
            return false;
        }
        SPDLOG_INFO("OpenXR Graphics Requirements satisfied (D3D11)");
    } else {
        SPDLOG_ERROR("Failed to load xrGetD3D11GraphicsRequirementsKHR function pointer");
        return false;
    }

    XrGraphicsBindingD3D11KHR graphicsBinding = { XR_TYPE_GRAPHICS_BINDING_D3D11_KHR };
    graphicsBinding.device = device;

    XrSessionCreateInfo createInfo = { XR_TYPE_SESSION_CREATE_INFO };
    createInfo.next = &graphicsBinding;
    createInfo.systemId = mSystemId;

    XrResult result = xrCreateSession(mInstance, &createInfo, &mSession);
    if (XR_FAILED(result)) {
        SPDLOG_ERROR("xrCreateSession failed with error: {}", (int)result);
        return false;
    }

    SPDLOG_INFO("OpenXR Session created!");

    // Create Space
    XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XrResult resultSpace = xrCreateReferenceSpace(mSession, &spaceInfo, &mStageSpace);
    if (XR_FAILED(resultSpace)) {
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        xrCreateReferenceSpace(mSession, &spaceInfo, &mStageSpace);
    }

    return true;
}

bool VRRuntime::CreateSwapchains() {
    // Enumerate formats
    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(mSession, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    xrEnumerateSwapchainFormats(mSession, formatCount, &formatCount, formats.data());

    SPDLOG_INFO("Supported OpenXR Swapchain Formats:");
    int64_t selectedFormat = -1;
    for (int64_t f : formats) {
        SPDLOG_INFO("  - {}", f);
        // Prefer R8G8B8A8_UNORM or B8G8R8A8_UNORM
        if (selectedFormat == -1 && (f == DXGI_FORMAT_R8G8B8A8_UNORM || f == DXGI_FORMAT_B8G8R8A8_UNORM || f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)) {
            selectedFormat = f;
        }
    }

    if (selectedFormat == -1) {
        SPDLOG_ERROR("No compatible DXGI swapchain format found");
        return false;
    }
    SPDLOG_INFO("Selected Swapchain Format: {}", selectedFormat);

    uint32_t configCount = 0;
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &configCount, nullptr);
    std::vector<XrViewConfigurationView> configViews(configCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, configCount, &configCount, configViews.data());

    for (int i = 0; i < 2; i++) {
        mSwapchains[i].width = configViews[i].recommendedImageRectWidth;
        mSwapchains[i].height = configViews[i].recommendedImageRectHeight;

        XrSwapchainCreateInfo createInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        createInfo.arraySize = 1;
        createInfo.format = selectedFormat;
        createInfo.width = mSwapchains[i].width;
        createInfo.height = mSwapchains[i].height;
        createInfo.mipCount = 1;
        createInfo.faceCount = 1;
        createInfo.sampleCount = 1;
        createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        XrResult result = xrCreateSwapchain(mSession, &createInfo, &mSwapchains[i].handle);
        if (XR_FAILED(result)) {
            SPDLOG_ERROR("xrCreateSwapchain failed for eye {} with error: {}", i, (int)result);
            return false;
        }

        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(mSwapchains[i].handle, 0, &imageCount, nullptr);
        std::vector<XrSwapchainImageD3D11KHR> images(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
        xrEnumerateSwapchainImages(mSwapchains[i].handle, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());

        for (const auto& img : images) {
            mSwapchains[i].images.push_back(img.texture);
        }
        
        SPDLOG_INFO("Created Swapchain for eye {} ({}x{}) with {} images", i, mSwapchains[i].width, mSwapchains[i].height, imageCount);
    }

    return true;
}

void VRRuntime::HandleSessionState(XrSessionState state) {
    mSessionState = state;
    SPDLOG_INFO("OpenXR Session State -> {}", (int)state);

    switch (state) {
        case XR_SESSION_STATE_READY: {
            XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
            beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
            xrBeginSession(mSession, &beginInfo);
            break;
        }
        case XR_SESSION_STATE_STOPPING:
            xrEndSession(mSession);
            break;
        case XR_SESSION_STATE_EXITING:
        case XR_SESSION_STATE_LOSS_PENDING:
            // Handle restart or exit
            break;
        default:
            break;
    }
}

} // namespace Ship
