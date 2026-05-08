#pragma once

#include "ship/window/VRPose.h"
#include "ship/controller/controldevice/controller/mapping/keyboard/KeyboardScancodes.h"
#include <memory>

namespace Ship {

class MockVRPose {
public:
    MockVRPose();
    ~MockVRPose();

    void Update();
    const VRPose& GetPose() const;

    void ProcessKeyboardEvent(KbEventType eventType, KbScancode scancode);

private:
    VRPose mPose;
    float mYaw;   // radians
    float mPitch; // radians
    float mPosX;  // meters
    float mPosY;  // meters
    float mPosZ;  // meters
    float mIPD;   // meters

    bool mAltPressed;
    bool mUpPressed;
    bool mDownPressed;
    bool mLeftPressed;
    bool mRightPressed;
    bool mPageUpPressed;
    bool mPageDownPressed;
    bool mPlusPressed;
    bool mMinusPressed;
};

} // namespace Ship
