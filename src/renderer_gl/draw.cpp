/*
Copyright (C) 2003-2006 Andrey Nazarov

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

#include "gl.h"
#include "renderer/common.h"
#include "renderer/kfont.h"
#include <array>
#include <cstring>

drawStatic_t draw;

// the final process in drawing any pic
static inline void GL_DrawPic(
    vec2_t vertices[4], vec2_t texcoords[4],
    color_t color, int texnum, int flags)
{
    glVertexDesc2D_t *dst_vert;
    glIndex_t *dst_indices;

    if (tess.numverts + 4 > TESS_MAX_VERTICES ||
        tess.numindices + 6 > TESS_MAX_INDICES ||
        (tess.numverts && tess.texnum[TMU_TEXTURE] != texnum))
        GL_Flush2D();

    tess.texnum[TMU_TEXTURE] = texnum;

    dst_vert = ((glVertexDesc2D_t *) tess.vertices) + tess.numverts;

    for (int i = 0; i < 4; i++, dst_vert++) {
        Vector2Copy(vertices[i], dst_vert->xy);
        Vector2Copy(texcoords[i], dst_vert->st);
        dst_vert->c = color.u32;
    }

    dst_indices = tess.indices + tess.numindices;
    dst_indices[0] = tess.numverts + 0;
    dst_indices[1] = tess.numverts + 2;
    dst_indices[2] = tess.numverts + 3;
    dst_indices[3] = tess.numverts + 0;
    dst_indices[4] = tess.numverts + 1;
    dst_indices[5] = tess.numverts + 2;

    if (flags & IF_TRANSPARENT) {
        if ((flags & IF_PALETTED) && draw.scale == 1)
            tess.flags |= GLS_ALPHATEST_ENABLE;
        else
            tess.flags |= GLS_BLEND_BLEND;
    }

    if (color.a != 255)
        tess.flags |= GLS_BLEND_BLEND;

    tess.numverts += 4;
    tess.numindices += 6;
}

static inline void GL_StretchPic_(
    float x, float y, float w, float h,
    float s1, float t1, float s2, float t2,
    color_t color, int texnum, int flags)
{
    std::array<vec2_t, 4> vertices{};
    std::array<vec2_t, 4> texcoords{};

    Vector2Set(vertices[0], x,     y    );
    Vector2Set(vertices[1], x + w, y    );
    Vector2Set(vertices[2], x + w, y + h);
    Vector2Set(vertices[3], x,     y + h);

    Vector2Set(texcoords[0], s1, t1);
    Vector2Set(texcoords[1], s2, t1);
    Vector2Set(texcoords[2], s2, t2);
    Vector2Set(texcoords[3], s1, t2);

    GL_DrawPic(vertices.data(), texcoords.data(), color, texnum, flags);
}

#define GL_StretchPic(x,y,w,h,s1,t1,s2,t2,color,image) \
    GL_StretchPic_(x,y,w,h,s1,t1,s2,t2,color,(image)->texnum,(image)->flags)

static inline void GL_StretchRotatePic_(
    float x, float y, float w, float h,
    float s1, float t1, float s2, float t2,
    float angle, float pivot_x, float pivot_y,
    color_t color, int texnum, int flags)
{
    std::array<vec2_t, 4> vertices{};
    std::array<vec2_t, 4> texcoords{};

    float hw = w / 2.0f;
    float hh = h / 2.0f;

    Vector2Set(vertices[0], -hw + pivot_x, -hh + pivot_y);
    Vector2Set(vertices[1],  hw + pivot_x, -hh + pivot_y);
    Vector2Set(vertices[2],  hw + pivot_x,  hh + pivot_y);
    Vector2Set(vertices[3], -hw + pivot_x,  hh + pivot_y);

    Vector2Set(texcoords[0], s1, t1);
    Vector2Set(texcoords[1], s2, t1);
    Vector2Set(texcoords[2], s2, t2);
    Vector2Set(texcoords[3], s1, t2);

    float s = sinf(angle);
    float c = cosf(angle);

    for (int i = 0; i < 4; i++) {
        float vert_x = vertices[i][0];
        float vert_y = vertices[i][1];
        
        vertices[i][0] = (vert_x * c - vert_y * s) + x;
        vertices[i][1] = (vert_x * s + vert_y * c) + y;
    }

    GL_DrawPic(vertices.data(), texcoords.data(), color, texnum, flags);
}

#define GL_StretchRotatePic(x,y,w,h,s1,t1,s2,t2,angle,px,py,color,image) \
    GL_StretchRotatePic_(x,y,w,h,s1,t1,s2,t2,angle,px,py,color,(image)->texnum,(image)->flags)

static void GL_DrawVignette(float frac, color_t outer, color_t inner)
{
    static const byte indices[24] = {
        0, 5, 4, 0, 1, 5, 1, 6, 5, 1, 2, 6, 6, 2, 3, 6, 3, 7, 0, 7, 3, 0, 4, 7
    };
    vec_t *dst_vert;
    glIndex_t *dst_indices;

    if (tess.numverts + 8 > TESS_MAX_VERTICES ||
        tess.numindices + 24 > TESS_MAX_INDICES ||
        (tess.numverts && tess.texnum[TMU_TEXTURE] != TEXNUM_WHITE))
        GL_Flush2D();

    tess.texnum[TMU_TEXTURE] = TEXNUM_WHITE;

    int x = 0, y = 0;
    int w = glr.fd.width, h = glr.fd.height;
    int distance = min(w, h) * frac;

    // outer vertices
    dst_vert = tess.vertices + tess.numverts * 5;
    Vector4Set(dst_vert,      x,     y,     0, 0);
    Vector4Set(dst_vert +  5, x + w, y,     0, 0);
    Vector4Set(dst_vert + 10, x + w, y + h, 0, 0);
    Vector4Set(dst_vert + 15, x,     y + h, 0, 0);

    WN32(dst_vert +  4, outer.u32);
    WN32(dst_vert +  9, outer.u32);
    WN32(dst_vert + 14, outer.u32);
    WN32(dst_vert + 19, outer.u32);

    // inner vertices
    x += distance;
    y += distance;
    w -= distance * 2;
    h -= distance * 2;

    dst_vert += 20;
    Vector4Set(dst_vert,      x,     y,     0, 0);
    Vector4Set(dst_vert +  5, x + w, y,     0, 0);
    Vector4Set(dst_vert + 10, x + w, y + h, 0, 0);
    Vector4Set(dst_vert + 15, x,     y + h, 0, 0);

    WN32(dst_vert +  4, inner.u32);
    WN32(dst_vert +  9, inner.u32);
    WN32(dst_vert + 14, inner.u32);
    WN32(dst_vert + 19, inner.u32);

    /*
    0             1
        4     5

        7     6
    3             2
    */

    dst_indices = tess.indices + tess.numindices;
    for (int i = 0; i < 24; i++)
        dst_indices[i] = tess.numverts + indices[i];

    tess.flags |= GLS_BLEND_BLEND | GLS_SHADE_SMOOTH;

    tess.numverts += 8;
    tess.numindices += 24;
}

