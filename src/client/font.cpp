#include "client.hpp"
#include "common/q3colors.hpp"

#include <algorithm>
#include <array>
#include <cmath>
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
static cvar_t* scr_font_size = nullptr;
static cvar_t* scr_fontpath = nullptr;
static cvar_t* scr_text_backend = nullptr;
static FT_Error scr_lastFreeTypeError = 0;

static void SCR_FreeFreeTypeFonts(void);
static bool SCR_LoadDefaultFreeTypeFont(qhandle_t handle);
static const ftfont_t* SCR_FTFontForHandle(qhandle_t handle);
#endif

namespace {
	constexpr const char* SCR_LEGACY_FONT = "conchars";
}

static qhandle_t SCR_RegisterFontPathInternal(const char* name, bool allowFreeTypeBaseCreation);

// nb: this is dumb but C doesn't allow
// `(T) { }` to count as a constant


#if USE_FREETYPE
/*
=============
SCR_CalculateFontSizeDefault

Determines the default pixel height for FreeType fonts based on the current screen scale.
=============
*/
static int SCR_CalculateFontSizeDefault(void)
{
	const int baseSize = 16;
	int scale = 1;

	if (cvar_t* scaleVar = Cvar_FindVar("scr_scale")) {
		if (scaleVar->value > 0.0f) {
			const float clamped = std::clamp(scaleVar->value, 1.0f, 6.0f);
			scale = static_cast<int>(std::lround(clamped));
			if (scale < 1)
				scale = 1;
		}
		else {
			scale = get_auto_scale();
			if (scale < 1)
				scale = 1;
		}
	}

	return baseSize * scale;
}

/*
=============
SCR_CurrentFontPixelHeight

Returns the pixel height that should be used for the active FreeType font.
=============
*/
static int SCR_CurrentFontPixelHeight(void)
{
	if (scr_font_size && scr_font_size->integer > 0)
		return scr_font_size->integer;

	return SCR_CalculateFontSizeDefault();
}

/*
=============
SCR_UpdateFontSizeDefault

Synchronizes the scr_font_size default string with the current screen scale.
=============
*/
static void SCR_UpdateFontSizeDefault(void)
{
	if (!scr_font_size)
		return;

	const int computedDefault = SCR_CalculateFontSizeDefault();
	char buffer[16];
	Q_snprintf(buffer, sizeof(buffer), "%d", computedDefault);

	const char* defaultString = scr_font_size->default_string ? scr_font_size->default_string : "";
	const bool valueMatchesDefault = (scr_font_size->string && !strcmp(scr_font_size->string, defaultString));

	if (scr_font_size->default_string)
		Z_Free(scr_font_size->default_string);
	scr_font_size->default_string = Z_CvarCopyString(buffer);

	if (valueMatchesDefault)
		Cvar_SetByVar(scr_font_size, buffer, FROM_CODE);
}

/*
=============
SCR_RefreshFontSizeDefault

Recomputes the default screen font size based on the current scale settings.
=============
*/
void SCR_RefreshFontSizeDefault(void)
{
	SCR_UpdateFontSizeDefault();
}

