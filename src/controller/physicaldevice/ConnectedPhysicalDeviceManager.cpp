#include "ConnectedPhysicalDeviceManager.h"
#include <SDL2/SDL.h>
#include "public/bridge/consolevariablebridge.h"
#include "utils/StringHelper.h"
#include <algorithm>

namespace Ship {
ConnectedPhysicalDeviceManager::ConnectedPhysicalDeviceManager() {
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_JOYSTICK_RAWINPUT, "0");
#endif
}

ConnectedPhysicalDeviceManager::~ConnectedPhysicalDeviceManager() {
    for (auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        SDL_GameControllerClose(gamepad);
    }
    mConnectedSDLGamepads.clear();
    mConnectedSDLGamepadNames.clear();
    mConnectedSDLGamepadGuids.clear();

    for (auto& [instanceId, joystick] : mConnectedSDLJoysticks) {
        SDL_JoystickClose(joystick);
    }
    mConnectedSDLJoysticks.clear();
    mConnectedSDLJoystickNames.clear();
    mConnectedSDLJoystickGuids.clear();
}

std::string ConnectedPhysicalDeviceManager::GetDeviceGuidForInstanceId(int32_t instanceId) {
    if (mConnectedSDLJoystickGuids.contains(instanceId)) {
        return mConnectedSDLJoystickGuids[instanceId];
    }
    if (mConnectedSDLGamepadGuids.contains(instanceId)) {
        return mConnectedSDLGamepadGuids[instanceId];
    }
    return "";
}

std::unordered_map<int32_t, SDL_GameController*>
ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadsForPort(uint8_t portIndex) {
    std::unordered_map<int32_t, SDL_GameController*> result;

    for (const auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        if (!PortIsIgnoringInstanceId(portIndex, instanceId)) {
            result[instanceId] = gamepad;
        }
    }

    return result;
}

std::unordered_map<int32_t, SDL_Joystick*>
ConnectedPhysicalDeviceManager::GetConnectedSDLJoysticksForPort(uint8_t portIndex) {
    std::unordered_map<int32_t, SDL_Joystick*> result;

    for (const auto& [instanceId, joystick] : mConnectedSDLJoysticks) {
        if (!PortIsIgnoringInstanceId(portIndex, instanceId)) {
            result[instanceId] = joystick;
        }
    }

    return result;
}

std::unordered_map<int32_t, std::string> ConnectedPhysicalDeviceManager::GetConnectedSDLGamepadNames() {
    return mConnectedSDLGamepadNames;
}

std::unordered_map<int32_t, std::string> ConnectedPhysicalDeviceManager::GetConnectedSDLJoystickNames() {
    return mConnectedSDLJoystickNames;
}

std::unordered_set<int32_t> ConnectedPhysicalDeviceManager::GetIgnoredInstanceIdsForPort(uint8_t portIndex) {
    return mIgnoredInstanceIds[portIndex];
}

bool ConnectedPhysicalDeviceManager::PortIsIgnoringInstanceId(uint8_t portIndex, int32_t instanceId) {
    return GetIgnoredInstanceIdsForPort(portIndex).contains(instanceId);
}

void ConnectedPhysicalDeviceManager::IgnoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].insert(instanceId);
    std::string guid = GetDeviceGuidForInstanceId(instanceId);
    if (!guid.empty()) {
        const std::string key = StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.IgnoredDevice.%s", portIndex + 1, guid.c_str());
        CVarSetInteger(key.c_str(), 1);
        CVarSave();
    }
}

