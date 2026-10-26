#include "SDLAxisDirectionToAxisDirectionMapping.h"
#include <spdlog/spdlog.h>
#include "utils/StringHelper.h"
#include "window/gui/IconsFontAwesome4.h"
#include "public/bridge/consolevariablebridge.h"
#include "Context.h"
#include "controller/controldeck/ControlDeck.h"

#define MAX_SDL_RANGE (float)INT16_MAX

namespace Ship {
SDLAxisDirectionToAxisDirectionMapping::SDLAxisDirectionToAxisDirectionMapping(uint8_t portIndex, StickIndex stickIndex,
                                                                               Direction direction,
                                                                               int32_t sdlControllerAxis,
                                                                               int32_t axisDirection,
                                                                               std::string deviceGuid)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad),
      ControllerAxisDirectionMapping(PhysicalDeviceType::SDLGamepad, portIndex, stickIndex, direction),
      SDLAxisDirectionToAnyMapping(sdlControllerAxis, axisDirection), mDeviceGuid(deviceGuid) {
}

float SDLAxisDirectionToAxisDirectionMapping::GetNormalizedAxisDirectionValue() {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return 0.0f;
    }

    std::vector<float> normalizedValues = {};
    auto mgr = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager();

    for (const auto& [instanceId, gamepad] : mgr->GetConnectedSDLGamepadsForPort(mPortIndex)) {
        if (!mDeviceGuid.empty() && mgr->GetDeviceGuidForInstanceId(instanceId) != mDeviceGuid) {
            continue;
        }

        const auto axisValue = SDL_GameControllerGetAxis(gamepad, mControllerAxis);

        if ((mAxisDirection == POSITIVE && axisValue < 0) || (mAxisDirection == NEGATIVE && axisValue > 0)) {
            normalizedValues.push_back(0.0f);
            continue;
        }

        // scale {-32768 ... +32767} to {-MAX_AXIS_RANGE ... +MAX_AXIS_RANGE}
        // and use the absolute value of it
        normalizedValues.push_back(fabs(axisValue * MAX_AXIS_RANGE / MAX_SDL_RANGE));
    }

    for (const auto& [instanceId, joystick] : mgr->GetConnectedSDLJoysticksForPort(mPortIndex)) {
        if (!mDeviceGuid.empty() && mgr->GetDeviceGuidForInstanceId(instanceId) != mDeviceGuid) {
            continue;
        }

        if ((int)mControllerAxis < SDL_JoystickNumAxes(joystick)) {
            const auto axisValue = SDL_JoystickGetAxis(joystick, (int)mControllerAxis);

            if ((mAxisDirection == POSITIVE && axisValue < 0) || (mAxisDirection == NEGATIVE && axisValue > 0)) {
                normalizedValues.push_back(0.0f);
                continue;
            }

            normalizedValues.push_back(fabs(axisValue * MAX_AXIS_RANGE / MAX_SDL_RANGE));
        }
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
    CVarSetString(StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str(),
                  "SDLAxisDirectionToAxisDirectionMapping");
    CVarSetInteger(StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str(), mStickIndex);
    CVarSetInteger(StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str(), mDirection);
    CVarSetInteger(StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str(), mControllerAxis);
    CVarSetInteger(StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str(), mAxisDirection);
    if (!mDeviceGuid.empty()) {
        CVarSetString(StringHelper::Sprintf("%s.DeviceGUID", mappingCvarKey.c_str()).c_str(), mDeviceGuid.c_str());
    }
    CVarSave();
}

void SDLAxisDirectionToAxisDirectionMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".AxisDirectionMappings." + GetAxisDirectionMappingId();
    CVarClear(StringHelper::Sprintf("%s.Stick", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.Direction", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.AxisDirectionMappingClass", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.SDLControllerAxis", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.AxisDirection", mappingCvarKey.c_str()).c_str());
    CVarSave();
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
