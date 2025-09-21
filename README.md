Q2REPRO
=====

Q2REPRO is a fork of [Q2PRO](https://github.com/skullernet/q2pro) designed
to be a drop-in replacement of the Kex Quake II re-release engine.

Goals:

* All re-release content fully playable (mostly complete; some minor visual things unfinished)
* Compatibility with existing Quake II network protocols (incomplete; our first focus is the first point)
* Full netplay support with the native re-release game DLL (incomplete; it works better with our custom game_x64 but is still a WIP)
* No installation required (complete! all you need is the exe; for Win64, no additional DLLs or files are required. Just open & play whenever; it will pick up your Steam / GoG installation directory. You can also drop it into your game dir like other clients.)
* Full support of all re-release features (mostly complete; see Issues tab for things unfinished)
* Default configuration should match re-release (complete but still being worked on)

Supported features include (from Q2PRO):

* unified OpenGL renderer with support for wide range of OpenGL versions
* enhanced console with persistent command history and autocompletion
* rendering / physics / packet rate separation
* syncing to GPU for reduced input lag with vsync on
* ZIP packfiles (.pkz)
* JPEG/PNG textures and screenshots
* MD3 and MD5 (re-release) models
* Ogg Vorbis music and Ogg Theora cinematics
* compatibility with re-release assets
* fast and secure HTTP downloads
* multichannel sound using OpenAL
* stereo WAV files support
* seeking in demos, recording from demos, server side multiview demos
* live game broadcasting capabilities
* network protocol extensions for larger maps
* eliminates frame overflows (even for legacy clients)
* won't crash if game data is corrupted

New features that differ from Q2PRO:

* Support for weapon wheels and carousel from re-release (+wheel / +wheel2)
* Reverb support (enabled by default; `al_reverb` cvar)
* Per-pixel lighting (enabled by default; `gl_per_pixel_lighting` cvar. Requires `gl_shaders`. Still a WIP and may reduce performance. No shadow mapping implemented yet.)
* Defer commands on map (when commands are entered after a `map` cmd, they are deferred until the map is loaded)
* Archive natsort (re-release uses natsort on pak, as well as always loading `pakN` first; this order is maintained in Q2REPRO)
* Navigation file support (can load .nav files from re-release, & supports `GetPathToGoal`; can be debugged, but not edited, with `nav_debug 1`)
* MapDB support (can load `mapdb.json` and display levels + episodes in Single Player menu)
* KFont support (only used for cgame currently)
* Minor gl_shadow visual tweaks
* Light style interpolation on by default (`cl_lerp_lightstyles` cvar)
* Shadow light support (only for lighting, no shadowing yet; `cl_shadowlights` can be set to 0 to disable these)
* 40hz tick rate, can be controlled with `sv_tick_rate` like re-release
* POI support (for compass, etc)
* `+holster` support (for carousel/wheel)
* Dog tag support (for multiplayer)
* Client game DLL support (only the thin game_x64 cgame impl)
* New menu, baked into the executable (`q2repro.menu` can be overridden by mod folder)
* No color clipping/clamping on entity lightmap/lightgrid colors
* Removed `gl_doublelight_entities`; use `gl_modulate_entities`
* Damage markers, hit markers
* OpenAL is now the default & only supported audio output in our builds, but you can compile software audio if you want

For building Q2REPRO, consult the BUILDING.md file.

For information on using and configuring Q2REPRO, refer to client and server
manuals available in doc/ subdirectory.
