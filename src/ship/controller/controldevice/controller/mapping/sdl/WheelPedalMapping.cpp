#include "ship/controller/controldevice/controller/mapping/sdl/WheelPedalMapping.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include "ship/utils/StringHelper.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

namespace Ship {
WheelPedalMapping::WheelPedalMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask, int32_t sdlControllerAxis,
                                     int32_t axisDirection, float threshold, bool inverted)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      SDLAxisDirectionToButtonMapping(portIndex, bitmask, sdlControllerAxis, axisDirection),
      mThreshold(threshold), mInverted(inverted) {
}

void WheelPedalMapping::UpdatePad(CONTROLLERBUTTONS_T& padButtons) {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return;
    }

    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        const auto axisValue = SDL_GameControllerGetAxis(gamepad, mControllerAxis);
        float val = (float)axisValue / 32767.0f;

        if (mInverted) {
            // Inverted: 1.0 released, 0.0 pressed (if POSITIVE) or -1.0 released, 0.0 pressed (if NEGATIVE)
            if (mAxisDirection == POSITIVE) {
                if (val < (1.0f - mThreshold)) {
                    padButtons |= mBitmask;
                }
            } else {
                if (val > -(1.0f - mThreshold)) {
                    padButtons |= mBitmask;
                }
            }
        } else {
            // Normal: 0.0 released, 1.0 pressed (if POSITIVE) or 0.0 released, -1.0 pressed (if NEGATIVE)
            if (mAxisDirection == POSITIVE) {
                if (val > mThreshold) {
                    padButtons |= mBitmask;
                }
            } else {
                if (val < -mThreshold) {
                    padButtons |= mBitmask;
                }
            }
        }
    }
}

int8_t WheelPedalMapping::GetMappingType() {
    return MAPPING_TYPE_WHEEL;
}

void WheelPedalMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str(), "WheelPedalMapping");
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str(), mBitmask);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str(), mControllerAxis);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str(), mAxisDirection);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetFloat(
        StringHelper::Sprintf("%s.Threshold", mappingCvarKey.c_str()).c_str(), mThreshold);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Inverted", mappingCvarKey.c_str()).c_str(), mInverted);
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

void WheelPedalMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();

    SDLAxisDirectionToButtonMapping::EraseFromConfig();

    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Threshold", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Inverted", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}
} // namespace Ship
