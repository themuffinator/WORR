#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winhttp.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <bcrypt.h>
#include <shellapi.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STATIC
#include "common/jsmn.h"

#include "miniz.h"

#define MAX_STR            256
#define MAX_URL            1024
#define MAX_PRESERVE       64
#define MAX_PATH_UTF8      512

#define WMU_STATUS   (WM_APP + 1)
#define WMU_PROGRESS (WM_APP + 2)
#define WMU_DONE     (WM_APP + 3)

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef enum {
    STAGE_DOWNLOAD = 0,
    STAGE_UPDATE,
    STAGE_INSTALL
} progress_stage_t;

typedef struct {
    int stage;
    uint64_t current;
    uint64_t total;
} progress_msg_t;

typedef struct {
    char repo[MAX_STR];
    char channel[MAX_STR];
    char manifest_asset[MAX_STR];
    char package_asset[MAX_STR];
    char launch_exe[MAX_STR];
    int autolaunch_default;
    int allow_prerelease;
    char preserve[MAX_PRESERVE][MAX_STR];
    size_t preserve_count;
} config_t;

typedef struct {
    char path[MAX_PATH_UTF8];
    char sha256[65];
    uint64_t size;
} manifest_file_t;

typedef struct {
    char version[MAX_STR];
    char package_name[MAX_STR];
    char package_sha256[65];
    uint64_t package_size;
    manifest_file_t *files;
    size_t file_count;
} manifest_t;

typedef struct {
    char tag[MAX_STR];
    char manifest_url[MAX_URL];
    char package_url[MAX_URL];
} release_info_t;

typedef enum {
    VERSION_KIND_UNKNOWN = 0,
    VERSION_KIND_SEMVER,
    VERSION_KIND_REVISION
} version_kind_t;

typedef struct {
    version_kind_t kind;
    int major;
    int minor;
    int patch;
    int revision;
    int has_prerelease;
    char prerelease[MAX_STR];
} version_t;

typedef struct {
    HWND hwnd;
    HWND status;
    HWND progress_download;
    HWND progress_update;
    HWND progress_install;
    HWND checkbox_autolaunch;
    HWND button_cancel;
    volatile LONG cancel_requested;
    volatile LONG update_done;
} ui_state_t;

typedef struct {
    ui_state_t *ui;
    wchar_t install_dir[MAX_PATH];
    int argc;
    wchar_t **argv;
} update_context_t;

typedef struct {
    wchar_t **items;
    size_t count;
    size_t capacity;
} path_list_t;

static const wchar_t *kWindowClass = L"WORR_UpdateBootstrapper";
static const wchar_t *kConfigName = L"worr_update.json";

static const config_t kDefaultConfig = {
    "themuffinator/WORR-2",
    "stable",
    "worr-client-win64.json",
    "worr-client-win64.zip",
    "worr.exe",
    1,
    0,
    {
        "worr_update.json",
        "worr_updater.exe",
        "baseq2/*.cfg",
        "baseq2/autoexec.cfg",
        "baseq2/config.cfg",
        "baseq2/saves/*",
        "baseq2/screenshots/*",
        "baseq2/demos/*",
        "baseq2/logs/*"
    },
    9
};
static void ui_post_status(ui_state_t *ui, const wchar_t *text)
{
    size_t len = wcslen(text) + 1;
    wchar_t *copy = (wchar_t *)calloc(len, sizeof(wchar_t));
    if (!copy) {
        return;
    }
    wcscpy_s(copy, len, text);
    PostMessageW(ui->hwnd, WMU_STATUS, 0, (LPARAM)copy);
}

static void ui_post_progress(ui_state_t *ui, int stage, uint64_t current, uint64_t total)
{
    progress_msg_t *msg = (progress_msg_t *)calloc(1, sizeof(progress_msg_t));
    if (!msg) {
        return;
    }
    msg->stage = stage;
    msg->current = current;
    msg->total = total;
    PostMessageW(ui->hwnd, WMU_PROGRESS, 0, (LPARAM)msg);
}

static void ui_post_done(ui_state_t *ui, int code)
{
    PostMessageW(ui->hwnd, WMU_DONE, (WPARAM)code, 0);
}

static int is_cancelled(ui_state_t *ui)
{
    return InterlockedCompareExchange(&ui->cancel_requested, 0, 0) != 0;
}

static int json_token_streq(const char *json, const jsmntok_t *tok, const char *s)
{
    size_t len = (size_t)(tok->end - tok->start);
    return tok->type == JSMN_STRING && strlen(s) == len
        && strncmp(json + tok->start, s, len) == 0;
}

static int json_skip(const jsmntok_t *tokens, int count, int index)
{
    if (index >= count) {
        return index;
    }

    const jsmntok_t *tok = &tokens[index];
    int i = index + 1;

    if (tok->type == JSMN_ARRAY) {
        for (int elem = 0; elem < tok->size; ++elem) {
            i = json_skip(tokens, count, i);
        }
        return i;
    }

    if (tok->type == JSMN_OBJECT) {
        for (int pair = 0; pair < tok->size; ++pair) {
            i = json_skip(tokens, count, i);
            i = json_skip(tokens, count, i);
        }
        return i;
    }

    return i;
}

static int json_object_find(const char *json, const jsmntok_t *tokens, int count, int obj_index, const char *key)
{
    if (obj_index < 0 || obj_index >= count) {
        return -1;
    }

    const jsmntok_t *obj = &tokens[obj_index];
    if (obj->type != JSMN_OBJECT) {
        return -1;
    }

    int i = obj_index + 1;
    for (int pair = 0; pair < obj->size; ++pair) {
        if (json_token_streq(json, &tokens[i], key)) {
            return i + 1;
        }
        i = json_skip(tokens, count, i + 1);
    }

    return -1;
}

static int json_copy_string(const char *json, const jsmntok_t *tok, char *out, size_t out_len)
{
    size_t len = (size_t)(tok->end - tok->start);
    if (tok->type != JSMN_STRING || len >= out_len) {
        return 0;
    }
    memcpy(out, json + tok->start, len);
    out[len] = '\0';
    return 1;
}

static int json_read_uint64(const char *json, const jsmntok_t *tok, uint64_t *out)
{
    char buf[32];
    size_t len = (size_t)(tok->end - tok->start);
    if (tok->type != JSMN_PRIMITIVE || len >= sizeof(buf)) {
        return 0;
    }
    memcpy(buf, json + tok->start, len);
    buf[len] = '\0';
    *out = _strtoui64(buf, NULL, 10);
    return 1;
}

static int json_read_bool(const char *json, const jsmntok_t *tok, int *out)
{
    size_t len = (size_t)(tok->end - tok->start);
    if (tok->type != JSMN_PRIMITIVE) {
        return 0;
    }
    if (len == 4 && strncmp(json + tok->start, "true", 4) == 0) {
        *out = 1;
        return 1;
    }
    if (len == 5 && strncmp(json + tok->start, "false", 5) == 0) {
        *out = 0;
        return 1;
    }
    return 0;
}

