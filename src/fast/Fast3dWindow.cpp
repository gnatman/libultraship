#include "fast/Fast3dWindow.h"
#include "vr/MockVRPose.h"
#include "vr/VRToggle.h"
#include "vr/VRRuntime.h"

#include "ship/Context.h"
#include "ship/config/Config.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include "ship/config/ConsoleVariable.h"
#include "libultraship/bridge/consolevariablebridge.h"
#include "fast/interpreter.h"
#include "fast/backends/gfx_sdl.h"
#include "fast/backends/gfx_dxgi.h"
#include "fast/backends/gfx_opengl.h"
#include "fast/backends/gfx_metal.h"
#include "fast/backends/gfx_direct3d_common.h"
#include "fast/backends/gfx_direct3d11.h"
#include "fast/backends/gfx_window_manager_api.h"

#include "fast/Fast3dGui.h"

#include <fstream>

namespace Fast {

extern void GfxSetInstance(std::shared_ptr<Interpreter> gfx);

Fast3dWindow::Fast3dWindow(std::shared_ptr<Ship::Gui> gui, std::shared_ptr<FastMouseStateManager> mouseStateManager)
    : Ship::Window(gui, mouseStateManager) {
    mWindowManagerApi = nullptr;
    mRenderingApi = nullptr;
    mInterpreter = std::make_shared<Interpreter>();
    GfxSetInstance(mInterpreter);
    mMockVRPose = std::make_shared<Ship::MockVRPose>();

#ifdef _WIN32
    AddAvailableWindowBackend(WindowBackend::FAST3D_DXGI_DX11);
#endif
#ifdef __APPLE__
    if (Metal_IsSupported()) {
        AddAvailableWindowBackend(WindowBackend::FAST3D_SDL_METAL);
    }
#endif
    AddAvailableWindowBackend(WindowBackend::FAST3D_SDL_OPENGL);
}

Fast3dWindow::Fast3dWindow(std::shared_ptr<Ship::Gui> gui)
    : Fast3dWindow(gui, std::make_shared<FastMouseStateManager>()) {
}

Fast3dWindow::Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows)
    : Fast3dWindow(std::make_shared<Fast3dGui>(guiWindows)) {
}

Fast3dWindow::Fast3dWindow() : Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>>()) {
}

Fast3dWindow::~Fast3dWindow() {
    SPDLOG_DEBUG("destruct fast3dwindow");
    mInterpreter->Destroy();
    delete mRenderingApi;
    delete mWindowManagerApi;
}

void Fast3dWindow::Init() {
    bool gameMode = false;

#ifdef __linux__
    std::ifstream osReleaseFile("/etc/os-release");
    if (osReleaseFile.is_open()) {
        std::string line;
        while (std::getline(osReleaseFile, line)) {
            if (line.find("VARIANT_ID") != std::string::npos) {
                if (line.find("steamdeck") != std::string::npos) {
                    gameMode = std::getenv("XDG_CURRENT_DESKTOP") != nullptr &&
                               std::string(std::getenv("XDG_CURRENT_DESKTOP")) == "gamescope";
                }
                break;
            }
        }
    }
#elif defined(__ANDROID__) || defined(__IOS__)
    gameMode = true;
#endif

    bool isFullscreen;
    uint32_t width, height;
    int32_t posX, posY;

    isFullscreen = Ship::Context::GetInstance()->GetConfig()->GetBool("Window.Fullscreen.Enabled", false) || gameMode;
    posX = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.PositionX", 100);
    posY = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.PositionY", 100);

    if (isFullscreen) {
        width = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Width", gameMode ? 1280 : 1920);
        height = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Fullscreen.Height", gameMode ? 800 : 1080);
    } else {
        width = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Width", 640);
        height = Ship::Context::GetInstance()->GetConfig()->GetInt("Window.Height", 480);
    }
    Ship::Context::GetInstance()->GetWindow()->SetFullscreenScancode(
        Ship::Context::GetInstance()->GetConfig()->GetInt("Shortcuts.Fullscreen", Ship::KbScancode::LUS_KB_F11));
    Ship::Context::GetInstance()->GetWindow()->SetMouseCaptureScancode(
        Ship::Context::GetInstance()->GetConfig()->GetInt("Shortcuts.MouseCapture", Ship::KbScancode::LUS_KB_F2));

    InitWindowManager();
    mGfxDebugger = std::make_shared<GfxDebugger>();
    mInterpreter->SetGfxDebugger(mGfxDebugger);
    mInterpreter->Init(mWindowManagerApi, mRenderingApi, Ship::Context::GetInstance()->GetName().c_str(), isFullscreen,
                       width, height, posX, posY);
    mWindowManagerApi->SetFullscreenChangedCallback(OnFullscreenChanged);
    mWindowManagerApi->SetKeyboardCallbacks(KeyDown, KeyUp, AllKeysUp);
    mWindowManagerApi->SetMouseCallbacks(MouseButtonDown, MouseButtonUp);

    SetTextureFilter((FilteringMode)Ship::Context::GetInstance()->GetConsoleVariables()->GetInteger(
        CVAR_TEXTURE_FILTER, FILTER_THREE_POINT));
}

