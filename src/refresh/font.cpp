#include "gl.hpp"
#include "font_freetype.hpp"
#include "refresh/text.hpp"
#include "gl_draw_utils.hpp"
#include "common/q3colors.hpp"
#include "common/utf8.hpp"
#include "refresh/refresh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

extern drawStatic_t draw;

namespace {

static constexpr int GLYPH_PADDING = 2;
static constexpr int MAX_ATLASES = 8;

static inline bool is_newline(uint32_t codepoint) noexcept
{
    return codepoint == '\n';
}

static inline float clamp_scale(int scale) noexcept
{
    return static_cast<float>((std::max)(scale, 1));
}

static inline float resolve_dpi(const text_render_request_t &req) noexcept
{
    if (req.dpi_scale > 0.0f)
        return req.dpi_scale;
    if (draw.scale > 0.0f)
        return draw.scale;
#ifdef _WIN32
    // Try to use system DPI on Windows if available
    UINT dpi = 0;
    HDC hdc = GetDC(nullptr);
    if (hdc) {
        dpi = static_cast<UINT>(GetDeviceCaps(hdc, LOGPIXELSY));
        ReleaseDC(nullptr, hdc);
    }
    if (dpi > 0) {
        const float normalized = dpi / 96.0f;
        if (normalized > 0.0f)
            return normalized;
    }
#endif

    // Fallback heuristic based on render resolution (avoid tiny text on high DPI).
    const float refW = 1920.0f;
    const float refH = 1080.0f;
    const float w = static_cast<float>(r_config.width);
    const float h = static_cast<float>(r_config.height);
    const float scaleW = (refW > 0.0f) ? (w / refW) : 1.0f;
    const float scaleH = (refH > 0.0f) ? (h / refH) : 1.0f;
    const float heuristic = (std::max)(1.0f, (std::max)(scaleW, scaleH));
    return heuristic;
}

static inline text_style_t style_from_flags(int flags, const text_render_request_t &req)
{
    text_style_t style = req.style;
    style.allow_color_codes = style.allow_color_codes && !(flags & UI_IGNORECOLOR);

    if (flags & UI_BOLD)
        style.bold = true;
    if (flags & UI_ITALIC)
        style.italic = true;
    if (flags & UI_UNDERLINE)
        style.underline = true;
    if (flags & UI_OUTLINE && style.outline_thickness <= 0.0f) {
        style.outline_thickness = clamp_scale(req.scale) * 0.6f;
        style.outline_color = ColorA(req.base_color.a);
    }

    if ((flags & UI_DROPSHADOW) && style.shadow.offset_x == 0.0f && style.shadow.offset_y == 0.0f) {
        style.shadow.offset_x = clamp_scale(req.scale);
        style.shadow.offset_y = clamp_scale(req.scale);
        style.shadow.color = ColorA(req.base_color.a);
    }

    return style;
}

static inline color_t apply_color_flags(int flags, color_t color)
{
    if (flags & (UI_ALTCOLOR | UI_XORCOLOR))
        return ColorSetAlpha(COLOR_WHITE, color.a);
    return color;
}

static void draw_skewed_quad(float x, float y, float w, float h,
                             float s0, float t0, float s1, float t1,
                             float italic_skew, color_t color, GLuint texnum, int flags)
{
    std::array<vec2_t, 4> vertices{};
    std::array<vec2_t, 4> texcoords{};

    Vector2Set(vertices[0], x, y);
    Vector2Set(vertices[1], x + w, y);

    const float skew = italic_skew * h;
    Vector2Set(vertices[2], x + w + skew, y + h);
    Vector2Set(vertices[3], x + skew, y + h);

    Vector2Set(texcoords[0], s0, t0);
    Vector2Set(texcoords[1], s1, t0);
    Vector2Set(texcoords[2], s1, t1);
    Vector2Set(texcoords[3], s0, t1);

    GL_DrawPic(vertices.data(), texcoords.data(), color, static_cast<int>(texnum), flags);
}

static void draw_styled_quad(float x, float y, float w, float h,
                             float s0, float t0, float s1, float t1,
                             const text_style_t &style, color_t color,
                             GLuint texnum, int flags)
{
    if (w <= 0.0f || h <= 0.0f)
        return;

    const float italic_skew = style.italic ? 0.2f : 0.0f;
    const float outline = (std::max)(style.outline_thickness, 0.0f);

    if (style.shadow.offset_x != 0.0f || style.shadow.offset_y != 0.0f) {
        draw_skewed_quad(
            x + style.shadow.offset_x, y + style.shadow.offset_y, w, h,
            s0, t0, s1, t1, italic_skew, style.shadow.color, texnum, flags | IF_TRANSPARENT);
    }

    if (outline > 0.0f) {
        const std::array<std::pair<float, float>, 8> offsets{ {
            {-outline, -outline}, {0.0f, -outline}, {outline, -outline},
            {-outline, 0.0f},                     {outline, 0.0f},
            {-outline, outline},  {0.0f, outline}, {outline, outline},
        } };

        for (const auto &o : offsets) {
            draw_skewed_quad(
                x + o.first, y + o.second, w, h,
                s0, t0, s1, t1, italic_skew, style.outline_color, texnum, flags | IF_TRANSPARENT);
        }
    }

    if (style.bold) {
        const float bold_offset = (std::max)(1.0f, w * 0.075f);
        draw_skewed_quad(
            x + bold_offset, y, w, h, s0, t0, s1, t1,
            italic_skew, color, texnum, flags | IF_TRANSPARENT);
    }

    draw_skewed_quad(x, y, w, h, s0, t0, s1, t1, italic_skew, color, texnum, flags | IF_TRANSPARENT);
}

#if USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SIZES_H

struct FtAtlas {
    GLuint texnum = 0;
    int width = 0;
    int height = 0;
    int pen_x = 0;
    int pen_y = 0;
    int row_height = 0;
};

struct FtGlyph {
    int atlas_index = -1;
    float s0 = 0.0f;
    float t0 = 0.0f;
    float s1 = 0.0f;
    float t1 = 0.0f;
    int width = 0;
    int height = 0;
    int bearing_x = 0;
    int bearing_y = 0;
    int advance = 0;
    FT_UInt glyph_index = 0;
};

struct FtFontSize {
    int pixel_height = 0;
    FT_Size size = nullptr;
    int ascent = 0;
    int descent = 0;
    int line_height = 0;
    std::vector<FtAtlas> atlases;
    std::unordered_map<uint32_t, FtGlyph> glyphs;
};

struct FtFont {
    image_t *image = nullptr;
    byte *file_buffer = nullptr;
    size_t file_size = 0;
    FT_Face face = nullptr;
    std::unordered_map<int, std::unique_ptr<FtFontSize>> sizes;
};

class FreeTypeBackend {
public:
    ~FreeTypeBackend() { shutdown(); }

