#pragma once

#include <cstdint>

namespace Ship {

class VRToggle {
public:
    static void Update();
    static bool IsVREnabled();

private:
    static int32_t mEnabled;
};

} // namespace Ship
