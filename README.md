WORR
====

**WORR** is a fork of [Q2REPRO](https://github.com/Paril/q2repro) which in turn
is a fork of [Q2PRO](https://github.com/skullernet/q2pro) which in turn is a fork
of [Quake II](https://github.com/id-Software/Quake-2)... forktacular! It is designed to be a
drop-in replacement of the Quake II re-release engine and intended for use
with the upcoming and extensive [Muff Mode](https://github.com/themuffinator/muffmode)-based mod **WORR**.
Future updates may break compatibility with non-**WORR** game modules.

PROJECT GOALS
-------------

* Implement a bare-minimum C++ migration, buildable and bug-free (WIP)
* Complete Vulkan renderer (covering GL feature set and compatibility), allow external binary build and renderer selector (WIP)
* Implement a functional bot system - workable in campaigns and multiplayer alike
* GUI:
  - JSON menu scripting [DONE]
  - Update GUI to allow for a full range of UI elements
  - Integrate full FT2 support and Q3 color escape sequences
  - Enhance aspect correction/screen positioning to be more robust and correct
  - Extend/enhance menu selection
* Online:
  - WEBSITE: Set up basic server browser, user environment, ladder, etc. 
  - ENGINE: Engine bootstrapper with auto-updater, set up cdn server for transfer
  - ALL: Discord OAuth integration for user management, integration into engine and game module too
  - ALL: Discord server bot handling or alternative means of communication between game server, web server and Discord server
  - Set up a game server (NL location for starters ideally)
* Rendering:
  - Depth of field/slowtime
  - Player outlines (and maybe rim lighting?) with teams support
  - Player bright skins with color selector
  - Shadowmapping, Q2Re maps compatible
  - Motion blur
  - Revise bloom if need be, color correction
  - HDR
* Structure:
  - Split game module into cgame/game for client/server
  - Migrate majority of UI code from engine into cgame
* Asset support:
  - IQM model support
  - Extended BSP support: IBSP29, BSP2, BSP2L, BSPX
 
BUILDING
--------

For building **WORR**, consult the `BUILDING.md` file.

For information on using and configuring **WORR**, refer to client and server
manuals available in doc/ subdirectory.
