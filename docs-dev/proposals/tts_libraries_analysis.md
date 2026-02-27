# Text-to-Speech (TTS) Library Analysis for WORR

## Introduction

The WORR engine includes provisions for Text-to-Speech functionality, specifically evidenced by the `PRINT_TTS` enum in `game.hpp` and its usage in client commands. This feature is intended to provide accessibility and narration, particularly for in-game chat. This document analyzes suitable open-source TTS libraries that can be integrated into the engine, considering the project's constraints (C/C++, Meson, GPL-compatible, cross-platform).

## Integration Context

*   **Build System:** Meson
*   **Languages:** C11 / C++17
*   **Audio Backend:** OpenAL / SDL3 (via `sndapi_t`)
*   **Integration Point:** The sound engine exposes `bool (*raw_samples)(int samples, int rate, int width, int channels, const void *data, float volume);` which is the ideal injection point for TTS-generated PCM data.
*   **License Requirement:** Must be compatible with `GPL-2.0-or-later`.

## Candidates

### 1. CMU Flite (Festival Lite)

*   **Description:** A small, fast run-time speech synthesis engine. It is the C version of the Festival Speech Synthesis System.
*   **Language:** C
*   **License:** BSD-like (permissive, GPL compatible).
*   **Pros:**
    *   **Extremely Lightweight:** Binary size is very small, minimal memory footprint.
    *   **Zero External Dependencies:** Self-contained, easy to embed directly into the source tree or link statically.
    *   **Performance:** Designed for embedded systems; negligible CPU usage on modern hardware.
    *   **Integration:** Simple C API that outputs waveforms.
*   **Cons:**
    *   **Voice Quality:** "Robotic" formant synthesis. Less natural than neural models.
    *   **Language Support:** Primarily English; other languages exist but are less common.
*   **Verdict:** **Strong Contender.** Its "retro" robotic voice fits the Quake 2 aesthetic perfectly. Its ease of embedding makes it the lowest-friction choice.

### 2. eSpeak NG

*   **Description:** A compact open-source software speech synthesizer for English and other languages.
*   **Language:** C
*   **License:** GPLv3+ (Compatible with WORR's `GPL-2.0-or-later`).
*   **Pros:**
    *   **Language Support:** Massive support for over 100 languages and accents.
    *   **Lightweight:** Small footprint (though larger than Flite).
    *   **Standard:** Used by many accessibility tools (NVDA, Orca).
*   **Cons:**
    *   **Voice Quality:** Also robotic (formant synthesis), arguably slightly clearer than Flite but distinctively artificial.
    *   **Build Complexity:** Slightly more complex build process than Flite due to data file generation.
*   **Verdict:** **Strong Contender.** Best choice if multi-language support is a priority.

### 3. Piper

*   **Description:** A fast, local neural text-to-speech system.
*   **Language:** C++ (Inference)
*   **License:** MIT.
*   **Pros:**
    *   **Voice Quality:** Near-human, high-quality neural synthesis. significantly better than Flite or eSpeak.
    *   **Performance:** Fast enough for real-time on modern CPUs (uses ONNX Runtime).
*   **Cons:**
    *   **Dependencies:** Requires ONNX Runtime, which is a large dependency.
    *   **Asset Size:** Voice models are larger (tens of MBs) compared to KB-sized data for Flite/eSpeak.
    *   **Complexity:** Higher integration complexity.
*   **Verdict:** **Alternative Choice.** Best if high-quality, modern voice is required, but brings significant weight to the project.

### 4. OS-Native APIs

*   **Description:** Using platform-specific APIs (SAPI/WinRT on Windows, Speech Dispatcher on Linux, NSSpeechSynthesizer on macOS).
*   **Pros:**
    *   **Zero Distribution Size:** No libraries or data files to ship.
    *   **Quality:** Uses the user's installed voices (often high quality).
*   **Cons:**
    *   **Inconsistency:** Voices sound completely different across platforms.
    *   **Complexity:** Requires implementing and maintaining three separate abstraction layers.
    *   **Latency:** Can have higher latency or harder synchronization with the game audio mixer (OpenAL).
*   **Verdict:** **Fallback.** Good for "system integration" but less ideal for a consistent game experience.

## Recommendation

**Primary Recommendation: CMU Flite**

For WORR, **Flite** is recommended as the starting point.
1.  **Aesthetic Fit:** The robotic voice aligns with the sci-fi/industrial theme of Quake 2.
2.  **Technical Fit:** It is pure C, trivial to build with Meson, and can directly buffer PCM data into the engine's `raw_samples` function.
3.  **No "Dependency Hell":** It avoids the complexities of ONNX Runtime or platform-specific COM/Objective-C bridges.

**Secondary Recommendation: eSpeak NG**
If internationalization (i18n) beyond English is required, **eSpeak NG** becomes the primary choice due to its superior language coverage.

## Proposed Integration Plan

1.  **Add Dependency:** Add Flite (or eSpeak NG) as a Meson subproject.
2.  **Backend Implementation:** Create a `src/client/sound/tts.c` module.
3.  **PCM Feed:** Implement a callback or ring buffer that feeds the synthesizer output into `snd_dma.raw_samples` or `snd_openal.raw_samples`.
4.  **Cvar Control:** Add cvars `tts_enable`, `tts_volume`, and `tts_speed`.
5.  **Hook:** Connect `CL_ParseTts` or the `PRINT_TTS` handler to the TTS module.
