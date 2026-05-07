#include "ship/ffb/FFBManager.h"
#include "ship/ffb/DirectInputFFB.h"

namespace Ship {
FFBManager& FFBManager::GetInstance() {
    static FFBManager instance;
    return instance;
}

void FFBManager::Init(const std::string& guid) {
#ifdef _WIN32
    mDevice = std::make_shared<DirectInputFFB>(guid);
#else
    // Fallback/Null implementation
#endif
}
}
