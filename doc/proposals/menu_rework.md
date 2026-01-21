# Comprehensive Menu Rework Proposal

## 1. Objective
To completely overhaul the existing menu system (`worr.menu`) to achieve a modern, minimalistic, and highly functional user experience. The rework aims to distinguish clearly between the Main Menu and the In-Game Menu, maximize screen real estate (especially for widescreen), improve navigation, and seamlessly integrate a wealth of new features.

## 2. Design Philosophy

*   **Brevity & Context:** The **Main Menu** should be a visual centerpiece, distinct from the **In-Game Menu**, which should be functional, unobtrusive, and focused on quick adjustments.
*   **Modernity:** Move away from pure text lists. Use icons, distinct visual groupings, and consistent typography.
*   **Widescreen First:** Leverage horizontal space. Categories on the left, content on the right.
*   **Adaptability:** The layout must gracefully reflow for 4:3 aspect ratios without losing functionality.
*   **Intelligent Traversal:** Use tabs and pages to reduce deep nesting while keeping related options together.

## 3. Layout Strategy

### 3.1. Main Menu (Widescreen)
*   **Left Column (Navigation):** A vertical list of high-level categories (Singleplayer, Multiplayer, Settings, etc.).
*   **Right Panel (Content):** A large dynamic area that updates based on the selected category.
*   **Background:** Immersive 3D scene or high-quality artwork.

### 3.2. Main Menu (4:3 / Narrow)
*   **Single Column:** The Navigation list takes center stage.
*   **Drill-down:** Selecting a category replaces the view with the content panel (breadcrumb navigation required to go back).

### 3.3. In-Game Menu
*   **Overlay Style:** A semi-transparent overlay (darkened game background).
*   **Center/Left Aligned:** Quick access options.
*   **Content:** "Resume Game", "Options" (opens a modal version of Settings), "Server Info", "Disconnect".

## 4. Navigation & Widget Usage

To facilitate elegant traversal, we will employ specific widget types for specific data:

*   **Tabs / Categories (`MTYPE_LIST` / `MTYPE_ACTION`):** For top-level navigation (e.g., Video, Audio, Input).
*   **Sliders (`MTYPE_SLIDER`):** exclusively for continuous values (Volume, Gamma, Sensitivity).
*   **Spin Controls (`MTYPE_SPINCONTROL`):** For discrete choices (Resolution, Texture Quality).
*   **Toggles (`MTYPE_TOGGLE`):** For On/Off states (VSync, Bloom).
*   **Binding Lists (`MTYPE_KEYBIND`):** Organized by category (Movement, Combat, Weapon Wheel).
*   **Action Buttons (`MTYPE_ACTION`):** For immediate effects (Apply, Reset, Defaults).

## 5. Detailed Menu Tree

### 5.1. Main Menu (`begin main`)
*   **Campaign** (New Game / Load Game / Episodes)
*   **Multiplayer** (Server Browser / Create Server / Player Setup)
*   **Settings** (Opens the comprehensive settings panel)
*   **Extras** (Demos / Credits / Mods)
*   **Quit**

### 5.2. In-Game Menu (`begin game`)
*   **Resume**
*   **Match Control** (Context-sensitive)
    *   **Welcome / MOTD:** View server message and rules.
    *   **Match Setup:** (Admin/Host only) Modify timelimit, fraglimit, map rotation, and game flags on the fly.
    *   **Match Status:** Current scores, players, and match time.
*   **Settings** (Reuses the Settings sub-menus)
*   **Vote** (Call vote / Vote Yes/No)
*   **Disconnect**
*   **Quit to Desktop**

#### **Deathmatch Specifics**
For Deathmatch modes, the "Match Control" section becomes the hub for the "Deathmatch Match Menu System".
*   **Welcome Menu:** displayed automatically on join, accessible later via "Match Control". Displays Server Name, MOTD, and specific rules/flags in a clean, widget-driven layout (not just text).
*   **Match Setup:** A dedicated submenu for configuring the *next* or *current* match.
    *   **Rules:** Time Limit, Frag Limit, Max Players.
    *   **Flags:** Weapons Stay, Powerups, Friendly Fire (checkboxes/toggles).
    *   **Map:** Vote or force next map from a visual list.

### 5.3. Settings Hierarchy

The settings menu is the core area requiring the most restructuring to accommodate new features.

#### **A. Video**
*   **Display:**
    *   Window Mode (Fullscreen/Windowed/Borderline)
    *   Resolution
    *   VSync (`gl_swapinterval`)
    *   FOV (`cl_adjustfov`)
    *   Monitor Selection (if applicable)
*   **Quality:**
    *   Texture Quality (`gl_picmip`)
    *   Filtering (`gl_texturemode`, `gl_anisotropy`)
    *   High Quality Upscaling (`hqx_*`)
*   **Advanced Effects:** (New Features Integration)
    *   Bloom (`gl_bloom_sigma`)
    *   Dynamic Lighting (`gl_dynamic`)
    *   Shadows (`gl_shadows`) & Soft Shadows
    *   Cel-Shading (`gl_celshading`)
    *   Per-Pixel Lighting (`gl_per_pixel_lighting`)
    *   Particles (`gl_partscale`)
