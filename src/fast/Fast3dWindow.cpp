#include "fast/Fast3dWindow.h"
#include "vr/MockVRPose.h"
#include "vr/VRToggle.h"
#include "vr/VRRuntime.h"
#include "ship/window/Window.h"
#include "ship/Context.h"
#include <spdlog/spdlog.h>
#include "fast/backends/gfx_rendering_api.h"

namespace Fast {

Fast3dWindow::Fast3dWindow(std::vector<std::shared_ptr<Ship::GuiWindow>> guiWindows) : Window(guiWindows) {
    mWindowManagerApi = nullptr;
    mRenderingApi = nullptr;
}

Fast3dWindow::~Fast3dWindow() {
}

void Fast3dWindow::Init() {
    Window::Init();
    
    mInterpreter = std::make_shared<Interpreter>();
}

void Fast3dWindow::DrawAndRunGraphicsCommands(Gfx* commands, const std::unordered_map<Mtx*, MtxF>& mtxReplacements) {
    auto rapi = GetRenderingApi();
    
    if (rapi) {
        bool initialized = false;
        bool shouldRender = false;
        bool beginFrameSuccess = false;

#ifdef ENABLE_VR
        auto runtime = Ship::VRRuntime::GetInstance();
        if (runtime) {
            initialized = runtime->IsInitialized();
            shouldRender = runtime->ShouldRender();
        }
#endif

        // Save original window dimensions
        auto originalDimensions = mInterpreter->mCurDimensions;

#ifdef ENABLE_VR
        if (initialized && (beginFrameSuccess = runtime->BeginFrame())) {
            if (shouldRender) {
                if (mVRHudLayerIndex == -1) {
                    mVRHudLayerIndex = runtime->CreateQuadLayer(1280, 720);
                    
                    // Position HUD 1.2 meters in front
                    XrPosef pose = { {0, 0, 0, 1}, {0, 0, -1.2f} };
                    runtime->SetQuadPose(mVRHudLayerIndex, pose);
                    XrExtent2Df size = { 1.6f, 0.9f }; // 16:9 aspect ratio
                    runtime->SetQuadSize(mVRHudLayerIndex, size);
                }

                uint32_t hudImgIdx = runtime->AcquireQuadImage(mVRHudLayerIndex);
                void* hudRtv = runtime->GetQuadRTV(mVRHudLayerIndex, hudImgIdx);
                void* hudDsv = runtime->GetQuadDSV(mVRHudLayerIndex, hudImgIdx);
                int32_t hudW, hudH;
                runtime->GetQuadDimensions(mVRHudLayerIndex, &hudW, &hudH);

                mInterpreter->SetVRHudTarget(hudRtv, hudDsv, hudW, hudH);

                for (int eye = 0; eye < 2; eye++) {
                    uint32_t imgIdx = runtime->AcquireImage(eye);
                    void* rtv = runtime->GetSwapchainRTV(eye, imgIdx);
                    void* dsv = runtime->GetSwapchainDSV(eye, imgIdx);
                    int32_t w, h;
                    runtime->GetSwapchainDimensions(eye, &w, &h);

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
                    
                    static int frameCounter = 0;
                    if (frameCounter % 500 == 0) {
                        SPDLOG_INFO("Stereo Pass {} - Eye X: {:.4f}, VR Res: {}x{}", eye, view[12], w, h);
                    }
                    if (eye == 1) frameCounter++;

                    mInterpreter->SetVRMatrices(true, proj, view, w, h, rtv, dsv, eye);
                    mInterpreter->Run(commands, mtxReplacements);

                    // 4. Release Image back to VR Runtime
                    rapi->SetOverrideRenderTarget(nullptr, nullptr, 0, 0);
                    runtime->ReleaseImage(eye);
                }
                
                runtime->ReleaseQuadImage(mVRHudLayerIndex);
                runtime->EndFrame();
            } else {
                runtime->EndFrame();
            }

            // Restore original window dimensions
            mInterpreter->mCurDimensions = originalDimensions;
            mInterpreter->SetVRMatrices(false, nullptr, nullptr, 0, 0, nullptr, nullptr, 0);
            mInterpreter->SetVRHudTarget(nullptr, nullptr, 0, 0);
        } else {
#endif
            // Standard non-VR path
            mInterpreter->Run(commands, mtxReplacements);
#ifdef ENABLE_VR
        }
#endif
    }
}

// ... (rest of the file remains the same)
} // namespace Fast
