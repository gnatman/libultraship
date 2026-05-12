#include "vr/VRToggle.h"
#include "vr/VRRuntime.h"
#include "ship/Context.h"
#include "ship/config/ConsoleVariable.h"
#include <spdlog/spdlog.h>

namespace Ship {

int32_t VRToggle::mEnabled = -1;

void VRToggle::Update() {
    auto context = Context::GetInstance();
    int32_t enabled = context->GetConsoleVariables()->GetInteger("gVREnabled", 0);
    
    if (enabled != mEnabled) {
        mEnabled = enabled;

        auto runtime = VRRuntime::GetInstance();
        if (mEnabled == 1) {
            runtime->Init();
        } else {
            runtime->Shutdown();
        }
    }

    if (mEnabled == 1) {
        VRRuntime::GetInstance()->Update();
    }
}

bool VRToggle::IsVREnabled() {
    return mEnabled == 1;
}

} // namespace Ship
