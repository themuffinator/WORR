/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
// console.c

#include "client.h"
#include "client/font.h"

#define CON_TIMES 16
#define CON_TIMES_MASK (CON_TIMES - 1)

#define CON_TOTALLINES 1024 // total lines in console scrollback
#define CON_TOTALLINES_MASK (CON_TOTALLINES - 1)

#define CON_LINEWIDTH 126 // fixed width, do not need more

typedef enum { CHAT_NONE, CHAT_DEFAULT, CHAT_TEAM } chatMode_t;

typedef enum { CON_POPUP, CON_DEFAULT, CON_REMOTE } consoleMode_t;

typedef struct {
  byte color;
  byte ts_len;
  char text[CON_LINEWIDTH];
} consoleLine_t;

typedef struct {
  consoleLine_t text[CON_TOTALLINES];

  int current; // line where next message will be printed
  int x;       // offset in current line for next print
  int display; // bottom of console displays this line
  color_index_t color;
  int newline;

  int linewidth; // characters across screen
  int vidWidth, vidHeight;
  float scale;
  color_t ts_color;

  unsigned times[CON_TIMES]; // cls.realtime time the line was generated
                             // for transparent notify lines
  bool skipNotify;
  bool initialized;

  qhandle_t backImage;
  qhandle_t charsetImage;
  font_t *font;
  float font_pixel_scale;

  float currentHeight; // approaches scr_conlines at scr_conspeed
  float destHeight;    // 0.0 to 1.0 lines of console to display

  commandPrompt_t chatPrompt;
  commandPrompt_t prompt;

  chatMode_t chat;
  consoleMode_t mode;
  netadr_t remoteAddress;
  char *remotePassword;

  load_state_t loadstate;
} console_t;

static console_t con;

static cvar_t *con_notifytime;
static cvar_t *con_notifylines;
static cvar_t *con_clock;
static cvar_t *con_height;
static cvar_t *con_speed;
static cvar_t *con_speed_legacy;
static cvar_t *con_alpha;
static cvar_t *con_scale;
static cvar_t *con_font;
static cvar_t *con_background;
static cvar_t *con_scroll;
static cvar_t *con_history;
static cvar_t *con_timestamps;
static cvar_t *con_timestampsformat;
static cvar_t *con_timestampscolor;
static cvar_t *con_auto_chat;
static cvar_t *ui_download_active;
static cvar_t *con_fontscale;
static cvar_t *con_fontsize;

static bool con_speed_alias_syncing;

static void con_speed_alias_changed(cvar_t *self)
{
  if (con_speed_alias_syncing)
    return;

  con_speed_alias_syncing = true;

  if (self == con_speed && con_speed_legacy)
    Cvar_SetByVar(con_speed_legacy, con_speed->string, FROM_CODE);
  else if (self == con_speed_legacy && con_speed)
    Cvar_SetByVar(con_speed, con_speed_legacy->string, FROM_CODE);

  con_speed_alias_syncing = false;
}

static void con_speed_alias_sync_defaults(void)
{
  if (con_speed_alias_syncing)
    return;

  con_speed_alias_syncing = true;

  if (con_speed && con_speed_legacy) {
    if (!(con_speed->flags & CVAR_MODIFIED) && (con_speed_legacy->flags & CVAR_MODIFIED))
      Cvar_SetByVar(con_speed, con_speed_legacy->string, FROM_CODE);
    else
      Cvar_SetByVar(con_speed_legacy, con_speed->string, FROM_CODE);
  }

  con_speed_alias_syncing = false;
}

static void con_speed_alias_register(void)
{
  if (con_speed)
    con_speed->changed = con_speed_alias_changed;
  if (con_speed_legacy)
    con_speed_legacy->changed = con_speed_alias_changed;

  con_speed_alias_sync_defaults();
}

static float Con_GetFontPixelScale(void);
static int Con_FontCharWidth(void);
static int Con_FontCharHeight(void);

// ============================================================================

/*
================
Con_SkipNotify
================
*/
void Con_SkipNotify(bool skip) { con.skipNotify = skip; }

/*
================
Con_ClearTyping
================
*/
void Con_ClearTyping(void) {
  // clear any typing
  IF_Clear(&con.prompt.inputLine);
  Prompt_ClearState(&con.prompt);
}

/*
================
Con_Close

Instantly removes the console. Unless `force' is true, does not remove the
console if user has typed something into it since the last call to Con_Popup.
================
*/
void Con_Close(bool force) {
  if (con.mode > CON_POPUP && !force) {
    return;
  }

  // if not connected, console or menu should be up
  if (cls.state < ca_active && !(cls.key_dest & KEY_MENU)) {
    return;
  }

  Con_ClearTyping();
  Con_ClearNotify_f();

  Key_SetDest(static_cast<keydest_t>(cls.key_dest & ~KEY_CONSOLE));

  con.destHeight = con.currentHeight = 0;
  con.mode = CON_POPUP;
  con.chat = CHAT_NONE;
}

/*
================
Con_Popup

Drop to connection screen. Unless `force' is true, does not change console mode
to popup.
================
*/
void Con_Popup(bool force) {
  if (force) {
    con.mode = CON_POPUP;
  }

  Key_SetDest(static_cast<keydest_t>(cls.key_dest | KEY_CONSOLE));
  Con_RunConsole();
}

/*
================
Con_ToggleConsole_f

Toggles console up/down animation.
================
*/
static void toggle_console(consoleMode_t mode, chatMode_t chat) {
  SCR_EndLoadingPlaque(); // get rid of loading plaque

  Con_ClearTyping();
  Con_ClearNotify_f();

  if (cls.key_dest & KEY_CONSOLE) {
    Key_SetDest(static_cast<keydest_t>(cls.key_dest & ~KEY_CONSOLE));
    con.mode = CON_POPUP;
    con.chat = CHAT_NONE;
    return;
  }

  // toggling console discards chat message
  Key_SetDest(
      static_cast<keydest_t>((cls.key_dest | KEY_CONSOLE) & ~KEY_MESSAGE));
  con.mode = mode;
  con.chat = chat;
}

void Con_ToggleConsole_f(void) { toggle_console(CON_DEFAULT, CHAT_NONE); }

