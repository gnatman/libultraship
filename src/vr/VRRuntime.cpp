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

namespace Ship {

std::shared_ptr<VRRuntime> VRRuntime::mInstancePtr = nullptr;

std::shared_ptr<VRRuntime> VRRuntime::GetInstance() {
    if (mInstancePtr == nullptr) {
        mInstancePtr = std::make_shared<VRRuntime>();
    }
    return mInstancePtr;
}

VRRuntime::VRRuntime() {}

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
    mInitialized = true;
    return true;
}

void VRRuntime::Shutdown() {
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

bool VRRuntime::CreateInstance() {
    std::vector<const char*> extensions;
    // We'll need XR_KHR_D3D11_ENABLE for Phase 3
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
