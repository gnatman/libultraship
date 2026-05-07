#pragma once
#include "ship/ffb/FFBDevice.h"
#ifdef _WIN32
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#endif
#include <string>

namespace Ship {
class DirectInputFFB : public FFBDevice {
public:
    DirectInputFFB(const std::string& guid);
    ~DirectInputFFB();
    void SetConstantForce(float force) override;
    void SetSpring(float coefficient) override;
    void SetDamper(float coefficient) override;
    void PlayPeriodic(float frequency, float amplitude) override;
    void Stop() override;
private:
#ifdef _WIN32
    LPDIRECTINPUT8 mDirectInput = nullptr;
    LPDIRECTINPUTDEVICE8 mDevice = nullptr;
    LPDIRECTINPUTEFFECT mConstantForceEffect = nullptr;
    LPDIRECTINPUTEFFECT mSpringEffect = nullptr;
#endif
};
}