/*
================
Con_Clear_f
================
*/
static void Con_Clear_f(void) {
  memset(con.text, 0, sizeof(con.text));
  con.display = con.current = 0;
  con.newline = '\r';
}

static void Con_Dump_c(genctx_t *ctx, int argnum) {
  if (argnum == 1) {
    FS_File_g("condumps", ".txt", FS_SEARCH_STRIPEXT, ctx);
  }
}

/*
================
Con_Dump_f

Save the console contents out to a file
================
*/
static void Con_Dump_f(void) {
  int l;
  qhandle_t f;
  char name[MAX_OSPATH];

  if (Cmd_Argc() != 2) {
    Com_Printf("$cl_filename_usage", Cmd_Argv(0));
    return;
  }

  f = FS_EasyOpenFile(name, sizeof(name), FS_MODE_WRITE | FS_FLAG_TEXT,
                      "condumps/", Cmd_Argv(1), ".txt");
  if (!f) {
    return;
  }

  // skip empty lines
  for (l = con.current - CON_TOTALLINES + 1; l <= con.current; l++) {
    if (con.text[l & CON_TOTALLINES_MASK].text[0]) {
      break;
    }
  }

  // write the remaining lines
  for (; l <= con.current; l++) {
    char buffer[CON_LINEWIDTH + 1];
    const char *p = con.text[l & CON_TOTALLINES_MASK].text;
    int i;

    for (i = 0; i < CON_LINEWIDTH && p[i]; i++)
      buffer[i] = Q_charascii(p[i]);
    buffer[i] = '\n';

    FS_Write(buffer, i + 1, f);
  }

  if (FS_CloseFile(f))
    Com_EPrintf("$cl_error_writing_file", name);
  else
    Com_Printf("$con_dumped_to_file", name);
}

/*
================
Con_ClearNotify_f
================
*/
void Con_ClearNotify_f(void) {
  int i;

  for (i = 0; i < CON_TIMES; i++)
    con.times[i] = 0;
}

/*
================
Con_MessageMode_f
================
*/
static void start_message_mode(chatMode_t mode) {
  if (cls.state != ca_active || cls.demo.playback) {
    Com_Printf("$cl_chat_requires_level");
    return;
  }

  // starting messagemode closes console
  if (cls.key_dest & KEY_CONSOLE) {
    Con_Close(true);
  }

  con.chat = mode;
  IF_Replace(&con.chatPrompt.inputLine, COM_StripQuotes(Cmd_RawArgs()));
  Key_SetDest(static_cast<keydest_t>(cls.key_dest | KEY_MESSAGE));
}

static void Con_MessageMode_f(void) { start_message_mode(CHAT_DEFAULT); }

static void Con_MessageMode2_f(void) { start_message_mode(CHAT_TEAM); }

/*
================
Con_RemoteMode_f
================
*/
static void Con_RemoteMode_f(void) {
  netadr_t adr;
  char *s;

  if (Cmd_Argc() != 3) {
    Com_Printf("$cl_rcon_usage", Cmd_Argv(0));
    return;
  }

  s = Cmd_Argv(1);
  if (!NET_StringToAdr(s, &adr, PORT_SERVER)) {
    Com_Printf("$cl_bad_address", s);
    return;
  }

  s = Cmd_Argv(2);

  if (!(cls.key_dest & KEY_CONSOLE)) {
    toggle_console(CON_REMOTE, CHAT_NONE);
  } else {
    con.mode = CON_REMOTE;
    con.chat = CHAT_NONE;
  }

  Z_Free(con.remotePassword);

  con.remoteAddress = adr;
  con.remotePassword = Z_CopyString(s);
}

static void CL_RemoteMode_c(genctx_t *ctx, int argnum) {
  if (argnum == 1) {
    Com_Address_g(ctx);
  }
}

/*
================
Con_CheckResize

If the line width has changed, reformat the buffer.
================
*/
void Con_CheckResize(void) {
  con.scale = R_ClampScale(con_scale);

  int base_width = scr.virtual_width ? scr.virtual_width : r_config.width;
  int base_height = scr.virtual_height ? scr.virtual_height : r_config.height;

  con.vidWidth = Q_rint(base_width * con.scale);
  con.vidHeight = Q_rint(base_height * con.scale);

  int char_width = Con_FontCharWidth();
  con.linewidth = Q_clip(con.vidWidth / char_width - 2, 0, CON_LINEWIDTH);
  con.prompt.inputLine.visibleChars = con.linewidth;
  con.prompt.widthInChars = con.linewidth;
  con.chatPrompt.inputLine.visibleChars = con.linewidth;

  if (con_timestamps->integer) {
    char temp[CON_LINEWIDTH];
    con.prompt.widthInChars -=
        Com_FormatLocalTime(temp, con.linewidth, con_timestampsformat->string);
  }

  if (con.font && cls.ref_initialized) {
    float pixel_scale = Con_GetFontPixelScale();
    if (con.font_pixel_scale != pixel_scale)
      Con_RegisterMedia();
  }
}

static float Con_GetFontPixelScale(void) {
  float scale_x = (float)r_config.width / VIRTUAL_SCREEN_WIDTH;
  float scale_y = (float)r_config.height / VIRTUAL_SCREEN_HEIGHT;
  float base_scale = max(scale_x, scale_y);
  int base_scale_int = (int)base_scale;

  if (base_scale_int < 1)
    base_scale_int = 1;

  float scale = con.scale > 0.0f ? con.scale : 1.0f;
  return (float)base_scale_int / scale;
}

static int Con_FontCharWidth(void) {
  if (con.font) {
    int width = Font_MeasureString(con.font, 1, 0, 1, "M", nullptr);
    if (width > 0)
      return width;
  }
  return CONCHAR_WIDTH;
}

static int Con_FontCharHeight(void) {
  if (con.font)
    return Font_LineHeight(con.font, 1);
  return CONCHAR_HEIGHT;
}

static int Con_MeasureString(const char *text, size_t max_chars) {
  if (!text || !*text)
    return 0;

  if (con.font)
    return Font_MeasureString(con.font, 1, 0, max_chars, text, nullptr);

  size_t len = strlen(text);
  if (len > max_chars)
    len = max_chars;
  return (int)Com_StrlenNoColor(text, len) * CONCHAR_WIDTH;
}

