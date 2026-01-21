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

### 3.1. Main Menu (Widescreen Layout > 16:10)
*   **Container:** Fullscreen background image/scene (3D map or artwork).
*   **Sidebar (Left 25%):**
    *   **Header:** Game Logo (Top 15%).
    *   **Navigation List:** Vertical list of categories (Singleplayer, Multiplayer, Options, Quit).
    *   **Footer:** Version info and copyright (Bottom 5%).
*   **Content Panel (Right 75%):**
    *   **Dynamic Container:** Loads the sub-menu for the selected category.
    *   **Padding:** Significant whitespace to avoid clutter.
    *   **Background:** Semi-transparent dark overlay (e.g., `rgba(0,0,0,0.8)`) to ensure text readability.

### 3.2. Main Menu (Narrow Layout 4:3)
*   **Container:** Fullscreen background.
*   **Central Stack (Width 60-80%):**
    *   **Header:** Game Logo (Top 20%).
    *   **Navigation List:** Centered vertical list.
*   **Drill-down Behavior:** Selecting a category *replaces* the list with the sub-menu items. A "Back" button appears prominently at the top or bottom.

### 3.3. In-Game Menu (Overlay)
*   **Style:** Modal Overlay.
*   **Background:** Screen blur or heavy darken (`gl_polyblend` style).
*   **Position:** Left-aligned or Centered card (approx 400-600px width).
*   **Structure:**
    *   **Header:** Server Name / Map Name.
    *   **Quick Actions:** Resume, Server Info, Vote, Settings (opens full settings modal), Disconnect.
    *   **Footer:** Current time / Connection status.

## 4. Navigation & Widget Usage

To facilitate elegant traversal, generic widgets are replaced with specialized ones:

*   **Tabs / Categories (`MTYPE_TAB`):**
    *   *Usage:* Top of the "Settings" menu to switch between Video, Audio, Input, etc.
    *   *Visual:* Horizontal strip or icon bar.
*   **Smart Lists (`MTYPE_LIST`):**
    *   *Usage:* Server browser, Demo list.
    *   *Features:* Sortable columns, scrollbars, distinct headers.
*   **Visual Sliders (`MTYPE_SLIDER`):**
    *   *Usage:* Volume, Gamma, Sensitivity.
    *   *Visual:* Filled bar indicating percentage, numeric value on the right.
*   **Palette Picker (`MTYPE_PALETTE`):**
    *   *Usage:* Crosshair color, Railgun color.
    *   *Visual:* Grid or strip of color swatches. Highlighting one updates the cvar immediately.
*   **Toggle Switch (`MTYPE_TOGGLE`):**
    *   *Usage:* Binary settings (VSync, Show FPS).
    *   *Visual:* "Switch" graphic (Left=Off, Right=On) rather than Yes/No text.
*   **Key Binder (`MTYPE_KEYBIND`):**
    *   *Visual:* Displays current key. Clicking enters "listening" mode (pulsing/color change). Supports clearing (DEL/Backspace).

## 5. Detailed Menu Tree & Cvars

### 5.1. Main Menu (`begin main`)
*   **Singleplayer:**
    *   New Game (Difficulty Select -> Episode Select)
    *   Load Game (Visual save slots with thumbnails if possible)
*   **Multiplayer:**
    *   Server Browser (`pushmenu servers`)
    *   Host Game (`pushmenu startserver`)
    *   Player Setup (Model, Skin, Name)
*   **Settings:** (Opens the comprehensive settings panel)
*   **Extras:**
    *   Demos (`pushmenu demos`)
    *   Credits
*   **Quit**

### 5.2. In-Game Menu (`begin game`)
*   **Resume**
*   **Match Control** (Context-sensitive)
    *   **Welcome / MOTD:** Server Message of the Day.
    *   **Match Setup:** (Admin) Change Map, Timelimit, Fraglimit.
    *   **Match Status:** Scoreboard summary, time remaining.
*   **Settings** (Reuses Settings Menu)
*   **Vote** (Call Vote / Yes / No)
*   **Disconnect**
*   **Quit to Desktop**

#### **Deathmatch Specifics (Match Menu)**
*   **Welcome:**
    *   *Widgets:* Scrollable text area for MOTD.
    *   *Actions:* "Join Game", "Spectate", "Ready".
*   **Match Setup (Admin):**
    *   **Rules:** `timelimit` (slider 0-60), `fraglimit` (slider 0-100), `maxclients`.
    *   **Flags:** `dmflags` bitmask managed via checkboxes:
        *   Weapons Stay (2)
        *   Falling Damage (~3)
        *   Instant Powerups (4)
        *   Allow Powerups (~1)
        *   Allow Health (~0)
        *   Allow Armor (~11)
        *   Spawn Farthest (9)
        *   Same Map (5)
        *   Force Respawn (10)
        *   Infinite Ammo (13)
        *   Fixed FOV (15)
        *   Quad Drop (14)