static wchar_t *utf8_to_wide(const char *text)
{
    int len = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (len <= 0) {
        return NULL;
    }
    wchar_t *out = (wchar_t *)calloc((size_t)len, sizeof(wchar_t));
    if (!out) {
        return NULL;
    }
    MultiByteToWideChar(CP_UTF8, 0, text, -1, out, len);
    return out;
}

static char *wide_to_utf8(const wchar_t *text)
{
    int len = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (len <= 0) {
        return NULL;
    }
    char *out = (char *)calloc((size_t)len, sizeof(char));
    if (!out) {
        return NULL;
    }
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out, len, NULL, NULL);
    return out;
}

static void normalize_slashes(char *path)
{
    for (char *p = path; *p; ++p) {
        if (*p == '/') {
            *p = '\\';
        }
    }
}

static void normalize_slashes_w(wchar_t *path)
{
    for (wchar_t *p = path; *p; ++p) {
        if (*p == L'/') {
            *p = L'\\';
        }
    }
}

static void trim_whitespace(char *text)
{
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\r' || text[len - 1] == '\n' || text[len - 1] == ' ' || text[len - 1] == '\t')) {
        text[len - 1] = '\0';
        len--;
    }
}

static int load_file_to_memory(const wchar_t *path, char **out_buf, size_t *out_size)
{
    FILE *fp = _wfopen(path, L"rb");
    if (!fp) {
        return 0;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return 0;
    }
    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        return 0;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return 0;
    }
    char *buf = (char *)malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        return 0;
    }
    size_t read = fread(buf, 1, (size_t)size, fp);
    fclose(fp);
    if (read != (size_t)size) {
        free(buf);
        return 0;
    }
    buf[size] = '\0';
    *out_buf = buf;
    *out_size = (size_t)size;
    return 1;
}

static int load_config(const wchar_t *install_dir, config_t *config)
{
    wchar_t path[MAX_PATH];
    swprintf_s(path, ARRAY_COUNT(path), L"%s\\%s", install_dir, kConfigName);

    *config = kDefaultConfig;

    char *json = NULL;
    size_t json_size = 0;
    if (!load_file_to_memory(path, &json, &json_size)) {
        return 1;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json, json_size, NULL, 0);
    if (token_count <= 0) {
        free(json);
        return 0;
    }
    jsmntok_t *tokens = (jsmntok_t *)calloc((size_t)token_count, sizeof(jsmntok_t));
    if (!tokens) {
        free(json);
        return 0;
    }
    jsmn_init(&parser);
    if (jsmn_parse(&parser, json, json_size, tokens, token_count) < 0) {
        free(tokens);
        free(json);
        return 0;
    }

    int root = 0;
    int idx;

    idx = json_object_find(json, tokens, token_count, root, "repo");
    if (idx > 0) {
        json_copy_string(json, &tokens[idx], config->repo, sizeof(config->repo));
    }

    idx = json_object_find(json, tokens, token_count, root, "channel");
    if (idx > 0) {
        json_copy_string(json, &tokens[idx], config->channel, sizeof(config->channel));
    }

    idx = json_object_find(json, tokens, token_count, root, "manifest_asset");
    if (idx > 0) {
        json_copy_string(json, &tokens[idx], config->manifest_asset, sizeof(config->manifest_asset));
    }

    idx = json_object_find(json, tokens, token_count, root, "package_asset");
    if (idx > 0) {
        json_copy_string(json, &tokens[idx], config->package_asset, sizeof(config->package_asset));
    }

    idx = json_object_find(json, tokens, token_count, root, "launch_exe");
    if (idx > 0) {
        json_copy_string(json, &tokens[idx], config->launch_exe, sizeof(config->launch_exe));
    }

    idx = json_object_find(json, tokens, token_count, root, "autolaunch");
    if (idx > 0) {
        json_read_bool(json, &tokens[idx], &config->autolaunch_default);
    }

    idx = json_object_find(json, tokens, token_count, root, "allow_prerelease");
    if (idx > 0) {
        json_read_bool(json, &tokens[idx], &config->allow_prerelease);
    }

    idx = json_object_find(json, tokens, token_count, root, "preserve");
    if (idx > 0 && tokens[idx].type == JSMN_ARRAY) {
        config->preserve_count = 0;
        int i = idx + 1;
        for (int elem = 0; elem < tokens[idx].size && config->preserve_count < MAX_PRESERVE; ++elem) {
            if (tokens[i].type == JSMN_STRING) {
                json_copy_string(json, &tokens[i], config->preserve[config->preserve_count], MAX_STR);
                config->preserve_count++;
            }
            i = json_skip(tokens, token_count, i);
        }
    }

    free(tokens);
    free(json);
    return 1;
}
static int parse_version(const char *text, version_t *out)
{
    memset(out, 0, sizeof(*out));

    if (!text || !*text) {
        return 0;
    }

    if (text[0] == 'r') {
        char *end = NULL;
        long rev = strtol(text + 1, &end, 10);
        if (end && end != text + 1) {
            out->kind = VERSION_KIND_REVISION;
            out->revision = (int)rev;
            return 1;
        }
    }

    int major = 0, minor = 0, patch = 0;
    char prerelease[MAX_STR] = {0};
    const char *start = text;
    if (*start == 'v' || *start == 'V') {
        start++;
    }

    if (sscanf_s(start, "%d.%d.%d", &major, &minor, &patch) >= 2) {
        out->kind = VERSION_KIND_SEMVER;
        out->major = major;
        out->minor = minor;
        out->patch = patch;

        const char *dash = strchr(start, '-');
        const char *plus = strchr(start, '+');
        if (dash && (!plus || dash < plus)) {
            size_t len = plus ? (size_t)(plus - dash - 1) : strlen(dash + 1);
            if (len >= sizeof(prerelease)) {
                len = sizeof(prerelease) - 1;
            }
            memcpy(prerelease, dash + 1, len);
            prerelease[len] = '\0';
            out->has_prerelease = 1;
            strcpy_s(out->prerelease, sizeof(out->prerelease), prerelease);
        }
        return 1;
    }

    return 0;
}

