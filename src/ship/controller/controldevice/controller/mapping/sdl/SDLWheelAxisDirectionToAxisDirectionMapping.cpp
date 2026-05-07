#include "ship/controller/controldevice/controller/mapping/sdl/SDLWheelAxisDirectionToAxisDirectionMapping.h"
#include <spdlog/spdlog.h>
#include "ship/utils/StringHelper.h"
#include "ship/window/gui/IconsFontAwesome4.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"
#include <algorithm>
#include <cmath>

#define MAX_SDL_RANGE (float)INT16_MAX

namespace Ship {
SDLWheelAxisDirectionToAxisDirectionMapping::SDLWheelAxisDirectionToAxisDirectionMapping(uint8_t portIndex, StickIndex stickIndex,
                                                                                       Direction direction,
                                                                                       int32_t sdlControllerAxis,
                                                                                       int32_t axisDirection)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerAxisDirectionMapping(PhysicalDeviceType::SDLGamepad, portIndex, stickIndex, direction),
      SDLAxisDirectionToAnyMapping(sdlControllerAxis, axisDirection) {
}

float SDLWheelAxisDirectionToAxisDirectionMapping::GetNormalizedAxisDirectionValue() {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return 0.0f;
    }

    std::vector<float> normalizedValues = {};
    auto mgr = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager();

    // CVars for Wheel tuning
    float deadzone = CVarGetFloat("gWheel.Deadzone", 0.02f);
    float linearity = CVarGetFloat("gWheel.Linearity", 0.85f);

    auto applyTuning = [&](int16_t axisValue) {
        float norm = (float)axisValue / MAX_SDL_RANGE;
        
        // Filter by direction
        if ((mAxisDirection == POSITIVE && norm < 0) || (mAxisDirection == NEGATIVE && norm > 0)) {
            return 0.0f;
        }

        float magnitude = std::abs(norm);
        if (magnitude < deadzone) {
            return 0.0f;
        }

        // Apply curve to the range [deadzone, 1.0]
        float adjusted = (magnitude - deadzone) / (1.0f - deadzone);
        float curved = std::pow(adjusted, linearity);
        
        return curved * MAX_AXIS_RANGE;
    };

    for (const auto& [instanceId, gamepad] : mgr->GetConnectedSDLGamepadsForPort(mPortIndex)) {
        const auto axisValue = SDL_GameControllerGetAxis(gamepad, mControllerAxis);
        normalizedValues.push_back(applyTuning(axisValue));
    }

    for (const auto& [instanceId, joystick] : mgr->GetConnectedSDLJoysticksForPort(mPortIndex)) {
        const auto axisValue = SDL_JoystickGetAxis(joystick, (int)mControllerAxis);
        normalizedValues.push_back(applyTuning(axisValue));
    }

    if (normalizedValues.size() == 0) {
        return 0.0f;
    }

    return *std::max_element(normalizedValues.begin(), normalizedValues.end());
}

std::string SDLWheelAxisDirectionToAxisDirectionMapping::GetAxisDirectionMappingId() {
    return StringHelper::Sprintf("P%d-S%d-D%d-SDLWA%d-AD%s", mPortIndex, mStickIndex, mDirection, mControllerAxis,
                                 mAxisDirection == 1 ? "P" : "N");
}

void SDLWheelAxisDirectionToAxisDirectionMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str(),
        "SDLWheelAxisDirectionToAxisDirectionMapping");
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str(), mStickIndex);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str(), mDirection);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str(), mControllerAxis);
    Ship::Context::GetInstance()->GetConsoleVariables()->SetInteger(
        StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str(), mAxisDirection);
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

void SDLWheelAxisDirectionToAxisDirectionMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->ClearVariable(
        StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str());
    Ship::Context::GetInstance()->GetConsoleVariables()->Save();
}

int8_t SDLWheelAxisDirectionToAxisDirectionMapping::GetMappingType() {
    return 10; // MAPPING_TYPE_WHEEL_AXIS
}

std::string SDLWheelAxisDirectionToAxisDirectionMapping::GetPhysicalDeviceName() {
    return SDLAxisDirectionToAnyMapping::GetPhysicalDeviceName();
}

std::string SDLWheelAxisDirectionToAxisDirectionMapping::GetPhysicalInputName() {
    return SDLAxisDirectionToAnyMapping::GetPhysicalInputName();
}
} // namespace Ship
