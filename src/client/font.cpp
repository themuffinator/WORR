#include "client.hpp"
#include "common/q3colors.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
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
		R_ReleaseFreeTypeFont(&entry.second.renderInfo);
	}

	scr.freetype.fonts.clear();
	scr.freetype.handleLookup.clear();
	scr.freetype.activeFontKey.clear();
	scr.freetype.activeFontHandle = 0;
}

static std::string SCR_NormalizeFontPath(const std::string& rawPath)
{
	if (rawPath.empty())
		return std::string();

	std::string sanitized = rawPath;
	std::replace(sanitized.begin(), sanitized.end(), '\\', '/');

	const char* input = sanitized.c_str();
	if (!sanitized.empty() && (sanitized.front() == '/' || sanitized.front() == '\\'))
		++input;

	std::array<char, MAX_QPATH> normalized{};
	size_t len = FS_NormalizePathBuffer(normalized.data(), input, normalized.size());
	if (len == 0 || len >= normalized.size())
		return std::string();

	len = COM_DefaultExtension(normalized.data(), ".ttf", normalized.size());
	if (len >= normalized.size())
		return std::string();

	const char* ext = COM_FileExtension(normalized.data());
	if (!Q_stricmp(ext, "pcx")) {
		if (len < 3)
			return std::string();
		normalized[len - 3] = 't';
		normalized[len - 2] = 't';
		normalized[len - 1] = 'f';
	}

	return std::string(normalized.data());
}

static std::string SCR_FindSystemFontPath(const std::string& requestedPath)
{
	const char* fileName = COM_SkipPath(requestedPath.c_str());
	if (!fileName || !*fileName)
		return std::string();

	std::vector<std::string> searchRoots;

#ifdef _WIN32
	if (const char* winDir = std::getenv("WINDIR")) {
		if (*winDir) {
			std::string fontsDir = winDir;
			std::replace(fontsDir.begin(), fontsDir.end(), '\\', '/');
			if (!fontsDir.empty() && fontsDir.back() != '/')
				fontsDir.push_back('/');
			fontsDir += "Fonts";
			searchRoots.emplace_back(std::move(fontsDir));
		}
	}
#endif

	searchRoots.emplace_back("/usr/share/fonts");
	searchRoots.emplace_back("/usr/local/share/fonts");
	searchRoots.emplace_back("/Library/Fonts");
	searchRoots.emplace_back("/System/Library/Fonts");
	searchRoots.emplace_back("/System/Library/Fonts/Supplemental");

	for (const auto& root : searchRoots) {
		if (root.empty())
			continue;

		std::string candidate = root;
		if (!candidate.empty() && candidate.back() != '/')
			candidate.push_back('/');
		candidate += fileName;

		std::array<char, MAX_OSPATH> normalizedCandidate{};
		size_t len = FS_NormalizePathBuffer(normalizedCandidate.data(), candidate.c_str(), normalizedCandidate.size());
		if (!len || len >= normalizedCandidate.size())
			continue;

		if (FS_FileExistsEx(normalizedCandidate.data(), FS_TYPE_REAL))
			return std::string(normalizedCandidate.data());
	}

	return std::string();
}

