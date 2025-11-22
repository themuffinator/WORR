<div align="center">
  <img src="assets/art/logo.png" alt="WORR Logo" width="300">
  <h2>WORR – The new way to experience <b>QUAKE II Rerelease</b></h2>
</div>

<p align="center">
  <b>WORR</b> is an advanced gameplay expansion and standalone engine fork for <b>QUAKE II Rerelease</b>,  
  designed as a drop-in replacement offering a richer, more dynamic and refined single-player and multiplayer experience.
</p>

<p align="center">
  The KEX-dependent server mod variant can be found here:  
  <a href="https://github.com/themuffinator/worr-kex">themuffinator/worr-kex</a>
</p>

<p align="center">
  WORR is the spiritual successor to  
  <a href="https://github.com/themuffinator/muffmode">Muff Mode</a>.
</p>

---

## About

**WORR** is a fork of [Q2REPRO](https://github.com/Paril/q2repro), which in turn is a fork of [Q2PRO](https://code.nephatrine.net/QuakeArchive/q2pro.git), which itself is a fork of the original [Quake II](https://github.com/id-Software/Quake-2).

The goals of WORR are:

- To act as a **drop-in engine replacement** for the official Quake II Rerelease assets.
- To provide a **modern C++ codebase** suitable for long-term development and experimentation.
- To power the extensive **WORR gameplay module** (and future projects) with:
  - Expanded entity and monster support (across Quake titles and mods),
  - Competitive and casual multiplayer improvements,
  - Modern rendering and UI systems.

> ⚠️ **Compatibility note:**  
> Future engine updates may break compatibility with non-**WORR** game modules.

---

## Project Goals

**Status key:** ✅ Complete · 🟡 In&nbsp;Progress · 🔴 Planned

### Core Milestones

- ✅ Implement a bare-minimum C++ migration, buildable and bug-free  
- 🟡 Complete Vulkan renderer (covering GL feature set and compatibility), allow external binary build and renderer selector  
- 🔴 Implement a functional bot system – workable in campaigns and multiplayer alike  

### GUI

- ✅ JSON menu scripting  
- 🟡 Update GUI to allow for a full range of UI elements  
- 🟡 Integrate full FreeType2 support and Quake III color escape sequences  
- ✅ Enhance aspect correction / screen positioning to be more robust and correct  
- 🟡 Extend / enhance menu selection  
- 🔴 Graphical obituaries, chatbox  

### Online

- 🟡 **Website:** Basic server browser, user environment, ladder, etc.  
- 🔴 **Engine:** Engine bootstrapper with auto-updater, CDN for asset and update delivery  
- 🔴 **All:** Discord OAuth integration for user management, wired into engine and game module  
- 🔴 **All:** Discord server bot (or alternative) to bridge game server, web backend and Discord  
- 🔴 Set up a public game server (NL location to start, ideally)  

### Rendering

- 🟡 Depth of field / slow-time  
- 🔴 Player outlines (and possibly rim lighting) with team support  
- 🔴 Player bright skins with color selector  
- 🟡 Shadowmapping, compatible with Quake II Rerelease maps  
- 🟡 Motion blur  
- 🟡 Revised bloom with modern tone/color correction  
- 🟡 HDR pipeline  

### Structure

- 🔴 Split game module into `cgame` / `game` for client / server separation  
- 🔴 Migrate the majority of UI code from the engine into `cgame`  

### Asset Support

- 🔴 IQM model support  
- 🔴 Extended BSP support: `IBSP29`, `BSP2`, `BSP2L`, `BSPX`  

---

## Building

For build instructions, see **[`BUILDING`](BUILDING.md)**.

This covers:

- Required toolchain and dependencies,
- Configuration options,
- Build targets for the engine and game module.

---

## Usage & Documentation

For information on using and configuring **WORR**, refer to the manuals in the `doc/` subdirectory:

- Client configuration and advanced options,
- Server configuration, match presets and hosting details,
- Notes on compatibility with Quake II Rerelease assets.

### UI Architecture

The client UI is now driven by a modern `MenuItem` class hierarchy that mirrors the legacy menu widgets while using RAII to manage ownership. Menu textures are reference-counted and shared across items, while child items are owned through `std::unique_ptr`, eliminating manual lifetime management and keeping activation callbacks self-contained.

---

## Related Repositories

- **WORR (KEX server mod):**  
  <https://github.com/themuffinator/worr-kex>

- **Muff Mode (legacy project):**  
  <https://github.com/themuffinator/muffmode>

- **Q2REPRO:**  
  <https://github.com/Paril/q2repro>

- **Q2PRO:**  
  <https://code.nephatrine.net/QuakeArchive/q2pro.git>

---

## License

See the [`LICENSE`](LICENSE) file in this repository for licensing details.
