#pragma once

#include "SDLAxisDirectionToButtonMapping.h"

namespace Ship {
class WheelPedalMapping final : public SDLAxisDirectionToButtonMapping {
  public:
    WheelPedalMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask, int32_t sdlControllerAxis,
                      int32_t axisDirection, float threshold, bool inverted);
    void UpdatePad(CONTROLLERBUTTONS_T& padButtons) override;
    int8_t GetMappingType() override;
    void SaveToConfig() override;
    void EraseFromConfig() override;

  private:
    float mThreshold;
    bool mInverted;
};
} // namespace Ship
