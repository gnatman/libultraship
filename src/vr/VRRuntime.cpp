#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <unknwn.h>
#include <d3d11.h>

#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32
#endif
#ifndef XR_USE_GRAPHICS_API_D3D11
#define XR_USE_GRAPHICS_API_D3D11
#endif
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include "vr/VRRuntime.h"
#include "vr/VRQuadLayer.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <cmath>
#include <vector>

#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include "libultraship/bridge/consolevariablebridge.h"
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
        mSwapchains[i].rtvs.clear();
        mSwapchains[i].dsvs.clear();
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

    // Register CVars with defaults
    auto cvars = Context::GetInstance()->GetConsoleVariables();
    // VR CVars - Migration & Initialization
    bool migrated = false;
    auto cvars_ptr = Context::GetInstance()->GetConsoleVariables();
    
    auto migrate = [&](const char* oldName, const char* newName, float def) {
        if (cvars_ptr->Get(oldName) != nullptr) {
            float val = cvars_ptr->GetFloat(oldName, def);
            cvars_ptr->SetFloat(newName, val);
            cvars_ptr->ClearVariable(oldName);
            SPDLOG_INFO("Migrated CVar {} -> {} (value: {})", oldName, newName, val);
            migrated = true;
        }
    };

    migrate("gVR.WorldScale", "gVRWorldScale", 1.0f);
    migrate("gVR.IPDScale", "gVRIPDScale", 1.0f);
    migrate("gVR.HUDDistance", "gVRHUDDistance", 1.5f);
    migrate("gVR.HUDWidth", "gVRHUDWidth", 1.0f);
    migrate("gVR.HUDScale", "gVRHUDScale", 1.0f);

    if (migrated) {
        cvars_ptr->Save();
    }

    CVarRegisterFloat("gVRWorldScale", 1.0f);
    CVarRegisterFloat("gVRIPDScale", 1.0f);
    CVarRegisterFloat("gVRHUDDistance", 1.5f);
    CVarRegisterFloat("gVRHUDWidth", 1.0f);
    CVarRegisterFloat("gVRHUDScale", 1.0f);
    
    spdlog::critical("VRRuntime Init Complete. Live Values - World: {}, IPD: {}", 
        CVarGetFloat("gVRWorldScale", 1.0f), 
        CVarGetFloat("gVRIPDScale", 1.0f));
    cvars->RegisterInteger("gVRPerformanceOverlay", 0);

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
    SPDLOG_INFO("OpenXR Shutdown starting...");

    auto window = Context::GetInstance()->GetWindow();
    if (window) {
        auto fastWindow = std::dynamic_pointer_cast<Fast::Fast3dWindow>(window);
        if (fastWindow && fastWindow->GetRenderingApi()) {
            fastWindow->GetRenderingApi()->SetOverrideRenderTarget(nullptr, nullptr, 0, 0);
        }
    }

    for (int i = 0; i < 2; i++) {
        if (mSwapchains[i].handle != XR_NULL_HANDLE) {
            xrDestroySwapchain(mSwapchains[i].handle);
            mSwapchains[i].handle = XR_NULL_HANDLE;
        }
        for (auto rtv : mSwapchains[i].rtvs) if (rtv) rtv->Release();
        for (auto dsv : mSwapchains[i].dsvs) if (dsv) dsv->Release();
        for (auto srv : mSwapchains[i].srvs) if (srv) srv->Release();
        mSwapchains[i].rtvs.clear();
        mSwapchains[i].dsvs.clear();
        mSwapchains[i].srvs.clear();
        mSwapchains[i].images.clear();
    }

    mQuadLayers.clear();

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
}

bool VRRuntime::BeginFrame() {
    if (!mInitialized) return false;
    mFrameCounter++;

    if (mSessionState < XR_SESSION_STATE_READY || mSessionState > XR_SESSION_STATE_FOCUSED) {
        return false;
    }

    XrFrameWaitInfo waitInfo = { XR_TYPE_FRAME_WAIT_INFO };
    mFrameState = { XR_TYPE_FRAME_STATE };
    XrResult res = xrWaitFrame(mSession, &waitInfo, &mFrameState);
    if (XR_FAILED(res)) return false;

    XrFrameBeginInfo beginInfo = { XR_TYPE_FRAME_BEGIN_INFO };
    res = xrBeginFrame(mSession, &beginInfo);
    if (XR_FAILED(res)) return false;

    if (mFrameState.shouldRender) {
        UpdatePose(mFrameState.predictedDisplayTime);
    }

    return true;
}