static int Con_DrawString(int x, int y, int flags, size_t max_chars,
                          const char *text, color_t color) {
  if (con.font)
    return Font_DrawString(con.font, x, y, 1, flags, max_chars, text, color);
  return R_DrawString(x, y, flags, max_chars, text, color, con.charsetImage);
}

/*
================
Con_CheckTop

Make sure at least one line is visible if console is backscrolled.
================
*/
static void Con_CheckTop(void) {
  int top = con.current - CON_TOTALLINES + 1;

  if (top < 0) {
    top = 0;
  }
  if (con.display < top) {
    con.display = top;
  }
}

static void con_media_changed(cvar_t *self) {
  if (con.initialized && cls.ref_initialized) {
    Con_RegisterMedia();
    Con_CheckResize();
  }
}

static void con_width_changed(cvar_t *self) {
  if (con.initialized && cls.ref_initialized) {
    Con_CheckResize();
  }
}

static void con_timestampscolor_changed(cvar_t *self) {
  if (!SCR_ParseColor(self->string, &con.ts_color)) {
    Com_WPrintf("$con_invalid_value", self->string, self->name);
    Cvar_Reset(self);
    con.ts_color = COLOR_RGB(170, 170, 170);
  }
}

static const cmdreg_t c_console[] = {
    {"toggleconsole", Con_ToggleConsole_f},
    {"messagemode", Con_MessageMode_f},
    {"messagemode2", Con_MessageMode2_f},
    {"remotemode", Con_RemoteMode_f, CL_RemoteMode_c},
    {"clear", Con_Clear_f},
    {"clearnotify", Con_ClearNotify_f},
    {"condump", Con_Dump_f, Con_Dump_c},

    {NULL}};

/*
================
Con_Init
================
*/
void Con_Init(void) {
  memset(&con, 0, sizeof(con));

  //
  // register our commands
  //
  Cmd_Register(c_console);

  con_notifytime = Cvar_Get("con_notifytime", "0", 0);
  con_notifytime->changed = cl_timeout_changed;
  con_notifytime->changed(con_notifytime);
  con_notifylines = Cvar_Get("con_notifylines", "4", 0);
  con_clock = Cvar_Get("con_clock", "0", 0);
  con_height = Cvar_Get("con_height", "0.66", 0);
  con_speed = Cvar_Get("con_speed", "3", 0);
  con_speed_legacy = Cvar_Get("scr_conspeed", con_speed->string, CVAR_NOARCHIVE);
  con_speed_alias_register();
  con_alpha = Cvar_Get("con_alpha", "1", 0);
  con_scale = Cvar_Get("con_scale", "0", 0);
  con_scale->changed = con_width_changed;
  con_font = Cvar_Get("con_font", "fonts/RobotoMono-Regular.ttf", 0);
  con_font->changed = con_media_changed;
  con_fontscale = Cvar_Get("con_fontscale", "5", 0);
  con_fontscale->changed = con_media_changed;
  con_fontsize = Cvar_Get("con_fontsize", con_fontscale->string, 0);
  con_fontsize->changed = con_media_changed;
  con_background = Cvar_Get("con_background", "conback", 0);
  con_background->changed = con_media_changed;
  con_scroll = Cvar_Get("con_scroll", "0", 0);
  con_history = Cvar_Get("con_history", STRINGIFY(HISTORY_SIZE), 0);
  con_timestamps = Cvar_Get("con_timestamps", "0", 0);
  con_timestamps->changed = con_width_changed;
  con_timestampsformat = Cvar_Get("con_timestampsformat", "%H:%M:%S ", 0);
  con_timestampsformat->changed = con_width_changed;
  con_timestampscolor = Cvar_Get("con_timestampscolor", "#aaa", 0);
  con_timestampscolor->changed = con_timestampscolor_changed;
  con_timestampscolor_changed(con_timestampscolor);
  con_auto_chat = Cvar_Get("con_auto_chat", "0", 0);
  ui_download_active = Cvar_Get("ui_download_active", "0", 0);

  IF_Init(&con.prompt.inputLine, 0, MAX_FIELD_TEXT - 1);
  IF_Init(&con.chatPrompt.inputLine, 0, MAX_FIELD_TEXT - 1);

  con.prompt.printf = Con_Printf;

  // use default width since no video is initialized yet
  r_config.width = 640;
  r_config.height = 480;
  con.linewidth = -1;
  con.scale = 1;
  con.color = COLOR_INDEX_NONE;
  con.newline = '\r';

  Con_CheckResize();

  con.initialized = true;
}

void Con_PostInit(void) {
  if (con_history->integer > 0) {
    Prompt_LoadHistory(&con.prompt, COM_HISTORYFILE_NAME);
  }
}

/*
================
Con_Shutdown
================
*/
void Con_Shutdown(void) {
  if (con_history->integer > 0) {
    Prompt_SaveHistory(&con.prompt, COM_HISTORYFILE_NAME, con_history->integer);
  }
  Prompt_Clear(&con.prompt);
}

static void Con_CarriageRet(void) {
  consoleLine_t *line = &con.text[con.current & CON_TOTALLINES_MASK];

  // add color from last line
  line->color = con.color;

  // add timestamp
  con.x = 0;
  if (con_timestamps->integer)
    con.x = Com_FormatLocalTime(line->text, con.linewidth,
                                con_timestampsformat->string);
  line->ts_len = con.x;

  // init text (must be after timestamp format which may overflow)
  memset(line->text + con.x, 0, CON_LINEWIDTH - con.x);

  // update time for transparent overlay
  if (!con.skipNotify)
    con.times[con.current & CON_TIMES_MASK] = cls.realtime;
}

static void Con_Linefeed(void) {
  if (con.display == con.current)
    con.display++;
  con.current++;

  Con_CarriageRet();

  if (con_scroll->integer & 2) {
    con.display = con.current;
  } else {
    Con_CheckTop();
  }

  // wrap to avoid integer overflow
  if (con.current >= CON_TOTALLINES * 2) {
    con.current -= CON_TOTALLINES;
    con.display -= CON_TOTALLINES;
  }
}

void Con_SetColor(color_index_t color) { con.color = color; }

/*
=================
CL_LoadState
=================
*/
void CL_LoadState(load_state_t state) {
  con.loadstate = state;
  SCR_UpdateScreen();
  if (vid)
    vid->pump_events();
  S_Update();
}

