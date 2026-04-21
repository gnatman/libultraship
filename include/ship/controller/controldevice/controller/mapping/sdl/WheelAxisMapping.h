#pragma once

#include "SDLAxisDirectionToAxisDirectionMapping.h"

namespace Ship {
class WheelAxisMapping final : public SDLAxisDirectionToAxisDirectionMapping {
  public:
    WheelAxisMapping(uint8_t portIndex, StickIndex stickIndex, Direction direction, int32_t sdlControllerAxis,
                     int32_t axisDirection, float linearity, float sensitivity, float deadzone);
    float GetNormalizedAxisDirectionValue() override;
    int8_t GetMappingType() override;
    void SaveToConfig() override;
    void EraseFromConfig() override;

  private:
    float mLinearity;
    float mSensitivity;
    float mDeadzone;
};
} // namespace Ship
