#include "gl.hpp"
#include "font_freetype.hpp"
#include "gl_draw_utils.hpp"
#include "common/q3colors.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#if USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SIZES_H

extern drawStatic_t draw;

namespace {

constexpr int FT_ATLAS_SIZE = 512;
constexpr int FT_GLYPH_PADDING = 1;
constexpr int FT_BASE_PIXEL_HEIGHT = CONCHAR_HEIGHT * 4;

struct FtAtlas {
    GLuint texnum = 0;
    int width = FT_ATLAS_SIZE;
    int height = FT_ATLAS_SIZE;
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

FT_Library ft_library = nullptr;
std::unordered_map<const image_t *, std::unique_ptr<FtFont>> ft_fonts;

static void Ft_DestroyFontSize(FtFontSize &fontSize)
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

static void Ft_DestroyFont(FtFont &font)
{
	for (auto &entry : font.sizes) {
		if (entry.second)
			Ft_DestroyFontSize(*entry.second);
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

static FtAtlas &Ft_CreateAtlas(FtFontSize &fontSize)
{
	FtAtlas atlas{};

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

struct FtAtlasPlacement {
	FtAtlas *atlas = nullptr;
	int atlas_index = -1;
	int x = 0;
	int y = 0;
};

static FtAtlasPlacement Ft_AllocateAtlasSpace(FtFontSize &fontSize, int width, int height)
{
	if (width <= 0 || height <= 0)
		return {};

	auto tryAllocate = [&](FtAtlas &atlas, int index) -> FtAtlasPlacement {
		if (width + FT_GLYPH_PADDING > atlas.width || height + FT_GLYPH_PADDING > atlas.height)
			return {};

		if (atlas.pen_x + width + FT_GLYPH_PADDING > atlas.width) {
			atlas.pen_x = 0;
			atlas.pen_y += atlas.row_height;
			atlas.row_height = 0;
		}

		if (atlas.pen_y + height + FT_GLYPH_PADDING > atlas.height)
			return {};

		FtAtlasPlacement placement{};
		placement.atlas = &atlas;
		placement.atlas_index = index;
		placement.x = atlas.pen_x;
		placement.y = atlas.pen_y;

		atlas.pen_x += width + FT_GLYPH_PADDING;
		atlas.row_height = (std::max)(atlas.row_height, height + FT_GLYPH_PADDING);

		return placement;
	};

	for (size_t i = 0; i < fontSize.atlases.size(); ++i) {
		FtAtlasPlacement placement = tryAllocate(fontSize.atlases[i], static_cast<int>(i));
		if (placement.atlas)
			return placement;
	}

	FtAtlas &atlas = Ft_CreateAtlas(fontSize);
	const int index = static_cast<int>(fontSize.atlases.size() - 1);
	FtAtlasPlacement placement = tryAllocate(atlas, index);
	if (!placement.atlas) {
		Com_DPrintf("Ft_AllocateAtlasSpace: glyph %dx%d exceeds atlas %dx%d\n",
			width + FT_GLYPH_PADDING, height + FT_GLYPH_PADDING, atlas.width, atlas.height);
		Com_SetLastError("FreeType glyph too large for atlas");
		return {};
	}

	return placement;
}

static FtGlyph *Ft_EmitGlyph(FtFont &font, FtFontSize &fontSize, uint32_t codepoint)
{
	auto it = fontSize.glyphs.find(codepoint);
	if (it != fontSize.glyphs.end())
		return &it->second;

	if (!font.face || !fontSize.size)
		return nullptr;

	FT_Error err = FT_Activate_Size(fontSize.size);
	if (err)
		return nullptr;

	err = FT_Load_Char(font.face, codepoint, FT_LOAD_RENDER);
	if (err)
		return nullptr;

	FT_GlyphSlot slot = font.face->glyph;

	FtGlyph glyph{};
	glyph.glyph_index = slot->glyph_index;
	glyph.advance = slot->advance.x >> 6;
	glyph.bearing_x = slot->bitmap_left;
	glyph.bearing_y = slot->bitmap_top;
	glyph.width = slot->bitmap.width;
	glyph.height = slot->bitmap.rows;

	if (glyph.width > 0 && glyph.height > 0 && slot->bitmap.buffer) {
		FtAtlasPlacement placement = Ft_AllocateAtlasSpace(fontSize, glyph.width, glyph.height);
		if (placement.atlas) {
			glyph.atlas_index = placement.atlas_index;

			const float inv_w = 1.0f / placement.atlas->width;
			const float inv_h = 1.0f / placement.atlas->height;

			glyph.s0 = placement.x * inv_w;
			glyph.t0 = placement.y * inv_h;
			glyph.s1 = (placement.x + glyph.width) * inv_w;
			glyph.t1 = (placement.y + glyph.height) * inv_h;

			GL_BindTexture(TMU_TEXTURE, placement.atlas->texnum);
			qglPixelStorei(GL_UNPACK_ALIGNMENT, 1);
#if defined(GL_RED)
			GLenum format = GL_RED;
#else
			GLenum format = GL_ALPHA;
#endif
			qglTexSubImage2D(GL_TEXTURE_2D, 0, placement.x, placement.y,
				glyph.width, glyph.height,
				format, GL_UNSIGNED_BYTE, slot->bitmap.buffer);
			qglPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		} else {
			glyph.width = 0;
			glyph.height = 0;
			Com_DPrintf("Ft_EmitGlyph: failed to allocate %u (%dx%d) in atlas\n",
				static_cast<unsigned int>(codepoint), slot->bitmap.width, slot->bitmap.rows);
		}
	}

	auto res = fontSize.glyphs.emplace(codepoint, glyph);
	return &res.first->second;
}

static FtGlyph *Ft_LookupGlyph(FtFont &font, FtFontSize &fontSize, uint32_t codepoint)
{
	FtGlyph *glyph = Ft_EmitGlyph(font, fontSize, codepoint);
	if (!glyph || (glyph->atlas_index < 0 && glyph->width == 0)) {
		if (codepoint != '?')
			glyph = Ft_EmitGlyph(font, fontSize, '?');
	}
	return glyph;
}

static FtFont *Ft_FontForImage(const image_t *image)
{
	auto it = ft_fonts.find(image);
	if (it == ft_fonts.end())
		return nullptr;
	return it->second.get();
}

static FtFontSize *Ft_GetFontSize(FtFont &font, int pixel_height)
{
	if (pixel_height <= 0)
		pixel_height = FT_BASE_PIXEL_HEIGHT;

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

static uint32_t DecodeNextUTF8(const char *&p, size_t &remaining)
{
	if (!remaining || !*p)
		return 0;

	const char *start = p;
	size_t rem = remaining;

	unsigned char lead = static_cast<unsigned char>(*start++);
	--rem;

	if (lead < 0x80) {
		p = start;
		remaining = rem;
		return lead;
	}

	size_t expected = 0;
	uint32_t codepoint = 0;
	uint32_t min_value = 0;

	if ((lead & 0xE0) == 0xC0) {
		expected = 1;
		codepoint = lead & 0x1F;
		min_value = 0x80;
	} else if ((lead & 0xF0) == 0xE0) {
		expected = 2;
		codepoint = lead & 0x0F;
		min_value = 0x800;
	} else if ((lead & 0xF8) == 0xF0 && lead <= 0xF4) {
		expected = 3;
		codepoint = lead & 0x07;
		min_value = 0x10000;
	} else {
		goto fail;
	}

	if (rem < expected)
		goto fail;

	for (size_t i = 0; i < expected; ++i) {
		unsigned char c = static_cast<unsigned char>(*start++);
		if ((c & 0xC0) != 0x80)
			goto fail;
		codepoint = (codepoint << 6) | (c & 0x3F);
	}

	if (codepoint < min_value || codepoint > 0x10FFFF ||
	    (codepoint >= 0xD800 && codepoint <= 0xDFFF))
		goto fail;

	remaining = rem - expected;
	p = start;
	return codepoint;

fail:
	++p;
	--remaining;
	return '?';
}

static int Ft_DrawString(FtFont &font, FtFontSize &fontSize, int x, int y, int scale, int flags,
                         size_t maxlen, const char *s, color_t color)
{
	if (!s)
		return x;

	const int base_height = (std::max)(fontSize.pixel_height, 1);
	const float target_height = CONCHAR_HEIGHT * (std::max)(scale, 1);
	const float scale_factor = target_height / static_cast<float>(base_height);
	const float line_advance = (fontSize.line_height ? fontSize.line_height : base_height) * scale_factor;
	const float ascent = (fontSize.ascent ? fontSize.ascent : base_height) * scale_factor;

	float pen_x = static_cast<float>(x);
	float pen_y = static_cast<float>(y);
	const float start_x = pen_x;
	const bool has_kerning = font.face && FT_HAS_KERNING(font.face);
	FT_UInt prev_glyph_index = 0;
	bool have_prev_glyph = false;

	const bool drop_shadow = (flags & UI_DROPSHADOW) != 0;
	const int shadow_offset = drop_shadow ? (std::max)(scale, 1) : 0;

	color_t currentColor = color;
	color_t shadow_color = ColorA(currentColor.a);

	const char *p = s;
	size_t remaining = maxlen;

	while (remaining && *p) {
		if (!(flags & UI_IGNORECOLOR)) {
			size_t consumed = 0;
			if (Q3_ParseColorEscape(p, remaining, currentColor, consumed)) {
				p += consumed;
				remaining -= consumed;
				shadow_color = ColorA(currentColor.a);
				continue;
			}
		}

		uint32_t codepoint = DecodeNextUTF8(p, remaining);
		if (!codepoint)
			break;

		if ((flags & UI_MULTILINE) && codepoint == '\n') {
			pen_y += line_advance + (1.0f / draw.scale);
			pen_x = start_x;
			have_prev_glyph = false;
			prev_glyph_index = 0;
			continue;
		}

		if (codepoint < 32)
			continue;

		FtGlyph *glyph = Ft_LookupGlyph(font, fontSize, codepoint);
		if (!glyph)
			continue;

		if (has_kerning && have_prev_glyph && prev_glyph_index && glyph->glyph_index) {
			FT_Vector delta{};
			if (!FT_Get_Kerning(font.face, prev_glyph_index, glyph->glyph_index, FT_KERNING_DEFAULT, &delta)) {
				pen_x += (delta.x / 64.0f) * scale_factor;
			}
		}

		have_prev_glyph = glyph->glyph_index != 0;
		if (have_prev_glyph)
			prev_glyph_index = glyph->glyph_index;

		float adv = glyph->advance * scale_factor;
		if (glyph->width > 0 && glyph->height > 0 && glyph->atlas_index >= 0 &&
		        static_cast<size_t>(glyph->atlas_index) < fontSize.atlases.size()) {
			const float xpos = pen_x + glyph->bearing_x * scale_factor;
			const float ypos = pen_y + ascent - glyph->bearing_y * scale_factor;
			const float gw = glyph->width * scale_factor;
			const float gh = glyph->height * scale_factor;

			const FtAtlas &atlas = fontSize.atlases[glyph->atlas_index];

			if (drop_shadow) {
				GL_StretchPic_(xpos + shadow_offset, ypos + shadow_offset, gw, gh,
				               glyph->s0, glyph->t0, glyph->s1, glyph->t1,
				               shadow_color, atlas.texnum, IF_TRANSPARENT);

				if (gl_fontshadow->integer > 1) {
					GL_StretchPic_(xpos + shadow_offset * 2, ypos + shadow_offset * 2, gw, gh,
					               glyph->s0, glyph->t0, glyph->s1, glyph->t1,
					               shadow_color, atlas.texnum, IF_TRANSPARENT);
				}
			}

			GL_StretchPic_(xpos, ypos, gw, gh,
			       glyph->s0, glyph->t0, glyph->s1, glyph->t1,
			       currentColor, atlas.texnum, IF_TRANSPARENT);
		}

		pen_x += adv;
	}

	return static_cast<int>(std::lround(pen_x));
}
static int Ft_MeasureString(FtFont &font, FtFontSize &fontSize, int scale, int flags,
                         size_t maxlen, const char *s)
{
	if (!s)
		return 0;

	const int base_height = (std::max)(fontSize.pixel_height, 1);
	const float target_height = CONCHAR_HEIGHT * (std::max)(scale, 1);
	const float scale_factor = target_height / static_cast<float>(base_height);
	float pen_x = 0.0f;
	float max_pen_x = 0.0f;
	const char *p = s;
	size_t remaining = maxlen;
	color_t ignoredColor{};
	const bool has_kerning = font.face && FT_HAS_KERNING(font.face);
	FT_UInt prev_glyph_index = 0;
	bool have_prev_glyph = false;

	while (remaining && *p) {
		if (!(flags & UI_IGNORECOLOR)) {
			size_t consumed = 0;
			if (Q3_ParseColorEscape(p, remaining, ignoredColor, consumed)) {
				p += consumed;
				remaining -= consumed;
				continue;
			}
		}

		uint32_t codepoint = DecodeNextUTF8(p, remaining);
		if (!codepoint)
			break;

		if ((flags & UI_MULTILINE) && codepoint == '\n') {
			max_pen_x = (std::max)(max_pen_x, pen_x);
			pen_x = 0.0f;
			have_prev_glyph = false;
			prev_glyph_index = 0;
			continue;
		}

		if (codepoint < 32)
			continue;

		FtGlyph *glyph = Ft_LookupGlyph(font, fontSize, codepoint);
		if (!glyph)
			continue;

		if (has_kerning && have_prev_glyph && prev_glyph_index && glyph->glyph_index) {
			FT_Vector delta{};
			if (!FT_Get_Kerning(font.face, prev_glyph_index, glyph->glyph_index, FT_KERNING_DEFAULT, &delta)) {
				pen_x += (delta.x / 64.0f) * scale_factor;
			}
		}

		have_prev_glyph = glyph->glyph_index != 0;
		if (have_prev_glyph)
			prev_glyph_index = glyph->glyph_index;

		pen_x += glyph->advance * scale_factor;
	}

	max_pen_x = (std::max)(max_pen_x, pen_x);

	return static_cast<int>(std::lround(max_pen_x));
}

} // namespace

bool Draw_LoadFreeTypeFont(image_t *image, const char *filename)
{
	if (!ft_library) {
		Com_SetLastError("FreeType library is not initialized");
		return false;
	}

	if (!image || !filename) {
		Com_SetLastError("Invalid font parameters");
		return false;
	}

	auto font = std::make_unique<FtFont>();
	font->image = image;

	byte *buffer = nullptr;
	int length = FS_LoadFile(filename, reinterpret_cast<void **>(&buffer));
	if (length < 0) {
		Com_SetLastError(va("Failed to load font '%s': %s", filename, Q_ErrorString(length)));
		return false;
	}

	font->file_buffer = buffer;
	font->file_size = static_cast<size_t>(length);

	FT_Face face = nullptr;
	FT_Error err = FT_New_Memory_Face(ft_library, reinterpret_cast<const FT_Byte *>(buffer), length, 0, &face);
	if (err) {
		Com_SetLastError(va("FreeType failed to create font face (%d)", err));
		FS_FreeFile(buffer);
		font->file_buffer = nullptr;
		font->file_size = 0;
		return false;
	}

	bool has_symbol_charmap = false;
	for (int i = 0; i < face->num_charmaps; ++i) {
		if (face->charmaps[i] && face->charmaps[i]->encoding == FT_ENCODING_MS_SYMBOL) {
			has_symbol_charmap = true;
			break;
		}
	}

	FT_Error charmap_err = FT_Select_Charmap(face, FT_ENCODING_UNICODE);
	bool using_fallback_charmap = false;
	if (charmap_err) {
		for (int i = 0; i < face->num_charmaps; ++i) {
			if (!FT_Set_Charmap(face, face->charmaps[i])) {
				using_fallback_charmap = true;
				break;
			}
		}

		if (!using_fallback_charmap) {
			Com_SetLastError(va("FreeType failed to select a usable charmap for '%s' (%d)", filename, charmap_err));
			FT_Done_Face(face);
			FS_FreeFile(buffer);
			font->file_buffer = nullptr;
			font->file_size = 0;
			return false;
		}
	}

	if (!using_fallback_charmap && has_symbol_charmap) {
		const FT_ULong probe = 'A';
		const FT_UInt glyph_index = FT_Get_Char_Index(face, probe);
		if (glyph_index) {
			Com_DPrintf("FreeType: verified Unicode glyph lookup for symbol font '%s' (U+%04lX)\n", filename, probe);
		} else {
			Com_DPrintf("FreeType: Unicode glyph lookup failed for symbol font '%s' (U+%04lX)\n", filename, probe);
		}
	}

	if (using_fallback_charmap && face->charmap && face->charmap->encoding == FT_ENCODING_MS_SYMBOL) {
		const FT_ULong probe = 0xF000 + 'A';
		const FT_UInt glyph_index = FT_Get_Char_Index(face, probe);
		if (glyph_index) {
			Com_DPrintf("FreeType: verified MS Symbol glyph lookup for '%s' (U+%04lX)\n", filename, probe);
		} else {
			Com_DPrintf("FreeType: MS Symbol glyph lookup failed for '%s' (U+%04lX)\n", filename, probe);
		}
	}

	font->face = face;

	FtFontSize *baseSize = Ft_GetFontSize(*font, FT_BASE_PIXEL_HEIGHT);
	if (!baseSize) {
		Com_SetLastError("FreeType failed to initialize default pixel size");
		FT_Done_Face(face);
		FS_FreeFile(buffer);
		font->face = nullptr;
		font->file_buffer = nullptr;
		font->file_size = 0;
		return false;
	}

	Draw_FreeFreeTypeFont(image);

	ft_fonts[image] = std::move(font);

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

void Draw_FreeFreeTypeFont(image_t *image)
{
    if (!image)
        return;

    auto it = ft_fonts.find(image);
    if (it == ft_fonts.end())
        return;

    Ft_DestroyFont(*it->second);
    ft_fonts.erase(it);
}

void Draw_InitFreeTypeFonts(void)
{
    if (ft_library)
        return;

    if (FT_Init_FreeType(&ft_library)) {
        Com_SetLastError("Failed to initialize FreeType");
        ft_library = nullptr;
    }
}

void Draw_ShutdownFreeTypeFonts(void)
{
    if (!ft_library)
        return;

    for (auto &entry : ft_fonts)
        Ft_DestroyFont(*entry.second);

    ft_fonts.clear();
    FT_Done_FreeType(ft_library);
    ft_library = nullptr;
}

bool R_AcquireFreeTypeFont(qhandle_t font, ftfont_t *outFont)
{
	if (!outFont)
		return false;

	outFont->driverData = nullptr;
	outFont->ascent = 0;
	outFont->descent = 0;
	outFont->lineHeight = 0;

	const image_t *image = IMG_ForHandle(font);
	if (!image)
		return false;

	FtFont *ft_font = Ft_FontForImage(image);
	if (!ft_font)
		return false;

	int pixelHeight = outFont->pixelHeight;
	if (pixelHeight <= 0)
		pixelHeight = FT_BASE_PIXEL_HEIGHT;

	FtFontSize *fontSize = Ft_GetFontSize(*ft_font, pixelHeight);
	if (!fontSize)
		return false;

	if (fontSize->size) {
		FT_Error err = FT_Activate_Size(fontSize->size);
		if (err)
			return false;
	}

	outFont->driverData = ft_font;
	outFont->pixelHeight = fontSize->pixel_height;
	outFont->ascent = fontSize->ascent;
	outFont->descent = fontSize->descent;
	outFont->lineHeight = fontSize->line_height ? fontSize->line_height : fontSize->pixel_height;
	outFont->face = ft_font->face;

	return true;
}

void R_ReleaseFreeTypeFont(ftfont_t *font)
{
	if (!font)
		return;

	font->driverData = nullptr;
	font->ascent = 0;
	font->descent = 0;
	font->lineHeight = 0;
}

/*
=============
R_FreeTypeInvalidateFontSize

Releases cached glyph data for a specific FreeType font size.
=============
*/
void R_FreeTypeInvalidateFontSize(qhandle_t font, int pixelHeight)
{
	if (!font)
		return;

	const image_t *image = IMG_ForHandle(font);
	if (!image)
		return;

	FtFont *ft_font = Ft_FontForImage(image);
	if (!ft_font)
		return;

	int targetHeight = pixelHeight > 0 ? pixelHeight : FT_BASE_PIXEL_HEIGHT;
	auto it = ft_font->sizes.find(targetHeight);
	if (it == ft_font->sizes.end())
		return;

	if (it->second)
		Ft_DestroyFontSize(*it->second);

	ft_font->sizes.erase(it);
}

int R_DrawFreeTypeString(int x, int y, int scale, int flags, size_t maxChars,
				 const char *string, color_t color, qhandle_t font,
				 const ftfont_t *ftFont)
{
	const image_t *image = IMG_ForHandle(font);

	FtFont *ft_font = nullptr;
	if (ftFont)
		ft_font = static_cast<FtFont *>(ftFont->driverData);

	if (!ft_font)
		ft_font = Ft_FontForImage(image);

	if (ft_font) {
		int pixelHeight = (ftFont && ftFont->pixelHeight > 0) ? ftFont->pixelHeight : FT_BASE_PIXEL_HEIGHT;
		FtFontSize *fontSize = Ft_GetFontSize(*ft_font, pixelHeight);
		if (fontSize) {
			if (gl_fontshadow->integer > 0)
				flags |= UI_DROPSHADOW;
			return Ft_DrawString(*ft_font, *fontSize, x, y, scale, flags, maxChars, string, color);
		}
	}

	return R_DrawStringStretch(x, y, scale, flags, maxChars, string, color, font, nullptr);
}

int R_MeasureFreeTypeString(int scale, int flags, size_t maxChars,
				 const char *string, qhandle_t font,
				 const ftfont_t *ftFont)
{
	const image_t *image = IMG_ForHandle(font);

	FtFont *ft_font = nullptr;
	if (ftFont)
		ft_font = static_cast<FtFont *>(ftFont->driverData);

	if (!ft_font)
		ft_font = Ft_FontForImage(image);

	if (ft_font) {
		int pixelHeight = (ftFont && ftFont->pixelHeight > 0) ? ftFont->pixelHeight : FT_BASE_PIXEL_HEIGHT;
		FtFontSize *fontSize = Ft_GetFontSize(*ft_font, pixelHeight);
		if (fontSize)
			return Ft_MeasureString(*ft_font, *fontSize, scale, flags, maxChars, string);
	}

	int width = 0;
	int maxWidth = 0;

	if (!string)
		return 0;

	while (maxChars-- && *string) {
		char ch = *string++;

		if ((flags & UI_MULTILINE) && ch == '\n') {
			maxWidth = (std::max)(maxWidth, width);
			width = 0;
			continue;
		}

		width += CONCHAR_WIDTH * scale;
	}

	return (std::max)(maxWidth, width);
}

float R_FreeTypeFontLineHeight(int scale, const ftfont_t *ftFont)
{
	FtFont *ft_font = nullptr;
	if (ftFont)
		ft_font = static_cast<FtFont *>(ftFont->driverData);

	if (!ft_font)
		return CONCHAR_HEIGHT * (std::max)(scale, 1);

	int pixelHeight = (ftFont && ftFont->pixelHeight > 0) ? ftFont->pixelHeight : FT_BASE_PIXEL_HEIGHT;
	FtFontSize *fontSize = Ft_GetFontSize(*ft_font, pixelHeight);
	if (!fontSize)
		return CONCHAR_HEIGHT * (std::max)(scale, 1);

	const int base_height = (std::max)(fontSize->pixel_height, 1);
	const float target_height = CONCHAR_HEIGHT * (std::max)(scale, 1);
	const float line_height = (fontSize->line_height ? fontSize->line_height : base_height) * (target_height / static_cast<float>(base_height));
	return line_height;
}


#endif // USE_FREETYPE