    void init()
    {
        if (library)
            return;
        if (FT_Init_FreeType(&library))
            library = nullptr;
    }

    void shutdown()
    {
        if (!library)
            return;
        for (auto &entry : fonts)
            destroy_font(*entry.second);
        fonts.clear();
        FT_Done_FreeType(library);
        library = nullptr;
    }

    bool register_font(image_t *image, const char *filename)
    {
        if (!library || !image || !filename)
            return false;

        auto font = std::make_unique<FtFont>();
        font->image = image;

        byte *buffer = nullptr;
        int length = FS_LoadFile(filename, reinterpret_cast<void **>(&buffer));
        if (length < 0) {
            FS_FreeFile(buffer);
            return false;
        }

        font->file_buffer = buffer;
        font->file_size = static_cast<size_t>(length);

        FT_Face face = nullptr;
        FT_Error err = FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte *>(buffer), length, 0, &face);
        if (err) {
            FS_FreeFile(buffer);
            font->file_buffer = nullptr;
            font->file_size = 0;
            return false;
        }

        FT_Error charmap_err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
        if (charmap_err) {
            for (int i = 0; i < face->num_charmaps; ++i) {
                if (!FT_Set_Charmap(face, face->charmaps[i])) {
                    charmap_err = 0;
                    break;
                }
            }
        }
        if (charmap_err) {
            FT_Done_Face(face);
            FS_FreeFile(buffer);
            font->file_buffer = nullptr;
            font->file_size = 0;
            return false;
        }

        font->face = face;

        FtFontSize *baseSize = get_font_size(*font, CONCHAR_HEIGHT * 4);
        if (!baseSize) {
            FT_Done_Face(face);
            FS_FreeFile(buffer);
            font->face = nullptr;
            font->file_buffer = nullptr;
            font->file_size = 0;
            return false;
        }