void VRRuntime::EndFrame() {
    if (!mInitialized) return;

    // Use a fixed-size array for layer headers to ensure pointers stay valid for xrEndFrame
    XrCompositionLayerBaseHeader* layerPtrs[16]; // Maximum 16 layers
    uint32_t layerCount = 0;

    XrCompositionLayerProjection projectionLayer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    projectionLayer.layerFlags = 0;
    projectionLayer.space = mStageSpace;

    std::vector<XrCompositionLayerProjectionView> projectionViews(2, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });

    if (mFrameState.shouldRender && mStageSpace != XR_NULL_HANDLE) {
        for (int i = 0; i < 2; i++) {
            projectionViews[i].pose = mViews[i].pose;
            projectionViews[i].fov.angleLeft = mCurrentPose.fov[i].angleLeft;
            projectionViews[i].fov.angleRight = mCurrentPose.fov[i].angleRight;
            projectionViews[i].fov.angleUp = mCurrentPose.fov[i].angleUp;
            projectionViews[i].fov.angleDown = mCurrentPose.fov[i].angleDown;
            projectionViews[i].subImage.swapchain = mSwapchains[i].handle;
            projectionViews[i].subImage.imageRect.offset = { 0, 0 };
            projectionViews[i].subImage.imageRect.extent = { mSwapchains[i].width, mSwapchains[i].height };
            projectionViews[i].subImage.imageArrayIndex = 0;
        }

        projectionLayer.viewCount = 2;
        projectionLayer.views = projectionViews.data();
        layerPtrs[layerCount++] = (XrCompositionLayerBaseHeader*)&projectionLayer;

        // Submit up to 14 quad layers (reserved 2 slots for projection and extra)
        for (const auto& quad : mQuadLayers) {
            if (quad && quad->IsValid() && layerCount < 16) {
                // Copy the struct locally to the frame stack to ensure persistence during the call
                static XrCompositionLayerQuad quadStructs[14]; 
                int idx = layerCount - 1;
                quadStructs[idx] = quad->GetCompositionLayer(mViewSpace);
                layerPtrs[layerCount++] = (XrCompositionLayerBaseHeader*)&quadStructs[idx];
            }
        }
    }

    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    endInfo.displayTime = mFrameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = layerCount;
    endInfo.layers = (layerCount > 0) ? layerPtrs : nullptr;
    
    static int frameCheck = 0;
    if (frameCheck++ % 100 == 0) {
        SPDLOG_INFO("VR Submission Heartbeat - Layers: {}, ShouldRender: {}, StageSpace: {}", 
            layerCount, (int)mFrameState.shouldRender, (void*)mStageSpace);
    }

    XrResult res = xrEndFrame(mSession, &endInfo);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrEndFrame failed with error: {}", (int)res);
    }
}

static void CreateProjectionMatrix(float* m, const XrFovf fov, float nearZ, float farZ) {
    const float tanLeft = tanf(fov.angleLeft);
    const float tanRight = tanf(fov.angleRight);
    const float tanDown = tanf(fov.angleDown);
    const float tanUp = tanf(fov.angleUp);

    const float tanWidth = tanRight - tanLeft;
    const float tanHeight = tanUp - tanDown;

    m[0] = 2.0f / tanWidth;
    m[1] = 0.0f;
    m[2] = 0.0f;
    m[3] = 0.0f;

    m[4] = 0.0f;
    m[5] = 2.0f / tanHeight;
    m[6] = 0.0f;
    m[7] = 0.0f;

    m[8] = (tanRight + tanLeft) / tanWidth;
    m[9] = (tanUp + tanDown) / tanHeight;
    m[10] = -(farZ + nearZ) / (farZ - nearZ);
    m[11] = -1.0f;

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
    m[0] = 1 - 2 * (y * y + z * z); m[1] = 2 * (x * y - z * w);     m[2] = 2 * (x * z + y * w);     m[3] = 0;
    m[4] = 2 * (x * y + z * w);     m[5] = 1 - 2 * (x * x + z * z); m[6] = 2 * (y * z - x * w);     m[7] = 0;
    m[8] = 2 * (x * z - y * w);     m[9] = 2 * (y * z + x * w);     m[10] = 1 - 2 * (x * x + y * y); m[11] = 0;
    m[12] = 0;                      m[13] = 0;                      m[14] = 0;                      m[15] = 1;
}

