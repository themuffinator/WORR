#include "client.hpp"
#include "common/q3colors.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#undef min
#undef max
#endif

static cvar_t* scr_font = nullptr;
#if USE_FREETYPE
static cvar_t* scr_fontpath = nullptr;
static cvar_t* scr_text_backend = nullptr;

static void SCR_FreeFreeTypeFonts(void);
static bool SCR_LoadDefaultFreeTypeFont(void);
static const ftfont_t* SCR_FTFontForHandle(qhandle_t handle);
#endif

// nb: this is dumb but C doesn't allow
// `(T) { }` to count as a constant


#if USE_FREETYPE
static bool SCR_EnsureFreeTypeLibrary()
{
	if (scr.freetype.library)
		return true;

	FT_Error error = FT_Init_FreeType(&scr.freetype.library);
	if (error) {
		Com_Printf("SCR: failed to initialize FreeType (error %d)\n", error);
		scr.freetype.library = nullptr;
		return false;
	}

	return true;
}

static void SCR_FreeFreeTypeFonts(void)
{
	for (auto& entry : scr.freetype.fonts) {
		if (entry.second.renderInfo.face) {
			FT_Done_Face(entry.second.renderInfo.face);
			entry.second.renderInfo.face = nullptr;
		}
		R_ReleaseFreeTypeFont(&entry.second.renderInfo);
	}

	scr.freetype.fonts.clear();
	scr.freetype.handleLookup.clear();
	scr.freetype.activeFontKey.clear();
	scr.freetype.activeFontHandle = 0;
}

static bool SCR_LoadFreeTypeFont(const std::string& cacheKey, const std::string& fontPath,
	int pixelHeight, qhandle_t handle)
{
	if (!SCR_EnsureFreeTypeLibrary())
		return false;

	void* fileBuffer = nullptr;
	int length = FS_LoadFile(fontPath.c_str(), &fileBuffer);
	if (length <= 0) {
		Com_Printf("SCR: failed to load font '%s'\n", fontPath.c_str());
		return false;
	}

	scr_freetype_font_entry_t entry;
	entry.buffer.resize(length);
	std::memcpy(entry.buffer.data(), fileBuffer, length);
	FS_FreeFile(fileBuffer);

	FT_Face face = nullptr;
	FT_Error error = FT_New_Memory_Face(scr.freetype.library, entry.buffer.data(), length, 0, &face);
	if (error) {
		Com_Printf("SCR: failed to create FreeType face for '%s' (error %d)\n", fontPath.c_str(), error);
		return false;
	}

	error = FT_Set_Pixel_Sizes(face, 0, pixelHeight);
	if (error) {
		Com_Printf("SCR: failed to set pixel height %d for '%s' (error %d)\n", pixelHeight, fontPath.c_str(), error);
		FT_Done_Face(face);
		return false;
	}

	entry.renderInfo.face = face;
	entry.renderInfo.pixelHeight = pixelHeight;

	auto existing = scr.freetype.fonts.find(cacheKey);
	if (existing != scr.freetype.fonts.end()) {
		if (existing->second.renderInfo.face)
			FT_Done_Face(existing->second.renderInfo.face);
		R_ReleaseFreeTypeFont(&existing->second.renderInfo);
		existing->second = std::move(entry);
		R_AcquireFreeTypeFont(handle, &existing->second.renderInfo);
	}
	else {
		auto [it, inserted] = scr.freetype.fonts.emplace(cacheKey, std::move(entry));
		if (inserted)
			R_AcquireFreeTypeFont(handle, &it->second.renderInfo);
	}

	scr.freetype.handleLookup[handle] = cacheKey;
	scr.freetype.activeFontKey = cacheKey;
	scr.freetype.activeFontHandle = handle;
	return true;
}
static bool SCR_LoadDefaultFreeTypeFont(void)
{
	constexpr int defaultPixelHeight = 16;

	std::string fontPath;
	if (scr_fontpath && scr_fontpath->string[0]) {
		fontPath = scr_fontpath->string;
		if (!fontPath.empty() && fontPath.back() != '/' && fontPath.back() != '\\')
			fontPath.push_back('/');
	}
	fontPath += "RobotoMono-Regular.ttf";

	std::string cacheKey = "RobotoMono-Regular-" + std::to_string(defaultPixelHeight);

	if (!SCR_LoadFreeTypeFont(cacheKey, fontPath, defaultPixelHeight, scr.font_pic))
		return false;

	return true;
}

