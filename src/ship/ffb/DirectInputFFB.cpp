#include "ship/ffb/DirectInputFFB.h"
#include <spdlog/spdlog.h>

namespace Ship {

DirectInputFFB::DirectInputFFB(const std::string& guid) {
#ifdef _WIN32
    SPDLOG_INFO("Initializing DirectInput FFB for device {}", guid);
    // TODO: Implement DirectInput8Create and device acquisition
#endif
}

DirectInputFFB::~DirectInputFFB() {
#ifdef _WIN32
    if (mDevice) {
        mDevice->Unacquire();
        mDevice->Release();
    }
    if (mDirectInput) {
        mDirectInput->Release();
    }
#endif
}

void DirectInputFFB::SetConstantForce(float force) {
    // Win32 DI implementation
}

void DirectInputFFB::SetSpring(float coefficient) {
    // Win32 DI implementation
}

void DirectInputFFB::SetDamper(float coefficient) {
    // Win32 DI implementation
}

void DirectInputFFB::PlayPeriodic(float frequency, float amplitude) {
    // Win32 DI implementation
}

void DirectInputFFB::Stop() {
    // Win32 DI implementation
}

}