static int compare_versions(const version_t *a, const version_t *b)
{
    if (a->kind == VERSION_KIND_SEMVER && b->kind == VERSION_KIND_SEMVER) {
        if (a->major != b->major) {
            return (a->major < b->major) ? -1 : 1;
        }
        if (a->minor != b->minor) {
            return (a->minor < b->minor) ? -1 : 1;
        }
        if (a->patch != b->patch) {
            return (a->patch < b->patch) ? -1 : 1;
        }
        if (a->has_prerelease != b->has_prerelease) {
            return a->has_prerelease ? -1 : 1;
        }
        if (a->has_prerelease) {
            int cmp = strcmp(a->prerelease, b->prerelease);
            if (cmp != 0) {
                return (cmp < 0) ? -1 : 1;
            }
        }
        return 0;
    }

    if (a->kind == VERSION_KIND_REVISION && b->kind == VERSION_KIND_REVISION) {
        if (a->revision == b->revision) {
            return 0;
        }
        return (a->revision < b->revision) ? -1 : 1;
    }

    if (a->kind == VERSION_KIND_UNKNOWN && b->kind == VERSION_KIND_UNKNOWN) {
        return 0;
    }

    if (a->kind == VERSION_KIND_UNKNOWN) {
        return -1;
    }
    if (b->kind == VERSION_KIND_UNKNOWN) {
        return 1;
    }

    if (a->kind == VERSION_KIND_REVISION && b->kind == VERSION_KIND_SEMVER) {
        return -1;
    }
    if (a->kind == VERSION_KIND_SEMVER && b->kind == VERSION_KIND_REVISION) {
        return 1;
    }

    return 0;
}

static int get_file_version_utf8(const wchar_t *path, char *out, size_t out_len)
{
    DWORD handle = 0;
    DWORD size = GetFileVersionInfoSizeW(path, &handle);
    if (size == 0) {
        return 0;
    }

    BYTE *data = (BYTE *)malloc(size);
    if (!data) {
        return 0;
    }

    if (!GetFileVersionInfoW(path, handle, size, data)) {
        free(data);
        return 0;
    }

    struct LANGANDCODEPAGE {
        WORD wLanguage;
        WORD wCodePage;
    } *translate = NULL;

    UINT translate_len = 0;
    wchar_t subblock[64];

    if (VerQueryValueW(data, L"\\VarFileInfo\\Translation", (LPVOID *)&translate, &translate_len)
        && translate_len >= sizeof(*translate)) {
        swprintf_s(subblock, ARRAY_COUNT(subblock),
            L"\\StringFileInfo\\%04x%04x\\ProductVersion",
            translate[0].wLanguage, translate[0].wCodePage);
    } else {
        swprintf_s(subblock, ARRAY_COUNT(subblock),
            L"\\StringFileInfo\\040904B0\\ProductVersion");
    }

    wchar_t *version_w = NULL;
    UINT version_len = 0;

    if (!VerQueryValueW(data, subblock, (LPVOID *)&version_w, &version_len) || version_len == 0) {
        swprintf_s(subblock, ARRAY_COUNT(subblock),
            L"\\StringFileInfo\\040904B0\\FileVersion");
        VerQueryValueW(data, subblock, (LPVOID *)&version_w, &version_len);
    }

    int ok = 0;
    if (version_w && version_len > 0) {
        char *utf8 = wide_to_utf8(version_w);
        if (utf8) {
            strncpy_s(out, out_len, utf8, _TRUNCATE);
            ok = 1;
            free(utf8);
        }
    }

    free(data);
    return ok;
}

static int sha256_file_hex(const wchar_t *path, char out_hex[65], uint64_t *out_size)
{
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    DWORD hash_object_len = 0;
    DWORD hash_len = 0;
    DWORD data_len = 0;
    NTSTATUS status;

    status = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    if (status != 0) {
        return 0;
    }

    status = BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&hash_object_len, sizeof(hash_object_len), &data_len, 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return 0;
    }

    status = BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hash_len, sizeof(hash_len), &data_len, 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        return 0;
    }

    BYTE *hash_object = (BYTE *)malloc(hash_object_len);
    BYTE *hash_value = (BYTE *)malloc(hash_len);
    if (!hash_object || !hash_value) {
        free(hash_object);
        free(hash_value);
        BCryptCloseAlgorithmProvider(alg, 0);
        return 0;
    }

    status = BCryptCreateHash(alg, &hash, hash_object, hash_object_len, NULL, 0, 0);
    if (status != 0) {
        free(hash_object);
        free(hash_value);
        BCryptCloseAlgorithmProvider(alg, 0);
        return 0;
    }

    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) {
        BCryptDestroyHash(hash);
        free(hash_object);
        free(hash_value);
        BCryptCloseAlgorithmProvider(alg, 0);
        return 0;
    }

    LARGE_INTEGER size;
    if (GetFileSizeEx(file, &size) && out_size) {
        *out_size = (uint64_t)size.QuadPart;
    }

    BYTE buffer[64 * 1024];
    DWORD bytes_read = 0;
    while (ReadFile(file, buffer, sizeof(buffer), &bytes_read, NULL) && bytes_read > 0) {
        status = BCryptHashData(hash, buffer, bytes_read, 0);
        if (status != 0) {
            CloseHandle(file);
            BCryptDestroyHash(hash);
            free(hash_object);
            free(hash_value);
            BCryptCloseAlgorithmProvider(alg, 0);
            return 0;
        }
    }

    CloseHandle(file);

    status = BCryptFinishHash(hash, hash_value, hash_len, 0);
    if (status != 0) {
        BCryptDestroyHash(hash);
        free(hash_object);
        free(hash_value);
        BCryptCloseAlgorithmProvider(alg, 0);
        return 0;
    }

    for (DWORD i = 0; i < hash_len; ++i) {
        sprintf_s(out_hex + (i * 2), 3, "%02x", hash_value[i]);
    }
    out_hex[hash_len * 2] = '\0';

    BCryptDestroyHash(hash);
    free(hash_object);
    free(hash_value);
    BCryptCloseAlgorithmProvider(alg, 0);
    return 1;
}
static int http_get_internal(const wchar_t *url, FILE *out_file, char **out_buf, size_t *out_size,
    ui_state_t *ui, int stage)
{
    URL_COMPONENTS components = {0};
    wchar_t host[256];
    wchar_t path[MAX_URL];

    components.dwStructSize = sizeof(components);
    components.lpszHostName = host;
    components.dwHostNameLength = ARRAY_COUNT(host);
    components.lpszUrlPath = path;
    components.dwUrlPathLength = ARRAY_COUNT(path);

    if (!WinHttpCrackUrl(url, 0, 0, &components)) {
        return 0;
    }

    HINTERNET session = WinHttpOpen(L"WORR-Updater/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session) {
        return 0;
    }

    HINTERNET connect = WinHttpConnect(session, host, components.nPort, 0);
    if (!connect) {
        WinHttpCloseHandle(session);
        return 0;
    }

    DWORD flags = (components.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET request = WinHttpOpenRequest(connect, L"GET", path, NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!request) {
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return 0;
    }

    WinHttpAddRequestHeaders(request, L"Accept: application/vnd.github+json\r\n", -1,
        WINHTTP_ADDREQ_FLAG_ADD);
    WinHttpAddRequestHeaders(request, L"Accept-Encoding: identity\r\n", -1,
        WINHTTP_ADDREQ_FLAG_ADD);

    if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return 0;
    }

    if (!WinHttpReceiveResponse(request, NULL)) {
        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);
        return 0;
    }

    DWORD status_code = 0;
    DWORD status_size = sizeof(status_code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

    if (status_code >= 300 && status_code < 400) {
        wchar_t location[MAX_URL] = {0};
        DWORD location_len = sizeof(location);
        if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
            location, &location_len, WINHTTP_NO_HEADER_INDEX)) {
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return http_get_internal(location, out_file, out_buf, out_size, ui, stage);
        }
    }

    DWORD content_length = 0;
    DWORD length_size = sizeof(content_length);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &content_length, &length_size, WINHTTP_NO_HEADER_INDEX)) {
        if (ui && stage >= 0) {
            ui_post_progress(ui, stage, 0, content_length);
        }
    }

    DWORD bytes_available = 0;
    DWORD total_read = 0;
    char *buffer = NULL;
    size_t buffer_size = 0;

    for (;;) {
        if (!WinHttpQueryDataAvailable(request, &bytes_available)) {
            break;
        }
        if (bytes_available == 0) {
            break;
        }

        if (!out_file) {
            size_t new_size = buffer_size + bytes_available;
            char *temp = (char *)realloc(buffer, new_size + 1);
            if (!temp) {
                break;
            }
            buffer = temp;
        }

        DWORD bytes_read = 0;
        char *write_ptr = out_file ? (char *)malloc(bytes_available) : buffer + buffer_size;
        if (!write_ptr) {
            break;
        }

        if (!WinHttpReadData(request, write_ptr, bytes_available, &bytes_read)) {
            if (out_file) {
                free(write_ptr);
            }
            break;
        }

        if (bytes_read == 0) {
            if (out_file) {
                free(write_ptr);
            }
            break;
        }

        if (out_file) {
            fwrite(write_ptr, 1, bytes_read, out_file);
            free(write_ptr);
        } else {
            buffer_size += bytes_read;
        }

        total_read += bytes_read;
        if (ui && stage >= 0) {
            ui_post_progress(ui, stage, total_read, content_length ? content_length : total_read);
        }

        if (ui && is_cancelled(ui)) {
            break;
        }
    }

    if (!out_file && buffer) {
        buffer[buffer_size] = '\0';
        *out_buf = buffer;
        *out_size = buffer_size;
    }

    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);

    if (ui && is_cancelled(ui)) {
        if (buffer && !out_file) {
            free(buffer);
        }
        return 0;
    }

    if (out_file) {
        return 1;
    }

    return buffer != NULL;
}