static const ftfont_t* SCR_FTFontForHandle(qhandle_t handle)
{
        auto handleIt = scr.freetype.handleLookup.find(handle);
        if (handleIt == scr.freetype.handleLookup.end())
                return nullptr;

        auto fontIt = scr.freetype.fonts.find(handleIt->second);
        if (fontIt == scr.freetype.fonts.end())
                return nullptr;

        return &fontIt->second.renderInfo;
}

#endif // USE_FREETYPE

enum class scr_text_backend_mode {
        LEGACY,
        TTF,
        KFONT,
};

static scr_text_backend_mode scr_activeTextBackend = scr_text_backend_mode::LEGACY;

#if USE_FREETYPE
static const char* SCR_TextBackendToString(scr_text_backend_mode mode)
{
	switch (mode) {
	case scr_text_backend_mode::LEGACY:
		return "legacy";
	case scr_text_backend_mode::TTF:
		return "ttf";
	case scr_text_backend_mode::KFONT:
		return "kfont";
	}

	return "legacy";
}

static scr_text_backend_mode SCR_ParseTextBackend(const char* value)
{
	if (!value || !*value)
		return scr_text_backend_mode::LEGACY;

	if (!Q_stricmp(value, "legacy") || !Q_stricmp(value, "bitmap"))
		return scr_text_backend_mode::LEGACY;

	if (!Q_stricmp(value, "ttf") || !Q_stricmp(value, "freetype") || !Q_stricmp(value, "ft"))
		return scr_text_backend_mode::TTF;

	if (!Q_stricmp(value, "kfont"))
		return scr_text_backend_mode::KFONT;

	if (Q_isdigit(*value) || (*value == '-' && Q_isdigit(value[1]))) {
		switch (Q_atoi(value)) {
		case 1:
			return scr_text_backend_mode::TTF;
		case 2:
			return scr_text_backend_mode::KFONT;
		default:
			return scr_text_backend_mode::LEGACY;
		}
	}

	return scr_text_backend_mode::LEGACY;
}
#endif

static bool SCR_ShouldUseKFont()
{
	return scr_activeTextBackend == scr_text_backend_mode::KFONT && scr.kfont.pic;
}

#if USE_FREETYPE
static bool SCR_ShouldUseFreeType(qhandle_t font)
{
        if (scr_activeTextBackend != scr_text_backend_mode::TTF)
                return false;

	if (!font)
		return false;

	if (scr.freetype.activeFontHandle && font == scr.freetype.activeFontHandle)
		return true;

        return SCR_FTFontForHandle(font) != nullptr;
}

static scr_text_backend_mode SCR_SelectFallbackTextBackend()
{
        if (scr.kfont.pic)
                return scr_text_backend_mode::KFONT;

        return scr_text_backend_mode::LEGACY;
}

static void scr_text_backend_changed(cvar_t* self)
{
        const auto requested = SCR_ParseTextBackend(self->string);
        auto newBackend = requested;

        if (requested == scr_text_backend_mode::TTF) {
                const char* failureReason = nullptr;
                bool fallbackRequired = false;
                if (!scr.font_pic) {
                        failureReason = "base font handle unavailable";
                        fallbackRequired = true;
                }
                else {
                        if (!scr.freetype.activeFontHandle) {
                                if (!SCR_LoadDefaultFreeTypeFont())
                                        failureReason = "failed to load default FreeType font";
                        }

                        if (!scr.freetype.activeFontHandle) {
                                if (!failureReason)
                                        failureReason = "no active FreeType font handle";
                                fallbackRequired = true;
                        }
                }

                if (fallbackRequired) {
                        const auto fallback = SCR_SelectFallbackTextBackend();
                        if (failureReason)
                                Com_DPrintf("SCR: %s; using %s text backend instead\n", failureReason,
                                        SCR_TextBackendToString(fallback));
                        else
                                Com_DPrintf("SCR: FreeType font unavailable; using %s text backend instead\n",
                                        SCR_TextBackendToString(fallback));
                        if (scr.freetype.activeFontHandle)
                                scr.freetype.handleLookup.erase(scr.freetype.activeFontHandle);
                        scr.freetype.activeFontKey.clear();
                        scr.freetype.activeFontHandle = 0;
                        newBackend = fallback;
                }
        }
        else if (requested == scr_text_backend_mode::KFONT) {
                if (!scr.kfont.pic)
                        newBackend = scr_text_backend_mode::LEGACY;
	}

	scr_activeTextBackend = newBackend;

	if (requested != newBackend && scr.initialized) {
		const char* fallback = SCR_TextBackendToString(newBackend);
		Com_WPrintf("Requested text backend '%s' unavailable, falling back to %s fonts.\n",
			self->string, fallback);
		if (Q_stricmp(self->string, fallback))
			Cvar_Set(self->name, fallback);
	}
}