/*
================
Con_Print

Handles cursor positioning, line wrapping, etc
All console printing must go through this in order to be displayed on screen
If no console is visible, the text will appear at the top of the game window
================
*/
void Con_Print(const char *txt) {
  char *p;
  int l;

  if (!con.initialized)
    return;

  while (*txt) {
    if (con.newline) {
      if (con.newline == '\n') {
        Con_Linefeed();
      } else {
        Con_CarriageRet();
      }
      con.newline = 0;
    }

    // count word length
    for (p = (char *)txt; *p > 32; p++)
      ;
    l = p - txt;

    // word wrap
    if (l < con.linewidth && con.x + l > con.linewidth) {
      Con_Linefeed();
    }

    switch (*txt) {
    case '\r':
    case '\n':
      con.newline = *txt;
      break;
    default: // display character and advance
      if (con.x == con.linewidth) {
        Con_Linefeed();
      }
      p = con.text[con.current & CON_TOTALLINES_MASK].text;
      p[con.x++] = *txt;
      break;
    }

    txt++;
  }

  // update time for transparent overlay
  if (!con.skipNotify)
    con.times[con.current & CON_TIMES_MASK] = cls.realtime;
}

/*
================
Con_Printf

Print text to graphical console only,
bypassing system console and logfiles
================
*/
void Con_Printf(const char *fmt, ...) {
  va_list argptr;
  char msg[MAXPRINTMSG];

  va_start(argptr, fmt);
  Q_vsnprintf(msg, sizeof(msg), fmt, argptr);
  va_end(argptr);

  Con_Print(msg);
}

/*
================
Con_RegisterMedia
================
*/
void Con_RegisterMedia(void) {
  if (con.font) {
    Font_Free(con.font);
    con.font = nullptr;
  }
  con.font_pixel_scale = 0.0f;

  float pixel_scale = Con_GetFontPixelScale();
  int con_size = con_fontscale ? Cvar_ClampInteger(con_fontscale, 1, 64)
                               : CONCHAR_HEIGHT;
  if (con_fontscale && con_fontsize &&
      con_fontscale->default_string &&
      !Q_strcasecmp(con_fontscale->string, con_fontscale->default_string)) {
    int legacy_size = Cvar_ClampInteger(con_fontsize, 1, 64);
    if (legacy_size > 0)
      con_size = legacy_size;
  }
  int con_line_height = max(1, con_size);
  int con_fixed_advance = max(1, con_size);
  con.font =
      Font_Load(con_font->string, con_line_height, pixel_scale,
                con_fixed_advance, "fonts/qfont.kfont", "conchars.png");
  if (!con.font && strcmp(con_font->string, con_font->default_string)) {
    Cvar_Reset(con_font);
    con.font =
        Font_Load(con_font->default_string, con_line_height, pixel_scale,
                  con_fixed_advance, "fonts/qfont.kfont", "conchars.png");
  }
  con.font_pixel_scale = pixel_scale;
  con.charsetImage = Font_LegacyHandle(con.font);
  if (!con.font) {
    Com_Error(ERR_FATAL, "%s", Com_GetLastError());
  }

  if (con.font) {
    int char_width = Con_FontCharWidth();
    con.linewidth = Q_clip(con.vidWidth / char_width - 2, 0, CON_LINEWIDTH);
    con.prompt.inputLine.visibleChars = con.linewidth;
    con.prompt.widthInChars = con.linewidth;
    con.chatPrompt.inputLine.visibleChars = con.linewidth;

    if (con_timestamps->integer) {
      char temp[CON_LINEWIDTH];
      con.prompt.widthInChars -= Com_FormatLocalTime(
          temp, con.linewidth, con_timestampsformat->string);
    }
  }

  con.backImage = R_RegisterPic(con_background->string);
  if (!con.backImage) {
    if (strcmp(con_background->string, con_background->default_string)) {
      Cvar_Reset(con_background);
      con.backImage = R_RegisterPic(con_background->default_string);
    }
  }
}

void Con_RendererShutdown(void) {
  con.font = nullptr;
  con.font_pixel_scale = 0.0f;
  con.charsetImage = 0;
  con.backImage = 0;
}

/*
==============================================================================

DRAWING

==============================================================================
*/

static int Con_DrawLine(int v, int row, float alpha, bool notify) {
  const consoleLine_t *line = &con.text[row & CON_TOTALLINES_MASK];
  const char *s = line->text;
  int flags = 0;
  int x = Con_FontCharWidth();
  int w = con.linewidth;
  color_t color;

  if (notify) {
    s += line->ts_len;
  } else if (line->ts_len) {
    color = COLOR_SETA_U8(con.ts_color, static_cast<uint8_t>(alpha * 255.0f));
    x = Con_DrawString(x, v, 0, line->ts_len, s, color);
    s += line->ts_len;
    w -= line->ts_len;
  }
  if (w < 1)
    return x;

  switch (line->color) {
  case COLOR_INDEX_ALT:
    flags = UI_ALTCOLOR;
    // fall through
  case COLOR_INDEX_NONE:
    color = COLOR_WHITE;
    break;
  default:
    color = colorTable[line->color & 7];
    break;
  }
  color.a *= alpha;

  return Con_DrawString(x, v, flags, w, s, color);
}

const char *Con_GetChatPromptText(int *skip_chars) {
  const char *prompt = con.chat == CHAT_TEAM ? "say_team: " : "say: ";

  if (skip_chars)
    *skip_chars = (int)strlen(prompt);
  return prompt;
}

inputField_t *Con_GetChatInputField(void) { return &con.chatPrompt.inputLine; }

static size_t Con_InputClampChars(const char *text, size_t max_chars) {
  if (!text)
    return 0;
  size_t available = UTF8_CountChars(text, strlen(text));
  if (max_chars)
    available = min(available, max_chars);
  return available;
}

static size_t Con_InputCharsForWidth(const char *text, size_t max_chars,
                                     int pixel_width) {
  if (!text || pixel_width <= 0 || max_chars == 0)
    return 0;

  size_t available = Con_InputClampChars(text, max_chars);
  size_t chars = 0;
  while (chars < available) {
    size_t next_bytes = UTF8_OffsetForChars(text, chars + 1);
    int width = Con_MeasureString(text, next_bytes);
    if (width > pixel_width)
      break;
    chars++;
  }
  return chars;
}