static int http_download_to_file(const wchar_t *url, const wchar_t *path, ui_state_t *ui, int stage)
{
    FILE *fp = _wfopen(path, L"wb");
    if (!fp) {
        return 0;
    }

    int ok = http_get_internal(url, fp, NULL, NULL, ui, stage);
    fclose(fp);
    return ok;
}

static int parse_release_object(const char *json, const jsmntok_t *tokens, int token_count, int root,
    const config_t *config, release_info_t *out)
{
    int idx = json_object_find(json, tokens, token_count, root, "tag_name");
    if (idx > 0) {
        json_copy_string(json, &tokens[idx], out->tag, sizeof(out->tag));
    }

    idx = json_object_find(json, tokens, token_count, root, "assets");
    if (idx > 0 && tokens[idx].type == JSMN_ARRAY) {
        int i = idx + 1;
        for (int elem = 0; elem < tokens[idx].size; ++elem) {
            if (tokens[i].type == JSMN_OBJECT) {
                int name_idx = json_object_find(json, tokens, token_count, i, "name");
                int url_idx = json_object_find(json, tokens, token_count, i, "browser_download_url");
                char name[MAX_STR] = {0};
                char url[MAX_URL] = {0};

                if (name_idx > 0) {
                    json_copy_string(json, &tokens[name_idx], name, sizeof(name));
                }
                if (url_idx > 0) {
                    json_copy_string(json, &tokens[url_idx], url, sizeof(url));
                }

                if (name[0] && url[0]) {
                    if (_stricmp(name, config->manifest_asset) == 0) {
                        strcpy_s(out->manifest_url, sizeof(out->manifest_url), url);
                    } else if (_stricmp(name, config->package_asset) == 0) {
                        strcpy_s(out->package_url, sizeof(out->package_url), url);
                    }
                }
            }
            i = json_skip(tokens, token_count, i);
        }
    }

    return out->manifest_url[0] != '\0';
}

static int parse_release_json(const char *json, size_t json_size, const config_t *config, release_info_t *out)
{
    jsmn_parser parser;
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json, json_size, NULL, 0);
    if (token_count <= 0) {
        return 0;
    }

    jsmntok_t *tokens = (jsmntok_t *)calloc((size_t)token_count, sizeof(jsmntok_t));
    if (!tokens) {
        return 0;
    }

    jsmn_init(&parser);
    if (jsmn_parse(&parser, json, json_size, tokens, token_count) < 0) {
        free(tokens);
        return 0;
    }

    int ok = 0;
    if (tokens[0].type == JSMN_ARRAY && tokens[0].size > 0) {
        int first = 1;
        if (tokens[first].type == JSMN_OBJECT) {
            ok = parse_release_object(json, tokens, token_count, first, config, out);
        }
    } else if (tokens[0].type == JSMN_OBJECT) {
        ok = parse_release_object(json, tokens, token_count, 0, config, out);
    }

    free(tokens);
    return ok;
}

