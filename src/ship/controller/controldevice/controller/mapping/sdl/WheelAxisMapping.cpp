#include "ship/controller/controldevice/controller/mapping/sdl/WheelAxisMapping.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include "ship/utils/StringHelper.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

#define MAX_SDL_RANGE (float)INT16_MAX

namespace Ship {
WheelAxisMapping::WheelAxisMapping(uint8_t portIndex, StickIndex stickIndex, Direction direction,
                                   int32_t sdlControllerAxis, int32_t axisDirection, float linearity, float sensitivity,
                                   float deadzone)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      SDLAxisDirectionToAxisDirectionMapping(portIndex, stickIndex, direction, sdlControllerAxis, axisDirection),
      mLinearity(linearity), mSensitivity(sensitivity), mDeadzone(deadzone) {
}

float WheelAxisMapping::GetNormalizedAxisDirectionValue() {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return 0.0f;
    }

    std::vector<float> values = {};
    for (const auto& [instanceId, gamepad] :
         Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager()->GetConnectedSDLGamepadsForPort(
             mPortIndex)) {
        const auto axisValue = SDL_GameControllerGetAxis(gamepad, mControllerAxis);

        if ((mAxisDirection == POSITIVE && axisValue < 0) || (mAxisDirection == NEGATIVE && axisValue > 0)) {
            values.push_back(0.0f);
            continue;
        }

        // scale {0 ... 32767} to {0.0 ... 1.0}
        float val = (float)fabs(axisValue) / MAX_SDL_RANGE;

        // Apply deadzone
        if (val < mDeadzone) {
            val = 0.0f;
        } else {
            val = (val - mDeadzone) / (1.0f - mDeadzone);
        }

        // Apply sensitivity
        val *= mSensitivity;

        // Apply linearity: val = pow(val, linearity)
        if (val > 0) {
            val = std::pow(val, mLinearity);
        }

        // Clamp to [0.0, 1.0]
        val = std::clamp(val, 0.0f, 1.0f);

        // scale to {0.0 ... MAX_AXIS_RANGE}
        values.push_back(val * MAX_AXIS_RANGE);
    }

    if (values.size() == 0) {
        return 0.0f;
    }

    return *std::max_element(values.begin(), values.end());
}

int8_t WheelAxisMapping::GetMappingType() {
    return MAPPING_TYPE_WHEEL;
}

void WheelAxisMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str(), "WheelAxisMapping");
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str(), mStickIndex);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str(), mDirection);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str(), mControllerAxis);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str(), mAxisDirection);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetFloat(
        StringHelper::Sprintf("%s.Linearity", mappingCvarKey.c_str()).c_str(), mLinearity);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetFloat(
        StringHelper::Sprintf("%s.Sensitivity", mappingCvarKey.c_str()).c_str(), mSensitivity);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetFloat(
        StringHelper::Sprintf("%s.Deadzone", mappingCvarKey.c_str()).c_str(), mDeadzone);
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

void WheelAxisMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();

    SDLAxisDirectionToAxisDirectionMapping::EraseFromConfig();

    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Linearity", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Sensitivity", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Deadzone", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}
} // namespace Ship