        destroy_font_for_image(image);
        fonts[image] = std::move(font);

        image->flags |= IF_TRANSPARENT;
        image->width = CONCHAR_WIDTH;
        image->height = CONCHAR_HEIGHT;
        image->upload_width = image->width;
        image->upload_height = image->height;
        image->texnum = 0;
        image->texnum2 = 0;
        image->aspect = 1.0f;
        image->sl = 0;
        image->sh = 1;
        image->tl = 0;
        image->th = 1;

        return true;
    }

    void destroy_font_for_image(image_t *image)
    {
        auto it = fonts.find(image);
        if (it == fonts.end())
            return;
        destroy_font(*it->second);
        fonts.erase(it);
    }

    bool acquire(qhandle_t handle, ftfont_t *out)
    {
        if (!out)
            return false;

        const image_t *image = IMG_ForHandle(handle);
        if (!image)
            return false;

        FtFont *ft_font = font_for_image(image);
        if (!ft_font)
            return false;

        int targetHeight = out->pixelHeight > 0 ? out->pixelHeight : CONCHAR_HEIGHT * 4;
        FtFontSize *fontSize = get_font_size(*ft_font, targetHeight);
        if (!fontSize)
            return false;

        out->driverData = ft_font;
        out->pixelHeight = fontSize->pixel_height;
        out->ascent = fontSize->ascent;
        out->descent = fontSize->descent;
        out->lineHeight = fontSize->line_height ? fontSize->line_height : fontSize->pixel_height;
        out->face = ft_font->face;
        return true;
    }

    void release(ftfont_t *font)
    {
        if (!font)
            return;
        font->driverData = nullptr;
        font->ascent = 0;
        font->descent = 0;
        font->lineHeight = 0;
        font->pixelHeight = 0;
        font->face = nullptr;
    }

    void invalidate_size(qhandle_t handle, int pixelHeight)
    {
        const image_t *image = IMG_ForHandle(handle);
        if (!image)
            return;
        FtFont *ft_font = font_for_image(image);
        if (!ft_font)
            return;
        int targetHeight = pixelHeight > 0 ? pixelHeight : CONCHAR_HEIGHT * 4;
        auto it = ft_font->sizes.find(targetHeight);
        if (it == ft_font->sizes.end())
            return;
        if (it->second)
            destroy_font_size(*it->second);
        ft_font->sizes.erase(it);
    }

    FtFont *font_for_image(const image_t *image)
    {
        auto it = fonts.find(image);
        if (it == fonts.end())
            return nullptr;
        return it->second.get();
    }

    FtFontSize *get_font_size(FtFont &font, int pixel_height)
    {
        if (pixel_height <= 0)
            pixel_height = CONCHAR_HEIGHT * 4;

        auto it = font.sizes.find(pixel_height);
        if (it != font.sizes.end())
            return it->second.get();

        if (!font.face)
            return nullptr;

        FT_Size size = nullptr;
        FT_Error err = FT_New_Size(font.face, &size);
        if (err)
            return nullptr;

        err = FT_Activate_Size(size);
        if (err) {
            FT_Done_Size(size);
            return nullptr;
        }

        err = FT_Set_Pixel_Sizes(font.face, 0, pixel_height);
        if (err) {
            FT_Done_Size(size);
            return nullptr;
        }

        auto fontSize = std::make_unique<FtFontSize>();
        fontSize->pixel_height = pixel_height;
        fontSize->size = size;
        fontSize->ascent = font.face->size->metrics.ascender >> 6;
        fontSize->descent = -(font.face->size->metrics.descender >> 6);
        fontSize->line_height = font.face->size->metrics.height >> 6;
        if (fontSize->line_height <= 0)
            fontSize->line_height = pixel_height;
        if (fontSize->ascent <= 0)
            fontSize->ascent = pixel_height - fontSize->descent;

        auto res = font.sizes.emplace(pixel_height, std::move(fontSize));
        return res.first->second.get();
    }

    struct FtAtlasPlacement {
        FtAtlas *atlas = nullptr;
        int atlas_index = -1;
        int x = 0;
        int y = 0;
    };

