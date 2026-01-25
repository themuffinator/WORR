Crosshair Pickup Pulse Trigger
==============================

Pickup pulses now restart based on the local player's pickup sounds so
repeated pickups of the same icon/name still snap the crosshair back to
normal and ramp again.

Detection logic:
- Only local player sounds on CHAN_ITEM are considered.
- Sound names containing "pkup.wav" or "_health.wav" are treated as pickup
  events (covers weapon/ammo/armor/health pickup sounds).
- When detected, the pickup pulse start time is reset (cl_crosshair_pulse
  still gates the effect).