static size_t Con_InputOffsetForWidth(const char *text, size_t cursor_chars,
                                      int pixel_width) {
  if (!text || pixel_width <= 0)
    return 0;

  size_t cursor_bytes = UTF8_OffsetForChars(text, cursor_chars);
  for (size_t start = 0; start < cursor_chars; ++start) {
    size_t start_bytes = UTF8_OffsetForChars(text, start);
    size_t len_bytes = cursor_bytes - start_bytes;
    int width = Con_MeasureString(text + start_bytes, len_bytes);
    if (width <= pixel_width)
      return start;
  }
  return cursor_chars;
}

static int Con_DrawInputField(const inputField_t *field, int x, int y,
                              int flags, size_t max_chars, color_t color) {
  if (!field)
    return 0;
  if (!field->maxChars || !field->visibleChars)
    return 0;
  if (!con.font)
    return IF_Draw(field, x, y, flags, con.charsetImage);

  int pixel_width = max(0, con.vidWidth - x);
  size_t total_chars = Con_InputClampChars(field->text, max_chars);
  size_t cursor_chars = UTF8_CountChars(field->text, field->cursorPos);
  if (cursor_chars > total_chars)
    cursor_chars = total_chars;
  size_t offset_chars = Con_InputOffsetForWidth(field->text, cursor_chars,
                                                pixel_width);
  if (offset_chars > total_chars)
    offset_chars = total_chars;

  size_t remaining_chars = (offset_chars < total_chars)
                               ? (total_chars - offset_chars)
                               : 0;
  size_t offset = UTF8_OffsetForChars(field->text, offset_chars);
  const char *text = field->text + offset;
  size_t draw_chars = Con_InputCharsForWidth(text, remaining_chars,
                                             pixel_width);
  size_t draw_len = UTF8_OffsetForChars(text, draw_chars);
  size_t cursor_chars_visible =
      (cursor_chars > offset_chars) ? (cursor_chars - offset_chars) : 0;
  if (cursor_chars_visible > draw_chars)
    cursor_chars_visible = draw_chars;
  size_t cursor_bytes = UTF8_OffsetForChars(text, cursor_chars_visible);

  if (field->selecting && field->selectionAnchor != field->cursorPos) {
    size_t sel_start = min(field->selectionAnchor, field->cursorPos);
    size_t sel_end = max(field->selectionAnchor, field->cursorPos);
    size_t sel_start_chars = UTF8_CountChars(field->text, sel_start);
    size_t sel_end_chars = UTF8_CountChars(field->text, sel_end);
    size_t sel_start_visible =
        (sel_start_chars > offset_chars) ? (sel_start_chars - offset_chars) : 0;
    size_t sel_end_visible =
        (sel_end_chars > offset_chars) ? (sel_end_chars - offset_chars) : 0;
    if (sel_start_visible > draw_chars)
      sel_start_visible = draw_chars;
    if (sel_end_visible > draw_chars)
      sel_end_visible = draw_chars;

    if (sel_end_visible > sel_start_visible) {
      size_t sel_start_bytes = UTF8_OffsetForChars(text, sel_start_visible);
      size_t sel_end_bytes = UTF8_OffsetForChars(text, sel_end_visible);
      int sel_start_x = x + Con_MeasureString(text, sel_start_bytes);
      int sel_end_x = x + Con_MeasureString(text, sel_end_bytes);
      int sel_w = max(0, sel_end_x - sel_start_x);
      color_t highlight = COLOR_RGBA(80, 120, 200, 120);
      int char_height = Con_FontCharHeight();
      R_DrawFill32(sel_start_x, y, sel_w, char_height, highlight);
    }
  }

  int end_x = Font_DrawString(con.font, x, y, 1, flags, draw_len, text, color);

  if ((flags & UI_DRAWCURSOR) && (com_localTime & BIT(8))) {
    int cursor_x = x + Con_MeasureString(text, cursor_bytes);
    int char_height = Con_FontCharHeight();
    size_t next_chars = min(draw_chars, cursor_chars_visible + 1);
    size_t next_bytes = UTF8_OffsetForChars(text, next_chars);
    int next_x = x + Con_MeasureString(text, next_bytes);
    int cursor_w = max(0, next_x - cursor_x);
    if (Key_GetOverstrikeMode()) {
      int width = max(2, cursor_w);
      if (width < 2)
        width = max(2, char_height / 2);
      color_t fill = COLOR_SETA_U8(color, 160);
      R_DrawFill32(cursor_x, y, width, char_height, fill);
    } else {
      color_t fill = COLOR_SETA_U8(color, 220);
      R_DrawFill32(cursor_x, y, 1, char_height, fill);
    }
  }

  return end_x;
}

