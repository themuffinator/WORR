# Cgame build fixes and warning cleanup

## Overview
This change set resolves the cgame link errors and removes Windows build warnings without altering runtime behavior.

## Cgame link fixes
- Added a small C++ support unit to provide Q_strncasecmp and Q_strlcpy for cgame-only code that includes q_std.h.
- Avoided pulling in q_std.cpp for cgame to prevent duplicate symbols with cg_fmt.cpp and cg_parse.cpp.

## Windows linker flags
- Subproject link arguments now detect MSVC-style linkers (link/lld-link) and use /NXCOMPAT, /DYNAMICBASE, /HIGHENTROPYVA, and /BASE instead of GNU-style --nxcompat/--dynamicbase flags.
- GNU-style flags and -static-libgcc/-static-libstdc++ remain in use only for non MSVC-style linkers.

## Warning cleanup
- Updated user-defined literal operator declarations in g_local.h to remove deprecated whitespace after operator"".
- Added explicit void* casts for memset calls on edict_t to acknowledge non-trivial types.
- Added _CRT_SECURE_NO_WARNINGS and _CRT_NONSTDC_NO_WARNINGS to C++ build args to silence CRT deprecation warnings on Windows.
- Ensured Com_Error in cgame always terminates to match its noreturn contract.
