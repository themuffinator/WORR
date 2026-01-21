# Comprehensive Menu Rework Proposal

## 1. Objective
To completely overhaul the existing menu system (`worr.json` in the cgame UI) to achieve a modern, minimalistic, and highly functional user experience. The rework aims to distinguish clearly between the Main Menu and the In-Game Menu, maximize screen real estate (especially for widescreen), improve navigation, and seamlessly integrate a wealth of new features.

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
    *   **Menu Font:** Controlled via `globals.font` in `worr.json`, which registers the font handle at UI init. Default should be a crisp, readable sans-serif or stylized industrial font, scalable for high resolutions.
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

To facilitate elegant traversal, generic widgets are replaced with specialized ones. **Drop-down Lists** are preferred over archaic spin controls for selection. The current JSON menu system supports a defined set of item types, so proposed widgets below are mapped to JSON types (see §5.2) or explicitly listed as extensions needed (see §11).

*   **Tabs / Categories:**
    *   *Usage:* Top of the "Settings" menu to switch between Video, Audio, Input, etc.
*   **Drop-Down Lists:**
    *   *Usage:* Resolution, Texture Quality, Sound Backend.
    *   *Behavior:* Clicking expands a vertical list of options overlapping other content. Replaces "cycling" spin controls.
*   **Smart Lists:**
    *   *Usage:* Server browser, Demo list.
*   **Visual Sliders:**
    *   *Usage:* Volume, Gamma, Sensitivity.
    *   *Visual:* Filled bar (Lime Green) indicating percentage, numeric value on the right.
*   **Palette Picker:**
    *   *Usage:* Crosshair color, Railgun color.
*   **Toggle Switch:**
    *   *Usage:* Binary settings (VSync, Show FPS).
    *   *Visual:* "Switch" graphic (Left=Off, Right=On) rather than Yes/No text.
*   **Key Binder:**
    *   *Visual:* Displays current key.

### 5.2. Widget-to-JSON Mapping (Current Engine)
The cgame UI uses JSON-defined widgets. Proposed widgets map to JSON item types as follows:

*   **Tabs / Categories:** Model as a top-level "Settings" page containing actions that push sub-pages (Video/Audio/Input/etc.). If true tabs are required, add a `tabs` menu style and a tab widget extension (see §11).
*   **Drop-Down Lists:** Use `values`, `strings`, or `pairs` items to represent discrete choices. These currently render as selection lists; to achieve actual drop-down behavior, extend the widget to render an expanded overlay list (see §11).
*   **Smart Lists:** Use `feeder` pages (servers/demos/players) and `list` page handling. The server/demo browsers already provide list widgets.
*   **Visual Sliders:** Use `range` items with `min/max/step` and render as filled bars. This requires slider styling updates in widget draw.
*   **Palette Picker:** Not supported by JSON yet; requires a `palette` item type and widget (see §11).
*   **Toggle Switch:** Use `toggle` or `switch` item types; update draw to use switch visuals.
*   **Key Binder:** Use `bind` items.

## 6. Detailed Menu Tree & Cvars

### 6.1. Main Menu (`pushmenu main` in JSON)
*   **Singleplayer:**
    *   New Game (Difficulty Select -> Episode Select)
    *   Load Game (Visual save slots with thumbnails if possible)
*   **Multiplayer:**
    *   Server Browser (`pushmenu servers`)
    *   Host Game (`pushmenu startserver`)
    *   Player Setup (Model, Skin, Name)
*   **Settings:** (Opens the comprehensive settings panel)
    *   Video
    *   Performance
    *   Effects
    *   Audio
    *   Input
    *   Controls
    *   Player Visuals
    *   Gameplay / HUD
    *   Screen
    *   Downloads
*   **Extras:**
    *   Demos (`pushmenu demos`)
    *   Credits
*   **Quit**

### 6.2. In-Game Menu (`pushmenu game` in JSON)
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

The core settings menu is a hub that pushes category pages for quick access, with optional tabs if a single-page layout is preferred (see §11).

