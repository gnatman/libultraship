#include "public/bridge/windowbridge.h"
#include "window/Window.h"
#include "Context.h"

extern "C" {

uint32_t WindowGetWidth() {
    return Ship::Context::GetInstance()->GetWindow()->GetWidth();
}

uint32_t WindowGetHeight() {
    return Ship::Context::GetInstance()->GetWindow()->GetHeight();
}

float WindowGetAspectRatio() {
    return Ship::Context::GetInstance()->GetWindow()->GetCurrentAspectRatio();
}

bool WindowIsRunning() {
    return Ship::Context::GetInstance()->GetWindow()->IsRunning();
}

int32_t WindowGetPosX() {
    return Ship::Context::GetInstance()->GetWindow()->GetPosX();
}

int32_t WindowGetPosY() {
    return Ship::Context::GetInstance()->GetWindow()->GetPosY();
}

bool WindowIsFullscreen() {
    return Ship::Context::GetInstance()->GetWindow()->IsFullscreen();
}
}

extern "C" void WindowGetVRPose(float* pos, float* quat) {
    if (auto window = Ship::Context::GetInstance()->GetWindow()) {
        if (auto vrPose = window->GetVRPose()) {
            if (pos) { pos[0] = vrPose->head.position[0]; pos[1] = vrPose->head.position[1]; pos[2] = vrPose->head.position[2]; }
            if (quat) { quat[0] = vrPose->head.orientation[0]; quat[1] = vrPose->head.orientation[1]; quat[2] = vrPose->head.orientation[2]; quat[3] = vrPose->head.orientation[3]; }
            return;
        }
    }
    if (pos) { pos[0] = pos[1] = pos[2] = 0.0f; }
    if (quat) { quat[0] = quat[1] = quat[2] = 0.0f; quat[3] = 1.0f; }
}

#include "vr/VRToggle.h"
extern "C" bool WindowIsVREnabled() {
#ifdef ENABLE_VR
    return Ship::VRToggle::IsVREnabled();
#else
    return false;
#endif
}

extern "C" void WindowSetVRBaseTrackingSpace(const float* pos, const float* rotQuat) {
    if (auto window = Ship::Context::GetInstance()->GetWindow()) {
        window->SetVRBaseTrackingSpace(pos, rotQuat);
    }
}

