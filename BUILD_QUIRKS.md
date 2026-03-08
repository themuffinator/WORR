# Build Quirks

Platform-specific issues and workarounds for building WORR.

## FFmpeg / nasm.wrap redirect failure

**Symptom:**
```
wrap-redirect E:...\subprojects\ffmpeg\subprojects\nasm.wrap filename does not exist
```

**Cause:** Meson processes wrap files at setup time. The `subprojects/nasm.wrap` redirect points to `ffmpeg/subprojects/nasm.wrap`, which either doesn't exist in the GStreamer ffmpeg meson-ports repo or isn't available before ffmpeg is cloned.

**Workaround:** If you don't need avcodec (cinematics, OGG audio):

1. Rename or delete `WORR/subprojects/nasm.wrap` so Meson won't process it.
2. Configure with avcodec disabled:
   ```bash
   meson setup build -Davcodec=disabled
   ```

## C++23 not recognized by Meson on MSVC

**Symptom:**
```
ERROR: None of values ['c++23'] are supported by the CPP compiler. Possible values for option "cpp_std" are ['none', 'c++11', 'c++14', 'c++17', 'c++20', 'c++latest', ...]
```

**Cause:** Meson 1.10.x has a fixed list of supported `cpp_std` values for MSVC. `c++23` isn't in that list, even though VC 2022+ supports it.

**Workaround:** Use `c++latest` instead of `c++23`. Meson accepts it, and MSVC maps it to `/std:c++latest` (C++23 on recent toolchains). The sgame DLL override uses `cpp_std=c++latest` for MSVC only; on GCC/Clang (Linux/BSD) it uses `c++23` because `c++latest` is not supported there.

## GCC/Clang: c++latest not supported

**Symptom (on Linux):**
```
ERROR: None of values ['c++latest'] are supported by the CPP compiler. Possible values for option "cpp_std" are ['none', 'c++98', ..., 'c++23', 'c++26', ...]
```

**Cause:** `c++latest` is an MSVC-specific value. GCC and Clang support `c++23` (and `c++26`) but not `c++latest`.

**Fix:** `meson.build` selects `cpp_std` per compiler: `c++latest` for MSVC, `c++23` for GCC/Clang.

## MSVC C4576: compound literal in C++ (COLOR_RGBA etc.)

**Symptom:**
```
error C4576: a parenthesized type followed by an initializer list is a non-standard explicit type conversion syntax
```
Seen in `inc/common/utils.h`, `inc/shared/shared.h`, and anywhere using `COLOR_RGBA`, `COLOR_RED`, etc.

**Cause:** MSVC with `/permissive-` treats C99 compound literals `(color_t){ .r = ..., .g = ... }` as non-standard in C++. The `COLOR_RGBA` and related macros in `shared.h` used this form.

**Fix:** `inc/shared/shared.h` now uses `#ifdef __cplusplus` to emit `color_t{ ... }` brace-initialization in C++ (valid in C++20) while keeping the C compound-literal form for C translation units.

**Same issue for `vec2_t` / `vec3_t`:** Use the `VEC2(x, y)` and `VEC3(x, y, z)` macros instead of `(const vec2_t){ x, y }` / `(const vec3_t){ x, y, z }`. In C++ they expand to brace-initialization; in C they keep the compound literal. Applied in `src/client/effects.cpp`, `src/client/view.cpp`.

**Same issue for array compound literals** (e.g. `(const alfunction_t[]){ ... }`): MSVC rejects these in C++. Refactor to named static arrays and reference them. Applied in `src/client/sound/qal.cpp` for the OpenAL function tables.

## MSVC C2099: compound literal in static initializer (C)

**Symptom:**
```
error C2099: initializer is not a constant
```
Seen in `inc/common/utils.h` when compiling C files (e.g. `field.c`) that use `COLOR_RGBA` in `static const` array initializers.

**Cause:** In C, compound literals like `(color_t){ .r = x, .g = y, ... }` are not constant expressions. MSVC rejects them in static storage initializers. GCC may accept this as an extension.

**Fix:** Use `{ .u32 = COLOR_U32_RGBA(r,g,b,a) }` instead of `COLOR_RGBA(r,g,b,a)` for static `color_t` arrays. The integer constant form is valid for static initialization. Applied in `inc/common/utils.h` for `q3_color_table` and `q3_rainbow_colors`.

## MSVC + meson.native.ini: LNK4044 / LNK1107

**Symptom:**
```
LINK : warning LNK4044: unrecognized option '/fuse-ld=lld'; ignored
libq2proto.a : fatal error LNK1107: invalid or corrupt file: cannot read at 0x...
```

**Cause:** `meson.native.ini` is for the Clang+LLVM toolchain on Windows (llvm-ar, -fuse-ld=lld). When building with MSVC (cl.exe + link.exe), that native file is wrong: MSVC's link.exe doesn't understand `-fuse-ld=lld`, and it cannot read Unix-style `.a` archives (it expects COFF `.lib` files). The build mixes Clang-style static libs with MSVC's linker.

**Workaround:** Do *not* use `meson.native.ini` when building with MSVC. Do a clean setup without the native file:
```bash
meson setup builddir --wipe
meson setup builddir -Davcodec=disabled
```
(Add other options as needed.) Meson will then use the default MSVC toolchain (lib.exe for static libs, link.exe for linking).

## MSVC LNK2038: RuntimeLibrary mismatch (MT vs MD)

**Symptom:**
```
error LNK2038: mismatch detected for 'RuntimeLibrary': value 'MT_StaticRelease' doesn't match value 'MD_DynamicRelease'
```

**Cause:** Static libraries (fmt, openal-soft) and executables were built with different CRT settings: some used `/MT` (static CRT), others `/MD` (dynamic CRT). All linked objects must use the same CRT.

**Fix:** The project now uses `b_vscrt=from_buildtype` consistently (dynamic CRT). After this change, do a clean rebuild so subprojects are recompiled:
```bash
rmdir /s /q builddir
meson setup builddir -Davcodec=disabled -Dwrap_mode=forcefallback
meson compile -C builddir
```