int32_t Fast3dWindow::GetTargetFps() {
    return mInterpreter->GetTargetFps();
}

void Fast3dWindow::SetTargetFps(int32_t fps) {
    mInterpreter->SetTargetFps(fps);
}

void Fast3dWindow::SetMaximumFrameLatency(int32_t latency) {
    mInterpreter->SetMaxFrameLatency(latency);
}

void Fast3dWindow::GetPixelDepthPrepare(float x, float y) {
    mInterpreter->GetPixelDepthPrepare(x, y);
}

uint16_t Fast3dWindow::GetPixelDepth(float x, float y) {
    return mInterpreter->GetPixelDepth(x, y);
}

void Fast3dWindow::InitWindowManager() {
    SetWindowBackend(GetSavedWindowBackend());

    switch (GetWindowBackend()) {
#ifdef ENABLE_DX11
        case WindowBackend::FAST3D_DXGI_DX11:
            mWindowManagerApi = new GfxWindowBackendDXGI();
            mRenderingApi = new GfxRenderingAPIDX11(static_cast<GfxWindowBackendDXGI*>(mWindowManagerApi));
            break;
#endif
#ifdef ENABLE_OPENGL
        case WindowBackend::FAST3D_SDL_OPENGL:
            mRenderingApi = new GfxRenderingAPIOGL();
            mWindowManagerApi = new GfxWindowBackendSDL2();
            break;
#endif
#ifdef __APPLE__
        case WindowBackend::FAST3D_SDL_METAL:
            mRenderingApi = new GfxRenderingAPIMetal();
            mWindowManagerApi = new GfxWindowBackendSDL2();
            break;
#endif
        default:
            SPDLOG_ERROR("Could not load the correct rendering backend");
            break;
    }
}

void Fast3dWindow::SetTextureFilter(FilteringMode filteringMode) {
    mInterpreter->GetCurrentRenderingAPI()->SetTextureFilter(filteringMode);
}

void Fast3dWindow::EnableSRGBMode() {
    mInterpreter->mRapi->SetSrgbMode();
}

void Fast3dWindow::SetRendererUCode(UcodeHandlers ucode) {
    gfx_set_target_ucode(ucode);
}

void Fast3dWindow::Close() {
    mWindowManagerApi->Close();
}

void Fast3dWindow::RunGuiOnly() {
    mInterpreter->RunGuiOnly();
}

void Fast3dWindow::StartFrame() {
    mInterpreter->StartFrame();
}

void Fast3dWindow::EndFrame() {
    mInterpreter->EndFrame();
}

bool Fast3dWindow::IsFrameReady() {
    return mWindowManagerApi->IsFrameReady();
}