static void scr_text_backend_g(genctx_t* ctx)
{
	Prompt_AddMatch(ctx, "legacy");
	Prompt_AddMatch(ctx, "bitmap");
	Prompt_AddMatch(ctx, "ttf");
	Prompt_AddMatch(ctx, "freetype");
	Prompt_AddMatch(ctx, "kfont");
}
#else
static bool SCR_ShouldUseFreeType(qhandle_t)
{
	return false;
}
#endif
/*
===============================================================================

UTILS

===============================================================================
*/

namespace {

	struct scr_text_metrics_t {
		size_t  visibleChars{ 0 };
		color_t finalColor;
	};

	static scr_text_metrics_t SCR_TextMetrics(const char* s, size_t maxlen, int flags, color_t color)
	{
		scr_text_metrics_t metrics{ 0, color };

		const char* p = s;
		size_t remaining = maxlen;

		while (remaining && *p) {
			if (!(flags & UI_IGNORECOLOR)) {
				size_t consumed = 0;
				if (Q3_ParseColorEscape(p, remaining, metrics.finalColor, consumed)) {
					p += consumed;
					remaining -= consumed;
					continue;
				}
			}

			++metrics.visibleChars;
			++p;
			--remaining;
		}

		return metrics;
	}

} // namespace

static int SCR_MeasureKFontString(int scale, int flags, size_t maxlen, const char* s)
{
	if (!SCR_ShouldUseKFont())
		return 0;

	const int clampedScale = std::max(scale, 1);
	const char* p = s;
	size_t remaining = maxlen;
	int width = 0;
	color_t currentColor = COLOR_WHITE;

	while (remaining && *p) {
		if (!(flags & UI_IGNORECOLOR)) {
			size_t consumed = 0;
			if (Q3_ParseColorEscape(p, remaining, currentColor, consumed)) {
				p += consumed;
				remaining -= consumed;
				continue;
			}
		}

		if (*p == '\n')
			break;

		const kfont_char_t* ch = SCR_KFontLookup(&scr.kfont, static_cast<unsigned char>(*p++));
		--remaining;

		if (!ch)
			continue;

		width += ch->w * clampedScale;
	}

	return width;
}

static int SCR_DrawKFontStringLine(int x, int y, int scale, int flags, size_t maxlen,
	const char* s, color_t color)
{
	if (!SCR_ShouldUseKFont())
		return x;

	const int clampedScale = std::max(scale, 1);
	const char* p = s;
	size_t remaining = maxlen;
	color_t currentColor = color;

	while (remaining && *p) {
		if (!(flags & UI_IGNORECOLOR)) {
			size_t consumed = 0;
			if (Q3_ParseColorEscape(p, remaining, currentColor, consumed)) {
				p += consumed;
				remaining -= consumed;
				continue;
			}
		}

		if (*p == '\n')
			break;

		unsigned char ch = static_cast<unsigned char>(*p++);
		--remaining;

		x += R_DrawKFontChar(x, y, clampedScale, flags, ch, currentColor, &scr.kfont);
	}

	return x;
}

qhandle_t SCR_DefaultFontHandle(void)
{
#if USE_FREETYPE
	if (scr_activeTextBackend == scr_text_backend_mode::TTF && scr.freetype.activeFontHandle)
		return scr.freetype.activeFontHandle;
#endif
	return scr.font_pic;
}