/*
================
Con_DrawSolidConsole

Draws the console with the solid background
================
*/
static void Con_DrawSolidConsole(void) {
  int i, x, y;
  int rows;
  const char *text;
  int row;
  char buffer[CON_LINEWIDTH];
  int vislines;
  uint8_t bg_alpha;
  int widths[2];
  int char_width;
  int char_height;
  int bottom_line_y;
  int separator_y;
  int input_y;
  int output_bottom_y;
  int text_rows;
  int text_top_y;
  int text_bottom_y;
  int right_edge;

  vislines = con.vidHeight * con.currentHeight;
  if (vislines <= 0)
    return;

  if (vislines > con.vidHeight)
    vislines = con.vidHeight;

  char_width = Con_FontCharWidth();
  char_height = Con_FontCharHeight();
  right_edge = con.vidWidth;
  bottom_line_y = vislines - 1;
  separator_y = bottom_line_y - char_height - 1;
  if (separator_y < 0)
    separator_y = 0;
  input_y = separator_y + 1;
  output_bottom_y = input_y - 2 * char_height;
  if (output_bottom_y < 0)
    output_bottom_y = 0;

  // setup transparency
  color_t color = COLOR_WHITE;
  bg_alpha = 204;

  // draw the background
  if (cls.state < ca_active || (cls.key_dest & KEY_MENU) || con_alpha->value) {
    bool use_rerelease =
        (!con.backImage || !con_background || !con_background->string[0] ||
         !Q_strcasecmp(con_background->string, "conback"));
    if (use_rerelease) {
      int top = vislines - con.vidHeight;
      int height = con.vidHeight;
      int split = (int)(height * 0.65f);
      color_t top_color = COLOR_SETA_U8(COLOR_RGB(18, 18, 18), bg_alpha);
      color_t bottom_color = COLOR_SETA_U8(COLOR_RGB(10, 10, 10), bg_alpha);
      color_t line_light = COLOR_SETA_U8(COLOR_RGB(56, 56, 56), bg_alpha);
      color_t line_dark = COLOR_SETA_U8(COLOR_RGB(32, 32, 32), bg_alpha);
      color_t line_accent = COLOR_SETA_U8(COLOR_RGB(86, 108, 66), bg_alpha);

      R_DrawFill32(0, top, con.vidWidth, split, top_color);
      R_DrawFill32(0, top + split, con.vidWidth, height - split, bottom_color);

      if (separator_y >= top && separator_y < vislines - 1) {
        R_DrawFill32(0, separator_y, con.vidWidth, 1, line_light);
        if (separator_y + 1 < vislines - 1)
          R_DrawFill32(0, separator_y + 1, con.vidWidth, 1, line_dark);
      }
      if (vislines > 0)
        R_DrawFill32(0, vislines - 1, con.vidWidth, 1, line_accent);
    } else {
      color_t bg_color = COLOR_SETA_U8(COLOR_WHITE, bg_alpha);
      R_DrawKeepAspectPic(0, vislines - con.vidHeight, con.vidWidth,
                          con.vidHeight, bg_color, con.backImage);
    }
  }

  // draw the text
  y = output_bottom_y;
  rows = y / char_height + 1; // rows of text to draw

  // draw arrows to show the buffer is backscrolled
  if (con.display != con.current) {
    for (i = 1; i < con.linewidth / 2; i += 4) {
      R_DrawStretchChar(i * char_width, y, char_width, char_height, 0, '^',
                        COLOR_SETA_U8(COLOR_RED, color.a), con.charsetImage);
    }

    y -= char_height;
    rows--;
  }

  text_rows = rows;
  text_bottom_y = y;
  text_top_y = text_bottom_y - (text_rows - 1) * char_height;
  if (text_top_y < 0)
    text_top_y = 0;

  // draw from the bottom up
  row = con.display;
  widths[0] = widths[1] = 0;
  for (i = 0; i < rows; i++) {
    if (row < 0)
      break;
    if (con.current - row > CON_TOTALLINES - 1)
      break; // past scrollback wrap point

    x = Con_DrawLine(y, row, 1, false);
    if (i < 2) {
      widths[i] = x;
    }

    y -= char_height;
    row--;
  }

  if (text_rows > 0) {
    int total_lines = con.current + 1;
    if (total_lines > CON_TOTALLINES)
      total_lines = CON_TOTALLINES;
    if (total_lines < 1)
      total_lines = 1;

    int scrollable = total_lines - text_rows;
    if (scrollable > 0) {
      int track_top = text_top_y;
      int track_bottom = text_bottom_y + char_height;
      int track_h = track_bottom - track_top;
      int bar_w = max(2, char_width / 6);
      int bar_x = con.vidWidth - bar_w - 2;
      if (bar_x < 0)
        bar_x = con.vidWidth - bar_w;
      right_edge = bar_x - 2;
      if (right_edge < 0)
        right_edge = 0;

      int scroll_offset = con.current - con.display;
      if (scroll_offset < 0)
        scroll_offset = 0;
      if (scroll_offset > scrollable)
        scroll_offset = scrollable;

      int thumb_h = max(char_height / 2,
                        Q_rint((float)track_h * text_rows / total_lines));
      if (thumb_h > track_h)
        thumb_h = track_h;

      int thumb_y = track_top;
      if (scrollable > 0 && track_h > thumb_h) {
        float frac = 1.0f - ((float)scroll_offset / (float)scrollable);
        thumb_y = track_top + Q_rint(frac * (float)(track_h - thumb_h));
      }

      color_t track_color = COLOR_SETA_U8(COLOR_RGB(255, 255, 255), 60);
      color_t thumb_color = COLOR_SETA_U8(COLOR_RGB(255, 255, 255), 160);

      R_DrawFill32(bar_x, track_top, bar_w, track_h, track_color);
      R_DrawFill32(bar_x, thumb_y, bar_w, thumb_h, thumb_color);
    }
  }

  // draw the download bar
  bool show_download_bar = true;
  if (ui_download_active && ui_download_active->integer)
    show_download_bar = false;
  if (cls.download.current && show_download_bar) {
    char pos[16], suf[32];
    int n, j;
    bool use_legacy_bar = !con.font || Font_IsLegacy(con.font);
    char bar_left = use_legacy_bar ? '\x80' : '[';
    char bar_fill = use_legacy_bar ? '\x81' : '=';
    char bar_right = use_legacy_bar ? '\x82' : ']';
    char bar_marker = use_legacy_bar ? '\x83' : '>';

    if ((text = strrchr(cls.download.current->path, '/')) != NULL)
      text++;
    else
      text = cls.download.current->path;

    Com_FormatSizeLong(pos, sizeof(pos), cls.download.position);
    n = 4 +
        Q_scnprintf(suf, sizeof(suf), " %d%% (%s)", cls.download.percent, pos);

    // figure out width
    x = con.linewidth;
    y = x - strlen(text) - n;
    i = x / 3;
    if (strlen(text) > i) {
      y = x - i - n - 3;
      memcpy(buffer, text, i);
      buffer[i] = 0;
      Q_strlcat(buffer, "...", sizeof(buffer));
    } else {
      Q_strlcpy(buffer, text, sizeof(buffer));
    }
    Q_strlcat(buffer, ": ", sizeof(buffer));
    i = strlen(buffer);
    buffer[i++] = bar_left;
    // where's the dot go?
    n = y * cls.download.percent / 100;
    for (j = 0; j < y; j++) {
      if (j == n) {
        buffer[i++] = bar_marker;
      } else {
        buffer[i++] = bar_fill;
      }
    }
    buffer[i++] = bar_right;
    buffer[i] = 0;

    Q_strlcat(buffer, suf, sizeof(buffer));

    // draw it
    y = output_bottom_y + char_height;
    Con_DrawString(char_width, y, 0, con.linewidth, buffer, COLOR_WHITE);
  } else if (cls.state == ca_loading) {
    // draw loading state
    switch (con.loadstate) {
    case LOAD_MAP:
      text = cl.configstrings[cl.csr.models + 1];
      break;
    case LOAD_MODELS:
      text = "models";
      break;
    case LOAD_IMAGES:
      text = "images";
      break;
    case LOAD_CLIENTS:
      text = "clients";
      break;
    case LOAD_SOUNDS:
      text = "sounds";
      break;
    default:
      text = NULL;
      break;
    }

    if (text) {
      Q_snprintf(buffer, sizeof(buffer), "Loading %s...", text);

      // draw it
      y = output_bottom_y + char_height;
      Con_DrawString(char_width, y, 0, con.linewidth, buffer, COLOR_WHITE);
    }
  }

  // draw the input prompt, user text, and cursor if desired
  x = 0;
  if (cls.key_dest & KEY_CONSOLE) {
    y = input_y;

    // draw command prompt
    i = con.mode == CON_REMOTE ? '#' : '>';
    if (con.font) {
      char prompt[2] = {(char)i, 0};
      Con_DrawString(char_width, y, 0, 1, prompt, COLOR_YELLOW);
    } else {
      R_DrawStretchChar(char_width, y, char_width, char_height, 0, i,
                        COLOR_YELLOW, con.charsetImage);
    }

    // draw input line
    x = Con_DrawInputField(&con.prompt.inputLine, 2 * char_width, y,
                           UI_DRAWCURSOR, con.prompt.inputLine.visibleChars,
                           COLOR_WHITE);
  }

#define APP_VERSION APPLICATION " " VERSION
  int ver_width = (int)(sizeof(APP_VERSION) * char_width);

  y = output_bottom_y;
  row = 0;
  // shift version upwards to prevent overdraw
  if (widths[0] > right_edge - ver_width - char_width) {
    y -= char_height;
    row++;
  }

  // draw clock
  color_t info_color = COLOR_SETA_U8(COLOR_RGB(255, 200, 64), color.a);
  if (con_clock->integer) {
    x = (Com_Time_m(buffer, sizeof(buffer)) + 1) * char_width;
    if (widths[row] + x + char_width <= right_edge) {
      int clock_width = Con_MeasureString(buffer, MAX_STRING_CHARS);
      Con_DrawString(right_edge - clock_width, y, 0,
                     MAX_STRING_CHARS, buffer, info_color);
  }
  }

  // draw version
  if (!row || widths[0] + ver_width + char_width <= right_edge) {
    int version_width = Con_MeasureString(APP_VERSION, MAX_STRING_CHARS);
    Con_DrawString(right_edge - version_width, y, 0,
                   MAX_STRING_CHARS, APP_VERSION, info_color);
  }
}

