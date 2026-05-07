#pragma once
#include <string>

namespace Ship {
class FFBDevice {
public:
    virtual ~FFBDevice() = default;
    virtual void SetConstantForce(float force) = 0;
    virtual void SetSpring(float coefficient) = 0;
    virtual void SetDamper(float coefficient) = 0;
    virtual void PlayPeriodic(float frequency, float amplitude) = 0;
    virtual void Stop() = 0;
};
}