int SCR_FontLineHeight(int scale, qhandle_t font)
{
	const int clampedScale = std::max(scale, 1);
#if USE_FREETYPE
	if (SCR_ShouldUseFreeType(font)) {
		if (const ftfont_t* ftFont = SCR_FTFontForHandle(font))
			return Q_rint(R_FreeTypeFontLineHeight(clampedScale, ftFont));
	}
#endif
	if (SCR_ShouldUseKFont() && scr.kfont.line_height)
		return scr.kfont.line_height * clampedScale;
	return CONCHAR_HEIGHT * clampedScale;
}

int SCR_MeasureString(int scale, int flags, size_t maxlen, const char* s, qhandle_t font)
{
	const auto metrics = SCR_TextMetrics(s, maxlen, flags, COLOR_WHITE);
	const size_t visibleChars = metrics.visibleChars;

#if USE_FREETYPE
	if (SCR_ShouldUseFreeType(font)) {
		if (const ftfont_t* ftFont = SCR_FTFontForHandle(font))
			return R_MeasureFreeTypeString(scale, flags, visibleChars, s, font, ftFont);
	}
#endif

	if (SCR_ShouldUseKFont())
		return SCR_MeasureKFontString(scale, flags, maxlen, s);

	return static_cast<int>(visibleChars) * CONCHAR_WIDTH * std::max(scale, 1);
}

/*
==============
SCR_DrawStringStretch
==============
*/
int SCR_DrawStringStretch(int x, int y, int scale, int flags, size_t maxlen,
	const char* s, color_t color, qhandle_t font)
{
	const auto metrics = SCR_TextMetrics(s, maxlen, flags, color);
	const size_t visibleChars = metrics.visibleChars;
	const int clampedScale = std::max(scale, 1);

#if USE_FREETYPE
	const bool useFreeType = SCR_ShouldUseFreeType(font);
	const ftfont_t* ftFont = useFreeType ? SCR_FTFontForHandle(font) : nullptr;
#else
	const bool useFreeType = false;
	const ftfont_t* ftFont = nullptr;
#endif

	if ((flags & UI_CENTER) == UI_CENTER) {
		int width = 0;
#if USE_FREETYPE
		if (useFreeType && ftFont)
			width = R_MeasureFreeTypeString(scale, flags & ~UI_MULTILINE, visibleChars, s, font, ftFont);
		else
#endif
			if (SCR_ShouldUseKFont())
				width = SCR_MeasureKFontString(clampedScale, flags & ~UI_MULTILINE, maxlen, s);
			else
				width = static_cast<int>(visibleChars) * CONCHAR_WIDTH * clampedScale;
		x -= width / 2;
	}
	else if (flags & UI_RIGHT) {
		int width = 0;
#if USE_FREETYPE
		if (useFreeType && ftFont)
			width = R_MeasureFreeTypeString(scale, flags & ~UI_MULTILINE, visibleChars, s, font, ftFont);
		else
#endif
			if (SCR_ShouldUseKFont())
				width = SCR_MeasureKFontString(clampedScale, flags & ~UI_MULTILINE, maxlen, s);
			else
				width = static_cast<int>(visibleChars) * CONCHAR_WIDTH * clampedScale;
		x -= width;
	}

	if (SCR_ShouldUseKFont())
		return SCR_DrawKFontStringLine(x, y, clampedScale, flags, maxlen, s, color);

#if USE_FREETYPE
	if (useFreeType && ftFont)
		return R_DrawFreeTypeString(x, y, scale, flags, maxlen, s, color, font, ftFont);
#endif

	return R_DrawStringStretch(x, y, scale, flags, maxlen, s, color, font, nullptr);
}


