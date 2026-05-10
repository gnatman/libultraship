#ifndef LUS_VR_HUD_BRIDGE_H
#define LUS_VR_HUD_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forward-declare Gfx (real definition in libultra/gbi.h) so this header
   is includable from C without pulling in the full gbi headers. */
union Gfx;

/**
 * Emit a VR_HUD_PASS_BEGIN marker into the display list pointed to by *dl.
 * Advances *dl by one Gfx command.
 *
 * Safe to call when VR is off; the marker is a harmless NOOP that the
 * interpreter's NOOP handler will skip.
 */
void Ship_VR_EmitHudPassBegin(union Gfx** dl);

/** Emit a VR_HUD_PASS_END marker. Pair with Ship_VR_EmitHudPassBegin. */
void Ship_VR_EmitHudPassEnd(union Gfx** dl);

#ifdef __cplusplus
}
#endif

#endif /* LUS_VR_HUD_BRIDGE_H */