bool Fast3dWindow::DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements) {
    std::shared_ptr<Window> wnd = Ship::Context::GetInstance()->GetWindow();

    // Skip dropped frames
    if (!wnd->IsFrameReady()) {
        return false;
    }

    auto gui = wnd->GetGui();
    // Setup mouse state manager
    wnd->GetMouseStateManager()->StartFrame();
    // Setup of the backend frames and draw initial Window and GUI menus
    gui->StartDraw();
    // Setup game framebuffers to match available window space
    mInterpreter->StartFrame();

#ifdef ENABLE_VR
    static bool firstVRCheck = true;
    if (firstVRCheck) {
        SPDLOG_INFO("ENABLE_VR macro is DEFINED in Fast3dWindow.cpp");
        firstVRCheck = false;
    }
#else
    static bool firstVRCheck = true;
    if (firstVRCheck) {
        SPDLOG_INFO("ENABLE_VR macro is NOT DEFINED in Fast3dWindow.cpp");
        firstVRCheck = false;
    }
#endif

#ifdef ENABLE_VR
    if (Ship::VRToggle::IsVREnabled()) {
        auto runtime = Ship::VRRuntime::GetInstance();
        bool initialized = runtime->IsInitialized();
        bool beginFrameSuccess = false;
        
        mVRMirrorSRV = 0;
        if (initialized && (beginFrameSuccess = runtime->BeginFrame())) {
            bool shouldRender = runtime->ShouldRender();
            static int frameTrack = 0;
            if (frameTrack++ % 1000 == 0) {
                SPDLOG_INFO("VR Path - Init: {}, State: {}, Render: {}", (int)initialized, (int)runtime->GetSessionState(), (int)shouldRender);
            }

            if (shouldRender) {
                auto rapi = GetRenderingApi();
                
                // Save original window dimensions
                auto originalDimensions = mInterpreter->mCurDimensions;

                if (mVRHudLayerIndex != -1 && runtime->GetQuadCount() == 0) {
                    mVRHudLayerIndex = -1;
                    SPDLOG_INFO("Resetting VR HUD Layer index due to empty runtime layers");
                }

                // Reset HUD pass state for the new frame.
                mHudPassDepth = 0;
                mHudWasClearedThisFrame = false;
                mHudImageAcquiredThisFrame = false;

                uint32_t eyeImgIdx[2];
                for (int eye = 0; eye < 2; eye++) {
                    // 1. Acquire Image from VR Runtime
                    uint32_t imgIdx = runtime->AcquireImage(eye);
                    eyeImgIdx[eye] = imgIdx;
                    mVRImgAcquired[eye] = true;
                    void* rtv = runtime->GetSwapchainRTV(eye, imgIdx);
                    void* dsv = runtime->GetSwapchainDSV(eye, imgIdx);
                    int32_t w, h;
                    runtime->GetSwapchainDimensions(eye, &w, &h);

                    // Update interpreter's eye state
                    mInterpreter->SetCurrentEye(eye);

                    // 2. Redirect Rendering and STATE
                    rapi->SetOverrideRenderTarget(rtv, dsv, w, h);
                    rapi->SetViewport(0, 0, w, h);
                    rapi->SetScissor(0, 0, w, h);
                    rapi->ClearFramebuffer(true, true);

                    // Crucial: Tell the interpreter we are rendering at VR resolution
                    mInterpreter->mCurDimensions.width = w;
                    mInterpreter->mCurDimensions.height = h;

                    // 3. Set Matrices and Run
                    float proj[16];
                    float view[16];
                    runtime->GetProjectionMatrix(eye, proj, 1.0f, 20000.0f);
                    runtime->GetViewMatrix(eye, view);

                    // Update interpreter's eye state
                    mInterpreter->SetCurrentEye(eye);
                    mInterpreter->SetVRMatrices(true, proj, view, w, h, rtv, dsv, eye);
                    
                    mInterpreter->Run(commands, mtxReplacements);

                    // 4. Release Image back to VR Runtime
                    rapi->SetOverrideRenderTarget(nullptr, nullptr, 0, 0);
                    if (eye == 0) {
                        mVRMirrorSRV = (uintptr_t)runtime->GetSwapchainSRV(0, eyeImgIdx[0]);
                    }
                }
                
                for (int eye = 0; eye < 2; eye++) {
                    runtime->ReleaseImage(eye);
                    mVRImgAcquired[eye] = false;
                }
                
                if (mHudImageAcquiredThisFrame) {
                    runtime->ReleaseQuadImage(mVRHudLayerIndex);
                }
                runtime->EndFrame();

                // Restore original window dimensions
                mInterpreter->mCurDimensions = originalDimensions;
                mInterpreter->SetVRMatrices(false, nullptr, nullptr, 0, 0, nullptr, nullptr, 0);
                mInterpreter->SetVRHudTarget(nullptr, nullptr, 0, 0);
                
                // Rebind the desktop framebuffer so that ImGui renders to the window and not the VR headset
                rapi->SetOverrideRenderTarget(nullptr, nullptr, 0, 0);
                rapi->StartDrawToFramebuffer(0, 0.0f);
                rapi->ClearFramebuffer(true, true);
            } else {
                runtime->EndFrame();
            }
        } else {
            mInterpreter->Run(commands, mtxReplacements);
        }
    } else {
        mInterpreter->Run(commands, mtxReplacements);
    }
#else
    // Execute the games gfx commands
    mInterpreter->Run(commands, mtxReplacements);
#endif

    // Renders the game frame buffer to the final window and finishes the GUI
    gui->EndDraw();

    // Now it's safe to release VR images after the desktop mirror (Gui) has finished drawing
    if (Ship::VRToggle::IsVREnabled()) {
        auto runtime = Ship::VRRuntime::GetInstance();
        if (runtime->IsInitialized()) {
            for (int i = 0; i < 2; i++) {
                if (mVRImgAcquired[i]) {
                    runtime->ReleaseImage(i);
                    mVRImgAcquired[i] = false;
                }
            }
        }
    }

    // Finalize swap buffers
    mInterpreter->EndFrame();

    return true;
}