/*
==============
SCR_DrawStringMultiStretch
==============
*/
void SCR_DrawStringMultiStretch(int x, int y, int scale, int flags, size_t maxlen,
        const char* s, color_t color, qhandle_t font)
{
        const char* p;
        size_t  len;
	int     last_x = x;
	int     last_y = y;
	color_t currentColor = color;

	const int lineHeight = SCR_FontLineHeight(scale, font);

	while (*s && maxlen) {
		p = strchr(s, '\n');
		if (!p) {
			const auto metrics = SCR_TextMetrics(s, maxlen, flags, currentColor);
			last_x = SCR_DrawStringStretch(x, y, scale, flags, maxlen, s, currentColor, font);
			last_y = y;
			currentColor = metrics.finalColor;
			break;
		}

                len = std::min<size_t>(static_cast<size_t>(p - s), maxlen);
		const auto metrics = SCR_TextMetrics(s, len, flags, currentColor);
		last_x = SCR_DrawStringStretch(x, y, scale, flags, len, s, currentColor, font);
		last_y = y;
		currentColor = metrics.finalColor;
		maxlen -= len;

		y += lineHeight;
		s = p + 1;
	}

	if (flags & UI_DRAWCURSOR && com_localTime & BIT(8)) {
		const int clampedScale = std::max(scale, 1);
		if (SCR_ShouldUseKFont())
			R_DrawKFontChar(last_x, last_y, clampedScale, flags, 11, currentColor, &scr.kfont);
		else
			R_DrawStretchChar(last_x, last_y, CONCHAR_WIDTH * clampedScale, CONCHAR_HEIGHT * clampedScale,
				flags, 11, currentColor, font);
	}
}

void SCR_DrawGlyph(int x, int y, int scale, int flags, unsigned char glyph, color_t color)
{
        const int clampedScale = std::max(scale, 1);
        const unsigned char baseGlyph = glyph & 0x7f;
        const bool hasAltColor = (glyph & 0x80) != 0;

#if USE_FREETYPE
        const qhandle_t fontHandle = SCR_DefaultFontHandle();
        if (SCR_ShouldUseFreeType(fontHandle) && baseGlyph >= KFONT_ASCII_MIN) {
                char text[2] = { static_cast<char>(baseGlyph), '\0' };
                color_t finalColor = color;
                if (hasAltColor)
                        finalColor = ColorSetAlpha(COLOR_WHITE, color.a);
                SCR_DrawStringStretch(x, y, clampedScale, flags, 1, text, finalColor, fontHandle);
                return;
        }
#endif

        if (SCR_ShouldUseKFont() && baseGlyph >= KFONT_ASCII_MIN) {
                color_t finalColor = color;
                if (hasAltColor)
                        finalColor = ColorSetAlpha(COLOR_WHITE, color.a);
                R_DrawKFontChar(x, y, clampedScale, flags, baseGlyph, finalColor, &scr.kfont);
                return;
        }

        R_DrawStretchChar(x, y, CONCHAR_WIDTH * clampedScale, CONCHAR_HEIGHT * clampedScale,
                flags, glyph, color, scr.font_pic);
}

namespace {
	constexpr const char* SCR_LEGACY_FONT = "conchars";

	static bool SCR_BuildFontLookupPath(const char* font_name, char* buffer, size_t size)
	{
		if (!font_name || !*font_name || !size)
			return false;

		std::array<char, MAX_QPATH> quake_path{};
		size_t len = 0;

		if (font_name[0] == '/' || font_name[0] == '\\')
			len = FS_NormalizePathBuffer(quake_path.data(), font_name + 1, quake_path.size());
		else
			len = Q_concat(quake_path.data(), quake_path.size(), "pics/", font_name);

		if (!len || len >= quake_path.size())
			return false;

		len = COM_DefaultExtension(quake_path.data(), ".pcx", quake_path.size());
		if (len >= quake_path.size())
			return false;

		const int written = Q_snprintf(buffer, size, "%s/%s", fs_gamedir, quake_path.data());
		if (written < 0 || static_cast<size_t>(written) >= size)
			return false;

		return true;
	}
} // namespace

qhandle_t SCR_RegisterFontPath(const char* name)
{
	if (!name || !*name)
		return 0;
	if (name[0] == '/' || name[0] == '\\')
		return R_RegisterFont(name);
	if (strpbrk(name, "/\\"))
		return R_RegisterFont(va("/%s", name));
	return R_RegisterFont(name);
}

