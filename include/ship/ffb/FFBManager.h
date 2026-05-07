#pragma once
#include "FFBDevice.h"
#include <memory>
#include <string>

namespace Ship {
class FFBManager {
public:
    static FFBManager& GetInstance();
    void Init(const std::string& guid);
    std::shared_ptr<FFBDevice> GetDevice() { return mDevice; }
private:
    std::shared_ptr<FFBDevice> mDevice;
};
}