void Fast3dWindow::HandleEvents() {
    mWindowManagerApi->HandleEvents();
    Ship::VRToggle::Update();
    if (mMockVRPose) {
        mMockVRPose->Update();
    }
}

void Fast3dWindow::SetCursorVisibility(bool visible) {
    mWindowManagerApi->SetCursorVisibility(visible);
}

uint32_t Fast3dWindow::GetWidth() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return width;
}

uint32_t Fast3dWindow::GetHeight() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return height;
}

float Fast3dWindow::GetAspectRatio() {
    return mInterpreter->mCurDimensions.aspect_ratio;
}

int32_t Fast3dWindow::GetPosX() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return posX;
}

int32_t Fast3dWindow::GetPosY() {
    uint32_t width, height;
    int32_t posX, posY;
    mWindowManagerApi->GetDimensions(&width, &height, &posX, &posY);
    return posY;
}

void Fast3dWindow::SetMousePos(Ship::Coords pos) {
    mWindowManagerApi->SetMousePos(pos.x, pos.y);
}

Ship::Coords Fast3dWindow::GetMousePos() {
    int32_t x, y;
    mWindowManagerApi->GetMousePos(&x, &y);
    return { x, y };
}

Ship::Coords Fast3dWindow::GetMouseDelta() {
    int32_t x, y;
    mWindowManagerApi->GetMouseDelta(&x, &y);
    return { x, y };
}

Ship::CoordsF Fast3dWindow::GetMouseWheel() {
    float x, y;
    mWindowManagerApi->GetMouseWheel(&x, &y);
    return { x, y };
}

bool Fast3dWindow::GetMouseState(Ship::MouseBtn btn) {
    return mWindowManagerApi->GetMouseState(static_cast<uint32_t>(btn));
}

void Fast3dWindow::SetMouseCapture(bool capture) {
    mWindowManagerApi->SetMouseCapture(capture);
}

