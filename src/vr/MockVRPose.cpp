#include "vr/MockVRPose.h"
#include "ship/Context.h"
#include "ship/window/Window.h"
#include "ship/config/ConsoleVariable.h"
#include <cmath>
#include <algorithm>

namespace Ship {

MockVRPose::MockVRPose() 
    : mYaw(0.0f), mPitch(0.0f), mPosX(0.0f), mPosY(0.0f), mPosZ(0.0f), mIPD(0.063f),
      mAltPressed(false), mUpPressed(false), mDownPressed(false), mLeftPressed(false), 
      mRightPressed(false), mPageUpPressed(false), mPageDownPressed(false),
      mPlusPressed(false), mMinusPressed(false) {
    mPose = {};
    mPose.head.orientation[3] = 1.0f; // w
    mPose.eyes[0].orientation[3] = 1.0f;
    mPose.eyes[1].orientation[3] = 1.0f;
}

MockVRPose::~MockVRPose() {}

static void EulerToQuaternion(float yaw, float pitch, float roll, float* q) {
    float cy = cos(yaw * 0.5f);
    float sy = sin(yaw * 0.5f);
    float cp = cos(pitch * 0.5f);
    float sp = sin(pitch * 0.5f);
    float cr = cos(roll * 0.5f);
    float sr = sin(roll * 0.5f);

    q[0] = cy * sp * cr + sy * cp * sr; // x
    q[1] = sy * cp * cr - cy * sp * sr; // y
    q[2] = cy * cp * sr - sy * sp * cr; // z
    q[3] = cy * cp * cr + sy * sp * sr; // w
}

void MockVRPose::ProcessKeyboardEvent(KbEventType eventType, KbScancode scancode) {
    bool isDown = (eventType == LUS_KB_EVENT_KEY_DOWN);
    
    if (eventType == LUS_KB_EVENT_ALL_KEYS_UP) {
        mAltPressed = mUpPressed = mDownPressed = mLeftPressed = mRightPressed = false;
        mPageUpPressed = mPageDownPressed = mPlusPressed = mMinusPressed = false;
        return;
    }

    if (eventType == LUS_KB_EVENT_KEY_DOWN || eventType == LUS_KB_EVENT_KEY_UP) {
        // SPDLOG_TRACE("MockVR Key Event: {} {}", (int)scancode, isDown ? "Down" : "Up");
    }

    switch (scancode) {
        case LUS_KB_ALT: 
            mAltPressed = isDown; 
            SPDLOG_INFO("MockVR Alt State: {}", mAltPressed);
            break;
        case LUS_KB_ARROWKEY_UP: mUpPressed = isDown; break;
        case LUS_KB_ARROWKEY_DOWN: mDownPressed = isDown; break;
        case LUS_KB_ARROWKEY_LEFT: mLeftPressed = isDown; break;
        case LUS_KB_ARROWKEY_RIGHT: mRightPressed = isDown; break;
        case LUS_KB_ADD:
        case LUS_KB_OEM_PLUS: mPlusPressed = isDown; break;
        case LUS_KB_SUBTRACT:
        case LUS_KB_OEM_MINUS: mMinusPressed = isDown; break;
        case LUS_KB_W: if (mAltPressed) mPageUpPressed = isDown; break;
        case LUS_KB_S: if (mAltPressed) mPageDownPressed = isDown; break;
        default: break;
    }
}

void MockVRPose::Update() {
    auto context = Context::GetInstance();
    int32_t enabled = context->GetConsoleVariables()->GetInteger("gMockVREnabled", 0);
    
    static int32_t lastEnabled = -1;
    if (enabled != lastEnabled) {
        SPDLOG_INFO("MockVR Enabled CVar: {}", enabled);
        lastEnabled = enabled;
    }

    if (!enabled) {
        return;
    }

    auto window = context->GetWindow();
    if (!window) return;

    // Rotation (Mouse + Alt)
    if (mAltPressed) {
        auto delta = window->GetMouseDelta();
        if (delta.x != 0 || delta.y != 0) {
            mYaw -= delta.x * 0.005f;
            mPitch -= delta.y * 0.005f;
            mPitch = std::max(-1.5f, std::min(1.5f, mPitch));
            SPDLOG_INFO("MockVR Pose Update - Yaw: {:.3f}, Pitch: {:.3f} (Delta: {}, {})", mYaw, mPitch, delta.x, delta.y);
        }
    }

    // Translation (Arrow keys)
    float speed = 0.05f;
    if (mUpPressed) mPosY += speed;
    if (mDownPressed) mPosY -= speed;
    if (mLeftPressed) mPosX -= speed;
    if (mRightPressed) mPosX += speed;
    if (mPageUpPressed) mPosZ -= speed;
    if (mPageDownPressed) mPosZ += speed;

    // IPD (+/-)
    if (mPlusPressed) mIPD += 0.001f;
    if (mMinusPressed) mIPD -= 0.001f;
    mIPD = std::max(0.01f, std::min(0.2f, mIPD));

    // Update Pose
    mPose.head.position[0] = mPosX;
    mPose.head.position[1] = mPosY;
    mPose.head.position[2] = mPosZ;
    EulerToQuaternion(mYaw, mPitch, 0.0f, mPose.head.orientation);

    // Eyes
    float halfIPD = mIPD * 0.5f;
    mPose.eyes[0].position[0] = mPosX - halfIPD; // Left
    mPose.eyes[1].position[0] = mPosX + halfIPD; // Right
    for (int i = 0; i < 3; i++) {
        if (i != 0) {
            mPose.eyes[0].position[i] = mPose.head.position[i];
            mPose.eyes[1].position[i] = mPose.head.position[i];
        }
    }
    for (int i = 0; i < 4; i++) {
        mPose.eyes[0].orientation[i] = mPose.head.orientation[i];
        mPose.eyes[1].orientation[i] = mPose.head.orientation[i];
    }

    // Default FOV (90 degrees)
    float fov = 1.0f; // tan(45 degrees)
    for (int i = 0; i < 2; i++) {
        mPose.fov[i].angleLeft = -fov;
        mPose.fov[i].angleRight = fov;
        mPose.fov[i].angleUp = fov;
        mPose.fov[i].angleDown = -fov;
    }
}

const VRPose& MockVRPose::GetPose() const {
    return mPose;
}

} // namespace Ship
