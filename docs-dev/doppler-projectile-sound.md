# Doppler Projectile Sound

## Changes
- Added a new renderfx flag `RF_DOPPLER` to mark entities whose looping sounds should receive doppler processing on the client.
- OpenAL now applies per-source and listener velocities so doppler pitch shifts track motion for flagged projectiles.
- Added `al_doppler` cvar to control doppler strength (0 disables).
- Doppler velocities are smoothed and clamped to avoid pitch spikes from net jitter or teleports.
- Loop merging now bypasses `RF_DOPPLER` sounds so their per-entity velocity is preserved.

## Cvars
- `al_doppler` (default 1): Doppler factor for OpenAL. 0 disables; higher values increase intensity. Effect only when OpenAL is active.
- `al_doppler_speed` (default 13500): speed of sound in Quake units per second.
- `al_doppler_min_speed` (default 30): minimum source speed before doppler applies.
- `al_doppler_max_speed` (default 4000): clamps source speed used for doppler to avoid spikes.
- `al_doppler_smooth` (default 12): exponential smoothing rate for source/listener velocities.

## Projectiles Flagged
- Blaster and hyperblaster bolts (including blue blaster and ion ripper).
- Rockets (rocket launcher, heat seeker, phalanx).
- Plasma gun bolts.
- Disruptor/tracker bolts.
- BFG and disintegrator balls.
- Vore homing pods.
- Quake 1 plasmaball and tesla bolt.

## Notes
- Doppler uses smoothed entity motion and a speed-of-sound constant controlled by `al_doppler_speed`.
- Merged looping sources (al_merge_looping) bypass merge when `RF_DOPPLER` is set so doppler remains active.