bool Fast3dWindow::IsMouseCaptured() {
    return mWindowManagerApi->IsMouseCaptured();
}

uint32_t Fast3dWindow::GetCurrentRefreshRate() {
    uint32_t refreshRate;
    mWindowManagerApi->GetActiveWindowRefreshRate(&refreshRate);
    return refreshRate;
}

bool Fast3dWindow::SupportsWindowedFullscreen() {
#ifdef __APPLE__
    return false;
#endif

    if (GetWindowBackend() == WindowBackend::FAST3D_SDL_OPENGL) {
        return true;
    }

    return false;
}

bool Fast3dWindow::CanDisableVerticalSync() {
    return mWindowManagerApi->CanDisableVsync();
}

void Fast3dWindow::SetResolutionMultiplier(float multiplier) {
    mInterpreter->SetResolutionMultiplier(multiplier);
}

void Fast3dWindow::SetMsaaLevel(uint32_t value) {
    mInterpreter->SetMsaaLevel(value);
}

void Fast3dWindow::SetFullscreen(bool isFullscreen) {
    // Save current window position before fullscreening
    SaveWindowToConfig();
    mWindowManagerApi->SetFullscreen(isFullscreen);
}

bool Fast3dWindow::IsFullscreen() {
    return mWindowManagerApi->IsFullscreen();
}

bool Fast3dWindow::IsRunning() {
    return mWindowManagerApi->IsRunning();
}

uintptr_t Fast3dWindow::GetGfxFrameBuffer() {
    if (Ship::VRToggle::IsVREnabled()) {
        return mVRMirrorSRV;
    }
    return mInterpreter->mGfxFrameBuffer;
}

const char* Fast3dWindow::GetKeyName(int32_t scancode) {
    return mWindowManagerApi->GetKeyName(scancode);
}

void Fast3dWindow::SetVRBaseTrackingSpace(const float* pos, const float* rotQuat) {
#ifdef ENABLE_VR
    auto runtime = Ship::VRRuntime::GetInstance();
    if (runtime->IsInitialized()) {
        runtime->SetBaseTrackingSpace(pos, rotQuat);
    }
#endif
}

#ifdef ENABLE_VR
#include "vr/VRRuntime.h"

Ship::VRPose* Fast3dWindow::GetVRPose() {
    auto runtime = Ship::VRRuntime::GetInstance();
    if (runtime->IsInitialized()) {
        return (Ship::VRPose*)&runtime->GetPose();
    }
    return nullptr;
}

/* ============================================================
 *  VR HUD pass — explicit hooks driven by display-list NOOP markers.
 * ============================================================ */