static int parse_manifest_json(const char *json, size_t json_size, manifest_t *manifest)
{
    memset(manifest, 0, sizeof(*manifest));

    jsmn_parser parser;
    jsmn_init(&parser);
    int token_count = jsmn_parse(&parser, json, json_size, NULL, 0);
    if (token_count <= 0) {
        return 0;
    }

    jsmntok_t *tokens = (jsmntok_t *)calloc((size_t)token_count, sizeof(jsmntok_t));
    if (!tokens) {
        return 0;
    }

    jsmn_init(&parser);
    if (jsmn_parse(&parser, json, json_size, tokens, token_count) < 0) {
        free(tokens);
        return 0;
    }

    int root = 0;
    int idx = json_object_find(json, tokens, token_count, root, "version");
    if (idx > 0) {
        json_copy_string(json, &tokens[idx], manifest->version, sizeof(manifest->version));
    }

    idx = json_object_find(json, tokens, token_count, root, "package");
    if (idx > 0 && tokens[idx].type == JSMN_OBJECT) {
        int name_idx = json_object_find(json, tokens, token_count, idx, "name");
        if (name_idx > 0) {
            json_copy_string(json, &tokens[name_idx], manifest->package_name, sizeof(manifest->package_name));
        }
        int sha_idx = json_object_find(json, tokens, token_count, idx, "sha256");
        if (sha_idx > 0) {
            json_copy_string(json, &tokens[sha_idx], manifest->package_sha256, sizeof(manifest->package_sha256));
        }
        int size_idx = json_object_find(json, tokens, token_count, idx, "size");
        if (size_idx > 0) {
            json_read_uint64(json, &tokens[size_idx], &manifest->package_size);
        }
    }

    idx = json_object_find(json, tokens, token_count, root, "files");
    if (idx > 0 && tokens[idx].type == JSMN_ARRAY) {
        manifest->file_count = (size_t)tokens[idx].size;
        manifest->files = (manifest_file_t *)calloc(manifest->file_count, sizeof(manifest_file_t));
        if (!manifest->files) {
            free(tokens);
            return 0;
        }

        int i = idx + 1;
        size_t file_idx = 0;
        for (int elem = 0; elem < tokens[idx].size && file_idx < manifest->file_count; ++elem) {
            if (tokens[i].type == JSMN_OBJECT) {
                int path_idx = json_object_find(json, tokens, token_count, i, "path");
                int sha_idx = json_object_find(json, tokens, token_count, i, "sha256");
                int size_idx = json_object_find(json, tokens, token_count, i, "size");

                if (path_idx > 0) {
                    json_copy_string(json, &tokens[path_idx], manifest->files[file_idx].path,
                        sizeof(manifest->files[file_idx].path));
                    normalize_slashes(manifest->files[file_idx].path);
                }
                if (sha_idx > 0) {
                    json_copy_string(json, &tokens[sha_idx], manifest->files[file_idx].sha256,
                        sizeof(manifest->files[file_idx].sha256));
                }
                if (size_idx > 0) {
                    json_read_uint64(json, &tokens[size_idx], &manifest->files[file_idx].size);
                }
                file_idx++;
            }
            i = json_skip(tokens, token_count, i);
        }
        manifest->file_count = file_idx;
    }

    free(tokens);
    return manifest->version[0] != '\0' && manifest->file_count > 0;
}

static void free_manifest(manifest_t *manifest)
{
    if (manifest->files) {
        free(manifest->files);
        manifest->files = NULL;
    }
    manifest->file_count = 0;
}
static int is_path_safe(const char *path)
{
    if (strstr(path, "..")) {
        return 0;
    }
    if (path[0] == '\\' || path[0] == '/' || strchr(path, ':')) {
        return 0;
    }
    return 1;
}

static int ensure_directory(const wchar_t *path)
{
    wchar_t temp[MAX_PATH];
    wcscpy_s(temp, ARRAY_COUNT(temp), path);

    for (wchar_t *p = temp + 1; *p; ++p) {
        if (*p == L'\\' || *p == L'/') {
            wchar_t old = *p;
            *p = L'\0';
            CreateDirectoryW(temp, NULL);
            *p = old;
        }
    }

    return CreateDirectoryW(temp, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

static int copy_if_needed(const wchar_t *src, const wchar_t *dst, const manifest_file_t *file)
{
    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (GetFileAttributesExW(dst, GetFileExInfoStandard, &attrs)) {
        ULARGE_INTEGER size;
        size.HighPart = attrs.nFileSizeHigh;
        size.LowPart = attrs.nFileSizeLow;
        if (file->size && size.QuadPart == file->size && file->sha256[0]) {
            char existing_hash[65] = {0};
            uint64_t size_check = 0;
            if (sha256_file_hex(dst, existing_hash, &size_check)) {
                if (_stricmp(existing_hash, file->sha256) == 0) {
                    return 1;
                }
            }
        }
    }

    wchar_t dst_dir[MAX_PATH];
    wcscpy_s(dst_dir, ARRAY_COUNT(dst_dir), dst);
    PathRemoveFileSpecW(dst_dir);
    ensure_directory(dst_dir);

    wchar_t tmp[MAX_PATH];
    swprintf_s(tmp, ARRAY_COUNT(tmp), L"%s.tmp", dst);
    if (!CopyFileW(src, tmp, FALSE)) {
        return 0;
    }
    if (!MoveFileExW(tmp, dst, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileW(tmp);
        return 0;
    }
    return 1;
}

static int extract_package(const wchar_t *zip_path, const wchar_t *out_dir, ui_state_t *ui)
{
    mz_zip_archive zip = {0};
    char *zip_path_utf8 = wide_to_utf8(zip_path);
    if (!zip_path_utf8) {
        return 0;
    }
    if (!mz_zip_reader_init_file(&zip, zip_path_utf8, 0)) {
        free(zip_path_utf8);
        return 0;
    }
    free(zip_path_utf8);

    mz_uint file_count = mz_zip_reader_get_num_files(&zip);
    ui_post_progress(ui, STAGE_UPDATE, 0, file_count);

    for (mz_uint i = 0; i < file_count; ++i) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            mz_zip_reader_end(&zip);
            return 0;
        }
        if (stat.m_is_directory) {
            ui_post_progress(ui, STAGE_UPDATE, i + 1, file_count);
            continue;
        }

        if (!is_path_safe(stat.m_filename)) {
            mz_zip_reader_end(&zip);
            return 0;
        }

        wchar_t *wide_rel = utf8_to_wide(stat.m_filename);
        if (!wide_rel) {
            mz_zip_reader_end(&zip);
            return 0;
        }
        wchar_t out_path[MAX_PATH];
        swprintf_s(out_path, ARRAY_COUNT(out_path), L"%s\\%s", out_dir, wide_rel);
        normalize_slashes_w(out_path);
        wchar_t out_dir_path[MAX_PATH];
        wcscpy_s(out_dir_path, ARRAY_COUNT(out_dir_path), out_path);
        PathRemoveFileSpecW(out_dir_path);
        ensure_directory(out_dir_path);
        free(wide_rel);

        char *out_path_utf8 = wide_to_utf8(out_path);
        if (!out_path_utf8) {
            mz_zip_reader_end(&zip);
            return 0;
        }
        if (!mz_zip_reader_extract_to_file(&zip, i, out_path_utf8, 0)) {
            free(out_path_utf8);
            mz_zip_reader_end(&zip);
            return 0;
        }
        free(out_path_utf8);

        ui_post_progress(ui, STAGE_UPDATE, i + 1, file_count);
        if (is_cancelled(ui)) {
            mz_zip_reader_end(&zip);
            return 0;
        }
    }

    mz_zip_reader_end(&zip);
    return 1;
}

static int manifest_contains(const manifest_t *manifest, const wchar_t *rel_path)
{
    char *utf8 = wide_to_utf8(rel_path);
    if (!utf8) {
        return 0;
    }
    normalize_slashes(utf8);

    for (size_t i = 0; i < manifest->file_count; ++i) {
        if (_stricmp(utf8, manifest->files[i].path) == 0) {
            free(utf8);
            return 1;
        }
    }

    free(utf8);
    return 0;
}

static int matches_preserve(const config_t *config, const wchar_t *rel_path)
{
    for (size_t i = 0; i < config->preserve_count; ++i) {
        wchar_t pattern[MAX_PATH];
        char temp[MAX_STR];
        strcpy_s(temp, sizeof(temp), config->preserve[i]);
        normalize_slashes(temp);
        wchar_t *wide = utf8_to_wide(temp);
        if (!wide) {
            continue;
        }
        swprintf_s(pattern, ARRAY_COUNT(pattern), L"%s", wide);
        free(wide);
        if (PathMatchSpecW(rel_path, pattern)) {
            return 1;
        }
    }
    return 0;
}

static int path_list_push(path_list_t *list, const wchar_t *path)
{
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity ? list->capacity * 2 : 256;
        wchar_t **items = (wchar_t **)realloc(list->items, new_cap * sizeof(wchar_t *));
        if (!items) {
            return 0;
        }
        list->items = items;
        list->capacity = new_cap;
    }

    size_t len = wcslen(path) + 1;
    wchar_t *copy = (wchar_t *)calloc(len, sizeof(wchar_t));
    if (!copy) {
        return 0;
    }
    wcscpy_s(copy, len, path);
    list->items[list->count++] = copy;
    return 1;
}

