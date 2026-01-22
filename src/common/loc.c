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

#include "shared/shared.h"
#include "common/cvar.h"
#include "common/files.h"
#include "common/common.h"
#include "common/loc.h"

#ifdef _WIN32
#include <windows.h>
#endif

static cvar_t *loc_file;
static cvar_t *loc_language;

#define LOC_LANG_TAG_MAX    64

typedef struct {
    const char *name;
    const char *file;
    const char *aliases;
} loc_language_t;

static const loc_language_t loc_languages[] = {
    { "english", "localization/loc_english.txt", "en" },
    { "french", "localization/loc_french.txt", "fr" },
    { "german", "localization/loc_german.txt", "de" },
    { "italian", "localization/loc_italian.txt", "it" },
    { "spanish", "localization/loc_spanish.txt", "es" },
    { "russian", "localization/loc_russian.txt", "ru" },
    { "arabic", "localization/loc_arabic.txt", "ar" },
    { "bulgarian", "localization/loc_bulgarian.txt", "bg" },
    { "czech", "localization/loc_czech.txt", "cs;cz" },
    { "danish", "localization/loc_danish.txt", "da" },
    { "dutch", "localization/loc_dutch.txt", "nl" },
    { "finnish", "localization/loc_finnish.txt", "fi" },
    { "norwegian", "localization/loc_norwegian.txt", "no;nb;nn" },
    { "polish", "localization/loc_polish.txt", "pl" },
    { "portuguese-br", "localization/loc_portuguese_br.txt", "pt-br" },
    { "portuguese-pt", "localization/loc_portuguese_pt.txt", "pt-pt;pt" },
    { "swedish", "localization/loc_swedish.txt", "sv" },
    { "turkish", "localization/loc_turkish.txt", "tr" },
    { "chinese-traditional", "localization/loc_chinese_traditional.txt", "zh-hant;zh-hk;zh-mo;zh-tw" },
    { "chinese-simplified", "localization/loc_chinese_simplified.txt", "zh-hans;zh-cn;zh-sg;zh-my;zh" },
    { "japanese", "localization/loc_japanese.txt", "ja" },
    { "korean", "localization/loc_korean.txt", "ko" },
    { NULL, NULL, NULL }
};

static bool Loc_IsAutoLanguage(const char *value)
{
    return !value || !*value || !Q_strcasecmp(value, "auto") ||
           !Q_strcasecmp(value, "default");
}

static void Loc_NormalizeTag(char *out, const char *in, size_t out_size)
{
    size_t i = 0;

    if (!out_size) {
        return;
    }

    for (; *in && i + 1 < out_size; ++in) {
        char c = *in;
        if (c == '.' || c == '@') {
            break;
        }
        if (c == '_') {
            c = '-';
        }
        out[i++] = Q_tolower((unsigned char)c);
    }

    out[i] = '\0';
}

static bool Loc_AliasMatch(const char *tag, const char *aliases)
{
    const char *start = aliases;

    if (!tag || !*tag || !aliases || !*aliases) {
        return false;
    }

    while (*start) {
        const char *end = strchr(start, ';');
        size_t len = end ? (size_t)(end - start) : strlen(start);

        if (len > 0 && !strncmp(tag, start, len)) {
            if (tag[len] == '\0' || tag[len] == '-') {
                return true;
            }
        }

        if (!end) {
            break;
        }
        start = end + 1;
    }

    return false;
}

static const char *Loc_BaseName(const char *path)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    if (bslash && (!slash || bslash > slash)) {
        slash = bslash;
    }
#endif
    return slash ? slash + 1 : path;
}

static bool Loc_FileMatches(const char *tag, const char *file)
{
    char normalized[LOC_LANG_TAG_MAX];
    const char *basename = Loc_BaseName(file);

    Loc_NormalizeTag(normalized, basename, sizeof(normalized));
    return !strcmp(tag, normalized);
}

static const loc_language_t *Loc_FindLanguageByName(const char *name)
{
    char normalized[LOC_LANG_TAG_MAX];

    if (!name || !*name) {
        return NULL;
    }

    Loc_NormalizeTag(normalized, name, sizeof(normalized));

    if (!normalized[0]) {
        return NULL;
    }

    for (const loc_language_t *lang = loc_languages; lang->name; ++lang) {
        if (!strcmp(normalized, lang->name) ||
            Loc_AliasMatch(normalized, lang->aliases) ||
            Loc_FileMatches(normalized, lang->file)) {
            return lang;
        }
    }

    return NULL;
}