    FtAtlasPlacement allocate_atlas_space(FtFontSize &fontSize, int width, int height)
    {
        if (width <= 0 || height <= 0)
            return {};

        auto tryAllocate = [&](FtAtlas &atlas, int index) -> FtAtlasPlacement {
            const int padded_w = width + GLYPH_PADDING;
            const int padded_h = height + GLYPH_PADDING;

            if (padded_w > atlas.width || padded_h > atlas.height)
                return {};

            if (atlas.pen_x + padded_w > atlas.width) {
                atlas.pen_x = 0;
                atlas.pen_y += atlas.row_height;
                atlas.row_height = 0;
            }

            if (atlas.pen_y + padded_h > atlas.height)
                return {};

            FtAtlasPlacement placement{};
            placement.atlas = &atlas;
            placement.atlas_index = index;
            placement.x = atlas.pen_x;
            placement.y = atlas.pen_y;

            atlas.pen_x += padded_w;
            atlas.row_height = (std::max)(atlas.row_height, padded_h);

            return placement;
        };

        for (size_t i = 0; i < fontSize.atlases.size(); ++i) {
            FtAtlasPlacement placement = tryAllocate(fontSize.atlases[i], static_cast<int>(i));
            if (placement.atlas)
                return placement;
        }

        FtAtlas &atlas = create_atlas(fontSize);
        const int index = static_cast<int>(fontSize.atlases.size() - 1);
        if (atlas.texnum == 0 || atlas.width == 0 || atlas.height == 0)
            return {};
        FtAtlasPlacement placement = tryAllocate(atlas, index);
        return placement;
    }

    static int atlas_dimension(int pixel_height)
    {
        // Heuristic: scale atlas size with requested pixel height; clamp to sane pow2 range.
        const int base_height = (pixel_height > 0) ? pixel_height : (CONCHAR_HEIGHT * 4);
        const float scale = (std::max)(1.0f, base_height / static_cast<float>(CONCHAR_HEIGHT * 4));
        int size = 512;
        if (scale > 3.0f)
            size = 2048;
        else if (scale > 1.5f)
            size = 1024;

        // ensure power of two
        int pow2 = 1;
        while (pow2 < size && pow2 < 4096)
            pow2 <<= 1;
        return (std::min)(pow2, 2048);
    }

    FtAtlas &create_atlas(FtFontSize &fontSize)
    {
        static FtAtlas invalid{};
        if (fontSize.atlases.size() >= MAX_ATLASES)
            return invalid;

        FtAtlas atlas{};
        const int target = atlas_dimension(fontSize.pixel_height);
        atlas.width = target;
        atlas.height = target;
        qglGenTextures(1, &atlas.texnum);
        GL_ForceTexture(TMU_TEXTURE, atlas.texnum);

        qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#if defined(GL_RED)
        GLenum internal_format = GL_RED;
        GLenum format = GL_RED;
#else
        GLenum internal_format = GL_ALPHA;
        GLenum format = GL_ALPHA;
#endif
        qglTexImage2D(GL_TEXTURE_2D, 0, internal_format,
                      atlas.width, atlas.height, 0,
                      format, GL_UNSIGNED_BYTE, nullptr);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
#ifdef GL_CLAMP_TO_EDGE
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#else
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        qglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
#endif
        qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);

        fontSize.atlases.push_back(atlas);
        return fontSize.atlases.back();
    }

private:
    void destroy_font_size(FtFontSize &fontSize)
    {
        for (FtAtlas &atlas : fontSize.atlases) {
            if (atlas.texnum) {
                qglDeleteTextures(1, &atlas.texnum);
                atlas.texnum = 0;
            }
        }
        fontSize.atlases.clear();
        fontSize.glyphs.clear();
        if (fontSize.size) {
            FT_Done_Size(fontSize.size);
            fontSize.size = nullptr;
        }
    }

    void destroy_font(FtFont &font)
    {
        for (auto &entry : font.sizes) {
            if (entry.second)
                destroy_font_size(*entry.second);
        }
        font.sizes.clear();
        if (font.face) {
            FT_Done_Face(font.face);
            font.face = nullptr;
        }
        if (font.file_buffer) {
            FS_FreeFile(font.file_buffer);
            font.file_buffer = nullptr;
            font.file_size = 0;
        }
    }

    FT_Library library = nullptr;
    std::unordered_map<const image_t *, std::unique_ptr<FtFont>> fonts;
};
#endif // USE_FREETYPE

class TextSystem {
public:
    static TextSystem &instance()
    {
        static TextSystem sys;
        return sys;
    }

    void init()
    {
#if USE_FREETYPE
        ft_backend.init();
#endif
    }