void GL_Blend(void)
{
    if (glr.fd.screen_blend[3]) {
        color_t color;

        color.r = glr.fd.screen_blend[0] * 255;
        color.g = glr.fd.screen_blend[1] * 255;
        color.b = glr.fd.screen_blend[2] * 255;
        color.a = glr.fd.screen_blend[3] * 255;

        GL_StretchPic_(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, 0, 0, 1, 1,
                       color, TEXNUM_WHITE, 0);
    }

    if (glr.fd.damage_blend[3]) {
        color_t outer, inner;

        outer.r = glr.fd.damage_blend[0] * 255;
        outer.g = glr.fd.damage_blend[1] * 255;
        outer.b = glr.fd.damage_blend[2] * 255;
        outer.a = glr.fd.damage_blend[3] * 255;

        inner = ColorSetAlpha(outer, 0);

        if (gl_damageblend_frac->value > 0)
            GL_DrawVignette(Cvar_ClampValue(gl_damageblend_frac, 0, 0.5f), outer, inner);
        else
            GL_StretchPic_(glr.fd.x, glr.fd.y, glr.fd.width, glr.fd.height, 0, 0, 1, 1,
                           outer, TEXNUM_WHITE, 0);
    }
}

void R_SetClipRect(const clipRect_t *clip)
{
    clipRect_t rc;
    float scale;

    GL_Flush2D();

    if (!clip) {
clear:
        if (draw.scissor) {
            qglDisable(GL_SCISSOR_TEST);
            draw.scissor = false;
        }
        return;
    }

    scale = 1 / draw.scale;

    rc.left = clip->left * scale;
    rc.top = clip->top * scale;
    rc.right = clip->right * scale;
    rc.bottom = clip->bottom * scale;

    if (rc.left < 0)
        rc.left = 0;
    if (rc.top < 0)
        rc.top = 0;
    if (rc.right > r_config.width)
        rc.right = r_config.width;
    if (rc.bottom > r_config.height)
        rc.bottom = r_config.height;
    if (rc.right < rc.left)
        goto clear;
    if (rc.bottom < rc.top)
        goto clear;

    qglEnable(GL_SCISSOR_TEST);
    qglScissor(rc.left, r_config.height - rc.bottom,
               rc.right - rc.left, rc.bottom - rc.top);
    draw.scissor = true;
}

