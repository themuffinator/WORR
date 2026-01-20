# VKPT Shader Search Order

## Summary
- Shader module loads now follow this order:
  1. current working directory basedir (CWD/BASEGAME)
  2. user files basedir (sys_homedir/BASEGAME)
  3. game installation basedir (sys_basedir via FS base search paths)

## Rationale
- Lets dev builds pick up local SPIR-V outputs before user or install copies.
- Keeps installation fallback working even when shaders live inside packs.

## Implementation
- Added helpers in src/rend_vk/vkpt/main.c to orchestrate the ordered lookup.
- create_shader_module_from_file now calls vkpt_load_shader_data instead of FS_LoadFile.
- The current dir and homedir checks read from disk; the install check uses FS_LoadFileEx
  with base-only flags to preserve pack/zip support.

## Notes
- This change only affects VKPT shader module loads.
- Other asset lookups continue to use the standard filesystem search order.