    void shutdown()
    {
#if USE_FREETYPE
        ft_backend.shutdown();
#endif
    }

    bool register_freetype_font(image_t *image, const char *filename)
    {
#if USE_FREETYPE
        return ft_backend.register_font(image, filename);
#else
        (void)image; (void)filename; return false;
#endif
    }

    void free_font(image_t *image)
    {
#if USE_FREETYPE
        ft_backend.destroy_font_for_image(image);
#else
        (void)image;
#endif
    }

    bool acquire_ft(qhandle_t font, ftfont_t *outFont)
    {
#if USE_FREETYPE
        return ft_backend.acquire(font, outFont);
#else
        (void)font; (void)outFont; return false;
#endif
    }

    void release_ft(ftfont_t *font)
    {
#if USE_FREETYPE
        ft_backend.release(font);
#else
        (void)font;
#endif
    }

    void invalidate_ft_size(qhandle_t font, int pixelHeight)
    {
#if USE_FREETYPE
        ft_backend.invalidate_size(font, pixelHeight);
#else
        (void)font; (void)pixelHeight;
#endif
    }

    int draw_string(const text_render_request_t &req)
    {
        if (!req.text)
            return req.x;

        if (req.kfont)
            return draw_kfont(req);

#if USE_FREETYPE
        if (req.ftfont && req.ftfont->driverData)
            return ft_backend.draw_string(req, req.base_color);
#endif

        return draw_legacy(req);
    }

    text_measure_result_t measure_string(const text_render_request_t &req)
    {
        text_measure_result_t result{};
        if (!req.text) {
            result.final_color = req.base_color;
            return result;
        }

        if (req.kfont)
            return measure_kfont(req);

#if USE_FREETYPE
        if (req.ftfont && req.ftfont->driverData)
            return ft_backend.measure_string(req, req.base_color);
#endif

        return measure_legacy(req);
    }

    float line_height(const text_render_request_t &req)
    {
        if (req.kfont) {
            const int lh = req.kfont->line_height ? req.kfont->line_height : CONCHAR_HEIGHT;
            return lh * clamp_scale(req.scale) * resolve_dpi(req);
        }

#if USE_FREETYPE
        if (req.ftfont && req.ftfont->driverData)
            return ft_backend.line_height(req);
#endif

        return CONCHAR_HEIGHT * clamp_scale(req.scale) * resolve_dpi(req);
    }

private:
    int draw_kfont(const text_render_request_t &req)
    {
        const text_style_t style = style_from_flags(req.flags, req);
        const float clampedScale = clamp_scale(req.scale) * resolve_dpi(req);
        const kfont_t *kfont = req.kfont;
        const char *p = req.text;
        size_t remaining = req.max_bytes;
        int x = req.x;
        int y = req.y;
        float max_width = 0.0f;
        const float line_height = (kfont->line_height ? kfont->line_height : CONCHAR_HEIGHT) * clampedScale;
        color_t currentColor = req.base_color;

        while (remaining && p && *p) {
            if (style.allow_color_codes) {
                size_t consumed = 0;
                if (Q3_ParseColorEscape(p, remaining, currentColor, consumed)) {
                    p += consumed;
                    remaining -= consumed;
                    continue;
                }
            }

            const uint32_t codepoint = utf8_next(p, remaining);
            if (!codepoint)
                break;

            if ((req.flags & UI_MULTILINE) && is_newline(codepoint)) {
                max_width = (std::max)(max_width, static_cast<float>(x - req.x));
                y += static_cast<int>(line_height + (1.0f / draw.scale));
                x = req.x;
                continue;
            }

            const kfont_char_t *ch = SCR_KFontLookup(kfont, codepoint);
            if (!ch)
                continue;

            const float w = ch->w * clampedScale;
            const float h = ch->h * clampedScale;
            const float s = ch->x * kfont->sw;
            const float t = ch->y * kfont->sh;
            const float sw = ch->w * kfont->sw;
            const float sh = ch->h * kfont->sh;
            image_t *image = IMG_ForHandle(kfont->pic);
            if (!image)
                continue;

            color_t drawColor = apply_color_flags(req.flags, currentColor);

            draw_styled_quad(
                static_cast<float>(x), static_cast<float>(y), w, h,
                s, t, s + sw, t + sh,
                style, drawColor, image->texnum, IF_TRANSPARENT);

            x += static_cast<int>(w + (style.bold ? (std::max)(1.0f, w * 0.075f) : 0.0f));
        }

        if (style.underline) {
            const float underline_height = (std::max)(1.0f, line_height * 0.1f);
            GL_StretchPic_(req.x, y + line_height - underline_height,
                           (std::max)(max_width, static_cast<float>(x - req.x)),
                           underline_height, 0, 0, 1, 1, currentColor, TEXNUM_WHITE, IF_TRANSPARENT);
        }

        return x;
    }

