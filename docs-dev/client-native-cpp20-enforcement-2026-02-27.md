# Client Native C++20 Enforcement (2026-02-27)

## Summary
- Enforced strict, native C++20 for client-side build targets in Meson.
- Removed ad-hoc compiler-specific standard selection (`c++2b`/`c++23`/`c++latest`) from client compile arguments.
- Preserved server-side C++ standard settings; this change is scoped to client module targets.

## Why
- Manual `-std=`/`/std:` injection bypassed Meson's native `cpp_std` handling and preferred evolving language modes over a fixed baseline.
- A fixed `cpp_std=c++20` gives deterministic language behavior across compilers and prevents unintentional standard drift.

## Build System Changes
- File: `meson.build`

1. Client compile-arg cleanup:
- Removed GCC/Clang `client_cpp_std_args` probing for:
  - `-std=gnu++2b`, `-std=c++2b`, `-std=gnu++23`, `-std=c++23`, `-std=gnu++20`, `-std=c++20`
- Removed MSVC `/std:c++latest` from `client_cpp_args`.
- Kept non-standard-related client warning/define arguments unchanged.

2. Client executable standard:
- Target `worr` now sets:
  - `override_options: ['cpp_std=c++20']`

3. Client game DLL standard:
- Target `cgame<cpu>` changed from:
  - `override_options: ['b_vscrt=static_from_buildtype', 'cpp_std=c++23']`
- To:
  - `override_options: ['b_vscrt=static_from_buildtype', 'cpp_std=c++20']`

## Scope
- Included:
  - Client executable (`worr`)
  - Client game DLL (`cgame<cpu>`)
- Not changed:
  - Server game DLL (`sgame<cpu>`), which remains on its existing standard configuration.
  - Subproject fallback defaults (`fmt`/`jsoncpp`) and global project default `cpp_std`.

## Verification
Commands run on 2026-02-27 (Windows host):

1. Reconfigure:
- `meson setup builddir-client-cpp20 --reconfigure`
- Result: configure succeeded with the updated Meson target options.

2. Verify generated compile flags:
- Inspected `builddir-client-cpp20/compile_commands.json`.
- `src/client/*.cpp` compile commands include `-std=c++20`.
- `src/game/cgame/*.cpp` compile commands include `-std=c++20`.
- No client-side `-std=c++23`, `-std=c++2b`, or `/std:c++latest` entries present.
- Count check:
  - `src/client/*.cpp`: 30/30 use `c++20`, 0 use disallowed higher/latest flags.
  - `src/game/cgame/*.cpp`: 28/28 use `c++20`, 0 use disallowed higher/latest flags.

3. Build client targets:
- `meson compile -C builddir-client-cpp20 worr cgamex86_64`
- Result: success (both targets linked).

4. Refresh staged install root:
- Built required runtime targets used by validation:
  - `meson compile -C builddir-client-cpp20 copy_cgame_dll copy_sgame_dll`
  - `meson compile -C builddir-client-cpp20 worr.ded`
  - `meson compile -C builddir-client-cpp20 worr_updater`
- Refreshed and validated staging:
  - `python tools\\refresh_install.py --build-dir builddir-client-cpp20 --install-dir .install --base-game baseq2 --platform-id windows-x86_64`
- Result: `Validated staged install for windows-x86_64`.
