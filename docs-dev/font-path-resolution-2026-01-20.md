Font Path Resolution for IT_FONT

Overview
This change ensures font assets in `Q2Game.kpf` resolve from their actual
subdirectories (such as `fonts/`) instead of being forced under `pics/`.

What Changed
- The renderer now treats `IT_FONT` names that already include a path
  separator (`/` or `\\`) as rooted paths and normalizes them directly.
- Bare font names (for example `conchars`) still map to `pics/` and receive
  the default `.pcx` extension for legacy support.

Why
The font cvars use paths like `fonts/RobotoMono-Regular.ttf`, and those files
live under `fonts/` inside `Q2Game.kpf`. Prefixing `pics/` caused invalid
lookups such as `pics/fonts/...`.

Scope
Applied consistently to both OpenGL and Vulkan render backends in
`R_RegisterImage`.