//=============================================================================

/*
==================
Con_RunConsole

Scroll it up or down
==================
*/
void Con_RunConsole(void) {
  if (cls.disable_screen) {
    con.destHeight = con.currentHeight = 0;
    return;
  }

  if (!(cls.key_dest & KEY_MENU)) {
    if (cls.state == ca_disconnected) {
      // draw fullscreen console
      con.destHeight = con.currentHeight = 1;
      return;
    }
    if (cls.state > ca_disconnected && cls.state < ca_active) {
      // draw half-screen console
      con.destHeight = con.currentHeight = 0.5f;
      return;
    }
  }

  // decide on the height of the console
  if (cls.key_dest & KEY_CONSOLE) {
    con.destHeight = Cvar_ClampValue(con_height, 0.1f, 1);
  } else {
    con.destHeight = 0; // none visible
  }

  if (con_speed->value <= 0) {
    con.currentHeight = con.destHeight;
    return;
  }

  CL_AdvanceValue(&con.currentHeight, con.destHeight, con_speed->value);
}

/*
==================
SCR_DrawConsole
==================
*/
void Con_DrawConsole(void) {
  R_SetScale(con.scale);
  Con_DrawSolidConsole();
  R_SetScale(1.0f);
}

/*
==============================================================================

            LINE TYPING INTO THE CONSOLE AND COMMAND COMPLETION

==============================================================================
*/

static void Con_Say(const char *msg) {
  CL_ClientCommand(
      va("say%s \"%s\"", con.chat == CHAT_TEAM ? "_team" : "", msg));
}

// don't close console after connecting
static void Con_InteractiveMode(void) {
  if (con.mode == CON_POPUP) {
    con.mode = CON_DEFAULT;
  }
}

static void Con_Action(void) {
  const char *cmd = Prompt_Action(&con.prompt);

  Con_InteractiveMode();

  if (!cmd) {
    Con_Printf("]\n");
    return;
  }

  // backslash text are commands, else chat
  int backslash = cmd[0] == '\\' || cmd[0] == '/';

  if (con.mode == CON_REMOTE) {
    CL_SendRcon(&con.remoteAddress, con.remotePassword, cmd + backslash);
  } else {
    if (!backslash && cls.state == ca_active) {
      switch (con_auto_chat->integer) {
      case CHAT_DEFAULT:
        Cbuf_AddText(&cmd_buffer, "cmd say ");
        break;
      case CHAT_TEAM:
        Cbuf_AddText(&cmd_buffer, "cmd say_team ");
        break;
      }
    }
    Cbuf_AddText(&cmd_buffer, cmd + backslash);
    Cbuf_AddText(&cmd_buffer, "\n");
  }

  Con_Printf("]%s\n", cmd);

  if (cls.state == ca_disconnected) {
    // force an update, because the command may take some time
    SCR_UpdateScreen();
  }
}

static void Con_Paste(char *(*func)(void)) {
  char *cbd, *s;

  Con_InteractiveMode();

  if (!func || !(cbd = func())) {
    return;
  }

  s = cbd;
  while (*s) {
    int c = *s++;
    switch (c) {
    case '\n':
      if (*s) {
        Con_Action();
      }
      break;
    case '\r':
    case '\t':
      IF_CharEvent(&con.prompt.inputLine, ' ');
      break;
    default:
      if (!Q_isprint(c)) {
        c = '?';
      }
      IF_CharEvent(&con.prompt.inputLine, c);
      break;
    }
  }

  Z_Free(cbd);
}

