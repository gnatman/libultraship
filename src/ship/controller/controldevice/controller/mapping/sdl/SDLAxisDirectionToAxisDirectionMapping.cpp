#include "ship/controller/controldevice/controller/mapping/sdl/SDLAxisDirectionToAxisDirectionMapping.h"
#include <spdlog/spdlog.h>
#include "ship/utils/StringHelper.h"
#include "ship/window/gui/IconsFontAwesome4.h"
#include "ship/config/ConsoleVariable.h"
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

#define MAX_SDL_RANGE (float)INT16_MAX

namespace Ship {
SDLAxisDirectionToAxisDirectionMapping::SDLAxisDirectionToAxisDirectionMapping(uint8_t portIndex, StickIndex stickIndex,
                                                                               Direction direction,
                                                                               int32_t sdlControllerAxis,
                                                                               int32_t axisDirection)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerAxisDirectionMapping(PhysicalDeviceType::SDLGamepad, portIndex, stickIndex, direction),
      SDLAxisDirectionToAnyMapping(sdlControllerAxis, axisDirection) {
}

float SDLAxisDirectionToAxisDirectionMapping::GetNormalizedAxisDirectionValue() {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return 0.0f;
    }

    std::vector<float> normalizedValues = {};
    auto mgr = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager();

    for (const auto& [instanceId, gamepad] : mgr->GetConnectedSDLGamepadsForPort(mPortIndex)) {
        const auto axisValue = SDL_GameControllerGetAxis(gamepad, mControllerAxis);

        if ((mAxisDirection == POSITIVE && axisValue < 0) || (mAxisDirection == NEGATIVE && axisValue > 0)) {
            normalizedValues.push_back(0.0f);
            continue;
        }

        normalizedValues.push_back(fabs(axisValue * MAX_AXIS_RANGE / MAX_SDL_RANGE));
    }

    for (const auto& [instanceId, joystick] : mgr->GetConnectedSDLJoysticksForPort(mPortIndex)) {
        // If this is a raw joystick, we use the axis index directly
        const auto axisValue = SDL_JoystickGetAxis(joystick, (int)mControllerAxis);

        if ((mAxisDirection == POSITIVE && axisValue < 0) || (mAxisDirection == NEGATIVE && axisValue > 0)) {
            normalizedValues.push_back(0.0f);
            continue;
        }

        normalizedValues.push_back(fabs(axisValue * MAX_AXIS_RANGE / MAX_SDL_RANGE));
    }

    if (normalizedValues.size() == 0) {
        return 0.0f;
    }

    return *std::max_element(normalizedValues.begin(), normalizedValues.end());
}

std::string SDLAxisDirectionToAxisDirectionMapping::GetAxisDirectionMappingId() {
    return StringHelper::Sprintf("P%d-S%d-D%d-SDLA%d-AD%s", mPortIndex, mStickIndex, mDirection, mControllerAxis,
                                 mAxisDirection == 1 ? "P" : "N");
}

void SDLAxisDirectionToAxisDirectionMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    Ship::Context::GetInstance()->GetConsoleVariables()->SetString(
        StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str(),
        "SDLAxisDirectionToAxisDirectionMapping");
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

void SDLAxisDirectionToAxisDirectionMapping::EraseFromConfig() {
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

int8_t SDLAxisDirectionToAxisDirectionMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLAxisDirectionToAxisDirectionMapping::GetPhysicalDeviceName() {
    return SDLAxisDirectionToAnyMapping::GetPhysicalDeviceName();
}

std::string SDLAxisDirectionToAxisDirectionMapping::GetPhysicalInputName() {
    return SDLAxisDirectionToAnyMapping::GetPhysicalInputName();
}
} // namespace Ship
