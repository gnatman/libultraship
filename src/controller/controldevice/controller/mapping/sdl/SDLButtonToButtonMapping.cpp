#include "SDLButtonToButtonMapping.h"
#include <spdlog/spdlog.h>
#include "utils/StringHelper.h"
#include "window/gui/IconsFontAwesome4.h"
#include "public/bridge/consolevariablebridge.h"
#include "controller/controldeck/ControlDeck.h"
#include "Context.h"

namespace Ship {
SDLButtonToButtonMapping::SDLButtonToButtonMapping(uint8_t portIndex, CONTROLLERBUTTONS_T bitmask,
                                                   int32_t sdlControllerButton)
    : ControllerInputMapping(PhysicalDeviceType::SDLGamepad), SDLButtonToAnyMapping(sdlControllerButton),
      ControllerButtonMapping(PhysicalDeviceType::SDLGamepad, portIndex, bitmask) {
}

void SDLButtonToButtonMapping::UpdatePad(CONTROLLERBUTTONS_T& padButtons) {
    if (Context::GetInstance()->GetControlDeck()->GamepadGameInputBlocked()) {
        return;
    }

    auto mgr = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager();

    for (const auto& [instanceId, gamepad] : mgr->GetConnectedSDLGamepadsForPort(mPortIndex)) {
        if (mControllerButton < SDL_CONTROLLER_BUTTON_MAX &&
            SDL_GameControllerGetButton(gamepad, mControllerButton)) {
            padButtons |= mBitmask;
            return;
        }
    }

    for (const auto& [instanceId, joystick] : mgr->GetConnectedSDLJoysticksForPort(mPortIndex)) {
        if (mControllerButton >= 100) {
            int hatIndex = (mControllerButton - 100) / 4;
            int hatDir = (mControllerButton - 100) % 4;
            if (hatIndex < SDL_JoystickNumHats(joystick)) {
                Uint8 hatVal = SDL_JoystickGetHat(joystick, hatIndex);
                bool active = false;
                if (hatDir == 0 && (hatVal & SDL_HAT_UP)) active = true;
                if (hatDir == 1 && (hatVal & SDL_HAT_RIGHT)) active = true;
                if (hatDir == 2 && (hatVal & SDL_HAT_DOWN)) active = true;
                if (hatDir == 3 && (hatVal & SDL_HAT_LEFT)) active = true;
                if (active) {
                    padButtons |= mBitmask;
                    return;
                }
            }
        } else if (mControllerButton < SDL_JoystickNumButtons(joystick)) {
            if (SDL_JoystickGetButton(joystick, mControllerButton)) {
                padButtons |= mBitmask;
                return;
            }
        }
    }
}

int8_t SDLButtonToButtonMapping::GetMappingType() {
    return MAPPING_TYPE_GAMEPAD;
}

std::string SDLButtonToButtonMapping::GetButtonMappingId() {
    return StringHelper::Sprintf("P%d-B%d-SDLB%d", mPortIndex, mBitmask, mControllerButton);
}

void SDLButtonToButtonMapping::SaveToConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();
    CVarSetString(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str(),
                  "SDLButtonToButtonMapping");
    CVarSetInteger(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str(), mBitmask);
    CVarSetInteger(StringHelper::Sprintf("%s.SDLControllerButton", mappingCvarKey.c_str()).c_str(), mControllerButton);
    CVarSave();
}

void SDLButtonToButtonMapping::EraseFromConfig() {
    const std::string mappingCvarKey = CVAR_PREFIX_CONTROLLERS ".ButtonMappings." + GetButtonMappingId();

    CVarClear(StringHelper::Sprintf("%s.ButtonMappingClass", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.Bitmask", mappingCvarKey.c_str()).c_str());
    CVarClear(StringHelper::Sprintf("%s.SDLControllerButton", mappingCvarKey.c_str()).c_str());
    CVarSave();
}

std::string SDLButtonToButtonMapping::GetPhysicalDeviceName() {
    return SDLButtonToAnyMapping::GetPhysicalDeviceName();
}

std::string SDLButtonToButtonMapping::GetPhysicalInputName() {
    return SDLButtonToAnyMapping::GetPhysicalInputName();
}
} // namespace Ship
