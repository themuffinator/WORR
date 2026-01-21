# Comprehensive Menu Rework Proposal

## 1. Objective
To completely overhaul the existing menu system (`worr.menu`) to achieve a modern, minimalistic, and highly functional user experience. The rework aims to distinguish clearly between the Main Menu and the In-Game Menu, maximize screen real estate (especially for widescreen), improve navigation, and seamlessly integrate a wealth of new features.

## 2. Design Philosophy

*   **Brevity & Context:** The **Main Menu** should be a visual centerpiece, distinct from the **In-Game Menu**, which should be functional, unobtrusive, and focused on quick adjustments.
*   **Modernity vs Retro:** The default aesthetic must be **Modern**—sleek, responsive, and using high-quality assets. A **Retro Theme** option should be available to mimic the classic look for purists.
*   **Widescreen First:** Leverage horizontal space. Categories on the left, content on the right.
*   **Adaptability:** The layout must gracefully reflow for 4:3 aspect ratios without losing functionality.
*   **Intelligent Traversal:** Use tabs and pages to reduce deep nesting while keeping related options together.

## 3. Visual Style & Theme

*   **Theme:** "Dark Metal & Neon Grime".
    *   **Palette:**
        *   *Primary Backgrounds:* Deep metallic greys (`#1a1a1a` to `#2e2e2e`) with grunge textures.
        *   *Accents:* Grungy greens (`#4b6e4b`) for secondary elements.
        *   *Highlights:* Bright Lime Green (`#00ff00` or `#32cd32`) for active selections, sliders, and focus states.
*   **Typography:**
    *   **Menu Font:** Controlled by the `ui_font` cvar. Default should be a crisp, readable sans-serif or stylized industrial font, scalable for high resolutions.
*   **Feedback:**
    *   *Hover:* Elements glow or shift slightly (Lime highlight).
    *   *Click:* Mechanical sound effects.

## 4. Layout Strategy

### 4.1. Main Menu (Widescreen Layout > 16:10)
*   **Container:** Fullscreen background image/scene (3D map or artwork).
*   **Sidebar (Left 25%):**
    *   **Header:** Game Logo (Top 15%).
    *   **Navigation List:** Vertical list of categories (Singleplayer, Multiplayer, Options, Quit).
    *   **Footer:** Version info and copyright (Bottom 5%).
*   **Content Panel (Right 75%):**
    *   **Dynamic Container:** Loads the sub-menu for the selected category.
    *   **Padding:** Significant whitespace to avoid clutter.
    *   **Background:** Semi-transparent dark overlay (e.g., `rgba(0,0,0,0.8)`) to ensure text readability.

### 4.2. Main Menu (Narrow Layout 4:3)
*   **Container:** Fullscreen background.
*   **Central Stack (Width 60-80%):**
    *   **Header:** Game Logo (Top 20%).
    *   **Navigation List:** Centered vertical list.
*   **Drill-down Behavior:** Selecting a category *replaces* the list with the sub-menu items. A "Back" button appears prominently at the top or bottom.

### 4.3. In-Game Menu (Overlay)
*   **Style:** Modal Overlay.
*   **Background:** Screen blur or heavy darken (`gl_polyblend` style).
*   **Position:** Left-aligned or Centered card (approx 400-600px width).
*   **Structure:**
    *   **Header:** Server Name / Map Name.
    *   **Quick Actions:** Resume, Server Info, Vote, Settings (opens full settings modal), Disconnect.
    *   **Footer:** Current time / Connection status.

## 5. Navigation & Widget Usage

To facilitate elegant traversal, generic widgets are replaced with specialized ones. **Drop-down Lists** are preferred over archaic spin controls for selection.

*   **Tabs / Categories (`MTYPE_TAB`):**
    *   *Usage:* Top of the "Settings" menu to switch between Video, Audio, Input, etc.
*   **Drop-Down Lists (`MTYPE_COMBO`):**
    *   *Usage:* Resolution, Texture Quality, Sound Backend.
    *   *Behavior:* Clicking expands a vertical list of options overlapping other content. Replaces "cycling" spin controls.
*   **Smart Lists (`MTYPE_LIST`):**
    *   *Usage:* Server browser, Demo list.
*   **Visual Sliders (`MTYPE_SLIDER`):**
    *   *Usage:* Volume, Gamma, Sensitivity.
    *   *Visual:* Filled bar (Lime Green) indicating percentage, numeric value on the right.
*   **Palette Picker (`MTYPE_PALETTE`):**
    *   *Usage:* Crosshair color, Railgun color.