*   **Map Vote:**
    *   *List:* Visual list of available maps (`maps/*.bsp`).
    *   *Action:* "Call Vote" or "Force" (if admin).

### 5.3. Settings Hierarchy

The core settings menu is tabbed for quick access.

#### **A. Video (`begin video`)**
*   **Display:**
    *   Mode: `vid_fullscreen` (Windowed/Fullscreen).
    *   Resolution: `vid_mode` (custom widget listing modes).
    *   Brightness: `vid_gamma` (slider 1.3 to 0.3).
    *   HW Gamma: `vid_hwgamma` (toggle).
    *   Contrast: `gl_contrast` (slider).
    *   VSync: `gl_swapinterval` (toggle).
*   **Quality:**
    *   Texture Quality: `gl_picmip` (3 to 0, High/Med/Low).
    *   Filtering: `gl_texturemode` (Nearest/Linear/Trilinear).
    *   Anisotropy: `gl_anisotropy` (Off/2x/4x/8x/16x).
    *   Multisample: `gl_multisamples` (Off/2x/4x/8x).
    *   Saturation: `gl_saturation` (0 to 1).
    *   Intensity: `intensity` (1x, 2x, 3x).
    *   Lightmap Sat: `gl_coloredlightmaps` (0 to 1).
    *   Lightmap Bright: `gl_brightness` (0 to 0.3).
*   **Advanced / Effects:**
    *   *Condition:* Visible only if `r_renderer` is OpenGL/Vulkan.
    *   *Condition:* Some items disabled if `gl_shaders` is 0.
    *   Shaders: `gl_shaders` (Toggle).
    *   Bloom: `gl_bloom_sigma` (Slider).
    *   Dynamic Lights: `gl_dynamic` (All/Switchable/None).
    *   Shadows: `gl_shadows` (Off/On/Fade).
    *   Cel-Shading: `gl_celshading` (Off/1x/2x).
    *   Particles: `gl_partscale`.
    *   Entity Glow: `cl_noglow` (toggle).
    *   Screen Blend: `gl_polyblend` (toggle).
    *   Water Warp: `gl_waterwarp` (toggle).
    *   Explosions: `cl_disable_explosions` (Rocket/Grenade toggles).

#### **B. Audio (`begin sound`)**
*   **Volume:**
    *   Master: `s_volume` (0 to 1).
    *   Music: `ogg_volume` (0 to 1).
*   **Options:**
    *   Sound Backend: `s_enable` (Software/OpenAL).
    *   Mix Ahead (Latency): `s_mixahead` (0.05 to 0.2).
    *   Ambient Sounds: `s_ambient` (No/Yes/Own).
    *   Underwater FX: `s_underwater` (toggle).
    *   Chat Beep: `cl_chat_sound`.
    *   Music Enable: `ogg_enable`.
    *   Shuffle: `ogg_shuffle`.

#### **C. Input (`begin input`)**
*   **Mouse:**
    *   Sensitivity: `sensitivity`.
    *   Filter: `m_filter`.
    *   Auto-Sens: `m_autosens`.
    *   Accel: `m_accel`.
    *   Free Look: `freelook`.
    *   Always Run: `cl_run`.
*   **Weapon Wheel:**
    *   Enable: `cl_use_wheel` (hypothetical).
    *   Hold Time: `ww_hover_time`.
    *   Deadzone: `ww_mouse_deadzone_speed`.
    *   Scale: `ww_hover_scale`.
    *   Screen Frac X/Y: `ww_screen_frac_x`, `ww_screen_frac_y`.

#### **D. Controls (`begin keys`)**
*   **Categories:**
    *   *Movement:* Forward, Back, Left, Right, Jump, Crouch, Walk, Run.
    *   *Combat:* Attack, Next/Prev Weapon, Weapon 1-10 (Blaster...BFG).
    *   *Interaction:* Inventory, Use Item, Drop Item, Prev/Next Item.
    *   *Communication:* Chat, Help.
    *   *UI:* Scoreboard, Pause.
    *   *Legacy:* Look Up/Down, Center View, Mouse/Keyboard Look.

#### **E. Gameplay / HUD (`begin game_options`)**
*   **Crosshair:**
    *   Type: `crosshair` (Image spinner `ch*`).
    *   Size: `cl_crosshairSize` (4 to 64).
    *   Color: `cl_crosshairColor` (Palette 1-26).
    *   Brightness: `cl_crosshairBrightness` (0 to 1).
    *   Hit Style: `cl_crosshairHitStyle` (Off/Color/Pulse/etc).
    *   Hit Color: `cl_crosshairHitColor`.
    *   Hit Time: `cl_crosshairHitTime`.
    *   Pickup Pulse: `cl_crosshairPulse`.
    *   Health Color: `cl_crosshairHealth`.