static int gather_extraneous_files(const wchar_t *root, const wchar_t *current, const manifest_t *manifest,
    const config_t *config, path_list_t *list)
{
    wchar_t search[MAX_PATH];
    swprintf_s(search, ARRAY_COUNT(search), L"%s\\*", current);

    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(search, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return 1;
    }

    do {
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }

        wchar_t full_path[MAX_PATH];
        swprintf_s(full_path, ARRAY_COUNT(full_path), L"%s\\%s", current, find_data.cFileName);

        wchar_t rel_path[MAX_PATH];
        wcsncpy_s(rel_path, ARRAY_COUNT(rel_path), full_path + wcslen(root) + 1, _TRUNCATE);

        if (matches_preserve(config, rel_path)) {
            continue;
        }

        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            gather_extraneous_files(root, full_path, manifest, config, list);
        } else {
            if (!manifest_contains(manifest, rel_path)) {
                if (!path_list_push(list, rel_path)) {
                    FindClose(handle);
                    return 0;
                }
            }
        }
    } while (FindNextFileW(handle, &find_data));

    FindClose(handle);
    return 1;
}

static void remove_empty_dirs(const wchar_t *root, const wchar_t *current, const config_t *config)
{
    (void)config;
    wchar_t search[MAX_PATH];
    swprintf_s(search, ARRAY_COUNT(search), L"%s\\*", current);

    WIN32_FIND_DATAW find_data;
    HANDLE handle = FindFirstFileW(search, &find_data);
    if (handle == INVALID_HANDLE_VALUE) {
        return;
    }

    int has_entries = 0;
    do {
        if (wcscmp(find_data.cFileName, L".") == 0 || wcscmp(find_data.cFileName, L"..") == 0) {
            continue;
        }
        has_entries = 1;
        wchar_t full_path[MAX_PATH];
        swprintf_s(full_path, ARRAY_COUNT(full_path), L"%s\\%s", current, find_data.cFileName);
        if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            remove_empty_dirs(root, full_path, config);
        }
    } while (FindNextFileW(handle, &find_data));

    FindClose(handle);

    if (!has_entries && _wcsicmp(root, current) != 0) {
        RemoveDirectoryW(current);
    }
}