void Fast3dWindow::BeginVRHudPass() {
    if (!Ship::VRToggle::IsVREnabled() || !mInterpreter) {
        return;
    }

    // Lazy-create the persistent HUD quad layer and companion framebuffer.
    constexpr int32_t HUD_W = 640;
    constexpr int32_t HUD_H = 480;

    // Save original dimensions on the first entry of the pass.
    if (mHudPassDepth == 0) {
        mSavedDims = mInterpreter->mCurDimensions;
        mSavedCurrentEye = mInterpreter->GetCurrentEye();
    }

    // Override dimensions for ALL eyes. This is critical for internal scaling logic
    // (RATIO_X/Y) to work correctly for HUD elements, even when draws are swallowed.
    mInterpreter->mCurDimensions.width = HUD_W;
    mInterpreter->mCurDimensions.height = HUD_H;
    mInterpreter->mCurDimensions.aspect_ratio = (float)HUD_W / HUD_H;

    if (mHudPassDepth++ > 0) {
        return; // nested call; outermost handled the RTV swap
    }

    // The display list is walked once per eye. Render the HUD only on eye 0
    // (it goes into the persistent quad layer once); subsequent eyes only
    // need to swallow the redundant draws.
    if (mInterpreter->GetCurrentEye() > 0) {
        mInterpreter->mInHudPass = true;
        return;
    }

    auto runtime = Ship::VRRuntime::GetInstance();
    if (!runtime || !runtime->IsInitialized()) {
        return;
    }

    auto rapi = GetRenderingApi();

    if (mVRHudLayerIndex == -1) {
        mVRHudLayerIndex = runtime->CreateQuadLayer(HUD_W, HUD_H);
        SPDLOG_INFO("Created VR HUD quad layer at index {}", mVRHudLayerIndex);
    }
    if (mVRHudFbId == -1) {
        mVRHudFbId = rapi->CreateFramebuffer();
        rapi->UpdateFramebufferParameters(
            mVRHudFbId, HUD_W, HUD_H, /*msaa*/1,
            /*opengl_invertY*/false, /*render_target*/true,
            /*has_depth_buffer*/true, /*can_extract_depth*/false);
        SPDLOG_INFO("Created native HUD framebuffer with id {}", mVRHudFbId);
    }

    // Refresh pose + size from CVars every Begin (cheap; allows live tuning).
    auto cvars = Ship::Context::GetInstance()->GetConsoleVariables();
    const float hudDist  = cvars->GetFloat("gVRHUDDistance", 1.5f);
    const float hudWidth = cvars->GetFloat("gVRHUDWidth",    1.0f);
    const float hudHeight = hudWidth * (static_cast<float>(HUD_H) / HUD_W);

    XrPosef pose = {};
    pose.orientation = { 0.0f, 0.0f, 0.0f, 1.0f }; // identity → faces viewer
    pose.position    = { 0.0f, 0.0f, -hudDist };   // -Z is forward in OpenXR
    runtime->SetQuadPose(mVRHudLayerIndex, pose);

    XrExtent2Df size = { hudWidth, hudHeight };
    runtime->SetQuadSize(mVRHudLayerIndex, size);

    // Acquire this frame's quad-layer swapchain image (once per frame).
    if (!mHudImageAcquiredThisFrame) {
        mVRHudImgIdx = runtime->AcquireQuadImage(mVRHudLayerIndex);
        mHudImageAcquiredThisFrame = true;
    }

    void* hudRtv = runtime->GetQuadRTV(mVRHudLayerIndex, mVRHudImgIdx);
    void* hudDsv = runtime->GetQuadDSV(mVRHudLayerIndex, mVRHudImgIdx);
    if (!hudRtv) {
        SPDLOG_WARN("BeginVRHudPass: HUD RTV unavailable; pass not entered");
        --mHudPassDepth;
        return;
    }

    int32_t hudW = 0, hudH = 0;
    runtime->GetQuadDimensions(mVRHudLayerIndex, &hudW, &hudH);

    // Flush any pending eye-buffer draws before swapping RTV.
    mInterpreter->Flush();

    // Save state so EndVRHudPass can restore it.
    mSavedEyeRtv       = mInterpreter->GetCurrentRtv();
    mSavedEyeDsv       = mInterpreter->GetCurrentDsv();
    mSavedEyeRtvWidth  = mInterpreter->GetCurrentRtvWidth();
    mSavedEyeRtvHeight = mInterpreter->GetCurrentRtvHeight();
    mSavedViewport     = mInterpreter->mRdp->viewport;
    mSavedScissor      = mInterpreter->mRdp->scissor;

    // Swap to the HUD render target and reset its viewport.
    rapi->SetOverrideRenderTarget(hudRtv, hudDsv, hudW, hudH);
    rapi->StartDrawToFramebuffer(0, 0.0f); // Force backend to bind the new RTV

    // Reset RDP state to the HUD quad dimensions (origin 0,0).
    mInterpreter->mRdp->viewport = { 0, 0, (uint32_t)hudW, (uint32_t)hudH };
    mInterpreter->mRdp->scissor  = { 0, 0, (uint32_t)hudW, (uint32_t)hudH };
    rapi->SetViewport(0, 0, hudW, hudH);
    rapi->SetScissor(0, 0, hudW, hudH);

    if (!mHudWasClearedThisFrame) {
        rapi->ClearFramebuffer(/*color*/true, /*depth*/true);
        mHudWasClearedThisFrame = true;
    }

    mInterpreter->mInHudPass = true;
}