#### **A. Video (`pushmenu video`)**
*   **Display:**
    *   Mode: `vid_fullscreen` (Drop-down: Windowed/Fullscreen).
    *   Resolution: `vid_mode` (Drop-down).
    *   FOV Scaling: `cl_adjustfov` (Drop-down: Vert-/Hor+).
    *   Brightness: `vid_gamma` (Slider).
    *   HW Gamma: `vid_hwgamma` (Toggle).
    *   Contrast: `gl_contrast` (Slider).
    *   VSync: `gl_swapinterval` (Toggle).
    *   UI Scale: `ui_scale` (Slider).
    *   Console Scale: `con_scale` (Slider).
    *   Screen Scale: `scr_scale` (Slider).
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
    *   Particles: `gl_partscale` (Slider).
    *   Entity Glow: `cl_noglow` (Toggle).
    *   Screen Blend: `gl_polyblend` (Toggle).
    *   Water Warp: `gl_waterwarp` (Toggle).
    *   Explosions: `cl_disable_explosions` (Toggle bits).
    *   AF/AA Overrides: `gl_anisotropy`, `gl_multisamples` (Drop-downs).

#### **B. Performance (`pushmenu performance`)**
*   **Frame Rate Caps:**
    *   Client FPS Cap: `cl_maxfps` (Drop-down).
    *   Renderer FPS Cap: `r_maxfps` (Drop-down).
*   **Client Behavior:**
    *   Client Prediction: `cl_predict` (Toggle).
    *   Async Frame Processing: `cl_async` (Toggle).
    *   Warn on FPS Rounding: `cl_warn_on_fps_rounding` (Toggle).
    *   Footsteps: `cl_footsteps` (Toggle).

#### **C. Audio (`pushmenu sound`)**
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

#### **D. Input (`pushmenu input`)**
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

#### **E. Player Visuals (`pushmenu player_visuals`)**
This new menu consolidates all visual customization for entities.
*   **Preview Area:** A large, rotating 3D model viewport (model preview widget) showing the effect of settings on: "Enemy", "Teammate", or "Self".
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

#### **F. Controls (`pushmenu keys`)**
*   **Categories:** Movement, Combat, Interaction, Communication, UI.

#### **G. Gameplay / HUD (`pushmenu game_options`)**
*   **Crosshair:**
    *   Type: `crosshair` (Image grid / tiles).
    *   Size: `cl_crosshairSize`.
    *   Color: `cl_crosshairColor` (Palette).
    *   Brightness: `cl_crosshairBrightness` (Slider).
    *   Color by Health: `cl_crosshairHealth` (Toggle).
    *   Hit Style: `cl_crosshairHitStyle`.
    *   Hit Color: `cl_crosshairHitColor`.
    *   Hit Time: `cl_crosshairHitTime`.
    *   Pickup Pulse: `cl_crosshairPulse` (Toggle).
    *   Preview: Live preview widget (see §11 for crosshair tile selector).
*   **HUD:**
    *   Screen Size: `viewsize`.
    *   Ping Graph: `scr_lag_draw`.
    *   Demo Bar: `scr_demobar`.
    *   HUD Opacity: `scr_alpha`.
    *   Console Opacity: `con_alpha`.
    *   Scales: `scr_scale`, `con_scale`, `ui_scale`.
    *   HUD Elements: `scr_draw2d` (Toggle/selector).
    *   Status/Chat: `con_notifytime` (Slider).
*   **Railgun:**
    *   Type: `cl_railtrail_type`.
    *   Core Color: `cl_railcore_color`.
    *   Spiral Color: `cl_railspiral_color`.
*   **View:**
    *   FOV: `cl_adjustfov` / `fov`.
    *   Gun Model: `cl_gun`.
    *   View Bob: `cl_bob`, `cl_bobcycle`, `cl_bobup` (Sliders).

#### **H. Effects (`pushmenu effects`)**
*   **Lighting & Models:**
    *   Dynamic Lighting: `gl_dynamic` (Drop-down).
    *   Ground Shadows: `gl_shadows` (Drop-down).
    *   Cel-Shading: `gl_celshading` (Drop-down).
    *   Entity Glow: `cl_noglow` (Toggle).
*   **Screen FX:**
    *   Screen Blend: `gl_polyblend` (Toggle).
    *   Water Warp: `gl_waterwarp` (Toggle).
*   **Explosions:**
    *   Grenade Explosions: `cl_disable_explosions` bit 0 (Toggle).
    *   Rocket Explosions: `cl_disable_explosions` bit 1 (Toggle).
*   **Extras:**
    *   Rail Trail Setup (`pushmenu railtrail`).