static bool SCR_LoadFreeTypeFont(const std::string& cacheKey, const std::string& fontPath,
	int pixelHeight, qhandle_t handle)
{
	if (!SCR_EnsureFreeTypeLibrary())
		return false;

	const std::string normalizedFontPath = SCR_NormalizeFontPath(fontPath);
	if (normalizedFontPath.empty()) {
		Com_Printf("SCR: failed to normalize font path '%s'\n", fontPath.c_str());
		return false;
	}

	std::string q2FontPath = normalizedFontPath;
	bool preferQ2Game = false;

	if (!fontPath.empty() && (fontPath.front() == '/' || fontPath.front() == '\\'))
		preferQ2Game = true;
	if (!preferQ2Game && !normalizedFontPath.empty() &&
	    !Q_stricmpn(normalizedFontPath.c_str(), CONST_STR_LEN("fonts/")))
		preferQ2Game = true;

	if (preferQ2Game && !q2FontPath.empty() && q2FontPath.front() != '/')
		q2FontPath.insert(q2FontPath.begin(), '/');

	const std::string *openPath = preferQ2Game ? &q2FontPath : &normalizedFontPath;
	std::string fallbackSystemFontPath;
	auto makeDisplayFontPath = [](const std::string& path) {
		std::string display = path;
		if (display.empty())
			return display;
		if (display.front() == '/')
			return display;
#ifdef _WIN32
		if (display.size() > 1 && display[1] == ':')
			return display;
#endif
		display.insert(display.begin(), '/');
		return display;
	};
	std::string displayFontPath = makeDisplayFontPath(*openPath);

	auto cached = scr.freetype.fonts.find(cacheKey);
	if (cached != scr.freetype.fonts.end()) {
		Com_DPrintf("SCR: using cached TrueType font '%s' for handle %d (pixel height %d)\n",
			displayFontPath.c_str(), handle, pixelHeight);

		cached->second.renderInfo.pixelHeight = pixelHeight;
		if (R_AcquireFreeTypeFont(handle, &cached->second.renderInfo)) {
			scr.freetype.handleLookup[handle] = cacheKey;
			scr.freetype.activeFontKey = cacheKey;
			scr.freetype.activeFontHandle = handle;
			return true;
		}

		Com_Printf("SCR: cached font '%s' could not be acquired for handle %d, reloading from source\n",
			displayFontPath.c_str(), handle);

		R_ReleaseFreeTypeFont(&cached->second.renderInfo);
		cached->second.renderInfo.face = nullptr;
		cached->second.renderInfo.driverData = nullptr;
		cached->second.renderInfo.ascent = 0;
		cached->second.renderInfo.descent = 0;
		cached->second.renderInfo.lineHeight = 0;

		for (auto it = scr.freetype.handleLookup.begin(); it != scr.freetype.handleLookup.end(); ) {
			if (it->second == cacheKey)
				it = scr.freetype.handleLookup.erase(it);
			else
				++it;
		}

		if (scr.freetype.activeFontKey == cacheKey) {
			scr.freetype.activeFontKey.clear();
			scr.freetype.activeFontHandle = 0;
		}

		scr.freetype.fonts.erase(cached);
	}

	Com_DPrintf("SCR: cache miss for '%s' (key '%s'), loading from source\n",
		displayFontPath.c_str(), cacheKey.c_str());

	qhandle_t fileHandle = 0;
	int64_t fileLength = FS_OpenFile(openPath->c_str(), &fileHandle, FS_MODE_READ);
	if (fileLength < 0) {
		Com_Printf("SCR: failed to open font '%s' (%s)\n",
			displayFontPath.c_str(), Q_ErrorString(fileLength));
		FS_LogFileLookup(openPath->c_str(), FS_MODE_READ, "        ");

		fallbackSystemFontPath = SCR_FindSystemFontPath(normalizedFontPath);
		if (!fallbackSystemFontPath.empty()) {
			int64_t fallbackLength = FS_OpenFile(fallbackSystemFontPath.c_str(), &fileHandle,
				FS_MODE_READ | FS_TYPE_REAL);
			if (fallbackLength >= 0) {
				fileLength = fallbackLength;
				openPath = &fallbackSystemFontPath;
				displayFontPath = makeDisplayFontPath(fallbackSystemFontPath);
			} else {
				Com_Printf("SCR: failed to open system font '%s' (%s)\n",
					fallbackSystemFontPath.c_str(), Q_ErrorString(fallbackLength));
			}
		} else {
			const char* baseName = COM_SkipPath(normalizedFontPath.c_str());
			Com_Printf("SCR: system font '%s' not found in fallback directories\n",
				(baseName && *baseName) ? baseName : normalizedFontPath.c_str());
		}

		if (fileLength < 0)
			return false;
	}

	fs_file_source_t source{};
	if (!FS_GetFileSource(fileHandle, &source)) {
		FS_CloseFile(fileHandle);
		Com_Printf("SCR: failed to resolve source for font '%s'\n", displayFontPath.c_str());
		return false;
	}

	if (source.from_pack && !source.from_builtin) {
		const char* packBaseName = COM_SkipPath(source.pack_path);
		const bool isExpectedPack = packBaseName
			&& (!Q_stricmp(packBaseName, "Q2Game.kpf"));
		if (!isExpectedPack) {
			const std::string unexpectedPack = source.pack_path;

			FS_CloseFile(fileHandle);
			fileHandle = 0;

			int64_t filesystemLength = FS_OpenFile(normalizedFontPath.c_str(), &fileHandle,
				FS_MODE_READ | FS_TYPE_REAL);
			if (filesystemLength < 0) {
				Com_Printf("SCR: font '%s' must be provided by Q2Game.kpf (found '%s')\n",
					displayFontPath.c_str(), unexpectedPack.c_str());
				return false;
			}

			fileLength = filesystemLength;
			openPath = &normalizedFontPath;
			displayFontPath = makeDisplayFontPath(*openPath);

			if (!FS_GetFileSource(fileHandle, &source)) {
				FS_CloseFile(fileHandle);
				Com_Printf("SCR: failed to resolve source for font '%s'\n", displayFontPath.c_str());
				return false;
			}

			if (source.from_pack) {
				const char* resolvedPack = source.pack_path[0] ? source.pack_path : "<unknown>";
				FS_CloseFile(fileHandle);
				Com_Printf("SCR: font '%s' still resolved to pack '%s' after filesystem retry\n",
					displayFontPath.c_str(), resolvedPack);
				return false;
			}

			Com_DPrintf("SCR: overriding font '%s' from pack '%s' with filesystem copy\n",
				displayFontPath.c_str(), unexpectedPack.c_str());
		}
	}

	if (fileLength <= 0 || fileLength > std::numeric_limits<size_t>::max()) {
		FS_CloseFile(fileHandle);
		Com_Printf("SCR: invalid length for font '%s'\n", displayFontPath.c_str());
		return false;
	}

	scr_freetype_font_entry_t entry;
	entry.buffer.resize(static_cast<size_t>(fileLength));
	int bytesRead = FS_Read(entry.buffer.data(), entry.buffer.size(), fileHandle);
	if (bytesRead < 0) {
		FS_CloseFile(fileHandle);
		Com_Printf("SCR: failed to read font '%s' (%s)\n", displayFontPath.c_str(), Q_ErrorString(bytesRead));
		return false;
	}

	if (static_cast<size_t>(bytesRead) != entry.buffer.size()) {
		FS_CloseFile(fileHandle);
		Com_Printf("SCR: short read while loading font '%s'\n", displayFontPath.c_str());
		return false;
	}

	FS_CloseFile(fileHandle);

	entry.renderInfo.face = nullptr;
	entry.renderInfo.pixelHeight = pixelHeight;
	entry.renderInfo.driverData = nullptr;
	entry.renderInfo.ascent = 0;
	entry.renderInfo.descent = 0;
	entry.renderInfo.lineHeight = 0;

	scr_freetype_font_entry_t* loadedEntry = nullptr;
	auto existing = scr.freetype.fonts.find(cacheKey);
	if (existing != scr.freetype.fonts.end()) {
		R_ReleaseFreeTypeFont(&existing->second.renderInfo);
		existing->second = std::move(entry);
		if (!R_AcquireFreeTypeFont(handle, &existing->second.renderInfo)) {
			Com_Printf("SCR: renderer rejected FreeType font '%s' for handle %d\n",
			        displayFontPath.c_str(), handle);
			R_ReleaseFreeTypeFont(&existing->second.renderInfo);
			scr.freetype.fonts.erase(existing);
			return false;
		}
		loadedEntry = &existing->second;
	} else {
		auto emplaced = scr.freetype.fonts.emplace(cacheKey, std::move(entry));
		auto it = emplaced.first;
		if (!R_AcquireFreeTypeFont(handle, &it->second.renderInfo)) {
			Com_Printf("SCR: renderer rejected FreeType font '%s' for handle %d\n",
			        displayFontPath.c_str(), handle);
			R_ReleaseFreeTypeFont(&it->second.renderInfo);
			scr.freetype.fonts.erase(it);
			return false;
		}
		loadedEntry = &it->second;
	}
	scr.freetype.handleLookup[handle] = cacheKey;
	scr.freetype.activeFontKey = cacheKey;
	scr.freetype.activeFontHandle = handle;

	const char* entryName = source.entry_path[0] ? source.entry_path : openPath->c_str();
	const char* packName = source.from_builtin ? "builtin" : source.pack_path;
	if (!packName || !packName[0])
		packName = "filesystem";

	Com_Printf("SCR: loaded TrueType font '%s' (%d px) from %s:%s (%zu bytes)\n",
		displayFontPath.c_str(), pixelHeight, packName, entryName,
		loadedEntry ? loadedEntry->buffer.size() : 0u);

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
void SCR_RefreshFontCvar(void)
{
	if (scr_font)
		scr_font_changed(scr_font);
}

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
		scr_text_backend_changed(scr_text_backend);
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