*   **HUD:**
    *   Screen Size: `viewsize`.
    *   Ping Graph: `scr_lag_draw`.
    *   Demo Bar: `scr_demobar`.
    *   HUD Opacity: `scr_alpha`.
    *   Console Opacity: `con_alpha`.
    *   Scales: `scr_scale`, `con_scale`, `ui_scale`.
*   **Railgun:**
    *   Type: `cl_railtrail_type`.
    *   Duration: `cl_railtrail_time`.
    *   Core Width: `cl_railcore_width`.
    *   Spiral Radius: `cl_railspiral_radius`.
    *   Core Color: `cl_railcore_color`.
    *   Spiral Color: `cl_railspiral_color`.
*   **View:**
    *   FOV: `cl_adjustfov` / `fov`.
    *   Gun Model: `cl_gun` (Handedness/Off).

#### **F. Downloads (`begin downloads`)**
*   **Allow:** `allow_download`.
*   **Types:** `allow_download_maps`, `_players`, `_models`, `_sounds`, `_textures`, `_pics`.
*   **Method:** `cl_http_downloads`.

## 6. Implementation Plan

1.  **Refactor `menu.cpp` Layout Engine:**
    *   Implement `Menu_Size` logic that respects "Containers" and "Columns".
    *   Add `ui_layout` cvar support (0=Auto, 1=Wide, 2=Narrow).
2.  **Enhance Widget Classes:**
    *   `MTYPE_SLIDER`: Add support for rendering a filled bar.
    *   `MTYPE_PALETTE`: Create new draw function for color grids.
3.  **Implement Conditional Logic:**
    *   Add `condition <cvar> <value>` keyword to menu parser.
    *   Code `Menu_AddItem` to respect this flag, setting `QMF_GRAYED` or `QMF_HIDDEN` if condition fails.
4.  **Rewrite `worr.menu`:**
    *   Translate the "Detailed Menu Tree" above into the actual script format.
    *   Utilize new `begin_tab` or similar grouping constructs if implemented, or simulate with `pushmenu`.

## 7. Deep Dive: Conditional & Complex Items

### 7.1. Conditional & Renderer-Specific Logic
*   **Problem:** Vulkan options shouldn't show in GL mode. High-end shader options shouldn't show if shaders are disabled.
*   **Solution:**
    *   **Menu Script:** `item ... condition gl_shaders 1`
    *   **Code:** `Menu_Draw` checks the condition. If false, item is drawn with `uis.color.disabled` and input is ignored.
    *   **Feedback:** Tooltip (if supported) or status text says "Requires Shaders Enabled".

### 7.2. Expanded Item Examples
*   **Weapon Wheel Configurator:**
    *   *Visual:* A circular preview drawn using `MTYPE_STATIC` or a custom draw routine.
    *   *Interaction:* Adjusting `ww_popout_amount` updates the radius of the preview circle in real-time.
*   **Resolution Picker:**
    *   *Logic:* Instead of a raw list, parse `vid_modelist`.
    *   *Grouping:* Detect "Desktop" resolution. Group others by aspect ratio.
    *   *Widget:* A combobox-style list (`MTYPE_SPINCONTROL` with a pop-up list variant).

## 8. Visual Style Guide

*   **Colors:**
    *   *Background:* Dark Grey / Transparent Black (#000000CC).
    *   *Text Normal:* White (#FFFFFF).
    *   *Text Active/Hover:* Bright Cyan (#00FFFF) or Theme Color.
    *   *Text Disabled:* Grey (#777777).
*   **Typography:**
    *   Use `scr_font` (conchars) for retro feel, or support TTF rendering for headers if engine allows.
*   **Feedback:**
    *   *Hover:* Slight scale up or brightness increase.
    *   *Click:* Sound effect (`misc/menu1.wav`).
    *   *Transition:* Simple fade-in/out (200ms).

## 9. Technical Considerations

*   **Legacy Preservation:** All CVARs from the original `worr.menu` (e.g., `gl_saturation`, `gl_waterwarp`) have been mapped to the new structure to ensure no functionality is lost.
*   **Scripting Limits:** Watch for the 1024 char limit on expanded lines. Use the `$$` hack for spin controls where necessary.
*   **Menu Depth:** Current limit is 8 (`MAX_MENU_DEPTH`). The new structure should be flat enough to avoid hitting this, but using "Tabs" (switching menus instead of pushing) helps.
*   **Responsiveness:** The engine's `Menu_Size` function may need updates to better handle dynamic width allocation for the split-pane design.