int get_auto_scale(void)
{
    int (*dpiScaleFn)() = nullptr;
    if (vid && vid->get_dpi_scale) {
        dpiScaleFn = vid->get_dpi_scale;
    }

    return Renderer_ComputeAutoScale(r_config, dpiScaleFn);
}

float R_ClampScale(cvar_t *var)
{
    if (!var)
        return 1.0f;

    if (var->value)
        return 1.0f / Cvar_ClampValue(var, 1.0f, 6.0f);

    const int autoScale = get_auto_scale();
    if (!autoScale)
        return 1.0f;

    return 1.0f / static_cast<float>(autoScale);
}

void R_SetScale(float scale)
{
    if (draw.scale == scale)
        return;

    GL_Flush2D();

    GL_Ortho(0, Q_rint(r_config.width * scale),
             Q_rint(r_config.height * scale), 0, -1, 1);

    draw.scale = scale;
}

void R_DrawStretchPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, w, h, image->sl, image->tl, image->sh, image->th,
                  color, image);
}

void R_DrawStretchRotatePic(int x, int y, int w, int h, color_t color, float angle, int pivot_x, int pivot_y, qhandle_t pic)
{
    image_t *image = IMG_ForHandle(pic);

    GL_StretchRotatePic(x, y, w, h, image->sl, image->tl, image->sh, image->th,
                        angle, pivot_x, pivot_y, color, image);
}

void R_DrawKeepAspectPic(int x, int y, int w, int h, color_t color, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    if (image->flags & IF_SCRAP) {
        R_DrawStretchPic(x, y, w, h, color, pic);
        return;
    }

    float scale_w = w;
    float scale_h = h * image->aspect;
    float scale = max(scale_w, scale_h);

    float s = (1.0f - scale_w / scale) * 0.5f;
    float t = (1.0f - scale_h / scale) * 0.5f;

    GL_StretchPic(x, y, w, h, s, t, 1.0f - s, 1.0f - t, color, image);
}

void R_DrawPic(int x, int y, color_t color, qhandle_t pic)
{
    const image_t *image = IMG_ForHandle(pic);

    GL_StretchPic(x, y, image->width, image->height,
                  image->sl, image->tl, image->sh, image->th, color, image);
}

void R_DrawStretchRaw(int x, int y, int w, int h)
{
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, COLOR_WHITE, TEXNUM_RAW, 0);
}

void R_UpdateRawPic(int pic_w, int pic_h, const uint32_t *pic)
{
    GL_ForceTexture(TMU_TEXTURE, TEXNUM_RAW);
    qglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, pic_w, pic_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pic);
}

#define DIV64 (1.0f / 64.0f)

void R_TileClear(int x, int y, int w, int h, qhandle_t pic)
{
    GL_StretchPic(x, y, w, h, x * DIV64, y * DIV64,
                  (x + w) * DIV64, (y + h) * DIV64, COLOR_WHITE, IMG_ForHandle(pic));
}

void R_DrawFill8(int x, int y, int w, int h, int c)
{
    if (!w || !h)
        return;
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, ColorU32(d_8to24table[c & 0xff]), TEXNUM_WHITE, 0);
}

void R_DrawFill32(int x, int y, int w, int h, color_t color)
{
    if (!w || !h)
        return;
    GL_StretchPic_(x, y, w, h, 0, 0, 1, 1, color, TEXNUM_WHITE, 0);
}

static inline void draw_char(int x, int y, int w, int h, int flags, int c, color_t color, const image_t *image)
{
    float s, t;

    if ((c & 127) == 32)
        return;

    if (flags & UI_ALTCOLOR)
        c |= 0x80;

    if (flags & UI_XORCOLOR)
        c ^= 0x80;

    s = (c & 15) * 0.0625f;
    t = (c >> 4) * 0.0625f;

    if (flags & UI_DROPSHADOW && c != 0x83) {
        color_t black = ColorA(color.a);

        GL_StretchPic(x + 1, y + 1, w, h, s, t,
                      s + 0.0625f, t + 0.0625f, black, image);

        if (gl_fontshadow->integer > 1)
            GL_StretchPic(x + 2, y + 2, w, h, s, t,
                          s + 0.0625f, t + 0.0625f, black, image);
    }

    if (c >> 7)
        color = ColorSetAlpha(COLOR_WHITE, color.a);

    GL_StretchPic(x, y, w, h, s, t,
                  s + 0.0625f, t + 0.0625f, color, image);
}

