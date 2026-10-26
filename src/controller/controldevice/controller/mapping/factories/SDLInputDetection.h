#pragma once

#include <unordered_map>
#include <memory>
#include <cmath>
#include <SDL2/SDL.h>
#include "Context.h"
#include "controller/controldeck/ControlDeck.h"
#include "controller/physicaldevice/ConnectedPhysicalDeviceManager.h"
#include "controller/controldevice/controller/mapping/ControllerAxisDirectionMapping.h"
#include "controller/controldevice/controller/mapping/sdl/SDLMapping.h"

namespace Ship {

struct DetectedSDLButton {
    int32_t button = -1; // normal button or >= 100 for hat
    std::string deviceGuid = "";
};

struct DetectedSDLAxis {
    int32_t axis = -1;
    int32_t direction = 0; // -1 for NEGATIVE, 1 for POSITIVE
    std::string deviceGuid = "";
};

class SDLInputDetection {
  public:
    static void ResetSession() {
        sNeedsInit = true;
    }

    static bool PollInput(uint8_t portIndex, DetectedSDLButton& outButton, DetectedSDLAxis& outAxis) {
        uint32_t now = SDL_GetTicks();
        if (sNeedsInit || (now - sLastTick > 150)) {
            InitBaseline(portIndex);
            sLastTick = now;
            sNeedsInit = false;
            return false;
        }
        sLastTick = now;

        auto mgr = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager();

        // 1. Check Gamepads
        for (const auto& [instanceId, gamepad] : mgr->GetConnectedSDLGamepadsForPort(portIndex)) {
            // Check buttons
            for (int32_t button = SDL_CONTROLLER_BUTTON_A; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
                uint8_t current = SDL_GameControllerGetButton(gamepad, static_cast<SDL_GameControllerButton>(button));
                uint8_t initial = sGamepadButtons[instanceId][button];
                if (!initial && current) {
                    outButton.button = button;
                    outButton.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
            }

            // Check axes
            for (int32_t i = SDL_CONTROLLER_AXIS_LEFTX; i < SDL_CONTROLLER_AXIS_MAX; i++) {
                float current = SDL_GameControllerGetAxis(gamepad, static_cast<SDL_GameControllerAxis>(i)) / 32767.0f;
                float initial = sGamepadAxes[instanceId][i];
                float delta = current - initial;
                if (delta > 0.5f) {
                    outAxis.axis = i;
                    outAxis.direction = POSITIVE;
                    outAxis.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                } else if (delta < -0.5f) {
                    outAxis.axis = i;
                    outAxis.direction = NEGATIVE;
                    outAxis.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
            }
        }

        // 2. Check Joysticks (e.g. Flight Stick / Thrustmaster Airbus)
        for (const auto& [instanceId, joystick] : mgr->GetConnectedSDLJoysticksForPort(portIndex)) {
            // Check buttons
            int numButtons = SDL_JoystickNumButtons(joystick);
            for (int32_t button = 0; button < numButtons; button++) {
                uint8_t current = SDL_JoystickGetButton(joystick, button);
                uint8_t initial = sJoystickButtons[instanceId][button];
                if (!initial && current) {
                    outButton.button = button;
                    outButton.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
            }

            // Check hats (POV Hat)
            int numHats = SDL_JoystickNumHats(joystick);
            for (int32_t h = 0; h < numHats; h++) {
                uint8_t current = SDL_JoystickGetHat(joystick, h);
                uint8_t initial = sJoystickHats[instanceId][h];
                if (!(initial & SDL_HAT_UP) && (current & SDL_HAT_UP)) {
                    outButton.button = 100 + h * 4 + 0;
                    outButton.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
                if (!(initial & SDL_HAT_RIGHT) && (current & SDL_HAT_RIGHT)) {
                    outButton.button = 100 + h * 4 + 1;
                    outButton.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
                if (!(initial & SDL_HAT_DOWN) && (current & SDL_HAT_DOWN)) {
                    outButton.button = 100 + h * 4 + 2;
                    outButton.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
                if (!(initial & SDL_HAT_LEFT) && (current & SDL_HAT_LEFT)) {
                    outButton.button = 100 + h * 4 + 3;
                    outButton.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
            }

            // Check axes
            int numAxes = SDL_JoystickNumAxes(joystick);
            for (int32_t axis = 0; axis < numAxes; axis++) {
                float current = SDL_JoystickGetAxis(joystick, axis) / 32767.0f;
                float initial = sJoystickAxes[instanceId][axis];
                float delta = current - initial;
                if (delta > 0.5f) {
                    outAxis.axis = axis;
                    outAxis.direction = POSITIVE;
                    outAxis.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                } else if (delta < -0.5f) {
                    outAxis.axis = axis;
                    outAxis.direction = NEGATIVE;
                    outAxis.deviceGuid = mgr->GetDeviceGuidForInstanceId(instanceId);
                    ResetSession();
                    return true;
                }
            }
        }

        return false;
    }

  private:
    static inline bool sNeedsInit = true;
    static inline uint32_t sLastTick = 0;
    static inline std::unordered_map<int32_t, std::unordered_map<int32_t, float>> sGamepadAxes;
    static inline std::unordered_map<int32_t, std::unordered_map<int32_t, uint8_t>> sGamepadButtons;
    static inline std::unordered_map<int32_t, std::unordered_map<int32_t, float>> sJoystickAxes;
    static inline std::unordered_map<int32_t, std::unordered_map<int32_t, uint8_t>> sJoystickButtons;
    static inline std::unordered_map<int32_t, std::unordered_map<int32_t, uint8_t>> sJoystickHats;

    static void InitBaseline(uint8_t portIndex) {
        sGamepadAxes.clear();
        sGamepadButtons.clear();
        sJoystickAxes.clear();
        sJoystickButtons.clear();
        sJoystickHats.clear();

        auto mgr = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager();

        for (const auto& [instanceId, gamepad] : mgr->GetConnectedSDLGamepadsForPort(portIndex)) {
            for (int32_t button = SDL_CONTROLLER_BUTTON_A; button < SDL_CONTROLLER_BUTTON_MAX; button++) {
                sGamepadButtons[instanceId][button] =
                    SDL_GameControllerGetButton(gamepad, static_cast<SDL_GameControllerButton>(button));
            }
            for (int32_t axis = SDL_CONTROLLER_AXIS_LEFTX; axis < SDL_CONTROLLER_AXIS_MAX; axis++) {
                sGamepadAxes[instanceId][axis] =
                    SDL_GameControllerGetAxis(gamepad, static_cast<SDL_GameControllerAxis>(axis)) / 32767.0f;
            }
        }

        for (const auto& [instanceId, joystick] : mgr->GetConnectedSDLJoysticksForPort(portIndex)) {
            int numButtons = SDL_JoystickNumButtons(joystick);
            for (int32_t button = 0; button < numButtons; button++) {
                sJoystickButtons[instanceId][button] = SDL_JoystickGetButton(joystick, button);
            }
            int numHats = SDL_JoystickNumHats(joystick);
            for (int32_t h = 0; h < numHats; h++) {
                sJoystickHats[instanceId][h] = SDL_JoystickGetHat(joystick, h);
            }
            int numAxes = SDL_JoystickNumAxes(joystick);
            for (int32_t axis = 0; axis < numAxes; axis++) {
                sJoystickAxes[instanceId][axis] = SDL_JoystickGetAxis(joystick, axis) / 32767.0f;
            }
        }
    }
};

} // namespace Ship