#ifdef _WIN32
static const char *Loc_GetWindowsLocaleTag(void)
{
    static char locale_tag[LOC_LANG_TAG_MAX];
    wchar_t wtag[LOCALE_NAME_MAX_LENGTH] = { 0 };
    int wlen = GetUserDefaultLocaleName(wtag, LOCALE_NAME_MAX_LENGTH);

    if (wlen <= 0) {
        return NULL;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, wtag, -1, locale_tag,
                            sizeof(locale_tag), NULL, NULL) <= 0) {
        return NULL;
    }

    Loc_NormalizeTag(locale_tag, locale_tag, sizeof(locale_tag));

    if (!locale_tag[0]) {
        return NULL;
    }

    return locale_tag;
}
#endif

static const loc_language_t *Loc_DetectLanguage(void)
{
#ifdef _WIN32
    const char *tag = Loc_GetWindowsLocaleTag();
    if (tag) {
        return Loc_FindLanguageByName(tag);
    }
#endif

    return NULL;
}

static const loc_language_t *Loc_SelectLanguage(void)
{
    const loc_language_t *lang = NULL;

    if (loc_language && !Loc_IsAutoLanguage(loc_language->string)) {
        lang = Loc_FindLanguageByName(loc_language->string);
        if (!lang) {
            Com_WPrintf("Unknown loc_language '%s', defaulting to English.\n",
                        loc_language->string);
        }
    } else {
        lang = Loc_DetectLanguage();
    }

    if (!lang) {
        lang = Loc_FindLanguageByName("english");
    }

    return lang;
}

static void Loc_ApplyLanguageSetting(bool force_auto)
{
    const loc_language_t *lang;

    if (!loc_language || !loc_file) {
        return;
    }

    if (!force_auto && Loc_IsAutoLanguage(loc_language->string) &&
        loc_file->default_string &&
        strcmp(loc_file->string, loc_file->default_string) != 0) {
        return;
    }

    lang = Loc_SelectLanguage();
    if (!lang) {
        return;
    }

    if (!FS_FileExists(lang->file)) {
        const loc_language_t *fallback = Loc_FindLanguageByName("english");
        if (fallback && FS_FileExists(fallback->file)) {
            lang = fallback;
        }
    }

    if (!lang || !lang->file) {
        return;
    }

    if (!strcmp(loc_file->string, lang->file)) {
        return;
    }

    Cvar_Set("loc_file", lang->file);
}

static const char *Loc_ResolvePath(char *out, size_t out_size)
{
    const char *path;
    size_t base_len;

    if (!loc_file || !loc_file->string[0]) {
        return NULL;
    }

    path = loc_file->string;
    if (FS_FileExists(path)) {
        return path;
    }

    base_len = strlen(BASEGAME);
    if (!Q_stricmpn(path, BASEGAME, base_len)) {
        const char *rest = path + base_len;
        if (*rest == '/' || *rest == '\\') {
            rest++;
            if (Q_strlcpy(out, rest, out_size) < out_size &&
                FS_FileExists(out)) {
                return out;
            }
        }
    }

    return path;
}

static void loc_file_changed(cvar_t *self)
{
    (void)self;
    Loc_ReloadFile();
}

static void loc_language_changed(cvar_t *self)
{
    (void)self;
    Loc_ApplyLanguageSetting(true);
}

#define MAX_LOC_KEY         64
#define MAX_LOC_FORMAT      1024
#define MAX_LOC_ARGS        8

// must be POT
#define LOC_HASH_SIZE        256

typedef struct {
    uint8_t     arg_index;
    uint16_t    start, end;
} loc_arg_t;

typedef struct loc_string_s {
    char    key[MAX_LOC_KEY];
    char    format[MAX_LOC_FORMAT];

    size_t      num_arguments;
    loc_arg_t   arguments[MAX_LOC_ARGS];

    struct loc_string_s    *next, *hash_next;
} loc_string_t;

static loc_string_t *loc_head;
static loc_string_t *loc_hash[LOC_HASH_SIZE];

static int loccmpfnc(const void *_a, const void *_b)
{
    const loc_arg_t *a = (const loc_arg_t *)_a;
    const loc_arg_t *b = (const loc_arg_t *)_b;
    return a->start - b->start;
}