void Fast3dWindow::EndVRHudPass() {
    if (!Ship::VRToggle::IsVREnabled() || !mInterpreter) {
        return;
    }
    if (--mHudPassDepth > 0) {
        return; // not outermost
    }
    if (mHudPassDepth < 0) {
        SPDLOG_WARN("EndVRHudPass without matching Begin (depth went negative)");
        mHudPassDepth = 0;
        return;
    }

    // Crucial: Flush NOW while mInHudPass is still true!
    // This ensures buffered draws are either rendered to the HUD RTV (eye 0) 
    // or swallowed correctly (eye > 0).
    mInterpreter->Flush();

    if (mInterpreter->GetCurrentEye() > 0) {
        mInterpreter->mInHudPass = false;
        mInterpreter->mCurDimensions = mSavedDims; // Restore original dimensions
        return;
    }

    // Outermost End on eye 0: restore eye RTV.
    if (mSavedEyeRtv) {
        auto rapi = GetRenderingApi();
        rapi->SetOverrideRenderTarget(mSavedEyeRtv, mSavedEyeDsv,
                                      mSavedEyeRtvWidth, mSavedEyeRtvHeight);
        rapi->StartDrawToFramebuffer(0, 0.0f); // Restore backend binding
        
        // Restore previous viewport and scissor.
        mInterpreter->mRdp->viewport = mSavedViewport;
        mInterpreter->mRdp->scissor  = mSavedScissor;
        rapi->SetViewport(mSavedViewport.x, mSavedViewport.y, 
                          mSavedViewport.width, mSavedViewport.height);
        rapi->SetScissor(mSavedScissor.x, mSavedScissor.y,
                         mSavedScissor.width, mSavedScissor.height);
    }
    mInterpreter->mCurDimensions = mSavedDims;
    mInterpreter->mInHudPass = false;

    mSavedEyeRtv = nullptr;
    mSavedEyeDsv = nullptr;
}
#endif

bool Fast3dWindow::KeyUp(int32_t scancode) {
    auto wnd = Ship::Context::GetInstance()->GetWindow();
    if (wnd) {
        auto fastWnd = std::dynamic_pointer_cast<Fast3dWindow>(wnd);
        if (fastWnd && fastWnd->mMockVRPose) {
            fastWnd->mMockVRPose->ProcessKeyboardEvent(Ship::LUS_KB_EVENT_KEY_UP, static_cast<Ship::KbScancode>(scancode));
        }
    }

    if (scancode == Ship::Context::GetInstance()->GetWindow()->GetFullscreenScancode()) {
        Ship::Context::GetInstance()->GetWindow()->ToggleFullscreen();
    }

    if (scancode == Ship::Context::GetInstance()->GetWindow()->GetMouseCaptureScancode()) {
        Ship::Context::GetInstance()->GetWindow()->GetMouseStateManager()->ToggleMouseCaptureOverride();
    }

    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(-1);
    return Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_UP, static_cast<Ship::KbScancode>(scancode));
}

bool Fast3dWindow::KeyDown(int32_t scancode) {
    auto wnd = Ship::Context::GetInstance()->GetWindow();
    if (wnd) {
        auto fastWnd = std::dynamic_pointer_cast<Fast3dWindow>(wnd);
        if (fastWnd && fastWnd->mMockVRPose) {
            fastWnd->mMockVRPose->ProcessKeyboardEvent(Ship::LUS_KB_EVENT_KEY_DOWN, static_cast<Ship::KbScancode>(scancode));
        }
    }

    bool isProcessed = Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(
        Ship::KbEventType::LUS_KB_EVENT_KEY_DOWN, static_cast<Ship::KbScancode>(scancode));
    Ship::Context::GetInstance()->GetWindow()->SetLastScancode(scancode);

    return isProcessed;
}