void R_DrawChar(int x, int y, int flags, int c, color_t color, qhandle_t font)
{
    if (gl_fontshadow->integer > 0)
        flags |= UI_DROPSHADOW;

    draw_char(x, y, CONCHAR_WIDTH, CONCHAR_HEIGHT, flags, c & 255, color, IMG_ForHandle(font));
}

void R_DrawStretchChar(int x, int y, int w, int h, int flags, int c, color_t color, qhandle_t font)
{
    draw_char(x, y, w, h, flags, c & 255, color, IMG_ForHandle(font));
}

int R_DrawStringStretch(int x, int y, int scale, int flags, size_t maxlen, const char *s, color_t color, qhandle_t font)
{
    const image_t *image = IMG_ForHandle(font);

    if (gl_fontshadow->integer > 0)
        flags |= UI_DROPSHADOW;

    int sx = x;

    while (maxlen-- && *s) {
        byte c = *s++;

        if ((flags & UI_MULTILINE) && c == '\n') {
            y += CONCHAR_HEIGHT * scale + (1.0 / draw.scale);
            x = sx;
            continue;
        }

        draw_char(x, y, CONCHAR_WIDTH * scale, CONCHAR_HEIGHT * scale, flags, c, color, image);
        x += CONCHAR_WIDTH * scale;
    }

    return x;
}

static inline int draw_kfont_char(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont)
{
    const kfont_char_t *ch = SCR_KFontLookup(kfont, codepoint);

    if (!ch)
        return 0;
    
    image_t *image = IMG_ForHandle(kfont->pic);

    float s = ch->x * kfont->sw;
    float t = ch->y * kfont->sh;
    
    float sw = ch->w * kfont->sw;
    float sh = ch->h * kfont->sh;

    int w = ch->w * scale;
    int h = ch->h * scale;

    int shadow_offset = 0;

    if ((flags & UI_DROPSHADOW) || gl_fontshadow->integer > 0) {
        shadow_offset = (1 * scale);
        
        color_t black = ColorA(color.a);

        GL_StretchPic(x + shadow_offset, y + shadow_offset, w, h, s, t,
                      s + sw, t + sh, black, image);

        if (gl_fontshadow->integer > 1)
            GL_StretchPic(x + (shadow_offset * 2), y + (shadow_offset * 2), w, h, s, t,
                          s + sw, t + sh, black, image);
    }

    GL_StretchPic(x, y, w, h, s, t,
                  s + sw, t + sh, color, image);

    return ch->w * scale;
}

int R_DrawKFontChar(int x, int y, int scale, int flags, uint32_t codepoint, color_t color, const kfont_t *kfont)
{
    return draw_kfont_char(x, y, scale, flags, codepoint, color, kfont);
}

const kfont_char_t *SCR_KFontLookup(const kfont_t *kfont, uint32_t codepoint)
{
    return Renderer_LookupKFontGlyph(kfont, codepoint);
}

void SCR_LoadKFont(kfont_t *font, const char *filename)
{
    if (!font)
        return;

    RendererKFontLoadContext context{};
    context.registerImage = [](void *, const char *path, imagetype_t type, imageflags_t flags) -> qhandle_t {
        return R_RegisterImage(path, type, flags);
    };

    RendererKFontData data{};
    if (!Renderer_LoadKFont(filename, context, &data)) {
        std::memset(font, 0, sizeof(*font));
        return;
    }

    Renderer_AssignKFont(font, data);
}

qhandle_t r_charset;

#if USE_DEBUG

void Draw_Lightmaps(void)
{
    int block = lm.block_size;
    int rows = 0, cols = 0;

    while (block) {
        rows = max(r_config.height / block, 1);
        cols = max(lm.nummaps / rows, 1);
        if (cols * block <= r_config.width)
            break;
        block >>= 1;
    }

    for (int i = 0; i < cols; i++) {
        for (int j = 0; j < rows; j++) {
            int k = j * cols + i;
            if (k < lm.nummaps)
                GL_StretchPic_(block * i, block * j, block, block,
                               0, 0, 1, 1, COLOR_WHITE, lm.texnums[k], 0);
        }
    }
}

void Draw_Scrap(void)
{
    GL_StretchPic_(0, 0, 256, 256,
                   0, 0, 1, 1, COLOR_WHITE, TEXNUM_SCRAP, IF_PALETTED | IF_TRANSPARENT);
}

#endif