*   **Toggle Switch (`MTYPE_TOGGLE`):**
    *   *Usage:* Binary settings (VSync, Show FPS).
    *   *Visual:* "Switch" graphic (Left=Off, Right=On) rather than Yes/No text.
*   **Key Binder (`MTYPE_KEYBIND`):**
    *   *Visual:* Displays current key.

## 6. Detailed Menu Tree & Cvars

### 6.1. Main Menu (`begin main`)
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

### 6.2. In-Game Menu (`begin game`)
*   **Resume**
*   **Match Control** (Context-sensitive)
    *   **Join Game:** (If spectator)
    *   **Spectate:** (If player)
    *   **Match Setup:** (Admin) Change Map, Timelimit, Fraglimit.
    *   **Match Status:** Scoreboard summary, time remaining.
*   **Settings** (Reuses Settings Menu)
*   **Vote** (Call Vote / Yes / No)
*   **Disconnect**
*   **Quit to Desktop**

#### **Deathmatch Specifics (Match Menu)**
*   **Join / Welcome Menu:**
    *   *Header:* Server MOTD (Scrollable text).
    *   *Visual:* Map Preview Image.
    *   *Actions:*
        *   "Join Battle" (`cmd join`)
        *   "Spectate" (`cmd spectate`)
        *   "Chase Cam" (Spectator option)
*   **Match Setup (Admin):**
    *   **Rules:**
        *   Time Limit: `timelimit` (Slider 0-60).
        *   Frag Limit: `fraglimit` (Slider 0-100).
        *   Max Players: `maxclients` (Slider).
    *   **Game Flags (`dmflags` checkboxes):**
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

### 6.3. Settings Hierarchy

The core settings menu is tabbed for quick access.

#### **A. Video (`begin video`)**
*   **Display:**
    *   Mode: `vid_fullscreen` (Drop-down: Windowed/Fullscreen).
    *   Resolution: `vid_mode` (Drop-down).
    *   Brightness: `vid_gamma` (Slider).
    *   HW Gamma: `vid_hwgamma` (Toggle).
    *   Contrast: `gl_contrast` (Slider).
    *   VSync: `gl_swapinterval` (Toggle).
*   **Quality:**
    *   Texture Quality: `gl_picmip` (Drop-down: High/Med/Low).
    *   Filtering: `gl_texturemode` (Drop-down).
    *   Anisotropy: `gl_anisotropy` (Drop-down).
    *   Multisample: `gl_multisamples` (Drop-down).
    *   Saturation: `gl_saturation` (Slider).
    *   Intensity: `intensity` (Drop-down).
    *   Lightmap Sat: `gl_coloredlightmaps` (Slider).
    *   Lightmap Bright: `gl_brightness` (Slider).
*   **Advanced / Effects:**
    *   Shaders: `gl_shaders` (Toggle).
    *   Bloom: `gl_bloom_sigma` (Slider).
    *   Dynamic Lights: `gl_dynamic` (Drop-down).
    *   Shadows: `gl_shadows` (Drop-down).
    *   Cel-Shading: `gl_celshading` (Drop-down).
    *   Particles: `gl_partscale`.
    *   Entity Glow: `cl_noglow` (Toggle).
    *   Screen Blend: `gl_polyblend` (Toggle).
    *   Water Warp: `gl_waterwarp` (Toggle).
    *   Explosions: `cl_disable_explosions` (Toggle bits).

#### **B. Audio (`begin sound`)**
*   **Volume:**
    *   Master: `s_volume` (Slider).
    *   Music: `ogg_volume` (Slider).
*   **Options:**
    *   Sound Backend: `s_enable` (Drop-down: Software/OpenAL).
    *   Mix Ahead (Latency): `s_mixahead` (Slider).
    *   Ambient Sounds: `s_ambient` (Drop-down).
    *   Underwater FX: `s_underwater` (Toggle).
    *   Chat Beep: `cl_chat_sound` (Drop-down).
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
    *   Enable: `cl_use_wheel`.
    *   Hold Time: `ww_hover_time`.
    *   Deadzone: `ww_mouse_deadzone_speed`.
    *   Scale: `ww_hover_scale`.
    *   Screen Frac X/Y: `ww_screen_frac_x`, `ww_screen_frac_y`.

#### **D. Player Visuals (`begin player_visuals`)**
This new menu consolidates all visual customization for entities.
*   **Preview Area:** A large, rotating 3D model viewport (`MTYPE_MODEL_PREVIEW`) showing the effect of settings on: "Enemy", "Teammate", or "Self".
*   **Visibility:**
    *   Brightskins (Enemy): `cl_brightskins_enemy_color` (Palette).
    *   Brightskins (Team): `cl_brightskins_team_color` (Palette).
    *   Brightskins Custom: `cl_brightskins_custom` (Toggle).
    *   Dead Skins: `cl_brightskins_dead` (Toggle).
