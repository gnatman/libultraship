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
#include "fast/backends/gfx_direct3d_common.h"

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
        mSwapchains[i].rtvs.clear();
        mSwapchains[i].dsvs.clear();
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
}

bool VRRuntime::BeginFrame() {
    if (!mInitialized) return false;

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

    std::vector<XrCompositionLayerBaseHeader*> layers;
    XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    std::vector<XrCompositionLayerProjectionView> projectionViews(2, { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW });

    if (mFrameState.shouldRender) {
        for (int i = 0; i < 2; i++) {
            // Submit the actual pose the image was rendered from for correct TimeWarp calculation
            if (i < mViews.size()) {
                projectionViews[i].pose = mViews[i].pose;
            } else {
                projectionViews[i].pose.position = { 0, 0, 0 };
                projectionViews[i].pose.orientation = { 0, 0, 0, 1 };
            }
            
            projectionViews[i].fov.angleLeft = mCurrentPose.fov[i].angleLeft;
            projectionViews[i].fov.angleRight = mCurrentPose.fov[i].angleRight;
            projectionViews[i].fov.angleUp = mCurrentPose.fov[i].angleUp;
            projectionViews[i].fov.angleDown = mCurrentPose.fov[i].angleDown;

            projectionViews[i].subImage.swapchain = mSwapchains[i].handle;
            projectionViews[i].subImage.imageRect.offset = { 0, 0 };
            projectionViews[i].subImage.imageRect.extent = { mSwapchains[i].width, mSwapchains[i].height };
        }

        // Switch to LOCAL space for layer submission to fix black screen/clipping
        layer.space = mStageSpace; // Wait, I created mStageSpace as STAGE or LOCAL.
        layer.viewCount = 2;
        layer.views = projectionViews.data();
        layers.push_back((XrCompositionLayerBaseHeader*)&layer);
    }

    XrFrameEndInfo endInfo = { XR_TYPE_FRAME_END_INFO };
    endInfo.displayTime = mFrameState.predictedDisplayTime;
    endInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    endInfo.layerCount = (uint32_t)layers.size();
    endInfo.layers = (layers.size() > 0) ? layers.data() : nullptr;
    
    XrResult res = xrEndFrame(mSession, &endInfo);
    if (XR_FAILED(res)) {
        SPDLOG_ERROR("xrEndFrame failed with error: {}", (int)res);
    }

    static int frameCounter = 0;
    if (frameCounter++ % 200 == 0) {
        SPDLOG_INFO("OpenXR Frame Submitted (Layers: {}, Render: {})", layers.size(), mFrameState.shouldRender);
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
    float x2 = x + x, y2 = y + y, z2 = z + z;
    float xx = x * x2, xy = x * y2, xz = x * z2;
    float yy = y * y2, yz = y * z2, zz = z * z2;
    float wx = w * x2, wy = w * y2, wz = w * z2;

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
    XrViewState viewState = { XR_TYPE_VIEW_STATE };
    xrLocateViews(mSession, &locateInfo, &viewState, 0, &viewCount, nullptr);
    
    std::vector<XrView> views(viewCount, { XR_TYPE_VIEW });
    xrLocateViews(mSession, &locateInfo, &viewState, viewCount, &viewCount, views.data());

    if (viewCount >= 2) {
        mCurrentPose.head.position[0] = (views[0].pose.position.x + views[1].pose.position.x) * 0.5f;
        mCurrentPose.head.position[1] = (views[0].pose.position.y + views[1].pose.position.y) * 0.5f;
        mCurrentPose.head.position[2] = (views[0].pose.position.z + views[1].pose.position.z) * 0.5f;
        
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

    XrReferenceSpaceCreateInfo spaceInfo = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL; // Use LOCAL for head-relative submission
    spaceInfo.poseInReferenceSpace.orientation.w = 1.0f;
    XrResult resultSpace = xrCreateReferenceSpace(mSession, &spaceInfo, &mStageSpace);
    if (XR_FAILED(resultSpace)) {
        SPDLOG_ERROR("Failed to create Local reference space");
        return false;
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
        // FORCE SRGB. If not SRGB, SteamVR often rejects the frame or renders black.
        if (f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB) {
            selectedFormat = f;
            break;
        }
    }

    if (selectedFormat == -1) {
        // Fallback
        for (int64_t f : formats) {
            if (f == DXGI_FORMAT_R8G8B8A8_UNORM || f == DXGI_FORMAT_B8G8R8A8_UNORM) {
                selectedFormat = f;
                break;
            }
        }
    }

    if (selectedFormat == -1) return false;
    SPDLOG_INFO("Selected Swapchain Format: {}", selectedFormat);


    uint32_t configCount = 0;
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, 0, &configCount, nullptr);
    std::vector<XrViewConfigurationView> configViews(configCount, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
    xrEnumerateViewConfigurationViews(mInstance, mSystemId, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, configCount, &configCount, configViews.data());

    auto window = Context::GetInstance()->GetWindow();
    auto fastWindow = std::dynamic_pointer_cast<Fast::Fast3dWindow>(window);
    auto device = (ID3D11Device*)fastWindow->GetRenderingApi()->GetDevice();

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
                // Ensure the texture is unbound before releasing to the compositor
                ID3D11RenderTargetView* nullRTV = nullptr;
                context->OMSetRenderTargets(1, &nullRTV, nullptr);
                context->Flush();
            }
        }
    }

    XrSwapchainImageReleaseInfo releaseInfo = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    xrReleaseSwapchainImage(mSwapchains[eye].handle, &releaseInfo);
}

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