    text_measure_result_t measure_kfont(const text_render_request_t &req)
    {
        text_measure_result_t result{};
        const text_style_t style = style_from_flags(req.flags, req);
        const float clampedScale = clamp_scale(req.scale) * resolve_dpi(req);
        const kfont_t *kfont = req.kfont;
        const char *p = req.text;
        size_t remaining = req.max_bytes;
        int width = 0;
        int maxWidth = 0;
        color_t final = req.base_color;

        while (remaining && p && *p) {
            if (style.allow_color_codes) {
                size_t consumed = 0;
                if (Q3_ParseColorEscape(p, remaining, final, consumed)) {
                    p += consumed;
                    remaining -= consumed;
                    continue;
                }
            }

            const uint32_t codepoint = utf8_next(p, remaining);
            if (!codepoint)
                break;

            if ((req.flags & UI_MULTILINE) && is_newline(codepoint)) {
                maxWidth = (std::max)(maxWidth, width);
                width = 0;
                continue;
            }

            const kfont_char_t *ch = SCR_KFontLookup(kfont, codepoint);
            if (!ch)
                continue;

            width += static_cast<int>(ch->w * clampedScale + (style.bold ? 1 : 0));
        }

        maxWidth = (std::max)(maxWidth, width);
        result.width = static_cast<int>(std::lround(maxWidth + style.outline_thickness * 2.0f));
        result.height = static_cast<int>((kfont->line_height ? kfont->line_height : CONCHAR_HEIGHT) * clampedScale);
        result.final_color = final;
        return result;
    }

    int draw_legacy(const text_render_request_t &req)
    {
        const text_style_t style = style_from_flags(req.flags, req);
        const float clampedScale = clamp_scale(req.scale) * resolve_dpi(req);
        const image_t *image = IMG_ForHandle(req.font);
        if (!image)
            return req.x;

        int x = req.x;
        int y = req.y;
        int start_x = x;
        color_t currentColor = req.base_color;
        const char *p = req.text;
        size_t remaining = req.max_bytes;

        while (remaining && p && *p) {
            if (style.allow_color_codes) {
                size_t consumed = 0;
                if (Q3_ParseColorEscape(p, remaining, currentColor, consumed)) {
                    p += consumed;
                    remaining -= consumed;
                    continue;
                }
            }

            const uint32_t codepoint = utf8_next(p, remaining);
            if (!codepoint)
                break;

            const byte c = static_cast<byte>(codepoint & 0xFF);

            if ((req.flags & UI_MULTILINE) && c == '\n') {
                y += static_cast<int>(CONCHAR_HEIGHT * clampedScale + (1.0f / draw.scale));
                x = start_x;
                continue;
            }

            if ((c & 127) != 32) {
                float s = (c & 15) * 0.0625f;
                float t = (c >> 4) * 0.0625f;
                float w = CONCHAR_WIDTH * clampedScale;
                float h = CONCHAR_HEIGHT * clampedScale;
                color_t drawColor = apply_color_flags(req.flags, currentColor);

                draw_styled_quad(
                    static_cast<float>(x), static_cast<float>(y), w, h,
                    s, t, s + 0.0625f, t + 0.0625f,
                    style, drawColor, image->texnum, image->flags);
            }

            x += static_cast<int>(CONCHAR_WIDTH * clampedScale + (style.bold ? 1.0f : 0.0f));
        }

        if (style.underline) {
            const float underline_height = (std::max)(1.0f, CONCHAR_HEIGHT * clampedScale * 0.1f);
            GL_StretchPic_(start_x, y + CONCHAR_HEIGHT * clampedScale - underline_height,
                           static_cast<float>(x - start_x), underline_height,
                           0, 0, 1, 1, currentColor, TEXNUM_WHITE, IF_TRANSPARENT);
        }

        return x;
    }