static void scr_font_changed(cvar_t* self)
{
	if (!cls.ref_initialized) {
		scr.font_pic = 0;
		return;
	}

	const char* loadedName = nullptr;
	const char* lastAttempt = nullptr;
	bool attemptedLegacy = false;
	std::array<const char*, 2> attempts{ self->string, nullptr };
	size_t attemptCount = 1;

	if (self->default_string && Q_stricmp(self->default_string, self->string))
		attempts[attemptCount++] = self->default_string;

	scr.font_pic = 0;

	for (size_t i = 0; i < attemptCount; ++i) {
		const char* candidate = attempts[i];
		if (!candidate || !*candidate)
			continue;

		if (!Q_stricmp(candidate, SCR_LEGACY_FONT))
			attemptedLegacy = true;

		lastAttempt = candidate;
		qhandle_t handle = SCR_RegisterFontPath(candidate);
		if (handle) {
			scr.font_pic = handle;
			loadedName = candidate;
			break;
		}
	}

	if (!scr.font_pic && !attemptedLegacy) {
		lastAttempt = SCR_LEGACY_FONT;
		scr.font_pic = SCR_RegisterFontPath(SCR_LEGACY_FONT);
		if (scr.font_pic)
			loadedName = SCR_LEGACY_FONT;
		attemptedLegacy = true;
	}

	if (!scr.font_pic) {
		const char* reason = Com_GetLastError();
		std::array<char, MAX_OSPATH> lookup_path{};
		const char* reportFont = lastAttempt;
		if (!reportFont || !*reportFont)
			reportFont = self->string[0] ? self->string : SCR_LEGACY_FONT;

		if (SCR_BuildFontLookupPath(reportFont, lookup_path.data(), lookup_path.size())) {
			if (reason && reason[0])
				Com_Error(ERR_FATAL, "%s: failed to load font '%s' (looked for '%s'): %s", __func__, reportFont, lookup_path.data(), reason);
			else
				Com_Error(ERR_FATAL, "%s: failed to load font '%s' (looked for '%s')", __func__, reportFont, lookup_path.data());
		} else {
			if (reason && reason[0])
				Com_Error(ERR_FATAL, "%s: failed to load font '%s': %s", __func__, reportFont, reason);
			else
				Com_Error(ERR_FATAL, "%s: failed to load font '%s'", __func__, reportFont);
		}

#if USE_FREETYPE
		SCR_LoadDefaultFreeTypeFont();
		if (scr_text_backend)
			scr_text_backend_changed(scr_text_backend);
#endif
		return;
	}
}
/*
=============
SCR_RefreshFontCvar

Refreshes the currently configured screen font from the associated cvar.
=============
*/
void SCR_RefreshFontCvar(void)
{
	if (scr_font)
		scr_font_changed(scr_font);
}

/*
=============
SCR_ApplyTextBackend

Ensures the active text backend matches the configured cvar value.
=============
*/
void SCR_ApplyTextBackend(void)
{
#if USE_FREETYPE
	if (scr_text_backend)
		scr_text_backend_changed(scr_text_backend);
#endif
}

/*
=============
SCR_InitFontSystem

Initializes font-related configuration cvars and callbacks.
=============
*/
void SCR_InitFontSystem(void)
{
#if USE_FREETYPE
	scr_font = Cvar_Get("scr_font", "/fonts/RobotoMono-Regular.ttf", 0);
#else
	scr_font = Cvar_Get("scr_font", SCR_LEGACY_FONT, 0);
#endif
	if (scr_font)
		scr_font->changed = scr_font_changed;
#if USE_FREETYPE
	scr_fontpath = Cvar_Get("scr_fontpath", "fonts", CVAR_ARCHIVE);
	scr_text_backend = Cvar_Get("scr_text_backend", "ttf", CVAR_ARCHIVE);
	if (scr_text_backend) {
		scr_text_backend->changed = scr_text_backend_changed;
		scr_text_backend->generator = scr_text_backend_g;
	}
#endif
}

void SCR_ShutdownFontSystem(void)
{
#if USE_FREETYPE
	SCR_FreeFreeTypeFonts();
	if (scr.freetype.library) {
		FT_Done_FreeType(scr.freetype.library);
		scr.freetype.library = nullptr;
	}
	scr.freetype.activeFontKey.clear();
	scr.freetype.activeFontHandle = 0;
#endif
}
