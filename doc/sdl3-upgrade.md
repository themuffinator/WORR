SDL 3.4.0 Upgrade
-----------------

Summary
=======
- The SDL backend now targets SDL3 3.4.0 and uses SDL3 headers/APIs throughout.
- Meson option name changed from sdl2 to sdl3; dependency is enforced as >= 3.4.0.

Build System Changes
====================
- Meson now uses dependency('sdl3', version: '>= 3.4.0').
- The project option is renamed to -Dsdl3 (feature) in meson_options.txt.
- Feature summaries and docs now refer to SDL3 and libsdl3-dev.

SDL Video Backend Changes
=========================
- Updated includes to SDL3 headers and SDL3 OpenGL header.
- SDL_CreateWindow now uses the SDL3 signature and SDL_WINDOW_HIGH_PIXEL_DENSITY.
- Fullscreen handling now uses SDL_SetWindowFullscreenMode + SDL_SetWindowFullscreen.
- Window pixel sizing uses SDL_GetWindowSizeInPixels for accurate DPI scaling.
- Event handling now uses SDL_EVENT_* constants and SDL3 event structs.
- Relative mouse mode uses SDL_SetWindowRelativeMouseMode and related APIs.
- Cursor visibility uses SDL_ShowCursor/SDL_HideCursor.
- SDL3 removed window gamma ramp APIs; the SDL driver disables vid_hwgamma and logs.

SDL Audio Backend Changes
=========================
- SDL2 callback audio is replaced by SDL3 audio streams.
- SDL_OpenAudioDeviceStream is used with a stream callback.
- The callback feeds the DMA ring buffer into the stream with SDL_PutAudioStreamData.
- BeginPainting/Submit lock the stream to serialize access with the callback.
- Audio device activation uses SDL_ResumeAudioDevice/SDL_PauseAudioDevice.

Behavior Notes
==============
- vid_hwgamma is not supported by the SDL backend under SDL3.
- The SDL DMA driver mixes 16-bit stereo at the configured s_khz rate; SDL handles
  device conversion if the hardware format differs.
- Window close requests are handled via SDL_EVENT_WINDOW_CLOSE_REQUESTED.

Follow-Up
=========
- Update any build scripts that passed -Dsdl2 to use -Dsdl3 instead.