    text_measure_result_t measure_legacy(const text_render_request_t &req)
    {
        text_measure_result_t result{};
        const text_style_t style = style_from_flags(req.flags, req);
        const float clampedScale = clamp_scale(req.scale) * resolve_dpi(req);
        const char *p = req.text;
        size_t remaining = req.max_bytes;
        int width = 0;
        int maxWidth = 0;
        color_t finalColor = req.base_color;

        while (remaining && p && *p) {
            if (style.allow_color_codes) {
                size_t consumed = 0;
                if (Q3_ParseColorEscape(p, remaining, finalColor, consumed)) {
                    p += consumed;
                    remaining -= consumed;
                    continue;
                }
            }

            const uint32_t codepoint = utf8_next(p, remaining);
            if (!codepoint)
                break;

            if ((req.flags & UI_MULTILINE) && codepoint == '\n') {
                maxWidth = (std::max)(maxWidth, width);
                width = 0;
                continue;
            }

            width += static_cast<int>(CONCHAR_WIDTH * clampedScale + (style.bold ? 1.0f : 0.0f));
        }

        maxWidth = (std::max)(maxWidth, width);
        result.width = maxWidth + static_cast<int>(style.outline_thickness * 2.0f);
        result.height = static_cast<int>(CONCHAR_HEIGHT * clampedScale);
        result.final_color = finalColor;
        return result;
    }

#if USE_FREETYPE
    FreeTypeBackend ft_backend;
#endif
};

} // namespace

void R_TextSystemInit(void)
{
    TextSystem::instance().init();
}

void R_TextSystemShutdown(void)
{
    TextSystem::instance().shutdown();
}

int R_TextDrawString(const text_render_request_t &request)
{
    return TextSystem::instance().draw_string(request);
}

text_measure_result_t R_TextMeasureString(const text_render_request_t &request)
{
    return TextSystem::instance().measure_string(request);
}

float R_TextLineHeight(const text_render_request_t &request)
{
    return TextSystem::instance().line_height(request);
}

// compatibility wrappers

bool Draw_LoadFreeTypeFont(image_t *image, const char *filename)
{
    return TextSystem::instance().register_freetype_font(image, filename);
}

void Draw_FreeFreeTypeFont(image_t *image)
{
    TextSystem::instance().free_font(image);
}

void Draw_InitFreeTypeFonts(void)
{
    R_TextSystemInit();
}

void Draw_ShutdownFreeTypeFonts(void)
{
    R_TextSystemShutdown();
}

bool R_AcquireFreeTypeFont(qhandle_t font, ftfont_t *outFont)
{
    return TextSystem::instance().acquire_ft(font, outFont);
}

void R_ReleaseFreeTypeFont(ftfont_t *font)
{
    TextSystem::instance().release_ft(font);
}

void R_FreeTypeInvalidateFontSize(qhandle_t font, int pixelHeight)
{
    TextSystem::instance().invalidate_ft_size(font, pixelHeight);
}

int R_DrawFreeTypeString(int x, int y, int scale, int flags, size_t maxBytes,
                         const char *string, color_t color, qhandle_t font,
                         const ftfont_t *ftFont)
{
    text_render_request_t req{};
    req.x = x;
    req.y = y;
    req.scale = scale;
    req.flags = flags;
    req.max_bytes = maxBytes;
    req.text = string;
    req.base_color = color;
    req.font = font;
    req.ftfont = ftFont;
    req.style.allow_color_codes = !(flags & UI_IGNORECOLOR);

    if (gl_fontshadow->integer > 0) {
        req.style.shadow.offset_x = clamp_scale(scale);
        req.style.shadow.offset_y = clamp_scale(scale);
        req.style.shadow.color = ColorA(color.a);
    }

    return R_TextDrawString(req);
}

int R_MeasureFreeTypeString(int scale, int flags, size_t maxBytes,
                            const char *string, qhandle_t font,
                            const ftfont_t *ftFont)
{
    text_render_request_t req{};
    req.scale = scale;
    req.flags = flags;
    req.max_bytes = maxBytes;
    req.text = string;
    req.font = font;
    req.ftfont = ftFont;
    req.style.allow_color_codes = !(flags & UI_IGNORECOLOR);

    auto res = R_TextMeasureString(req);
    return res.width;
}

float R_FreeTypeFontLineHeight(int scale, const ftfont_t *ftFont)
{
    text_render_request_t req{};
    req.scale = scale;
    req.ftfont = ftFont;
    return R_TextLineHeight(req);
}
