#pragma once

#ifdef _WIN32
#include "DirectInputFFBDevice.h"
#include <vector>
#include <memory>
#include <map>

namespace Ship {

class DirectInputFFBManager {
  public:
    static DirectInputFFBManager& Instance() {
        static DirectInputFFBManager instance;
        return instance;
    }

    void Refresh();
    std::shared_ptr<DirectInputFFBDevice> GetDevice(SDL_JoystickGUID guid);
    std::shared_ptr<DirectInputFFBDevice> GetDevice(int index);

  private:
    DirectInputFFBManager() = default;
    std::map<std::string, std::shared_ptr<DirectInputFFBDevice>> mDevices;
};

} // namespace Ship
#endif