*   **Tools:**
    *   Screenshot Format (`gl_screenshot_format`)
    *   Screenshot Quality (`gl_screenshot_quality`)

#### **B. Audio**
*   **Volume:** Master, Sound Effects, Music (`s_volume`, `ogg_volume`).
*   **System:** Sound Engine (Software/OpenAL), Quality, Speaker Config.
*   **Ambience:** Ambient Sounds, Underwater Effects.

#### **C. Input**
*   **Mouse:**
    *   Sensitivity (`sensitivity`), Invert, Filter (`m_filter`).
    *   Auto-Sensitivity (`m_autosens`).
*   **Weapon Wheel:** (New Feature Integration)
    *   Enable/Disable
    *   Deadzone Speed (`ww_mouse_deadzone_speed`)
    *   Sensitivity (`ww_mouse_sensitivity`)
    *   Hover Time/Scale (`ww_hover_time`, `ww_hover_scale`)
    *   Screen Fraction (`ww_screen_frac_x/y`)
*   **Key Bindings:**
    *   Grouped: Movement, Combat, Interaction, Weapon Wheel (`+wheel`), UI.
*   **Gamepad:** (If supported, sensitivity and layout)

#### **D. Gameplay / Interface**
*   **HUD:**
    *   Crosshair: Style, Size, Color, Hit Marker (`cl_crosshair*`).
    *   Scale: HUD Scale (`scr_scale`), Console Scale (`con_scale`).
    *   Opacity: HUD/Console Alpha.
*   **Feedback:**
    *   Railgun Trails (`cl_railtrail_type`, core/spiral colors).
    *   Bobbing / Sway.
    *   Explosions (Toggle Rocket/Grenade explosions).
*   **Downloads:** (HTTP download toggles)

#### **E. Player**
*   **Identity:** Name, Model, Skin.
*   **Team:** Handedness.

## 6. Implementation Plan

1.  **Refactor `worr.menu`:**
    *   Split `video` into `video_display`, `video_quality`, `video_effects`.
    *   Create a master `settings` menu that acts as the navigation hub (Tabs).
2.  **Widget Enhancements:**
    *   Ensure `MTYPE_LIST` can support "categories" that trigger `pushmenu` commands effectively for the split-view layout.
    *   Implement logic (C++ side in `menu.cpp`) to detect aspect ratio and adjust `RCOLUMN_OFFSET` / `LCOLUMN_OFFSET` or use new `ui_layout` cvars to switch between split-view and single-view.
3.  **New Cvars:**
    *   Expose all identified `ww_*` (Weapon Wheel) and `gl_*` (Render) cvars in their respective new sections.
4.  **Visual Polish:**
    *   Update `banner`, `plaque`, and `background` assets to match the modern aesthetic.
    *   Use `MTYPE_STATIC` for descriptive headers within lists.

## 7. Deep Dive: Conditional & Complex Items

To achieve a modern experience, generic widgets are insufficient for certain complex settings.

### 7.1. Conditional & Renderer-Specific Logic
The menu must adapt availability based on the engine state.
*   **Renderer Context:** Checks against `r_renderer` (or `vid_ref`) must be performed.
    *   *OpenGL:* Show `gl_swapinterval`, `gl_shaders`.
    *   *Vulkan:* Show `vk_present_mode` (hypothetical), `vk_vsync`.
*   **Feature Gating:**
    *   If `gl_shaders` is **OFF**: Options like `gl_bloom_sigma`, `gl_celshading`, and `gl_shadows` must be **disabled (grayed out)** or **hidden**.
    *   Visual feedback (e.g., a lock icon or tooltip) should explain *why* an option is unavailable.

### 7.2. Expanded Item Examples
We will move beyond simple number spinners for key customization options.

*   **Crosshair Color (`cl_crosshairColor`)**
    *   *Current:* A number from 1-26.
    *   *Proposed:* A **Palette Slider** widget.
    *   *Visuals:* Instead of displaying "12", display the actual color swatch. The slider track shows the full 26-color spectrum.
*   **Resolution (`vid_modelist`)**
    *   *Current:* A long text string list.
    *   *Proposed:* A **Resolution Picker**.
    *   *Logic:* Group by Aspect Ratio (4:3, 16:9, 16:10). Show "Native" marker for desktop resolution.
*   **Field of View (`cl_adjustfov` / `fov`)**
    *   *Proposed:* A visual **FOV Slider** with degrees.
    *   *Preview:* Real-time background update (if in-game) to show the effect.

## 8. Technical Considerations

*   **Scripting Limits:** Watch for the 1024 char limit on expanded lines. Use the `$$` hack for spin controls where necessary.
*   **Menu Depth:** Current limit is 8 (`MAX_MENU_DEPTH`). The new structure should be flat enough to avoid hitting this, but using "Tabs" (switching menus instead of pushing) helps.
*   **Responsiveness:** The engine's `Menu_Size` function may need updates to better handle dynamic width allocation for the split-pane design.