static int sync_installation(const wchar_t *install_dir, const wchar_t *staging_dir, const manifest_t *manifest,
    const config_t *config, ui_state_t *ui, const wchar_t *self_rel)
{
    path_list_t extraneous = {0};
    if (!gather_extraneous_files(install_dir, install_dir, manifest, config, &extraneous)) {
        for (size_t i = 0; i < extraneous.count; ++i) {
            free(extraneous.items[i]);
        }
        free(extraneous.items);
        return 0;
    }

    uint64_t total = (uint64_t)manifest->file_count + (uint64_t)extraneous.count;
    ui_post_progress(ui, STAGE_INSTALL, 0, total ? total : 1);

    uint64_t current = 0;
    int success = 1;
    for (size_t i = 0; i < manifest->file_count; ++i) {
        if (is_cancelled(ui)) {
            break;
        }

        const manifest_file_t *file = &manifest->files[i];
        if (!is_path_safe(file->path)) {
            continue;
        }

        wchar_t *rel = utf8_to_wide(file->path);
        if (!rel) {
            continue;
        }

        wchar_t src_path[MAX_PATH];
        wchar_t dst_path[MAX_PATH];
        swprintf_s(src_path, ARRAY_COUNT(src_path), L"%s\\%s", staging_dir, rel);
        swprintf_s(dst_path, ARRAY_COUNT(dst_path), L"%s\\%s", install_dir, rel);

        if (matches_preserve(config, rel)) {
            free(rel);
            current++;
            ui_post_progress(ui, STAGE_INSTALL, current, total);
            continue;
        }

        if (self_rel && self_rel[0] && _wcsicmp(rel, self_rel) == 0) {
            free(rel);
            current++;
            ui_post_progress(ui, STAGE_INSTALL, current, total);
            continue;
        }

        if (!copy_if_needed(src_path, dst_path, file)) {
            free(rel);
            success = 0;
            break;
        }
        free(rel);

        current++;
        ui_post_progress(ui, STAGE_INSTALL, current, total);
    }

    for (size_t i = 0; success && i < extraneous.count; ++i) {
        if (is_cancelled(ui)) {
            break;
        }

        wchar_t full_path[MAX_PATH];
        swprintf_s(full_path, ARRAY_COUNT(full_path), L"%s\\%s", install_dir, extraneous.items[i]);
        DeleteFileW(full_path);

        current++;
        ui_post_progress(ui, STAGE_INSTALL, current, total);
    }

    for (size_t i = 0; i < extraneous.count; ++i) {
        free(extraneous.items[i]);
    }
    free(extraneous.items);
    remove_empty_dirs(install_dir, install_dir, config);

    return success && !is_cancelled(ui);
}
static int launch_target(const wchar_t *install_dir, const char *launch_exe, int argc, wchar_t **argv)
{
    wchar_t exe_path[MAX_PATH];
    wchar_t *launch_wide = utf8_to_wide(launch_exe);
    if (!launch_wide) {
        return 0;
    }

    swprintf_s(exe_path, ARRAY_COUNT(exe_path), L"%s\\%s", install_dir, launch_wide);
    free(launch_wide);

    size_t cmd_len = wcslen(exe_path) + 3;
    for (int i = 1; i < argc; ++i) {
        cmd_len += wcslen(argv[i]) + 3;
    }

    wchar_t *cmdline = (wchar_t *)calloc(cmd_len, sizeof(wchar_t));
    if (!cmdline) {
        return 0;
    }

    wcscat_s(cmdline, cmd_len, L"\"");
    wcscat_s(cmdline, cmd_len, exe_path);
    wcscat_s(cmdline, cmd_len, L"\"");

    for (int i = 1; i < argc; ++i) {
        wcscat_s(cmdline, cmd_len, L" ");
        wcscat_s(cmdline, cmd_len, L"\"");
        wcscat_s(cmdline, cmd_len, argv[i]);
        wcscat_s(cmdline, cmd_len, L"\"");
    }

    STARTUPINFOW si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    BOOL ok = CreateProcessW(NULL, cmdline, NULL, NULL, FALSE, 0, NULL, install_dir, &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    free(cmdline);
    return ok ? 1 : 0;
}

static DWORD WINAPI update_thread(LPVOID param)
{
    update_context_t *ctx = (update_context_t *)param;
    ui_state_t *ui = ctx->ui;

    config_t config;
    if (!load_config(ctx->install_dir, &config)) {
        ui_post_status(ui, L"Failed to load updater config.");
        ui_post_done(ui, 1);
        return 0;
    }
    SendMessageW(ui->checkbox_autolaunch, BM_SETCHECK,
        config.autolaunch_default ? BST_CHECKED : BST_UNCHECKED, 0);

    wchar_t target_path[MAX_PATH];
    wchar_t *launch_exe_w = utf8_to_wide(config.launch_exe);
    if (!launch_exe_w) {
        ui_post_status(ui, L"Failed to resolve launch binary.");
        ui_post_done(ui, 1);
        return 0;
    }
    swprintf_s(target_path, ARRAY_COUNT(target_path), L"%s\\%s", ctx->install_dir, launch_exe_w);
    free(launch_exe_w);

    char local_version_str[MAX_STR] = {0};
    version_t local_version;
    if (get_file_version_utf8(target_path, local_version_str, sizeof(local_version_str))) {
        trim_whitespace(local_version_str);
    }
    if (!parse_version(local_version_str, &local_version)) {
        memset(&local_version, 0, sizeof(local_version));
    }

    ui_post_status(ui, L"Checking for updates...");

    wchar_t release_url[MAX_URL];
    swprintf_s(release_url, ARRAY_COUNT(release_url),
        L"https://api.github.com/repos/%S/%S",
        config.repo,
        config.allow_prerelease ? "releases" : "releases/latest");

    char *release_json = NULL;
    size_t release_size = 0;
    if (!http_get_internal(release_url, NULL, &release_json, &release_size, ui, -1)) {
        ui_post_status(ui, L"Failed to reach GitHub.");
        ui_post_done(ui, 1);
        return 0;
    }

    release_info_t release = {0};
    if (!parse_release_json(release_json, release_size, &config, &release)) {
        free(release_json);
        ui_post_status(ui, L"No update manifest found.");
        ui_post_done(ui, 1);
        return 0;
    }

    free(release_json);

    wchar_t *manifest_url_w = utf8_to_wide(release.manifest_url);
    if (!manifest_url_w) {
        ui_post_status(ui, L"Invalid manifest URL.");
        ui_post_done(ui, 1);
        return 0;
    }

    char *manifest_json = NULL;
    size_t manifest_size = 0;
    if (!http_get_internal(manifest_url_w, NULL, &manifest_json, &manifest_size, ui, -1)) {
        free(manifest_url_w);
        ui_post_status(ui, L"Failed to download manifest.");
        ui_post_done(ui, 1);
        return 0;
    }
    free(manifest_url_w);

    manifest_t manifest;
    if (!parse_manifest_json(manifest_json, manifest_size, &manifest)) {
        free(manifest_json);
        ui_post_status(ui, L"Invalid update manifest.");
        ui_post_done(ui, 1);
        return 0;
    }
    free(manifest_json);

    version_t remote_version;
    parse_version(manifest.version, &remote_version);

    int cmp = compare_versions(&local_version, &remote_version);
    if (cmp >= 0) {
        ui_post_status(ui, L"Up to date.");
        if (SendMessageW(ui->checkbox_autolaunch, BM_GETCHECK, 0, 0) == BST_CHECKED) {
            ui_post_status(ui, L"Loading...");
            launch_target(ctx->install_dir, config.launch_exe, ctx->argc, ctx->argv);
        }
        ui_post_done(ui, 0);
        free_manifest(&manifest);
        return 0;
    }

    ui_post_status(ui, L"Downloading update...");

    wchar_t temp_dir[MAX_PATH];
    GetTempPathW(ARRAY_COUNT(temp_dir), temp_dir);

    wchar_t package_path[MAX_PATH];
    wchar_t temp_file[MAX_PATH];
    GetTempFileNameW(temp_dir, L"worr", 0, temp_file);
    DeleteFileW(temp_file);
    swprintf_s(package_path, ARRAY_COUNT(package_path), L"%s.zip", temp_file);

    wchar_t *package_url_w = utf8_to_wide(release.package_url[0] ? release.package_url : "");
    if (!package_url_w) {
        ui_post_status(ui, L"No package URL found.");
        ui_post_done(ui, 1);
        free_manifest(&manifest);
        return 0;
    }

    if (!http_download_to_file(package_url_w, package_path, ui, STAGE_DOWNLOAD)) {
        free(package_url_w);
        ui_post_status(ui, L"Download failed.");
        ui_post_done(ui, 1);
        free_manifest(&manifest);
        return 0;
    }
    free(package_url_w);

    if (manifest.package_sha256[0]) {
        char hash_hex[65] = {0};
        uint64_t package_size = 0;
        if (!sha256_file_hex(package_path, hash_hex, &package_size)) {
            ui_post_status(ui, L"Failed to hash update package.");
            ui_post_done(ui, 1);
            free_manifest(&manifest);
            return 0;
        }
        if (_stricmp(hash_hex, manifest.package_sha256) != 0) {
            ui_post_status(ui, L"Package hash mismatch.");
            ui_post_done(ui, 1);
            free_manifest(&manifest);
            return 0;
        }
    }

    ui_post_status(ui, L"Updating...");

    wchar_t staging_dir[MAX_PATH];
    swprintf_s(staging_dir, ARRAY_COUNT(staging_dir), L"%s_stage", temp_file);
    CreateDirectoryW(staging_dir, NULL);

    if (!extract_package(package_path, staging_dir, ui)) {
        ui_post_status(ui, L"Failed to extract update.");
        ui_post_done(ui, 1);
        free_manifest(&manifest);
        return 0;
    }

    ui_post_status(ui, L"Installing...");

    wchar_t self_path[MAX_PATH];
    GetModuleFileNameW(NULL, self_path, ARRAY_COUNT(self_path));
    wchar_t self_rel[MAX_PATH] = L"";
    if (_wcsnicmp(self_path, ctx->install_dir, wcslen(ctx->install_dir)) == 0) {
        const wchar_t *rel = self_path + wcslen(ctx->install_dir);
        if (*rel == L'\\' || *rel == L'/') {
            rel++;
        }
        wcsncpy_s(self_rel, ARRAY_COUNT(self_rel), rel, _TRUNCATE);
    }

    if (!sync_installation(ctx->install_dir, staging_dir, &manifest, &config, ui, self_rel)) {
        ui_post_status(ui, L"Install failed.");
        ui_post_done(ui, 1);
        free_manifest(&manifest);
        return 0;
    }

    ui_post_status(ui, L"Loading...");
    if (SendMessageW(ui->checkbox_autolaunch, BM_GETCHECK, 0, 0) == BST_CHECKED) {
        launch_target(ctx->install_dir, config.launch_exe, ctx->argc, ctx->argv);
    }

    free_manifest(&manifest);
    ui_post_done(ui, 0);
    return 0;
}
static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    ui_state_t *ui = (ui_state_t *)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE:
        ui = (ui_state_t *)((CREATESTRUCTW *)lparam)->lpCreateParams;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)ui);
        return 0;
    case WM_COMMAND:
        if (LOWORD(wparam) == 2) {
            InterlockedExchange(&ui->cancel_requested, 1);
            EnableWindow(ui->button_cancel, FALSE);
            ui_post_status(ui, L"Cancelling...");
            return 0;
        }
        break;
    case WM_CLOSE:
        if (ui && InterlockedCompareExchange(&ui->update_done, 0, 0) == 0) {
            InterlockedExchange(&ui->cancel_requested, 1);
            EnableWindow(ui->button_cancel, FALSE);
            ui_post_status(ui, L"Cancelling...");
            return 0;
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WMU_STATUS: {
        wchar_t *text = (wchar_t *)lparam;
        SetWindowTextW(ui->status, text);
        free(text);
        return 0;
    }
    case WMU_PROGRESS: {
        progress_msg_t *pm = (progress_msg_t *)lparam;
        HWND bar = NULL;
        if (pm->stage == STAGE_DOWNLOAD) {
            bar = ui->progress_download;
        } else if (pm->stage == STAGE_UPDATE) {
            bar = ui->progress_update;
        } else if (pm->stage == STAGE_INSTALL) {
            bar = ui->progress_install;
        }
        if (bar) {
            SendMessageW(bar, PBM_SETRANGE32, 0, (LPARAM)(pm->total > 0 ? (LONG)pm->total : 1));
            SendMessageW(bar, PBM_SETPOS, (WPARAM)(pm->current > pm->total ? pm->total : pm->current), 0);
        }
        free(pm);
        return 0;
    }
    case WMU_DONE:
        InterlockedExchange(&ui->update_done, 1);
        if (wparam != 0) {
            MessageBoxW(hwnd, L"Update failed. See status for details.", L"WORR Updater", MB_OK | MB_ICONERROR);
        }
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

static int create_ui(ui_state_t *ui)
{
    INITCOMMONCONTROLSEX icc = {0};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = window_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&wc)) {
        return 0;
    }

    const int width = 520;
    const int height = 260;

    RECT rect = {0, 0, width, height};
    AdjustWindowRect(&rect, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);

    ui->hwnd = CreateWindowExW(0, kWindowClass, L"WORR Updater",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, wc.hInstance, ui);

    if (!ui->hwnd) {
        return 0;
    }

    int margin = 16;
    int y = 16;
    int label_height = 20;
    int bar_height = 18;
    int spacing = 10;

    ui->status = CreateWindowExW(0, L"STATIC", L"Starting...",
        WS_CHILD | WS_VISIBLE,
        margin, y, width - margin * 2, label_height,
        ui->hwnd, NULL, wc.hInstance, NULL);
    y += label_height + spacing;

    CreateWindowExW(0, L"STATIC", L"Download",
        WS_CHILD | WS_VISIBLE,
        margin, y, 100, label_height,
        ui->hwnd, NULL, wc.hInstance, NULL);
    y += label_height;

    ui->progress_download = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE,
        margin, y, width - margin * 2, bar_height,
        ui->hwnd, NULL, wc.hInstance, NULL);
    y += bar_height + spacing;

    CreateWindowExW(0, L"STATIC", L"Update",
        WS_CHILD | WS_VISIBLE,
        margin, y, 100, label_height,
        ui->hwnd, NULL, wc.hInstance, NULL);
    y += label_height;

    ui->progress_update = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE,
        margin, y, width - margin * 2, bar_height,
        ui->hwnd, NULL, wc.hInstance, NULL);
    y += bar_height + spacing;

    CreateWindowExW(0, L"STATIC", L"Install",
        WS_CHILD | WS_VISIBLE,
        margin, y, 100, label_height,
        ui->hwnd, NULL, wc.hInstance, NULL);
    y += label_height;

    ui->progress_install = CreateWindowExW(0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE,
        margin, y, width - margin * 2, bar_height,
        ui->hwnd, NULL, wc.hInstance, NULL);
    y += bar_height + spacing;

    ui->checkbox_autolaunch = CreateWindowExW(0, L"BUTTON", L"Launch after update",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        margin, y, 200, label_height,
        ui->hwnd, (HMENU)1, wc.hInstance, NULL);

    ui->button_cancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        width - margin - 90, y - 4, 90, label_height + 8,
        ui->hwnd, (HMENU)2, wc.hInstance, NULL);

    SendMessageW(ui->checkbox_autolaunch, BM_SETCHECK, BST_CHECKED, 0);

    ShowWindow(ui->hwnd, SW_SHOW);
    UpdateWindow(ui->hwnd);
    return 1;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE prev, PWSTR cmdline, int show)
{
    (void)instance;
    (void)prev;
    (void)cmdline;
    (void)show;

    ui_state_t ui = {0};
    if (!create_ui(&ui)) {
        return 1;
    }

    wchar_t module_path[MAX_PATH];
    GetModuleFileNameW(NULL, module_path, ARRAY_COUNT(module_path));
    PathRemoveFileSpecW(module_path);

    int argc = 0;
    wchar_t **argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    update_context_t ctx = {0};
    ctx.ui = &ui;
    wcscpy_s(ctx.install_dir, ARRAY_COUNT(ctx.install_dir), module_path);
    ctx.argc = argc;
    ctx.argv = argv;

    HANDLE thread = CreateThread(NULL, 0, update_thread, &ctx, 0, NULL);
    if (thread) {
        CloseHandle(thread);
    }

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (argv) {
        LocalFree(argv);
    }

    return 0;
}