// console lines are not necessarily NUL-terminated
static void Con_ClearLine(char *buf, int row) {
  const consoleLine_t *line = &con.text[row & CON_TOTALLINES_MASK];
  const char *s = line->text + line->ts_len;
  int w = con.linewidth - line->ts_len;

  while (w-- > 0 && *s)
    *buf++ = *s++ & 127;
  *buf = 0;
}

static void Con_SearchUp(void) {
  char buf[CON_LINEWIDTH + 1];
  const char *s = con.prompt.inputLine.text;
  int top = con.current - CON_TOTALLINES + 1;

  if (top < 0)
    top = 0;

  if (!*s)
    return;

  for (int row = con.display - 1; row >= top; row--) {
    Con_ClearLine(buf, row);
    if (Q_stristr(buf, s)) {
      con.display = row;
      break;
    }
  }
}

static void Con_SearchDown(void) {
  char buf[CON_LINEWIDTH + 1];
  const char *s = con.prompt.inputLine.text;

  if (!*s)
    return;

  for (int row = con.display + 1; row <= con.current; row++) {
    Con_ClearLine(buf, row);
    if (Q_stristr(buf, s)) {
      con.display = row;
      break;
    }
  }
}

/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
void Key_Console(int key) {
  if (key == 'l' && Key_IsDown(K_CTRL)) {
    Con_Clear_f();
    return;
  }

  if (key == 'd' && Key_IsDown(K_CTRL)) {
    con.mode = CON_DEFAULT;
    return;
  }

  if (key == K_ENTER || key == K_KP_ENTER) {
    Con_Action();
    goto scroll;
  }

  if (key == 'v' && Key_IsDown(K_CTRL)) {
    if (vid)
      Con_Paste(vid->get_clipboard_data);
    goto scroll;
  }

  if ((key == K_INS && Key_IsDown(K_SHIFT)) || key == K_MOUSE3) {
    if (vid)
      Con_Paste(vid->get_selection_data);
    goto scroll;
  }

  if (key == K_TAB) {
    if (con_timestamps->integer)
      Con_CheckResize();
    Prompt_CompleteCommand(&con.prompt, true);
    goto scroll;
  }

  if (key == 'r' && Key_IsDown(K_CTRL)) {
    Prompt_CompleteHistory(&con.prompt, false);
    goto scroll;
  }

  if (key == 's' && Key_IsDown(K_CTRL)) {
    Prompt_CompleteHistory(&con.prompt, true);
    goto scroll;
  }

  if (key == K_UPARROW && Key_IsDown(K_CTRL)) {
    Con_SearchUp();
    return;
  }

  if (key == K_DOWNARROW && Key_IsDown(K_CTRL)) {
    Con_SearchDown();
    return;
  }

  if (key == K_UPARROW || (key == 'p' && Key_IsDown(K_CTRL))) {
    Prompt_HistoryUp(&con.prompt);
    goto scroll;
  }

  if (key == K_DOWNARROW || (key == 'n' && Key_IsDown(K_CTRL))) {
    Prompt_HistoryDown(&con.prompt);
    goto scroll;
  }

  if (key == K_PGUP || key == K_MWHEELUP) {
    if (Key_IsDown(K_CTRL)) {
      con.display -= 6;
    } else {
      con.display -= 2;
    }
    Con_CheckTop();
    return;
  }

  if (key == K_PGDN || key == K_MWHEELDOWN) {
    if (Key_IsDown(K_CTRL)) {
      con.display += 6;
    } else {
      con.display += 2;
    }
    if (con.display > con.current) {
      con.display = con.current;
    }
    return;
  } else if (key == K_END) {
    con.display = con.current;
    return;
  }

  if (key == K_HOME && Key_IsDown(K_CTRL)) {
    con.display = 0;
    Con_CheckTop();
    return;
  }

  if (key == K_END && Key_IsDown(K_CTRL)) {
    con.display = con.current;
    return;
  }

  if (IF_KeyEvent(&con.prompt.inputLine, key)) {
    Prompt_ClearState(&con.prompt);
    Con_InteractiveMode();
  }

scroll:
  if (con_scroll->integer & 1) {
    con.display = con.current;
  }
}

void Char_Console(int key) {
  if (IF_CharEvent(&con.prompt.inputLine, key)) {
    Con_InteractiveMode();
  }
}

/*
====================
Key_Message
====================
*/
void Key_Message(int key) {
  if (key == 'l' && Key_IsDown(K_CTRL)) {
    IF_Clear(&con.chatPrompt.inputLine);
    return;
  }

  if (key == K_MWHEELUP) {
    SCR_NotifyScrollLines(1.0f);
    return;
  }

  if (key == K_MWHEELDOWN) {
    SCR_NotifyScrollLines(-1.0f);
    return;
  }

  if (key == K_MOUSE1) {
    SCR_NotifyMouseDown(key);
    return;
  }

  if (key == K_ENTER || key == K_KP_ENTER) {
    const char *cmd = Prompt_Action(&con.chatPrompt);

    if (cmd) {
      Con_Say(cmd);
    }
    Key_SetDest(static_cast<keydest_t>(cls.key_dest & ~KEY_MESSAGE));
    return;
  }

  if (key == K_ESCAPE) {
    Key_SetDest(static_cast<keydest_t>(cls.key_dest & ~KEY_MESSAGE));
    IF_Clear(&con.chatPrompt.inputLine);
    return;
  }

  if (key == 'r' && Key_IsDown(K_CTRL)) {
    Prompt_CompleteHistory(&con.chatPrompt, false);
    return;
  }

  if (key == 's' && Key_IsDown(K_CTRL)) {
    Prompt_CompleteHistory(&con.chatPrompt, true);
    return;
  }

  if (key == K_UPARROW || (key == 'p' && Key_IsDown(K_CTRL))) {
    Prompt_HistoryUp(&con.chatPrompt);
    return;
  }

  if (key == K_DOWNARROW || (key == 'n' && Key_IsDown(K_CTRL))) {
    Prompt_HistoryDown(&con.chatPrompt);
    return;
  }

  if (IF_KeyEvent(&con.chatPrompt.inputLine, key)) {
    Prompt_ClearState(&con.chatPrompt);
  }
}

void Char_Message(int key) { IF_CharEvent(&con.chatPrompt.inputLine, key); }