static bool SCR_EnsureFreeTypeLibrary()
{
	if (scr.freetype.library)
		return true;

	FT_Error error = FT_Init_FreeType(&scr.freetype.library);
	if (error) {
		scr_lastFreeTypeError = error;
		Com_SetLastError(va("Failed to initialize FreeType (%d)", error));
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
	scr_lastFreeTypeError = 0;
	Com_SetLastError(nullptr);
	if (!SCR_EnsureFreeTypeLibrary())
		return false;

	void* fileBuffer = nullptr;
	int length = FS_LoadFile(fontPath.c_str(), &fileBuffer);
	if (length <= 0) {
		Com_SetLastError(va("Failed to load font '%s': %s", fontPath.c_str(), Q_ErrorString(length)));
		Com_Printf("SCR: failed to load font '%s' (%s)\n", fontPath.c_str(), Q_ErrorString(length));
		return false;
	}

	scr_freetype_font_entry_t entry;
	entry.buffer.resize(length);
	std::memcpy(entry.buffer.data(), fileBuffer, length);
	FS_FreeFile(fileBuffer);

	FT_Face face = nullptr;
	FT_Error error = FT_New_Memory_Face(scr.freetype.library, entry.buffer.data(), length, 0, &face);
	if (error) {
		scr_lastFreeTypeError = error;
		Com_SetLastError(va("FreeType failed to create font face (%d)", error));
		Com_Printf("SCR: failed to create FreeType face for '%s' (error %d)\n", fontPath.c_str(), error);
		return false;
	}

	error = FT_Set_Pixel_Sizes(face, 0, pixelHeight);
	if (error) {
		scr_lastFreeTypeError = error;
		Com_SetLastError(va("FreeType failed to set pixel height %d (%d)", pixelHeight, error));
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

/*
=============
SCR_LoadDefaultFreeTypeFont

Loads the default FreeType font into the provided renderer handle.
=============
*/
static bool SCR_LoadDefaultFreeTypeFont(qhandle_t handle)
{
	const int defaultPixelHeight = SCR_CurrentFontPixelHeight();

	std::string fontPath;
	if (scr_fontpath && scr_fontpath->string[0]) {
		fontPath = scr_fontpath->string;
		if (!fontPath.empty() && fontPath.back() != '/' && fontPath.back() != '\\')
			fontPath.push_back('/');
	}
	fontPath += "RobotoMono-Regular.ttf";

	std::string cacheKey = "RobotoMono-Regular-" + std::to_string(defaultPixelHeight);

	if (!handle)
		return false;

	if (!SCR_LoadFreeTypeFont(cacheKey, fontPath, defaultPixelHeight, handle))
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

#if !USE_FREETYPE
/*
=============
SCR_RefreshFontSizeDefault
=============
*/
void SCR_RefreshFontSizeDefault(void)
{
}
#endif

#if USE_FREETYPE
/*
=============
SCR_IsTrueTypeFontPath

Returns whether the provided path points to a TrueType/OpenType font.
=============
*/
static bool SCR_IsTrueTypeFontPath(const char* path)
{
	if (!path || !*path)
		return false;

	const char* extension = COM_FileExtension(path);
	if (!*extension)
		return false;

	if (*extension == '.')
		++extension;

	return !Q_stricmp(extension, "ttf") || !Q_stricmp(extension, "otf");
}

/*
=============
SCR_RegisterTrueTypeFontPath

Attempts to load a TrueType/OpenType font using FreeType and returns its renderer handle.
=============
*/
static qhandle_t SCR_RegisterTrueTypeFontPath(const char* path, bool allowBaseHandleCreation, qhandle_t preferredHandle)
{
	if (!path || !*path)
		return 0;

	std::array<char, MAX_QPATH> normalized{};
	size_t normalizedLen = 0;

	if (path[0] == '/' || path[0] == '\')
		normalizedLen = FS_NormalizePathBuffer(normalized.data(), path + 1, normalized.size());
	else
		normalizedLen = FS_NormalizePathBuffer(normalized.data(), path, normalized.size());

	if (!normalizedLen || normalizedLen >= normalized.size())
		return 0;

	const int pixelHeight = SCR_CurrentFontPixelHeight();
	std::string cacheKey(normalized.data());
	cacheKey.push_back('-');
	cacheKey += std::to_string(pixelHeight);

	qhandle_t targetHandle = preferredHandle;
	if (!targetHandle) {
		if (scr.freetype.activeFontHandle)
			targetHandle = scr.freetype.activeFontHandle;
		else if (scr.font_pic)
			targetHandle = scr.font_pic;
	}

	if (!targetHandle && allowBaseHandleCreation)
		targetHandle = SCR_RegisterFontPathInternal(SCR_LEGACY_FONT, false);

	if (!targetHandle)
		return 0;

	if (SCR_LoadFreeTypeFont(cacheKey, normalized.data(), pixelHeight, targetHandle))
		return targetHandle;

	return 0;
}

#endif // USE_FREETYPE

/*
=============
SCR_RegisterFontPathInternal

Internal helper that registers a font path while optionally permitting FreeType handle creation.
=============
*/
static qhandle_t SCR_RegisterFontPathInternal(const char* name, bool allowFreeTypeBaseCreation)
{
	if (!name || !*name)
		return 0;

#if USE_FREETYPE
	if (SCR_IsTrueTypeFontPath(name))
		return SCR_RegisterTrueTypeFontPath(name, allowFreeTypeBaseCreation, 0);
#endif

	if (name[0] == '/' || name[0] == '\')
		return R_RegisterFont(name);
	if (strpbrk(name, "/\\"))
		return R_RegisterFont(va("/%s", name));
	return R_RegisterFont(name);
}
#if USE_FREETYPE

enum class scr_text_backend_mode {
        LEGACY,
        TTF,
        KFONT,
};

static scr_text_backend_mode scr_activeTextBackend = scr_text_backend_mode::LEGACY;
#if USE_FREETYPE
static bool scr_reportedFreeTypeFailure = false;

/*
=============
SCR_CreateFreeTypeBaseHandle

Ensures a renderer font handle exists for FreeType text rendering.
=============
*/
static qhandle_t SCR_CreateFreeTypeBaseHandle(void)
{
        if (scr.font_pic)
                return scr.font_pic;

        const char* candidates[] = {
                (scr_font && SCR_IsTrueTypeFontPath(scr_font->string)) ? scr_font->string : nullptr,
                "/fonts/RobotoMono-Regular.ttf",
                nullptr,
        };

	for (const char* candidate : candidates) {
		if (!candidate || !*candidate)
			continue;

		qhandle_t handle = SCR_RegisterFontPathInternal(candidate, true);
		if (handle)
			return handle;
        }

        return 0;
}
#endif

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
	bool loggedFallback = false;

	if (requested == scr_text_backend_mode::TTF) {
		const char* failureReason = nullptr;
		bool fallbackRequired = false;
		const qhandle_t originalFontHandle = scr.font_pic;
		bool createdBaseHandle = false;
		qhandle_t targetHandle = scr.freetype.activeFontHandle ? scr.freetype.activeFontHandle : scr.font_pic;

		if (!targetHandle) {
			targetHandle = SCR_CreateFreeTypeBaseHandle();
			if (targetHandle) {
				scr.font_pic = targetHandle;
				createdBaseHandle = true;
			}
			else {
				failureReason = "base font handle unavailable";
				fallbackRequired = true;
			}
		}

		if (!fallbackRequired && !scr.freetype.activeFontHandle) {
			if (!SCR_LoadDefaultFreeTypeFont(targetHandle))
				failureReason = "failed to load default FreeType font";
		}

		if (!scr.freetype.activeFontHandle) {
			if (!failureReason)
				failureReason = "no active FreeType font handle";
			fallbackRequired = true;
		}
		else {
			scr_reportedFreeTypeFailure = false;
			scr.font_pic = scr.freetype.activeFontHandle;
		}

		if (fallbackRequired) {
			const auto fallback = SCR_SelectFallbackTextBackend();
			const char* fallbackName = SCR_TextBackendToString(fallback);
			if (!scr_reportedFreeTypeFailure) {
				if (failureReason)
					Com_WPrintf("SCR: %s; using %s text backend instead\n", failureReason, fallbackName);
				else
					Com_WPrintf("SCR: FreeType font unavailable; using %s text backend instead\n", fallbackName);
				scr_reportedFreeTypeFailure = true;
			}
			if (scr.freetype.activeFontHandle)
				scr.freetype.handleLookup.erase(scr.freetype.activeFontHandle);
			scr.freetype.activeFontKey.clear();
			scr.freetype.activeFontHandle = 0;
			if (createdBaseHandle)
				scr.font_pic = originalFontHandle;
			newBackend = fallback;
			loggedFallback = true;
		}
	}
	else if (requested == scr_text_backend_mode::KFONT) {
		if (!scr.kfont.pic)
			newBackend = scr_text_backend_mode::LEGACY;
	}

	scr_activeTextBackend = newBackend;

	if (requested != newBackend) {
		const char* fallback = SCR_TextBackendToString(newBackend);
		if (!loggedFallback)
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

/*
=============
SCR_RegisterFontPath

Registers a font by path, delegating to FreeType for TrueType/OpenType sources when available.
=============
*/
qhandle_t SCR_RegisterFontPath(const char* name)
{
	return SCR_RegisterFontPathInternal(name, true);
}

/*
=============
scr_font_size_changed

Reloads the active FreeType font when the configured pixel height changes.
=============
*/
static void scr_font_size_changed(cvar_t* self)
{
	if (!self)
		return;

	SCR_UpdateFontSizeDefault();

	if (self->integer <= 0) {
		const int fallback = SCR_CalculateFontSizeDefault();
		char buffer[16];
		Q_snprintf(buffer, sizeof(buffer), "%d", fallback);
		Cvar_SetByVar(self, buffer, FROM_CODE);
		return;
	}

	if (!cls.ref_initialized)
		return;

	const qhandle_t activeHandle = scr.freetype.activeFontHandle;
	const std::string previousKey = scr.freetype.activeFontKey;
	int previousPixelHeight = 0;

	if (!previousKey.empty()) {
		const auto it = scr.freetype.fonts.find(previousKey);
		if (it != scr.freetype.fonts.end()) {
			previousPixelHeight = it->second.renderInfo.pixelHeight;
			if (it->second.renderInfo.face) {
				FT_Done_Face(it->second.renderInfo.face);
				it->second.renderInfo.face = nullptr;
			}
			R_ReleaseFreeTypeFont(&it->second.renderInfo);
			scr.freetype.fonts.erase(it);
		}
	}

	if (activeHandle && previousPixelHeight > 0)
		R_FreeTypeInvalidateFontSize(activeHandle, previousPixelHeight);

	if (activeHandle)
		scr.freetype.handleLookup.erase(activeHandle);

	scr.freetype.activeFontKey.clear();

	if (scr_font)
		scr_font_changed(scr_font);
}

/*
=============
scr_font_changed

Handles updates to the screen font configuration and loads the requested asset.
=============
*/
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

#if USE_FREETYPE
	const qhandle_t previousHandle = scr.font_pic;
	qhandle_t freetypeHandle = scr.freetype.activeFontHandle ? scr.freetype.activeFontHandle : previousHandle;
#endif

	scr.font_pic = 0;

	for (size_t i = 0; i < attemptCount; ++i) {
		const char* candidate = attempts[i];
		if (!candidate || !*candidate)
			continue;

#if USE_FREETYPE
		if (SCR_IsTrueTypeFontPath(candidate)) {
			lastAttempt = candidate;

			const qhandle_t targetHandle = freetypeHandle ? freetypeHandle : previousHandle;
			qhandle_t handle = SCR_RegisterTrueTypeFontPath(candidate, true, targetHandle);
			if (handle) {
				scr.font_pic = handle;
				loadedName = candidate;
				scr_activeTextBackend = scr_text_backend_mode::TTF;
				return;
			}

			continue;
		}
#endif

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
		const bool hasReason = reason && Q_stricmp(reason, "No error");
		const char* reportFont = lastAttempt;
		if (!reportFont || !*reportFont)
			reportFont = self->string[0] ? self->string : SCR_LEGACY_FONT;

#if USE_FREETYPE
		const bool attemptedTTF = reportFont && SCR_IsTrueTypeFontPath(reportFont);
		if (attemptedTTF) {
			std::string resolvedPath = reportFont;
			if (scr_fontpath && scr_fontpath->string[0] && reportFont[0] != '/' && reportFont[0] != '\\') {
				resolvedPath = scr_fontpath->string;
				if (!resolvedPath.empty() && resolvedPath.back() != '/' && resolvedPath.back() != '\\')
					resolvedPath.push_back('/');
				resolvedPath += reportFont;
			}
			if (hasReason)
				Com_WPrintf("%s: failed to load TrueType font '%s' (looked for '%s'; FreeType error %d): %s. Attempting fallbacks.\n",
						__func__, reportFont, resolvedPath.c_str(), static_cast<int>(scr_lastFreeTypeError), reason);
			else
				Com_WPrintf("%s: failed to load TrueType font '%s' (looked for '%s'; FreeType error %d). Attempting fallbacks.\n",
						__func__, reportFont, resolvedPath.c_str(), static_cast<int>(scr_lastFreeTypeError));
		}
		else
#endif
		{
			std::array<char, MAX_OSPATH> lookup_path{};
			if (SCR_BuildFontLookupPath(reportFont, lookup_path.data(), lookup_path.size())) {
				if (hasReason)
					Com_WPrintf("%s: failed to load font '%s' (looked for '%s'): %s. Attempting fallbacks.\n",
						__func__, reportFont, lookup_path.data(), reason);
				else
					Com_WPrintf("%s: failed to load font '%s' (looked for '%s'). Attempting fallbacks.\n",
						__func__, reportFont, lookup_path.data());
			}
			else {
				if (hasReason)
					Com_WPrintf("%s: failed to load font '%s': %s. Attempting fallbacks.\n",
						__func__, reportFont, reason);
				else
					Com_WPrintf("%s: failed to load font '%s'. Attempting fallbacks.\n", __func__, reportFont);
			}
		}
#if USE_FREETYPE
		const qhandle_t originalHandle = scr.font_pic;
		qhandle_t fallbackHandle = freetypeHandle ? freetypeHandle : previousHandle;
		if (fallbackHandle)
			scr.font_pic = fallbackHandle;
		if (fallbackHandle && SCR_LoadDefaultFreeTypeFont(fallbackHandle) && scr.freetype.activeFontHandle) {
			scr.font_pic = scr.freetype.activeFontHandle;
			scr_activeTextBackend = scr_text_backend_mode::TTF;
			return;
		}
		scr.font_pic = originalHandle;
#endif

		if (scr.kfont.pic) {
			scr_activeTextBackend = scr_text_backend_mode::KFONT;
			return;
		}

		scr_activeTextBackend = scr_text_backend_mode::LEGACY;
		scr.font_pic = SCR_RegisterFontPath(SCR_LEGACY_FONT);
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
SCR_ReportActiveFontStatus

Outputs the currently active text backend, font identifier, and pixel height for diagnostics.
=============
*/
static void SCR_ReportActiveFontStatus(void)
{
	std::string fontKey;
	int pixelHeight = 0;
	const char* backendName = "legacy";

#if USE_FREETYPE
	backendName = SCR_TextBackendToString(scr_activeTextBackend);

	if (scr_activeTextBackend == scr_text_backend_mode::TTF) {
		if (!scr.freetype.activeFontKey.empty())
			fontKey = scr.freetype.activeFontKey;
		else if (scr_font && scr_font->string && *scr_font->string)
			fontKey = scr_font->string;

		auto fontIt = scr.freetype.fonts.find(scr.freetype.activeFontKey);
		if (fontIt != scr.freetype.fonts.end() && fontIt->second.renderInfo.pixelHeight > 0)
			pixelHeight = fontIt->second.renderInfo.pixelHeight;
		else
			pixelHeight = SCR_CurrentFontPixelHeight();
	}
	else if (scr_activeTextBackend == scr_text_backend_mode::KFONT) {
		fontKey = "/fonts/qconfont.kfont";
		pixelHeight = scr.kfont.line_height ? scr.kfont.line_height : CONCHAR_HEIGHT;
	}
	else {
		fontKey = SCR_LEGACY_FONT;
		pixelHeight = CONCHAR_HEIGHT;
	}
#else
	fontKey = SCR_LEGACY_FONT;
	pixelHeight = CONCHAR_HEIGHT;
#endif

	if (fontKey.empty()) {
		if (scr_font && scr_font->string && *scr_font->string)
			fontKey = scr_font->string;
		else
			fontKey = SCR_LEGACY_FONT;
	}

	if (pixelHeight <= 0)
		pixelHeight = CONCHAR_HEIGHT;

	Com_Printf("scr_font_reload: backend=%s font=%s pixelHeight=%d\n", backendName, fontKey.c_str(), pixelHeight);
}

/*
=============
SCR_FontReload_f

Re-applies font-related configuration cvars so changes take effect immediately.
=============
*/
static void SCR_FontReload_f(void)
{
#if USE_FREETYPE
	SCR_UpdateFontSizeDefault();
	if (scr_font_size)
		scr_font_size_changed(scr_font_size);
	else
		SCR_RefreshFontCvar();
	SCR_ApplyTextBackend();
#else
	SCR_RefreshFontCvar();
#endif

	SCR_ReportActiveFontStatus();
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
	char fontSizeDefault[16];
	Q_snprintf(fontSizeDefault, sizeof(fontSizeDefault), "%d", SCR_CalculateFontSizeDefault());
	scr_font_size = Cvar_Get("scr_font_size", fontSizeDefault, CVAR_ARCHIVE);
#else
	scr_font = Cvar_Get("scr_font", SCR_LEGACY_FONT, 0);
#endif
	if (scr_font)
		scr_font->changed = scr_font_changed;
#if USE_FREETYPE
	if (scr_font_size)
		scr_font_size->changed = scr_font_size_changed;
	scr_fontpath = Cvar_Get("scr_fontpath", "fonts", CVAR_ARCHIVE);
	scr_text_backend = Cvar_Get("scr_text_backend", "ttf", CVAR_ARCHIVE);
	if (scr_text_backend) {
		scr_text_backend->changed = scr_text_backend_changed;
		scr_text_backend->generator = scr_text_backend_g;
	}
	SCR_UpdateFontSizeDefault();
#endif
	Cmd_AddCommand("scr_font_reload", SCR_FontReload_f);
}

void SCR_ShutdownFontSystem(void)
{
	Cmd_RemoveCommand("scr_font_reload");
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
