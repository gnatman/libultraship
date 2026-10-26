#include "public/bridge/VRHudBridge.h"
#include "libultraship/libultra/gbi.h"
#include "graphic/Fast3D/lus_gbi.h"

extern "C" void Ship_VR_EmitHudPassBegin(union Gfx** dl) {
    if (!dl || !*dl) {
        return;
    }
    gDPNoOpTag((*dl)++, LUS_NOOP_TAG_VR_HUD_PASS_BEGIN);
}

extern "C" void Ship_VR_EmitHudPassEnd(union Gfx** dl) {
    if (!dl || !*dl) {
        return;
    }
    gDPNoOpTag((*dl)++, LUS_NOOP_TAG_VR_HUD_PASS_END);
}