/*
================
Loc_Parse
================
*/
static bool Loc_Parse(loc_string_t *loc)
{
    // if -1, a positional argument was encountered
    int32_t arg_index = 0;

    // parse out arguments
    size_t arg_rover = 0;

    size_t format_len = strlen(loc->format);

    while (true) {
        if (arg_rover >= format_len || !loc->format[arg_rover]) {
            break;
        }

        if (loc->format[arg_rover] == '{') {
            size_t arg_start = arg_rover;

            arg_rover++;

            if (loc->format[arg_rover] && loc->format[arg_rover] == '{') {
                continue; // escape sequence
            }

            // argument encountered
            if (loc->num_arguments == MAX_LOC_ARGS) {
                Com_SetLastError("too many arguments");
                return false;
            }

            loc_arg_t *arg = &loc->arguments[loc->num_arguments++];

            arg->start = arg_start;

            // check if we have a numerical value
            char *end_ptr;
            arg->arg_index = strtol(&loc->format[arg_rover], &end_ptr, 10);

            if (end_ptr == &loc->format[arg_rover]) {

                if (arg_index == -1) {
                    Com_SetLastError("encountered sequential argument, but has positional args");
                    return false;
                }

                // sequential
                arg->arg_index = arg_index++;
            } else {

                // positional
                if (arg_index > 0) {
                    Com_SetLastError("encountered positional argument, but has sequential args");
                    return false;
                }

                // mark us off so we can't find sequentials
                arg_index = -1;
            }

            // find the end of this argument
            arg_rover = (end_ptr - loc->format) - 1;

            while (true) {

                if (arg_rover >= format_len || !loc->format[arg_rover]) {
                    Com_SetLastError("EOF before end of argument found");
                    return false;
                }

                arg_rover++;
                
                if (loc->format[arg_rover] != '}') {
                    continue;
                }

                size_t arg_end = arg_rover;

                arg_rover++;

                if (loc->format[arg_rover] && loc->format[arg_rover] == '}') {
                    continue; // escape sequence
                }

                // we found it
                arg->end = arg_end + 1;
                break;
            }
        } else
            arg_rover++;
    }

    if (loc->num_arguments) {
        // sort the arguments by start position
        qsort(loc->arguments, loc->num_arguments, sizeof(loc_arg_t), loccmpfnc);
    }

    return true;
}

// just as best guess as to whether the given in-place string
// has arguments to parse or not. fmt uses {{ as escape, so
// we can assume any { that isn't followed by a { means we
// have an arg.
static bool Loc_HasArguments(const char *base)
{
    for (const char *rover = base; *rover; rover++) {
        if (*rover == '{') {
            rover++;

            if (!*rover) {
                return false;
            } else if (*rover != '{') {
                return true;
            }
        }
    }

    return false;
}

// find the given loc_string_t from the hashed list of localized
// strings
static const loc_string_t *Loc_Find(const char *base)
{
    // find loc via hash
    uint32_t hash = Com_HashString(base, LOC_HASH_SIZE) & (LOC_HASH_SIZE - 1);

    if (!loc_hash[hash]) {
        return NULL;
    }

    for (const loc_string_t *str = loc_hash[hash]; str != NULL; str = str->hash_next) {
        if (!strcmp(str->key, base)) {
            return str;
        }
    }

    return NULL;
}

size_t Loc_Localize(const char *base, bool allow_in_place, const char **arguments, size_t num_arguments, char *output, size_t output_length)
{
    Q_assert(base);

    static loc_string_t in_place_loc;
    const loc_string_t *str;

    // re-release supports two types of localizations - ones in
    // the loc file (prefixed with $) and in-place localizations
    // that are formatted at runtime.
    if (!allow_in_place) {
        if (*base != '$') {
            return Q_strlcpy(output, base, output_length);
        }

        base++;
        str = Loc_Find(base);
    } else {
        if (*base == '$') {
            base++;
            str = Loc_Find(base);
        } else if (Loc_HasArguments(base)) {
            in_place_loc.num_arguments = 0;
            Q_strlcpy(in_place_loc.format, base, sizeof(in_place_loc.format));

            if (!Loc_Parse(&in_place_loc)) {
                Com_WPrintf("in-place localization of \"%s\" failed: %s\n", base, Com_GetLastError());
                return Q_strlcpy(output, base, output_length);
            }

            str = &in_place_loc;
        } else {
            return Q_strlcpy(output, base, output_length);
        }
    }

    if (!str) {
        return Q_strlcpy(output, base, output_length);
    }

    // easy case
    if (!str->num_arguments) {
        return Q_strlcpy(output, str->format, output_length);
    }

    // check args
    for (size_t i = 0; i < str->num_arguments; i++) {
        if (str->arguments[i].arg_index >= num_arguments) {
            Com_WPrintf("%s: base \"%s\" localized with too few arguments\n", __func__, base);
            return Q_strlcpy(output, base, output_length);
        }
    }
    
    for (size_t i = 0; i < num_arguments; i++) {
        if (!arguments[i]) {
            Com_WPrintf("%s: invalid argument at position %zu\n", __func__, i);
            return Q_strlcpy(output, base, output_length);
        }
    }

    // fill prefix if we have one
    const loc_arg_t *arg = &str->arguments[0];

    Q_strnlcpy(output, str->format, arg->start, output_length);

    static char localized_arg[MAX_STRING_CHARS];

    for (size_t i = 0; i < str->num_arguments - 1; i++) {

        Loc_Localize(arguments[arg->arg_index], false, NULL, 0, localized_arg, sizeof(localized_arg));
        Q_strlcat(output, localized_arg, output_length);

        const loc_arg_t *next_arg = &str->arguments[i + 1];

        Q_strnlcat(output, str->format + arg->end, next_arg->start - arg->end, output_length);

        arg = next_arg;
    }
    
    Loc_Localize(arguments[arg->arg_index], false, NULL, 0, localized_arg, sizeof(localized_arg));
    Q_strlcat(output, localized_arg, output_length);

    return Q_strlcat(output, str->format + arg->end, output_length);
}