#### **I. Screen (`pushmenu screen`)**
*   **Scaling & Visibility:**
    *   Screen Size: `viewsize`.
    *   HUD Opacity: `scr_alpha`.
    *   Console Opacity: `con_alpha`.
    *   HUD Scale: `scr_scale`.
    *   Console Scale: `con_scale`.
    *   Menu Scale: `ui_scale`.
*   **Status & Graphs:**
    *   Ping Graph: `scr_lag_draw` (Toggle).
    *   Demo Bar: `scr_demobar` (Drop-down).
*   **Crosshair Setup:**
    *   Crosshair Menu (`pushmenu crosshair`) for image tiles and hit-feedback settings.

#### **J. Downloads (`pushmenu downloads`)**
*   **Allow:** `allow_download`.
*   **Types:** `allow_download_maps`, `_players`, `_models`, `_sounds`, `_textures`, `_pics`.
*   **Method:** `cl_http_downloads`.

## 7. Implementation Plan

1.  **Align with JSON UI Architecture:**
    *   Update `worr.json` under `src/game/cgame/ui/` to implement the menu tree and settings.
    *   Use `globals.font` to set typography.
2.  **Enhance JSON Widgets:**
    *   Update `ui_widgets.cpp` to render filled sliders and toggle switch visuals.
    *   Extend JSON schema and widget support for palette picking and drop-down overlays (see §11).
3.  **Conditional Logic in JSON:**
    *   Use JSON `show` / `enable` / `default` condition strings in `worr.json`.
    *   Implement any additional condition types in `ui_conditions.cpp` if required.

## 8. Deep Dive: Conditional & Complex Items

### 8.1. Conditional & Renderer-Specific Logic (JSON)
*   **Menu JSON:** `"show": "cvar:gl_shaders==1"` or `"enable": "cvar:gl_dynamic==1"`
*   **Code:** JSON conditions are parsed in `ui_json.cpp` and evaluated in `ui_conditions.cpp`. If a `show` condition fails, the widget is hidden; if `enable` fails, it is drawn disabled and input is ignored.

### 8.2. Expanded Item Examples
*   **Player Visuals Preview:**
    *   *Widget:* Model preview widget (JSON extension).
    *   *Input:* `preview_mode` (Enemy/Team/Self).
    *   *Logic:* Draws the player model with the *active* cvars applied (rimlight intensity, outline color, brightskin color) so the user sees exactly what they are configuring.

## 9. Visual Style Guide

*   **Colors:**
    *   *Background:* Dark Grey / Transparent Black (#000000CC).
    *   *Text Normal:* White (#FFFFFF).
    *   *Text Active/Hover:* Lime Green (#32cd32).
    *   *Text Disabled:* Dark Grey (#777777).
*   **Typography:**
    *   Use `globals.font` in `worr.json`. Default to a modern industrial sans-serif.
*   **Feedback:**
    *   *Hover:* Lime Green glow.
    *   *Click:* Industrial mechanical click sound.

## 10. Technical Considerations

*   **Legacy Preservation:** All CVARs from the original `worr.menu` (e.g., `gl_saturation`, `gl_waterwarp`) have been mapped.
*   **JSON Limits:** Item expansion still respects string limits; avoid huge macro expansions.
*   **Menu Depth:** Current limit is 8 (`MAX_MENU_DEPTH`). The new structure should be flat enough to avoid hitting this.

## 11. Missing Widget Support / Extensions Required
The following widgets or behaviors are not currently in the JSON menu system and need implementation:

*   **Palette Picker:** A `palette` item type to pick colors for crosshair/rail. Requires JSON parsing, widget drawing, and input (possibly a popover grid of color swatches).
*   **Drop-Down Overlay Lists:** `values/strings/pairs` currently render as standard selectable lists; implement an expanded drop-down overlay with scroll support and z-ordering.
*   **Crosshair Tile Selector:** A scrollable grid of crosshair previews (image tiles) for `crosshair` selection. Similar to `imagevalues` but with a grid layout, hover preview, and optional page controls.
*   **Tabs/Segmented Control:** Optional tab bar widget at the top of Settings if a single-page tabbed UI is desired instead of separate pages.
*   **Model Preview Widget:** `model_preview` widget that renders the player model in-menu, with configuration based on cvars.
*   **Read-Only Status Fields:** Text-only widgets for renderer/driver info (or use `action` with empty command and disabled state).