void Fast3dWindow::AllKeysUp() {
    auto wnd = Ship::Context::GetInstance()->GetWindow();
    if (wnd) {
        auto fastWnd = std::dynamic_pointer_cast<Fast3dWindow>(wnd);
        if (fastWnd && fastWnd->mMockVRPose) {
            fastWnd->mMockVRPose->ProcessKeyboardEvent(Ship::LUS_KB_EVENT_ALL_KEYS_UP, Ship::LUS_KB_UNKNOWN);
        }
    }

    Ship::Context::GetInstance()->GetControlDeck()->ProcessKeyboardEvent(Ship::KbEventType::LUS_KB_EVENT_ALL_KEYS_UP,
                                                                         Ship::KbScancode::LUS_KB_UNKNOWN);
}

bool Fast3dWindow::MouseButtonUp(int button) {
    return Ship::Context::GetInstance()->GetControlDeck()->ProcessMouseButtonEvent(false,
                                                                                   static_cast<Ship::MouseBtn>(button));
}

bool Fast3dWindow::MouseButtonDown(int button) {
    bool isProcessed = Ship::Context::GetInstance()->GetControlDeck()->ProcessMouseButtonEvent(
        true, static_cast<Ship::MouseBtn>(button));
    return isProcessed;
}

void Fast3dWindow::OnFullscreenChanged(bool isNowFullscreen) {
    std::shared_ptr<Window> wnd = Ship::Context::GetInstance()->GetWindow();

    // Re-save fullscreen enabled after
    Ship::Context::GetInstance()->GetConfig()->SetBool("Window.Fullscreen.Enabled", isNowFullscreen);
}

std::weak_ptr<Interpreter> Fast3dWindow::GetInterpreterWeak() const {
    return mInterpreter;
}

std::string Fast3dWindow::GetWindowBackendName() {
    switch (GetWindowBackend()) {
        case WindowBackend::FAST3D_DXGI_DX11:
            return "DirectX 11";
        case WindowBackend::FAST3D_SDL_OPENGL:
            return "OpenGL";
        case WindowBackend::FAST3D_SDL_METAL:
            return "Metal";
        default:
            return "";
    }
}

void Fast3dWindow::SetCurrentDimensions(uint32_t width, uint32_t height) {
    SetCurrentDimensions(width, height, GetPosX(), GetPosY());
}

void Fast3dWindow::SetCurrentDimensions(uint32_t width, uint32_t height, int32_t posX, int32_t posY) {
    mWindowManagerApi->SetDimensions(width, height, posX, posY);
    SaveWindowToConfig();
}

void Fast3dWindow::SetCurrentDimensions(bool isFullscreen, uint32_t width, uint32_t height) {
    SetCurrentDimensions(isFullscreen, width, height, GetPosX(), GetPosY());
}

void Fast3dWindow::SetCurrentDimensions(bool isFullscreen, uint32_t width, uint32_t height, int32_t posX,
                                        int32_t posY) {
    auto config = Ship::Context::GetInstance()->GetConfig();
    if (!isFullscreen) {
        config->SetInt("Window.Width", static_cast<int32_t>(width));
        config->SetInt("Window.Height", static_cast<int32_t>(height));
        config->SetInt("Window.PositionX", posX);
        config->SetInt("Window.PositionY", posY);
    } else {
        config->SetInt("Window.Fullscreen.Width", static_cast<int32_t>(width));
        config->SetInt("Window.Fullscreen.Height", static_cast<int32_t>(height));
    }
    mWindowManagerApi->SetFullscreen(isFullscreen);
    mWindowManagerApi->SetDimensions(width, height, posX, posY);
    SaveWindowToConfig();
}

Ship::WindowRect Fast3dWindow::GetPrimaryMonitorRect() {
    return mWindowManagerApi->GetPrimaryMonitorRect();
}

std::shared_ptr<GfxDebugger> Fast3dWindow::GetGfxDebugger() const {
    return mGfxDebugger;
}

} // namespace Fast