*   **Outlines & Rimlighting:**
    *   Enemy Outline: `cl_enemy_outline` (Slider 0-1).
    *   Self Outline: `cl_enemy_outline_self` (Slider 0-1).
    *   Enemy Rimlight: `cl_enemy_rimlight` (Slider 0-1).
    *   Self Rimlight: `cl_enemy_rimlight_self` (Slider 0-1).
*   **Models:**
    *   Force Enemy Model: `cl_force_enemy_model` (Drop-down list of available models).
    *   Force Team Model: `cl_force_team_model` (Drop-down).

#### **E. Controls (`begin keys`)**
*   **Categories:** Movement, Combat, Interaction, Communication, UI.

#### **F. Gameplay / HUD (`begin game_options`)**
*   **Crosshair:**
    *   Type: `crosshair` (Image spinner).
    *   Size: `cl_crosshairSize`.
    *   Color: `cl_crosshairColor` (Palette).
    *   Hit Style: `cl_crosshairHitStyle`.
    *   Hit Color: `cl_crosshairHitColor`.
*   **HUD:**
    *   Screen Size: `viewsize`.
    *   Ping Graph: `scr_lag_draw`.
    *   Demo Bar: `scr_demobar`.
    *   HUD Opacity: `scr_alpha`.
    *   Console Opacity: `con_alpha`.
    *   Scales: `scr_scale`, `con_scale`, `ui_scale`.
*   **Railgun:**
    *   Type: `cl_railtrail_type`.
    *   Core Color: `cl_railcore_color`.
    *   Spiral Color: `cl_railspiral_color`.
*   **View:**
    *   FOV: `cl_adjustfov` / `fov`.
    *   Gun Model: `cl_gun`.

#### **G. Downloads (`begin downloads`)**
*   **Allow:** `allow_download`.
*   **Types:** `allow_download_maps`, `_players`, `_models`, `_sounds`, `_textures`, `_pics`.
*   **Method:** `cl_http_downloads`.

## 7. Implementation Plan

1.  **Refactor `menu.cpp` Layout Engine:**
    *   Implement `Menu_Size` logic for Widescreen/Narrow detection.
    *   Support `MTYPE_COMBO` drop-down rendering (z-ordering is critical).
2.  **Enhance Widget Classes:**
    *   `MTYPE_SLIDER`: Render filled Lime Green bars.
    *   `MTYPE_MODEL_PREVIEW`: Implement `R_RenderModel` call within menu draw for Player Visuals.
3.  **Implement Conditional Logic:**
    *   Add `condition <cvar> <value>` support to hide advanced shader options when disabled.
4.  **Rewrite `worr.menu`:**
    *   Implement the detailed tree above.
    *   Use `ui_font` for all text rendering.

## 8. Deep Dive: Conditional & Complex Items

### 8.1. Conditional & Renderer-Specific Logic
*   **Menu Script:** `item ... condition gl_shaders 1`
*   **Code:** `Menu_Draw` checks the condition. If false, item is drawn with `uis.color.disabled` and input is ignored.

### 8.2. Expanded Item Examples
*   **Player Visuals Preview:**
    *   *Widget:* `MTYPE_MODEL_PREVIEW`
    *   *Input:* `preview_mode` (Enemy/Team/Self).
    *   *Logic:* Draws the player model with the *active* cvars applied (rimlight intensity, outline color, brightskin color) so the user sees exactly what they are configuring.

## 9. Visual Style Guide

*   **Colors:**
    *   *Background:* Dark Grey / Transparent Black (#000000CC).
    *   *Text Normal:* White (#FFFFFF).
    *   *Text Active/Hover:* Lime Green (#32cd32).
    *   *Text Disabled:* Dark Grey (#777777).
*   **Typography:**
    *   Use `ui_font` cvar. Default to a modern industrial sans-serif.
*   **Feedback:**
    *   *Hover:* Lime Green glow.
    *   *Click:* Industrial mechanical click sound.

## 10. Technical Considerations

*   **Legacy Preservation:** All CVARs from the original `worr.menu` (e.g., `gl_saturation`, `gl_waterwarp`) have been mapped.
*   **Scripting Limits:** Watch for the 1024 char limit on expanded lines.
*   **Menu Depth:** Current limit is 8 (`MAX_MENU_DEPTH`). The new structure should be flat enough to avoid hitting this.
