#pragma once

#include <openxr/openxr.h>

namespace LUS {

struct VRPose {
    XrPosef head;
    XrPosef eyes[2];
    XrFovf fov[2];
    XrTime displayTime;
    bool shouldRender;
};

} // namespace LUS
