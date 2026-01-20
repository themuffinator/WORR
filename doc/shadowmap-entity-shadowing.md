# Shadowmap Follow-up: Shadowlight Emission and Model Casting

## Overview
- Ensure map entity shadowlights always emit light using their key values, even when shadowmaps are disabled.
- Include bmodels and alias models in shadowmap rendering passes.
- Improve default shadowmap filtering and softness for smoother results.

## Map Shadowlight Emission
- CL_AddShadowLights no longer gates on cl_shadowlights or per-pixel support; lights are always added when configstrings exist.
- V_AddLightEx only marks lights as shadow-casting when cl_shadowlights is enabled and per-pixel lighting is supported.
- Result: shadowlight entities always emit light based on radius/intensity/style; shadowmaps remain optional.

## Shadow Casting for Entities
- Shadowmap render pass now draws alpha_back and alpha_front entity lists in addition to bmodels and opaque.
- RF_NOSHADOW no longer suppresses shadowmap casting; it only affects the legacy blob shadow path.
- Sprite models still skip shadowmap rendering; weapon models remain excluded.

## Default Shadowmap Tuning
- gl_shadowmap_softness default: 1.5
- gl_shadowmap_quality default: 2 (3x3 PCF)
- gl_shadowmap_filter default remains 1 (PCF)

## Notes
- Translucent models now cast full shadows (no partial transparency).
- cl_shadowlights 0 disables shadowmap casting for shadowlights but keeps their light contribution.

## Manual Check
- Use a map with func_rotating, player, and monster models under a shadowlight.
- Verify they cast shadows in motion.
- Toggle cl_shadowlights 0/1 to confirm shadows disable but lighting remains.
- Adjust gl_shadowmap_* cvars to taste.