static void Loc_Unload(void)
{
    if (!loc_head) {
        return;
    }

    for (loc_string_t *rover = loc_head; rover; ) {
        loc_string_t *next = rover->next;
        Z_Free(rover);
        rover = next;
    }

    loc_head = NULL;
    memset(loc_hash, 0, sizeof(loc_hash));
}

/*
================
Loc_ReloadFile
================
*/
void Loc_ReloadFile(void)
{
    Loc_Unload();

    char *buffer = NULL;
    char resolved_path[MAX_OSPATH];
    const char *load_path = Loc_ResolvePath(resolved_path, sizeof(resolved_path));

    if (!load_path) {
        return;
    }

    FS_LoadFile(load_path, (void**)&buffer);

    if (!buffer) {
        const loc_language_t *fallback = Loc_FindLanguageByName("english");
        if (fallback && strcmp(load_path, fallback->file)) {
            Com_WPrintf("Localization file '%s' not found, falling back to English.\n",
                        load_path);
            load_path = fallback->file;
            FS_LoadFile(load_path, (void**)&buffer);
        }
    }

    if (!buffer) {
        return;
    }

    size_t num_locs = 0;

    loc_string_t **tail = &loc_head;

    const char *parse_buf = buffer;

    while (true) {
        loc_string_t loc; 

        COM_ParseToken(&parse_buf, loc.key, sizeof(loc.key), PARSE_FLAG_NONE);

        if (!*loc.key) {
            break;
        }

        const char *equals = COM_Parse(&parse_buf);
        bool has_platform_spec = false;

        // check for console specs
        if (!*equals) {
            break;
        } else if (*equals == '<') {
            has_platform_spec = true;

            // skip these for now
            while (*equals && equals[strlen(equals) - 1] != '>') {
                equals = COM_Parse(&parse_buf);
            }

            equals = COM_Parse(&parse_buf);
        }

        // syntax error
        if (strcmp(equals, "=")) {
            break;
        }

        COM_ParseToken(&parse_buf, loc.format, sizeof(loc.format), PARSE_FLAG_ESCAPE);

        // skip platform specifiers
        if (has_platform_spec) {
            continue;
        }

        loc.num_arguments = 0;
        loc.next = loc.hash_next = NULL;

        if (!Loc_Parse(&loc)) {
            goto line_error;
        }

        // link us in and copy off
        *tail = Z_Malloc(sizeof(loc_string_t));
        memcpy(*tail, &loc, sizeof(loc));

        // hash
        uint32_t hash = Com_HashString(loc.key, LOC_HASH_SIZE) & (LOC_HASH_SIZE - 1);
        if (!loc_hash[hash]) {
            loc_hash[hash] = *tail;
        } else {
            (*tail)->hash_next = loc_hash[hash];
            loc_hash[hash] = *tail;
        }

        tail = &((*tail)->next);

        num_locs++;
        continue;

line_error:
        Com_WPrintf("%s (%s): %s\n", load_path, loc.key, Com_GetLastError());
    }

	FS_FreeFile(buffer);

    Com_Printf("Loaded %zu localization strings\n", num_locs);
}

/*
================
Loc_Init
================
*/
void Loc_Init(void)
{
    loc_file = Cvar_Get("loc_file", "localization/loc_english.txt", 0);
    loc_language = Cvar_Get("loc_language", "auto", CVAR_ARCHIVE);

    Loc_ApplyLanguageSetting(!Loc_IsAutoLanguage(loc_language->string));

    loc_file->changed = loc_file_changed;
    loc_language->changed = loc_language_changed;

    Loc_ReloadFile();
}
