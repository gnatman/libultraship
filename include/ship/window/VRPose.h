#pragma once

#include <cstdint>

namespace Ship {

struct VRPose {
    struct {
        float position[3];    // x, y, z
        float orientation[4]; // x, y, z, w (quaternion)
    } head;

    struct {
        float position[3];
        float orientation[4];
    } eyes[2];

    struct {
        float angleLeft;
        float angleRight;
        float angleUp;
        float angleDown;
    } fov[2];

    int64_t displayTime;
};

} // namespace Ship
