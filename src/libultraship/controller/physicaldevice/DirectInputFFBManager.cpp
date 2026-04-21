#ifdef _WIN32
#include "DirectInputFFBManager.h"
#include <SDL2/SDL.h>
#include <spdlog/spdlog.h>

namespace Ship {

void DirectInputFFBManager::Refresh() {
    SPDLOG_TRACE("Refreshing DirectInputFFBManager");
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; ++i) {
        SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(i);
        char guidStr[33];
        SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
        
        if (mDevices.find(guidStr) == mDevices.end()) {
            std::string name = SDL_JoystickNameForIndex(i);
            SPDLOG_DEBUG("Attempting to initialize DirectInput FFB for {} ({})", name, guidStr);
            auto device = std::make_shared<DirectInputFFBDevice>(guid, name);
            if (device->Init()) {
                mDevices[guidStr] = device;
            }
        }
    }
}

std::shared_ptr<DirectInputFFBDevice> DirectInputFFBManager::GetDevice(SDL_JoystickGUID guid) {
    char guidStr[33];
    SDL_JoystickGetGUIDString(guid, guidStr, sizeof(guidStr));
    if (mDevices.find(guidStr) != mDevices.end()) {
        return mDevices[guidStr];
    }
    return nullptr;
}

std::shared_ptr<DirectInputFFBDevice> DirectInputFFBManager::GetDevice(int index) {
    if (index >= 0 && index < (int)mDevices.size()) {
        auto it = mDevices.begin();
        std::advance(it, index);
        return it->second;
    }
    return nullptr;
}

} // namespace Ship
#endif