static void QuaternionMultiply(const float* q1, const float* q2, float* out) {
    out[0] = q1[3] * q2[0] + q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1];
    out[1] = q1[3] * q2[1] + q1[1] * q2[3] + q1[2] * q2[0] - q1[0] * q2[2];
    out[2] = q1[3] * q2[2] + q1[2] * q2[3] + q1[0] * q2[1] - q1[1] * q2[0];
    out[3] = q1[3] * q2[3] - q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2];
}

static void QuaternionRotateVector(const float* q, const float* v, float* out) {
    float qv[4] = { v[0], v[1], v[2], 0.0f };
    float q_inv[4] = { -q[0], -q[1], -q[2], q[3] };
    float tmp[4];
    QuaternionMultiply(q, qv, tmp);
    QuaternionMultiply(tmp, q_inv, qv);
    out[0] = qv[0];
    out[1] = qv[1];
    out[2] = qv[2];
}

void VRRuntime::SetBaseTrackingSpace(const float* pos, const float* rotQuat) {
    mBasePosition[0] = pos[0];
    mBasePosition[1] = pos[1];
    mBasePosition[2] = pos[2];
    mBaseRotation[0] = rotQuat[0];
    mBaseRotation[1] = rotQuat[1];
    mBaseRotation[2] = rotQuat[2];
    mBaseRotation[3] = rotQuat[3];
}

void VRRuntime::DrawPerformanceOverlay() {
    auto cvars = Context::GetInstance()->GetConsoleVariables();
    if (!cvars->GetInteger("gVRPerformanceOverlay", 0)) return;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("VR Performance", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav)) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 1.0f, 1.0f), "VR RUNTIME STATUS");
        ImGui::Separator();
        ImGui::Text("Session State: %d", (int)mSessionState);
        ImGui::Text("Eye Resolution: %dx%d", mSwapchains[0].width, mSwapchains[0].height);
        ImGui::Text("World Scale: %.2fx", cvars->GetFloat("gVRWorldScale", 1.0f));
        ImGui::Text("IPD Scale: %.2fx", cvars->GetFloat("gVRIPDScale", 1.0f));
        
        static float frameTimes[120] = {0};
        static int offset = 0;
        frameTimes[offset] = ImGui::GetIO().DeltaTime * 1000.0f;
        offset = (offset + 1) % 120;
        
        float avg = 0;
        for (int i = 0; i < 120; i++) avg += frameTimes[i];
        avg /= 120.0f;
        
        ImGui::Text("Avg Frame Time: %.2f ms (%.1f FPS)", avg, 1000.0f / avg);
        ImGui::PlotLines("##FrameTimes", frameTimes, 120, offset, nullptr, 0.0f, 33.3f, ImVec2(0, 40));
    }
    ImGui::End();
}

