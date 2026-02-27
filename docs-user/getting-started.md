# Getting Started

This guide is the quick path to running WORR with your own Quake II data.

## What You Need

- A built WORR workspace (or a release package).
- Quake II data files (`baseq2/pak*.pak`).
- A fresh staged runtime in `.install/` if you are building locally.

## Local Build Flow (Fast Path)

1. Configure:

   ```bash
   meson setup builddir --wrap-mode=forcefallback --buildtype=release -Dtests=false
   ```

2. Compile:

   ```bash
   meson compile -C builddir
   ```

3. Refresh the local runtime package:

   ```bash
   python3 tools/refresh_install.py --build-dir builddir --install-dir .install --base-game baseq2
   ```

4. Launch from `.install/`:
   - Windows: `.install/worr.exe`
   - Linux/macOS: `.install/worr`

## First Run Checklist

- Put your Quake II data in `.install/baseq2/` (or point `basedir` at a valid data tree).
- Keep `.install/baseq2/worr-assets.pkz` in place.
- If the game boots to console only, check renderer selection and your GPU driver.

## Useful Start Arguments

```text
+set basedir <path>
+set r_renderer opengl
+set r_renderer vulkan
+set r_renderer rtx
```

## If Something Feels Off

- No sound: verify OpenAL device output and in-game volume cvars.
- Missing UI/textures: confirm `worr-assets.pkz` exists under `baseq2/`.
- Startup crash after pull: rebuild, then rerun `tools/refresh_install.py`.
