#include "renderer.h"

namespace {
    refresh::vk::VulkanRenderer g_renderer;
}

bool R_Init(bool total) {
    return g_renderer.init(total);
}

void R_Shutdown(bool total) {
    g_renderer.shutdown(total);
}

void R_BeginRegistration(const char *map) {
    g_renderer.beginRegistration(map);
}

qhandle_t R_RegisterModel(const char *name) {
    return g_renderer.registerModel(name);
}

qhandle_t R_RegisterImage(const char *name, imagetype_t type, imageflags_t flags) {
    return g_renderer.registerImage(name, type, flags);
}

void R_SetSky(const char *name, float rotate, bool autorotate, const vec3_t axis) {
    g_renderer.setSky(name, rotate, autorotate, axis);
}

void R_EndRegistration(void) {
    g_renderer.endRegistration();
}

void R_RenderFrame(const refdef_t *fd) {
    g_renderer.renderFrame(fd);
}

void R_LightPoint(const vec3_t origin, vec3_t light) {
    g_renderer.lightPoint(origin, light);
}

void R_SetClipRect(const clipRect_t *clip) {
    g_renderer.setClipRect(clip);
}

float R_ClampScale(cvar_t *var) {
    return g_renderer.clampScale(var);
}

void R_SetScale(float scale) {
    g_renderer.setScale(scale);
}

int get_auto_scale(void) {
    return g_renderer.autoScale();
}

void R_DrawChar(int x, int y, int flags, int ch, color_t color, qhandle_t font) {
    g_renderer.drawChar(x, y, flags, ch, color, font);
}

void R_DrawStretchChar(int x, int y, int w, int h, int flags, int ch, color_t color, qhandle_t font) {
    g_renderer.drawStretchChar(x, y, w, h, flags, ch, color, font);
}

int R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxChars,
                        const char *string, color_t color, qhandle_t font) {
    return g_renderer.drawStringStretch(x, y, scale, flags, maxChars, string, color, font);
}

int R_DrawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont) {
    return g_renderer.drawKFontChar(x, y, scale, flags, codepoint, color, kfont);
}

const kfont_char_t *SCR_KFontLookup(const kfont_t *kfont, uint32_t codepoint) {
    return g_renderer.lookupKFontChar(kfont, codepoint);
}

void SCR_LoadKFont(kfont_t *font, const char *filename) {
    g_renderer.loadKFont(font, filename);
}

bool R_GetPicSize(int *w, int *h, qhandle_t pic) {
    return g_renderer.getPicSize(w, h, pic);
}

void R_DrawPic(int x, int y, color_t color, qhandle_t pic) {
    g_renderer.drawPic(x, y, color, pic);
}

void R_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic) {
    g_renderer.drawStretchPic(x, y, w, h, color, pic);
}

void R_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle, int pivot_x, int pivot_y, qhandle_t pic) {
    g_renderer.drawStretchRotatePic(x, y, w, h, color, angle, pivot_x, pivot_y, pic);
}

void R_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic) {
    g_renderer.drawKeepAspectPic(x, y, w, h, color, pic);
}

void R_DrawStretchRaw(int x, int y, int w, int h) {
    g_renderer.drawStretchRaw(x, y, w, h);
}

void R_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic) {
    g_renderer.updateRawPic(pic_w, pic_h, pic);
}

void R_TileClear(int x, int y, int w, int h, qhandle_t pic) {
    g_renderer.tileClear(x, y, w, h, pic);
}

void R_DrawFill8(int x, int y, int w, int h, int c) {
    g_renderer.drawFill8(x, y, w, h, c);
}

void R_DrawFill32(int x, int y, int w, int h, color_t color) {
    g_renderer.drawFill32(x, y, w, h, color);
}

void R_BeginFrame(void) {
    g_renderer.beginFrame();
}

void R_EndFrame(void) {
    g_renderer.endFrame();
}

void R_ModeChanged(int width, int height, int flags) {
    g_renderer.modeChanged(width, height, flags);
}

bool R_VideoSync(void) {
    return g_renderer.videoSync();
}

void GL_ExpireDebugObjects(void) {
    g_renderer.expireDebugObjects();
}

bool R_SupportsPerPixelLighting(void) {
    return g_renderer.supportsPerPixelLighting();
}

r_opengl_config_t R_GetGLConfig(void) {
    return g_renderer.getGLConfig();
}