void VRRuntime::GetViewMatrix(int eye, float* m) const {
    auto cvars = Context::GetInstance()->GetConsoleVariables();
    
    // Read CVars with fallback to old names if they somehow survived or new ones are default
    float worldScale = CVarGetFloat("gVRWorldScale", 1.0f);
    if (worldScale == 1.0f) worldScale = CVarGetFloat("gVR.WorldScale", 1.0f);

    float ipdScale = CVarGetFloat("gVRIPDScale", 1.0f);
    if (ipdScale == 1.0f) ipdScale = CVarGetFloat("gVR.IPDScale", 1.0f);

    if (mFrameCounter % 500 == 0) {
        spdlog::critical("VR Live Sync [{}]: World={}, IPD={}", mFrameCounter, worldScale, ipdScale);
    }

    // 1. Calculate eye rotation in world space: Q_world = Q_base * Q_eye_local
    float q_eye_world[4];
    QuaternionMultiply(mBaseRotation, mCurrentPose.eyes[eye].orientation, q_eye_world);

    // 2. Convert to matrix (this handles the rotation part of the view matrix)
    float rot[16];
    QuaternionToMatrix(q_eye_world, rot);

    // 3. Calculate eye position in world space
    // Eye offset relative to head center in tracking space (scaled by IPD)
    float eyeOffsetLocal[3] = {
        (mCurrentPose.eyes[eye].position[0] - mCurrentPose.head.position[0]) * ipdScale,
        (mCurrentPose.eyes[eye].position[1] - mCurrentPose.head.position[1]) * ipdScale,
        (mCurrentPose.eyes[eye].position[2] - mCurrentPose.head.position[2]) * ipdScale
    };

    // Head position in tracking space scaled by world scale
    float headPosLocalScaled[3] = {
        mCurrentPose.head.position[0] * worldScale,
        mCurrentPose.head.position[1] * worldScale,
        mCurrentPose.head.position[2] * worldScale
    };

    // Total local eye position relative to tracking origin
    float eyePosLocal[3] = {
        headPosLocalScaled[0] + eyeOffsetLocal[0],
        headPosLocalScaled[1] + eyeOffsetLocal[1],
        headPosLocalScaled[2] + eyeOffsetLocal[2]
    };

    // Rotate local eye position by base rotation
    float eyePosWorldOffset[3];
    QuaternionRotateVector(mBaseRotation, eyePosLocal, eyePosWorldOffset);

    // Add base position to get final world position
    float x = mBasePosition[0] + eyePosWorldOffset[0];
    float y = mBasePosition[1] + eyePosWorldOffset[1];
    float z = mBasePosition[2] + eyePosWorldOffset[2];

    m[0] = rot[0]; m[1] = rot[4]; m[2] = rot[8];  m[3] = 0.0f;
    m[4] = rot[1]; m[5] = rot[5]; m[6] = rot[9];  m[7] = 0.0f;
    m[8] = rot[2]; m[9] = rot[6]; m[10] = rot[10]; m[11] = 0.0f;
    
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
    mViewState.type = XR_TYPE_VIEW_STATE;
    xrLocateViews(mSession, &locateInfo, &mViewState, 0, &viewCount, nullptr);
    
    mViews.assign(viewCount, { XR_TYPE_VIEW });
    xrLocateViews(mSession, &locateInfo, &mViewState, viewCount, &viewCount, mViews.data());

    if (viewCount >= 2) {
        mCurrentPose.head.position[0] = (mViews[0].pose.position.x + mViews[1].pose.position.x) * 0.5f;
        mCurrentPose.head.position[1] = (mViews[0].pose.position.y + mViews[1].pose.position.y) * 0.5f;
        mCurrentPose.head.position[2] = (mViews[0].pose.position.z + mViews[1].pose.position.z) * 0.5f;
        
        mCurrentPose.head.orientation[0] = mViews[0].pose.orientation.x;
        mCurrentPose.head.orientation[1] = mViews[0].pose.orientation.y;
        mCurrentPose.head.orientation[2] = mViews[0].pose.orientation.z;
        mCurrentPose.head.orientation[3] = mViews[0].pose.orientation.w;

        for (int i = 0; i < 2; i++) {
            mCurrentPose.eyes[i].position[0] = mViews[i].pose.position.x;
            mCurrentPose.eyes[i].position[1] = mViews[i].pose.position.y;
            mCurrentPose.eyes[i].position[2] = mViews[i].pose.position.z;
            mCurrentPose.eyes[i].orientation[0] = mViews[i].pose.orientation.x;
            mCurrentPose.eyes[i].orientation[1] = mViews[i].pose.orientation.y;
            mCurrentPose.eyes[i].orientation[2] = mViews[i].pose.orientation.z;
            mCurrentPose.eyes[i].orientation[3] = mViews[i].pose.orientation.w;

            mCurrentPose.fov[i].angleLeft = mViews[i].fov.angleLeft;
            mCurrentPose.fov[i].angleRight = mViews[i].fov.angleRight;
            mCurrentPose.fov[i].angleUp = mViews[i].fov.angleUp;
            mCurrentPose.fov[i].angleDown = mViews[i].fov.angleDown;
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

    XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL; 
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XrResult resultSpace = xrCreateReferenceSpace(mSession, &spaceInfo, &mStageSpace);
    if (XR_FAILED(resultSpace)) {
        SPDLOG_ERROR("Failed to create Local reference space");
        return false;
    }

    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    xrCreateReferenceSpace(mSession, &spaceInfo, &mViewSpace);

    return true;
}

bool VRRuntime::CreateSwapchains() {
    uint32_t formatCount = 0;
    xrEnumerateSwapchainFormats(mSession, 0, &formatCount, nullptr);
    std::vector<int64_t> formats(formatCount);
    xrEnumerateSwapchainFormats(mSession, formatCount, &formatCount, formats.data());

    int64_t selectedFormat = -1;
    for (int64_t f : formats) {
        if (f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            selectedFormat = f;
            break;
        }
    }

    if (selectedFormat == -1) {
        for (int64_t f : formats) {
            if (f == DXGI_FORMAT_R8G8B8A8_UNORM || f == DXGI_FORMAT_B8G8R8A8_UNORM) {
                selectedFormat = f;
                break;
            }
        }
    }

    if (selectedFormat == -1) return false;

    uint32_t configCount = 0;
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &configCount, nullptr);
    std::vector<XrViewConfigurationView> configViews(configCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, configCount, &configCount, configViews.data());

    auto window = Context::GetInstance()->GetWindow();
    auto fastWindow = std::dynamic_pointer_cast<Fast::Fast3dWindow>(window);
    auto device = (ID3D11Device*)fastWindow->GetRenderingApi()->GetDevice();

    auto cvars = Context::GetInstance()->GetConsoleVariables();
    float supersampling = cvars->GetFloat("gVRSupersampling", 1.0f);
    int msaa = cvars->GetInteger("gVRMSAA", 1);

    for (int i = 0; i < 2; i++) {
        mSwapchains[i].width = (int32_t)(configViews[i].recommendedImageRectWidth * supersampling);
        mSwapchains[i].height = (int32_t)(configViews[i].recommendedImageRectHeight * supersampling);

        XrSwapchainCreateInfo createInfo = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        createInfo.arraySize = 1;
        createInfo.format = selectedFormat;
        createInfo.width = mSwapchains[i].width;
        createInfo.height = mSwapchains[i].height;
        createInfo.mipCount = 1;
        createInfo.faceCount = 1;
        createInfo.sampleCount = (uint32_t)msaa;
        createInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;

        xrCreateSwapchain(mSession, &createInfo, &mSwapchains[i].handle);

        uint32_t imageCount = 0;
        xrEnumerateSwapchainImages(mSwapchains[i].handle, 0, &imageCount, nullptr);
        std::vector<XrSwapchainImageD3D11KHR> images(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
        xrEnumerateSwapchainImages(mSwapchains[i].handle, imageCount, &imageCount, (XrSwapchainImageBaseHeader*)images.data());

        for (const auto& img : images) {
            mSwapchains[i].images.push_back(img.texture);
            
            D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
            rtvDesc.Format = (DXGI_FORMAT)selectedFormat;
            rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
            
            ID3D11RenderTargetView* rtv = nullptr;
            device->CreateRenderTargetView(img.texture, &rtvDesc, &rtv);
            mSwapchains[i].rtvs.push_back(rtv);

            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = (DXGI_FORMAT)selectedFormat;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            
            ID3D11ShaderResourceView* srv = nullptr;
            device->CreateShaderResourceView(img.texture, &srvDesc, &srv);
            mSwapchains[i].srvs.push_back(srv);

            D3D11_TEXTURE2D_DESC depthDesc = {};
            depthDesc.Width = mSwapchains[i].width;
            depthDesc.Height = mSwapchains[i].height;
            depthDesc.MipLevels = 1;
            depthDesc.ArraySize = 1;
            depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            depthDesc.SampleDesc.Count = 1;
            depthDesc.Usage = D3D11_USAGE_DEFAULT;
            depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

            ID3D11Texture2D* depthTex = nullptr;
            device->CreateTexture2D(&depthDesc, nullptr, &depthTex);
            ID3D11DepthStencilView* dsv = nullptr;
            device->CreateDepthStencilView(depthTex, nullptr, &dsv);
            mSwapchains[i].dsvs.push_back(dsv);
            depthTex->Release();
        }
    }
    return true;
}

uint32_t VRRuntime::AcquireImage(int eye) {
    uint32_t index = 0;
    XrSwapchainImageAcquireInfo acquireInfo = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    xrAcquireSwapchainImage(mSwapchains[eye].handle, &acquireInfo, &index);
    XrSwapchainImageWaitInfo waitInfo = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
    waitInfo.timeout = XR_INFINITE_DURATION;
    xrWaitSwapchainImage(mSwapchains[eye].handle, &waitInfo);
    return index;
}

void VRRuntime::ReleaseImage(int eye) {
    auto window = Context::GetInstance()->GetWindow();
    if (window) {
        auto fastWindow = std::dynamic_pointer_cast<Fast::Fast3dWindow>(window);
        if (fastWindow && fastWindow->GetRenderingApi()) {
            auto context = static_cast<ID3D11DeviceContext*>(fastWindow->GetRenderingApi()->GetContext());
            if (context) {
                ID3D11RenderTargetView* nullRTV = nullptr;
                context->OMSetRenderTargets(1, &nullRTV, nullptr);
                context->Flush();
            }
        }
    }

    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    xrReleaseSwapchainImage(mSwapchains[eye].handle, &releaseInfo);
}

int VRRuntime::CreateQuadLayer(int32_t width, int32_t height) {
    auto layer = std::make_shared<VRQuadLayer>(mSession, width, height, XR_EYE_VISIBILITY_BOTH);
    mQuadLayers.push_back(layer);
    return (int)mQuadLayers.size() - 1;
}

uint32_t VRRuntime::AcquireQuadImage(int layerIndex) {
    return mQuadLayers[layerIndex]->AcquireImage();
}

void VRRuntime::ReleaseQuadImage(int layerIndex) {
    mQuadLayers[layerIndex]->ReleaseImage();
}

void* VRRuntime::GetQuadRTV(int layerIndex, uint32_t imageIndex) const {
    if (layerIndex < 0 || layerIndex >= (int)mQuadLayers.size()) {
        static int errCount = 0;
        if (errCount++ % 500 == 0) SPDLOG_ERROR("GetQuadRTV: layerIndex {} out of bounds (size {})", layerIndex, mQuadLayers.size());
        return nullptr;
    }
    if (!mQuadLayers[layerIndex]->IsValid()) {
        static int errCount = 0;
        if (errCount++ % 500 == 0) SPDLOG_ERROR("GetQuadRTV: layer {} is INVALID", layerIndex);
        return nullptr;
    }
    void* rtv = mQuadLayers[layerIndex]->GetRTV(imageIndex);
    if (!rtv) {
        static int errCount = 0;
        if (errCount++ % 500 == 0) SPDLOG_ERROR("GetQuadRTV: layer {} RTV for image {} is NULL", layerIndex, imageIndex);
    }
    return rtv;
}

void* VRRuntime::GetQuadDSV(int layerIndex, uint32_t imageIndex) const {
    if (layerIndex < 0 || layerIndex >= (int)mQuadLayers.size()) return nullptr;
    if (!mQuadLayers[layerIndex]->IsValid()) return nullptr;
    return mQuadLayers[layerIndex]->GetDSV(imageIndex);
}

void* VRRuntime::GetQuadSRV(int layerIndex, uint32_t imageIndex) const {
    if (layerIndex < 0 || layerIndex >= (int)mQuadLayers.size()) return nullptr;
    if (!mQuadLayers[layerIndex]->IsValid()) return nullptr;
    return mQuadLayers[layerIndex]->GetSRV(imageIndex);
}

void VRRuntime::GetQuadDimensions(int layerIndex, int32_t* w, int32_t* h) const {
    mQuadLayers[layerIndex]->GetDimensions(w, h);
}

void VRRuntime::SetQuadPose(int layerIndex, XrPosef pose) {
    mQuadLayers[layerIndex]->SetPose(pose);
}

void VRRuntime::SetQuadSize(int layerIndex, XrExtent2Df size) {
    mQuadLayers[layerIndex]->SetSize(size);
}

#include <imgui.h>

void VRRuntime::HandleSessionState(XrSessionState state) {
    mSessionState = state;
    SPDLOG_INFO("OpenXR Session State -> {}", (int)state);
    if (state == XR_SESSION_STATE_READY) {
        XrSessionBeginInfo beginInfo = { XR_TYPE_SESSION_BEGIN_INFO };
        beginInfo.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        xrBeginSession(mSession, &beginInfo);
    } else if (state == XR_SESSION_STATE_STOPPING) {
        xrEndSession(mSession);
    }
}

} // namespace Ship