void ConnectedPhysicalDeviceManager::UnignoreInstanceIdForPort(uint8_t portIndex, int32_t instanceId) {
    mIgnoredInstanceIds[portIndex].erase(instanceId);
    std::string guid = GetDeviceGuidForInstanceId(instanceId);
    if (!guid.empty()) {
        const std::string key = StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.IgnoredDevice.%s", portIndex + 1, guid.c_str());
        CVarSetInteger(key.c_str(), 0);
        CVarSave();
    }
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceConnect(int32_t sdlDeviceIndex) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::HandlePhysicalDeviceDisconnect(int32_t sdlJoystickInstanceId) {
    RefreshConnectedSDLGamepads();
}

void ConnectedPhysicalDeviceManager::RefreshConnectedSDLGamepads() {
    for (auto& [instanceId, gamepad] : mConnectedSDLGamepads) {
        SDL_GameControllerClose(gamepad);
    }
    mConnectedSDLGamepads.clear();
    mConnectedSDLGamepadNames.clear();
    mConnectedSDLGamepadGuids.clear();

    for (auto& [instanceId, joystick] : mConnectedSDLJoysticks) {
        SDL_JoystickClose(joystick);
    }
    mConnectedSDLJoysticks.clear();
    mConnectedSDLJoystickNames.clear();
    mConnectedSDLJoystickGuids.clear();

    for (uint8_t port = 0; port < 4; port++) {
        mIgnoredInstanceIds[port].clear();
    }

    for (int32_t i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) {
            auto gamepad = SDL_GameControllerOpen(i);
            if (gamepad == nullptr) {
                continue;
            }
            auto joy = SDL_GameControllerGetJoystick(gamepad);
            auto instanceId = SDL_JoystickInstanceID(joy);
            if (instanceId < 0) {
                SDL_GameControllerClose(gamepad);
                continue;
            }
            auto name = SDL_GameControllerName(gamepad);
            SDL_JoystickGUID guid = SDL_JoystickGetGUID(joy);
            char guidStr[64] = { 0 };
            SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));

            mConnectedSDLGamepads[instanceId] = gamepad;
            mConnectedSDLGamepadNames[instanceId] = (name != nullptr) ? name : "SDL Gamepad";
            mConnectedSDLGamepadGuids[instanceId] = guidStr;

            for (uint8_t port = 0; port < 4; port++) {
                const std::string key = StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.IgnoredDevice.%s", port + 1, guidStr);
                int32_t cvarVal = CVarGetInteger(key.c_str(), -1);
                if (cvarVal == 1) {
                    mIgnoredInstanceIds[port].insert(instanceId);
                } else if (cvarVal == 0) {
                    mIgnoredInstanceIds[port].erase(instanceId);
                } else if (port > 0) {
                    mIgnoredInstanceIds[port].insert(instanceId);
                }
            }
        } else {
            auto joystick = SDL_JoystickOpen(i);
            if (joystick == nullptr) {
                continue;
            }
            auto instanceId = SDL_JoystickInstanceID(joystick);
            if (instanceId < 0) {
                SDL_JoystickClose(joystick);
                continue;
            }
            auto name = SDL_JoystickName(joystick);
            std::string nameStr = (name != nullptr) ? name : "SDL Joystick";
            SDL_JoystickGUID guid = SDL_JoystickGetGUID(joystick);
            char guidStr[64] = { 0 };
            SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));

            mConnectedSDLJoysticks[instanceId] = joystick;
            mConnectedSDLJoystickNames[instanceId] = nameStr;
            mConnectedSDLJoystickGuids[instanceId] = guidStr;

            std::string lowerName = nameStr;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            bool isRacingPeripheral = (lowerName.find("racer") != std::string::npos ||
                                       lowerName.find("wheel") != std::string::npos ||
                                       lowerName.find("pedal") != std::string::npos ||
                                       lowerName.find("gear shift") != std::string::npos ||
                                       lowerName.find("shifter") != std::string::npos ||
                                       lowerName.find("fanatec") != std::string::npos);

            for (uint8_t port = 0; port < 4; port++) {
                const std::string key = StringHelper::Sprintf(CVAR_PREFIX_CONTROLLERS ".Port%d.IgnoredDevice.%s", port + 1, guidStr);
                int32_t cvarVal = CVarGetInteger(key.c_str(), -1);
                if (cvarVal == 1) {
                    mIgnoredInstanceIds[port].insert(instanceId);
                } else if (cvarVal == 0) {
                    mIgnoredInstanceIds[port].erase(instanceId);
                } else if (port > 0 || isRacingPeripheral) {
                    mIgnoredInstanceIds[port].insert(instanceId);
                }
            }
        }
    }
}
} // namespace Ship
