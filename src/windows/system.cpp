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

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "client.hpp"
#include "common/cvar.hpp"
#include "common/field.hpp"
#include "common/prompt.hpp"
#include "common/steam.hpp"
#include "shared/atomic.hpp"

#include <array>
#include <algorithm>
#include <cstring>
#include <cwchar>
#include <cwctype>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#if USE_WINSVC
#include <winsvc.h>
#include <setjmp.h>
#endif

#if defined(_WIN32)
#include <appmodel.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <versionhelpers.h>
#endif

HINSTANCE                       hGlobalInstance;

#if USE_WINSVC
static SERVICE_STATUS_HANDLE    statusHandle;
static jmp_buf                  exitBuf;
#endif

static atomic_int               shouldExit;
static atomic_int               errorEntered;

static LARGE_INTEGER            timer_freq;
static DWORD                    main_thread_id;

static cvar_t                   *sys_exitonerror;

cvar_t  *sys_basedir;
cvar_t  *sys_libdir;
cvar_t  *sys_homedir;
#ifdef _DEBUG
cvar_t  *sys_debugprint;
#endif

/*
===============================================================================

CONSOLE I/O

===============================================================================
*/

#if USE_SYSCON

namespace {

constexpr DWORD kMaxConsoleInputEvents = 64;
constexpr wchar_t kPromptPrefix = L']';

class Utf8Converter {
public:
    std::wstring fromUtf8(std::string_view text) const
    {
        if (text.empty()) {
            return {};
        }

        int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (required <= 0) {
            std::wstring fallback(text.size(), L'\0');
            std::transform(text.begin(), text.end(), fallback.begin(), [](unsigned char c) { return static_cast<wchar_t>(c); });
            return fallback;
        }

        std::wstring result(required, L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), required);
        return result;
    }
};

class ConsoleFontController {
public:
    ConsoleFontController()
    {
        HMODULE kernel = GetModuleHandleW(L"kernel32.dll");
        getFont_ = reinterpret_cast<GetFontFn>(GetProcAddress(kernel, "GetCurrentConsoleFontEx"));
        setFont_ = reinterpret_cast<SetFontFn>(GetProcAddress(kernel, "SetCurrentConsoleFontEx"));
        available_ = getFont_ && setFont_;
        if (available_) {
            original_.cbSize = sizeof(original_);
        }
    }

    void captureOriginal(HANDLE output)
    {
        if (!available_ || captured_) {
            return;
        }

        CONSOLE_FONT_INFOEX font{ sizeof(font) };
        if (getFont_(output, FALSE, &font)) {
            original_ = font;
            captured_ = true;
        }
    }

    void applyPreferred(HANDLE output)
    {
        if (!available_) {
            return;
        }

        captureOriginal(output);

        CONSOLE_FONT_INFOEX font{ sizeof(font) };
        if (!getFont_(output, FALSE, &font)) {
            return;
        }

        constexpr wchar_t kPreferredFont[] = L"Consolas";
        std::wcsncpy(font.FaceName, kPreferredFont, std::size(font.FaceName) - 1);
        font.FaceName[std::size(font.FaceName) - 1] = L'\0';
        font.dwFontSize.Y = std::max<SHORT>(font.dwFontSize.Y, 16);
        setFont_(output, FALSE, &font);
    }

    void restore(HANDLE output)
    {
        if (!available_ || !captured_) {
            return;
        }

        auto font = original_;
        setFont_(output, FALSE, &font);
    }

private:
    using GetFontFn = BOOL (WINAPI *)(HANDLE, BOOL, PCONSOLE_FONT_INFOEX);
    using SetFontFn = BOOL (WINAPI *)(HANDLE, BOOL, PCONSOLE_FONT_INFOEX);

    GetFontFn getFont_ = nullptr;
    SetFontFn setFont_ = nullptr;
    CONSOLE_FONT_INFOEX original_{};
    bool available_ = false;
    bool captured_ = false;
};

#ifdef _DEBUG
// hack'd version of OutputDebugStringA that can be given a specific size
static void __stdcall QOutputDebugStringA(LPCSTR lpOutputString, size_t len)
{
    std::array<ULONG_PTR, 2> args{};
    args[0] = static_cast<ULONG_PTR>(len);
    args[1] = reinterpret_cast<ULONG_PTR>(lpOutputString);

    __try {
        RaiseException(DBG_PRINTEXCEPTION_C, 0, static_cast<DWORD>(args.size()), args.data());
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
    }
}
#endif

class WinConsole {
public:
    bool initialize();
    void shutdown();
    void run();
    void write(std::string_view text);
    void setColor(color_index_t color);
    void setTitle(const char *title);
    void loadHistory();
    void saveHistory();

private:
    class InputScope {
    public:
        explicit InputScope(WinConsole &console) : console_(console) { console_.hideInput(); }
        ~InputScope() { console_.showInput(); }

        InputScope(const InputScope &) = delete;
        InputScope &operator=(const InputScope &) = delete;

    private:
        WinConsole &console_;
    };

    std::optional<CONSOLE_SCREEN_BUFFER_INFO> queryBufferInfo() const;
    void updateDimensions(const CONSOLE_SCREEN_BUFFER_INFO &info);
    void markInputDirty();
    void hideInput();
    void showInput();
    void renderInputLine();
    void applyFont();
    void handleWindowEvent(const WINDOW_BUFFER_SIZE_RECORD &record);
    void handleKeyEvent(const KEY_EVENT_RECORD &record);
    bool handleCtrlShortcut(WORD key, DWORD controlState);
    bool handleAltShortcut(WORD key);
    bool handleCtrlNavigation(WORD key);
    void handlePrintableCharacter(char ch);
    void submitCurrentLine();
    void moveCursorTo(size_t position);
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorWordLeft();
    void moveCursorWordRight();
    void moveCursorToStart();
    void moveCursorToEnd();
    void deleteCharAtCursor();
    void deletePreviousChar();
    void deletePreviousWord();
    void deleteNextWord();
    void deleteToStart();
    void deleteToEnd();
    void insertCharacter(char ch);
    void historyUp();
    void historyDown();
    void searchHistory(bool forward);
    void completeCommand(bool useBackslash);
    void scroll(int deltaRows);
    void scrollPage(int direction);
    void scrollToEdge(bool top);
    void adjustWindow(const CONSOLE_SCREEN_BUFFER_INFO &info, int deltaRows);
    void clearWindow();
    void writeWrapped(std::string_view text);
    void writeWrapped(const std::wstring &text);

    HANDLE input_ = INVALID_HANDLE_VALUE;
    HANDLE output_ = INVALID_HANDLE_VALUE;
    bool ready_ = false;
    uint16_t width_ = 0;
    int hiddenDepth_ = 0;
    bool inputDirty_ = true;
    ConsoleFontController fontController_{};
    Utf8Converter utf8_{};
};

static commandPrompt_t sys_con;
static WinConsole g_console;
static bool g_allocatedConsole = false;

#define FOREGROUND_BLACK    0
#define FOREGROUND_WHITE    (FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_RED)

constexpr std::array<WORD, 8> kTextColors = {
    FOREGROUND_BLACK,
    FOREGROUND_RED,
    FOREGROUND_GREEN,
    FOREGROUND_RED | FOREGROUND_GREEN,
    FOREGROUND_BLUE,
    FOREGROUND_BLUE | FOREGROUND_GREEN,
    FOREGROUND_RED | FOREGROUND_BLUE,
    FOREGROUND_WHITE
};

std::optional<CONSOLE_SCREEN_BUFFER_INFO> WinConsole::queryBufferInfo() const
{
    if (output_ == INVALID_HANDLE_VALUE) {
        return std::nullopt;
    }

    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(output_, &info)) {
        return std::nullopt;
    }

    return info;
}

void WinConsole::updateDimensions(const CONSOLE_SCREEN_BUFFER_INFO &info)
{
    width_ = static_cast<uint16_t>(std::max<LONG>(info.dwSize.X, 1));
    sys_con.widthInChars = width_;

    auto &field = sys_con.inputLine;
    if (field.maxChars) {
        const size_t available = width_ > 0 ? static_cast<size_t>(std::max<int>(width_ - 1, 0)) : 0;
        field.visibleChars = std::min(available, field.maxChars);
    }
}

void WinConsole::markInputDirty()
{
    inputDirty_ = true;
}

void WinConsole::hideInput()
{
    if (!ready_) {
        return;
    }

    if (++hiddenDepth_ > 1) {
        return;
    }

    if (auto info = queryBufferInfo()) {
        COORD pos{ 0, info->dwCursorPosition.Y };
        DWORD cleared = 0;
        FillConsoleOutputCharacterW(output_, L' ', info->dwSize.X, pos, &cleared);
        FillConsoleOutputAttribute(output_, info->wAttributes, info->dwSize.X, pos, &cleared);
        SetConsoleCursorPosition(output_, pos);
    }
}

void WinConsole::showInput()
{
    if (!ready_ || hiddenDepth_ == 0) {
        return;
    }

    if (--hiddenDepth_ == 0) {
        renderInputLine();
    }
}

void WinConsole::renderInputLine()
{
    if (!ready_) {
        return;
    }

    auto info = queryBufferInfo();
    if (!info) {
        return;
    }

    updateDimensions(*info);

    auto &field = sys_con.inputLine;
    const size_t maxChars = field.maxChars ? field.maxChars - 1 : 0;
    const size_t cursor = std::min(field.cursorPos, maxChars);
    const size_t textLen = std::strlen(field.text);

    size_t startIndex = 0;
    if (field.visibleChars > 0 && cursor >= field.visibleChars) {
        startIndex = cursor - (field.visibleChars - 1);
    }
    if (startIndex > textLen) {
        startIndex = textLen;
    }

    const size_t visibleLen = field.visibleChars == 0 ? 0 : std::min(field.visibleChars, textLen - startIndex);
    std::string_view view(field.text + startIndex, visibleLen);
    std::wstring line = utf8_.fromUtf8(view);
    std::wstring buffer(field.visibleChars, L' ');
    std::copy_n(line.begin(), std::min(line.size(), buffer.size()), buffer.begin());

    COORD promptPos{ 0, info->dwCursorPosition.Y };
    DWORD written = 0;
    FillConsoleOutputCharacterW(output_, L' ', info->dwSize.X, promptPos, &written);
    FillConsoleOutputAttribute(output_, info->wAttributes, info->dwSize.X, promptPos, &written);
    WriteConsoleOutputCharacterW(output_, &kPromptPrefix, 1, promptPos, &written);

    if (!buffer.empty()) {
        WriteConsoleOutputCharacterW(output_, buffer.c_str(), static_cast<DWORD>(buffer.size()), COORD{ 1, promptPos.Y }, &written);
    }

    const size_t cursorOffset = cursor >= startIndex ? cursor - startIndex : 0;
    SetConsoleCursorPosition(output_, COORD{ static_cast<SHORT>(cursorOffset + 1), promptPos.Y });
    inputDirty_ = false;
}

void WinConsole::applyFont()
{
    if (output_ != INVALID_HANDLE_VALUE) {
        fontController_.applyPreferred(output_);
    }
}

void WinConsole::handleWindowEvent(const WINDOW_BUFFER_SIZE_RECORD &record)
{
    if (record.dwSize.X < 2) {
        Com_EPrintf("Invalid console buffer width.\n");
        return;
    }

    width_ = record.dwSize.X;
    markInputDirty();
    showInput();

    Com_DPrintf("System console resized (%d cols, %d rows).\n", record.dwSize.X, record.dwSize.Y);
}

void WinConsole::handleKeyEvent(const KEY_EVENT_RECORD &record)
{
    if (!record.bKeyDown) {
        return;
    }

    const WORD key = record.wVirtualKeyCode;
    const DWORD state = record.dwControlKeyState;
    const bool ctrl = (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    const bool alt = (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
    const bool shift = (state & SHIFT_PRESSED) != 0;

    if (ctrl && handleCtrlShortcut(key, state)) {
        return;
    }

    if (alt && handleAltShortcut(key)) {
        return;
    }

    if (ctrl && handleCtrlNavigation(key)) {
        return;
    }

    switch (key) {
    case VK_UP:
        historyUp();
        return;
    case VK_DOWN:
        historyDown();
        return;
    case VK_PRIOR:
        scrollPage(-1);
        return;
    case VK_NEXT:
        scrollPage(1);
        return;
    case VK_RETURN:
        submitCurrentLine();
        return;
    case VK_BACK:
        deletePreviousChar();
        return;
    case VK_DELETE:
        deleteCharAtCursor();
        return;
    case VK_HOME:
        moveCursorToStart();
        return;
    case VK_END:
        moveCursorToEnd();
        return;
    case VK_LEFT:
        moveCursorLeft();
        return;
    case VK_RIGHT:
        moveCursorRight();
        return;
    case VK_TAB:
        completeCommand(shift);
        return;
    default:
        break;
    }

    const wchar_t ch = record.uChar.UnicodeChar;
    if (!ctrl && !alt && ch >= 32 && ch < 128) {
        handlePrintableCharacter(static_cast<char>(ch));
    }
}

bool WinConsole::handleCtrlShortcut(WORD key, DWORD controlState)
{
    switch (key) {
    case 'A':
        moveCursorToStart();
        return true;
    case 'E':
        moveCursorToEnd();
        return true;
    case 'B':
        moveCursorLeft();
        return true;
    case 'F':
        moveCursorRight();
        return true;
    case 'C':
        GenerateConsoleCtrlEvent(CTRL_C_EVENT, 0);
        return true;
    case 'D':
        deleteCharAtCursor();
        return true;
    case 'W':
        deletePreviousWord();
        return true;
    case 'U':
        deleteToStart();
        return true;
    case 'K':
        deleteToEnd();
        return true;
    case 'L':
        clearWindow();
        return true;
    case 'N':
        historyDown();
        return true;
    case 'P':
        historyUp();
        return true;
    case 'R':
        searchHistory(false);
        return true;
    case 'S':
        searchHistory(true);
        return true;
    case VK_HOME:
        scrollToEdge(true);
        return true;
    case VK_END:
        scrollToEdge(false);
        return true;
    case VK_PRIOR:
        scrollPage(-1);
        return true;
    case VK_NEXT:
        scrollPage(1);
        return true;
    case VK_BACK:
        deletePreviousWord();
        return true;
    case VK_DELETE:
        deleteNextWord();
        return true;
    default:
        break;
    }

    if (key == VK_LEFT || key == VK_RIGHT) {
        if (key == VK_LEFT) {
            moveCursorWordLeft();
        } else {
            moveCursorWordRight();
        }
        return true;
    }

    if (key == 'V' && (controlState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
        return false;
    }

    return false;
}

bool WinConsole::handleAltShortcut(WORD key)
{
    switch (key) {
    case 'B':
        moveCursorWordLeft();
        return true;
    case 'F':
        moveCursorWordRight();
        return true;
    case 'D':
        deleteNextWord();
        return true;
    default:
        break;
    }

    return false;
}

bool WinConsole::handleCtrlNavigation(WORD key)
{
    switch (key) {
    case VK_LEFT:
        moveCursorWordLeft();
        return true;
    case VK_RIGHT:
        moveCursorWordRight();
        return true;
    default:
        break;
    }

    return false;
}

void WinConsole::handlePrintableCharacter(char ch)
{
    markInputDirty();
    InputScope scope(*this);
    insertCharacter(ch);
}

void WinConsole::submitCurrentLine()
{
    markInputDirty();
    InputScope scope(*this);
    char *s = Prompt_Action(&sys_con);
    if (s) {
        if (*s == '\\' || *s == '/') {
            ++s;
        }
        Sys_Printf("]%s\n", s);
        Cbuf_AddText(&cmd_buffer, s);
        Cbuf_AddText(&cmd_buffer, "\n");
    } else {
        write("]\n");
    }
}

void WinConsole::moveCursorTo(size_t position)
{
    auto &field = sys_con.inputLine;
    if (!field.maxChars) {
        return;
    }

    const size_t limit = field.maxChars ? field.maxChars - 1 : 0;
    position = std::min(position, limit);
    if (field.cursorPos != position) {
        field.cursorPos = position;
        markInputDirty();
        if (hiddenDepth_ > 0) {
            showInput();
        } else {
            renderInputLine();
        }
    }
}

void WinConsole::moveCursorLeft()
{
    auto &field = sys_con.inputLine;
    if (field.cursorPos > 0) {
        moveCursorTo(field.cursorPos - 1);
    }
}

void WinConsole::moveCursorRight()
{
    auto &field = sys_con.inputLine;
    const size_t len = std::strlen(field.text);
    if (field.cursorPos < len && field.cursorPos < field.maxChars - 1) {
        moveCursorTo(field.cursorPos + 1);
    }
}

void WinConsole::moveCursorWordLeft()
{
    auto &field = sys_con.inputLine;
    size_t pos = field.cursorPos;
    while (pos > 0 && field.text[pos - 1] <= ' ') {
        --pos;
    }
    while (pos > 0 && field.text[pos - 1] > ' ') {
        --pos;
    }
    moveCursorTo(pos);
}

void WinConsole::moveCursorWordRight()
{
    auto &field = sys_con.inputLine;
    size_t pos = field.cursorPos;
    const size_t len = std::strlen(field.text);
    while (pos < len && field.text[pos] <= ' ') {
        ++pos;
    }
    while (pos < len && field.text[pos] > ' ') {
        ++pos;
    }
    moveCursorTo(pos);
}

void WinConsole::moveCursorToStart()
{
    moveCursorTo(0);
}

void WinConsole::moveCursorToEnd()
{
    moveCursorTo(std::strlen(sys_con.inputLine.text));
}

void WinConsole::deleteCharAtCursor()
{
    auto &field = sys_con.inputLine;
    if (!field.text[field.cursorPos]) {
        return;
    }

    markInputDirty();
    InputScope scope(*this);
    const size_t len = std::strlen(field.text);
    memmove(field.text + field.cursorPos, field.text + field.cursorPos + 1, len - field.cursorPos);
}

void WinConsole::deletePreviousChar()
{
    auto &field = sys_con.inputLine;
    if (field.cursorPos == 0) {
        return;
    }

    markInputDirty();
    InputScope scope(*this);
    const size_t len = std::strlen(field.text);
    memmove(field.text + field.cursorPos - 1, field.text + field.cursorPos, len - field.cursorPos + 1);
    field.cursorPos--;
}

void WinConsole::deletePreviousWord()
{
    auto &field = sys_con.inputLine;
    if (field.cursorPos == 0) {
        return;
    }

    size_t pos = field.cursorPos;
    while (pos > 0 && field.text[pos - 1] <= ' ') {
        --pos;
    }
    while (pos > 0 && field.text[pos - 1] > ' ') {
        --pos;
    }

    markInputDirty();
    InputScope scope(*this);
    memmove(field.text + pos, field.text + field.cursorPos, std::strlen(field.text + field.cursorPos) + 1);
    field.cursorPos = pos;
}

void WinConsole::deleteNextWord()
{
    auto &field = sys_con.inputLine;
    size_t pos = field.cursorPos;
    const size_t len = std::strlen(field.text);
    if (pos >= len) {
        return;
    }

    while (pos < len && field.text[pos] <= ' ') {
        ++pos;
    }
    while (pos < len && field.text[pos] > ' ') {
        ++pos;
    }

    markInputDirty();
    InputScope scope(*this);
    memmove(field.text + field.cursorPos, field.text + pos, len - pos + 1);
}

void WinConsole::deleteToStart()
{
    auto &field = sys_con.inputLine;
    if (field.cursorPos == 0) {
        return;
    }

    markInputDirty();
    InputScope scope(*this);
    memmove(field.text, field.text + field.cursorPos, std::strlen(field.text + field.cursorPos) + 1);
    field.cursorPos = 0;
}

void WinConsole::deleteToEnd()
{
    auto &field = sys_con.inputLine;
    markInputDirty();
    InputScope scope(*this);
    field.text[field.cursorPos] = 0;
}

void WinConsole::insertCharacter(char ch)
{
    auto &field = sys_con.inputLine;
    if (!field.maxChars) {
        return;
    }

    const size_t len = std::strlen(field.text);
    if (field.cursorPos >= field.maxChars - 1) {
        field.text[field.cursorPos] = ch;
        if (field.cursorPos == len) {
            field.text[field.cursorPos + 1] = 0;
        }
        return;
    }

    memmove(field.text + field.cursorPos + 1, field.text + field.cursorPos, len - field.cursorPos + 1);
    field.text[field.cursorPos++] = ch;
}

void WinConsole::historyUp()
{
    markInputDirty();
    InputScope scope(*this);
    Prompt_HistoryUp(&sys_con);
}

void WinConsole::historyDown()
{
    markInputDirty();
    InputScope scope(*this);
    Prompt_HistoryDown(&sys_con);
}

void WinConsole::searchHistory(bool forward)
{
    markInputDirty();
    InputScope scope(*this);
    Prompt_CompleteHistory(&sys_con, forward);
}

void WinConsole::completeCommand(bool useBackslash)
{
    markInputDirty();
    InputScope scope(*this);
    Prompt_CompleteCommand(&sys_con, useBackslash);
}

void WinConsole::adjustWindow(const CONSOLE_SCREEN_BUFFER_INFO &info, int deltaRows)
{
    if (deltaRows == 0) {
        return;
    }

    SMALL_RECT window = info.srWindow;
    const int height = window.Bottom - window.Top + 1;
    const int maxTop = std::max<int>(0, info.dwSize.Y - height);
    const int desiredTop = std::clamp<int>(window.Top + deltaRows, 0, maxTop);
    const int offset = desiredTop - window.Top;
    if (!offset) {
        return;
    }

    window.Top = static_cast<SHORT>(window.Top + offset);
    window.Bottom = static_cast<SHORT>(window.Bottom + offset);
    SetConsoleWindowInfo(output_, TRUE, &window);
}

void WinConsole::scroll(int deltaRows)
{
    if (!ready_ || deltaRows == 0) {
        return;
    }

    if (auto info = queryBufferInfo()) {
        adjustWindow(*info, deltaRows);
    }
}

void WinConsole::scrollPage(int direction)
{
    if (!ready_ || direction == 0) {
        return;
    }

    if (auto info = queryBufferInfo()) {
        const int height = info->srWindow.Bottom - info->srWindow.Top + 1;
        adjustWindow(*info, direction * std::max(1, height));
    }
}

void WinConsole::scrollToEdge(bool top)
{
    if (!ready_) {
        return;
    }

    if (auto info = queryBufferInfo()) {
        const int height = info->srWindow.Bottom - info->srWindow.Top + 1;
        const int maxTop = std::max<int>(0, info->dwSize.Y - height);
        const int target = top ? 0 : maxTop;
        adjustWindow(*info, target - info->srWindow.Top);
    }
}

void WinConsole::clearWindow()
{
    if (!ready_) {
        return;
    }

    markInputDirty();
    InputScope scope(*this);
    if (auto info = queryBufferInfo()) {
        const int height = info->srWindow.Bottom - info->srWindow.Top + 1;
        const DWORD cells = static_cast<DWORD>(height) * info->dwSize.X;
        COORD origin{ 0, info->srWindow.Top };
        DWORD written = 0;
        FillConsoleOutputCharacterW(output_, L' ', cells, origin, &written);
        FillConsoleOutputAttribute(output_, info->wAttributes, cells, origin, &written);
        SetConsoleCursorPosition(output_, origin);
    }
}

void WinConsole::writeWrapped(std::string_view text)
{
    std::wstring wide = utf8_.fromUtf8(text);
    wide.erase(std::remove(wide.begin(), wide.end(), L'\r'), wide.end());
    writeWrapped(wide);
}

void WinConsole::writeWrapped(const std::wstring &text)
{
    if (text.empty()) {
        return;
    }

    const size_t width = width_ ? static_cast<size_t>(width_) : 80;
    size_t index = 0;
    DWORD written = 0;

    while (index < text.size()) {
        size_t newline = text.find(L'\n', index);
        const size_t lineEnd = (newline == std::wstring::npos) ? text.size() : newline;
        size_t start = index;

        while (start < lineEnd) {
            size_t remaining = lineEnd - start;
            size_t chunk = std::min(remaining, width);
            if (chunk < remaining) {
                size_t wrap = chunk;
                while (wrap > 0 && !std::iswspace(text[start + wrap - 1])) {
                    --wrap;
                }
                if (wrap > 0) {
                    chunk = wrap;
                }
            }

            if (chunk == 0) {
                chunk = std::min(remaining, width);
                if (chunk == 0) {
                    break;
                }
            }

            WriteConsoleW(output_, text.data() + start, static_cast<DWORD>(chunk), &written, nullptr);
            start += chunk;

            if (start < lineEnd) {
                WriteConsoleW(output_, L"\n", 1, &written, nullptr);
                while (start < lineEnd && std::iswspace(text[start]) && text[start] != L'\n') {
                    ++start;
                }
            }
        }

        if (newline != std::wstring::npos) {
            WriteConsoleW(output_, L"\n", 1, &written, nullptr);
            index = newline + 1;
        } else {
            index = lineEnd;
        }
    }
}

bool WinConsole::initialize()
{
    input_ = GetStdHandle(STD_INPUT_HANDLE);
    output_ = GetStdHandle(STD_OUTPUT_HANDLE);

    if (input_ == INVALID_HANDLE_VALUE || output_ == INVALID_HANDLE_VALUE) {
        Com_EPrintf("Couldn't acquire console handles.\n");
        return false;
    }

    auto info = queryBufferInfo();
    if (!info) {
        Com_EPrintf("Couldn't get console buffer info.\n");
        return false;
    }

    DWORD inputMode = 0;
    if (!GetConsoleMode(input_, &inputMode)) {
        Com_EPrintf("Couldn't get console input mode.\n");
        return false;
    }

    inputMode &= ~(ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);
    inputMode |= ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE;
    if (!SetConsoleMode(input_, inputMode)) {
        Com_EPrintf("Couldn't set console input mode.\n");
        return false;
    }

    DWORD outputMode = 0;
    if (GetConsoleMode(output_, &outputMode)) {
        outputMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(output_, outputMode);
    }

    applyFont();

    SetConsoleTitleA(PRODUCT " console");

    sys_con.printf = Sys_Printf;
    sys_con.widthInChars = info->dwSize.X;
    IF_Init(&sys_con.inputLine, info->dwSize.X > 0 ? info->dwSize.X - 1 : 0, MAX_FIELD_TEXT - 1);

    updateDimensions(*info);

    ready_ = true;
    hiddenDepth_ = 1;
    markInputDirty();
    showInput();

    Com_DPrintf("System console initialized (%d cols, %d rows).\n", info->dwSize.X, info->dwSize.Y);
    return true;
}

void WinConsole::shutdown()
{
    if (!ready_) {
        return;
    }

    fontController_.restore(output_);
    ready_ = false;
    input_ = INVALID_HANDLE_VALUE;
    output_ = INVALID_HANDLE_VALUE;
}

void WinConsole::run()
{
    if (!ready_ || input_ == INVALID_HANDLE_VALUE) {
        return;
    }

    std::array<INPUT_RECORD, kMaxConsoleInputEvents> records{};

    while (true) {
        DWORD pending = 0;
        if (!GetNumberOfConsoleInputEvents(input_, &pending)) {
            Com_EPrintf("Error %lu getting number of console events.\n", GetLastError());
            shutdown();
            return;
        }

        if (pending == 0) {
            break;
        }

        if (pending > records.size()) {
            pending = static_cast<DWORD>(records.size());
        }

        DWORD read = 0;
        if (!ReadConsoleInputW(input_, records.data(), pending, &read)) {
            Com_EPrintf("Error %lu reading console input.\n", GetLastError());
            shutdown();
            return;
        }

        for (DWORD i = 0; i < read; ++i) {
            const INPUT_RECORD &event = records[i];
            if (event.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                handleWindowEvent(event.Event.WindowBufferSizeEvent);
            } else if (event.EventType == KEY_EVENT) {
                handleKeyEvent(event.Event.KeyEvent);
            }
        }
    }
}

void WinConsole::write(std::string_view text)
{
    if (text.empty()) {
        return;
    }

#ifdef _DEBUG
    if (sys_debugprint && sys_debugprint->integer) {
        QOutputDebugStringA(text.data(), text.size());
        QOutputDebugStringA("\r\n", 2);
    }
#endif

    if (!ready_ || output_ == INVALID_HANDLE_VALUE) {
        HANDLE handle = output_ != INVALID_HANDLE_VALUE ? output_ : GetStdHandle(STD_OUTPUT_HANDLE);
        if (handle == INVALID_HANDLE_VALUE) {
            return;
        }
        DWORD written = 0;
        WriteFile(handle, text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
        return;
    }

    markInputDirty();
    InputScope scope(*this);
    writeWrapped(text);
}

void WinConsole::setColor(color_index_t color)
{
    if (!ready_) {
        return;
    }

    auto info = queryBufferInfo();
    if (!info) {
        return;
    }

    WORD attr = info->wAttributes & ~FOREGROUND_WHITE;
    WORD value = attr | FOREGROUND_WHITE;

    switch (color) {
    case COLOR_INDEX_NONE:
        value = attr | FOREGROUND_WHITE;
        break;
    case COLOR_INDEX_ALT:
        value = attr | FOREGROUND_GREEN;
        break;
    default:
        value = attr | kTextColors[color & 7];
        break;
    }

    if (color != COLOR_INDEX_NONE) {
        hideInput();
    }
    SetConsoleTextAttribute(output_, value);
    if (color == COLOR_INDEX_NONE) {
        markInputDirty();
        showInput();
    }
}

void WinConsole::setTitle(const char *title)
{
    if (ready_) {
        SetConsoleTitleA(title);
    }
}

void WinConsole::loadHistory()
{
	if (ready_ && sys_history && sys_history->integer > 0) {
		Prompt_LoadHistory(&sys_con, SYS_CON_HISTORY_FILE);
		markInputDirty();
		showInput();
	}
}

void WinConsole::saveHistory()
{
	if (ready_ && sys_history && sys_history->integer > 0) {
		Prompt_SaveHistory(&sys_con, SYS_CON_HISTORY_FILE, sys_history->integer);
	}
}

} // namespace

void Sys_RunConsole(void)
{
    g_console.run();
}

void Sys_ConsoleOutput(const char *text, size_t len)
{
    g_console.write(std::string_view{text, len});
}

void Sys_SetConsoleTitle(const char *title)
{
    g_console.setTitle(title);
}

void Sys_SetConsoleColor(color_index_t color)
{
    g_console.setColor(color);
}

void Sys_LoadHistory(void)
{
    g_console.loadHistory();
}

void Sys_SaveHistory(void)
{
    g_console.saveHistory();
}

static BOOL WINAPI Sys_ConsoleCtrlHandler(DWORD dwCtrlType)
{
    if (atomic_load(&errorEntered)) {
        exit(1);
    }
    atomic_store(&shouldExit, TRUE);
    Sleep(INFINITE);
    return TRUE;
}

static void Sys_ConsoleInit(void)
{
#if USE_WINSVC
    if (statusHandle) {
        return;
    }
#endif

#if USE_CLIENT
    if (!AllocConsole()) {
        Com_EPrintf("Couldn't create system console.\n");
        return;
    }
    g_allocatedConsole = true;
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    SetConsoleCtrlHandler(Sys_ConsoleCtrlHandler, TRUE);

    if (!g_console.initialize()) {
        return;
    }
}

#endif // USE_SYSCON


/*
===============================================================================

SERVICE CONTROL

===============================================================================
*/

#if USE_WINSVC

static void Sys_InstallService_f(void)
{
    std::array<char, 1024> servicePath{};
    std::array<char, 256> serviceName{};
    SC_HANDLE scm, service;
    DWORD length;
    char *commandline;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <servicename> [+command ...]\n"
                   "Example: %s deathmatch +set net_port 27910 +map q2dm1\n",
                   Cmd_Argv(0), Cmd_Argv(0));
        return;
    }

    scm = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        Com_EPrintf("Couldn't open Service Control Manager: %s\n", Sys_ErrorString(GetLastError()));
        return;
    }

    Q_concat(serviceName.data(), serviceName.size(), PRODUCT " - ", Cmd_Argv(1));

    length = GetModuleFileNameA(NULL, servicePath.data(), static_cast<DWORD>(servicePath.size() - 1));
    if (!length) {
        Com_EPrintf("Couldn't get module file name: %s\n", Sys_ErrorString(GetLastError()));
        goto fail;
    }
    commandline = Cmd_RawArgsFrom(2);
    if (length + strlen(commandline) + 10 > servicePath.size() - 1) {
        Com_Printf("Oversize service command line.\n");
        goto fail;
    }
    strcpy(servicePath.data() + length, " -service ");
    strcpy(servicePath.data() + length + 10, commandline);

    service = CreateServiceA(scm, serviceName.data(), serviceName.data(), SERVICE_START,
                             SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                             SERVICE_ERROR_IGNORE, servicePath.data(),
                             NULL, NULL, NULL, NULL, NULL);
    if (!service) {
        Com_EPrintf("Couldn't create service: %s\n", Sys_ErrorString(GetLastError()));
        goto fail;
    }

    Com_Printf("Service created successfully.\n");

    CloseServiceHandle(service);

fail:
    CloseServiceHandle(scm);
}

static void Sys_DeleteService_f(void)
{
    std::array<char, 256> serviceName{};
    SC_HANDLE scm, service;

    if (Cmd_Argc() < 2) {
        Com_Printf("Usage: %s <servicename>\n", Cmd_Argv(0));
        return;
    }

    scm = OpenSCManager(NULL, SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS);
    if (!scm) {
        Com_EPrintf("Couldn't open Service Control Manager: %s\n", Sys_ErrorString(GetLastError()));
        return;
    }

    Q_concat(serviceName.data(), serviceName.size(), PRODUCT " - ", Cmd_Argv(1));

    service = OpenServiceA(scm, serviceName.data(), DELETE);
    if (!service) {
        Com_EPrintf("Couldn't open service: %s\n", Sys_ErrorString(GetLastError()));
        goto fail;
    }

    if (!DeleteService(service)) {
        Com_EPrintf("Couldn't delete service: %s\n", Sys_ErrorString(GetLastError()));
    } else {
        Com_Printf("Service deleted successfully.\n");
    }

    CloseServiceHandle(service);

fail:
    CloseServiceHandle(scm);
}

#endif // USE_WINSVC

/*
===============================================================================

MISC

===============================================================================
*/

/*
================
Sys_Error
================
*/
void Sys_Error(const char *error, ...)
{
    va_list     argptr;
    char        text[MAXERRORMSG];

    va_start(argptr, error);
    Q_vsnprintf(text, sizeof(text), error, argptr);
    va_end(argptr);

#if USE_CLIENT
    Win_Shutdown();
#endif

#if USE_SYSCON
    Sys_SetConsoleColor(COLOR_INDEX_RED);
    Sys_Printf("********************\n"
               "FATAL: %s\n"
               "********************\n", text);
    Sys_SetConsoleColor(COLOR_INDEX_NONE);
#endif

#if USE_WINSVC
    if (statusHandle)
        longjmp(exitBuf, 1);
#endif

    atomic_store(&errorEntered, TRUE);

    if (atomic_load(&shouldExit) || (sys_exitonerror && sys_exitonerror->integer))
        exit(1);

#if USE_SYSCON
    if (g_allocatedConsole) {
        DWORD list;
        if (GetConsoleProcessList(&list, 1) > 1)
            exit(1);
        g_console.shutdown();
        HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
        if (input != INVALID_HANDLE_VALUE) {
            SetConsoleMode(input, ENABLE_PROCESSED_INPUT);
        }
        Sys_Printf("Press Ctrl+C to exit.\n");
        Sleep(INFINITE);
    }
#endif

    MessageBoxA(NULL, text, PRODUCT " Fatal Error", MB_ICONERROR | MB_OK);
    exit(1);
}

/*
================
Sys_Quit

This function never returns.
================
*/
void Sys_Quit(void)
{
#if USE_WINSVC
    if (statusHandle)
        longjmp(exitBuf, 1);
#endif

    exit(0);
}

void Sys_DebugBreak(void)
{
    DebugBreak();
}

bool Sys_IsMainThread(void)
{
    return GetCurrentThreadId() == main_thread_id;
}

unsigned Sys_Milliseconds(void)
{
    LARGE_INTEGER tm;
    QueryPerformanceCounter(&tm);
    return tm.QuadPart * 1000ULL / timer_freq.QuadPart;
}

void Sys_AddDefaultConfig(void)
{
}

void Sys_Sleep(int msec)
{
    Sleep(msec);
}

const char *Sys_ErrorString(int err)
{
    static std::array<char, 256> buf;

    if (!FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                        FORMAT_MESSAGE_IGNORE_INSERTS |
                        FORMAT_MESSAGE_MAX_WIDTH_MASK, NULL, err,
                        MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
                        buf.data(), static_cast<DWORD>(buf.size()), NULL))
        Q_snprintf(buf.data(), buf.size(), "unknown error %d", err);

    return buf.data();
}

/*
================
Sys_Init
================
*/
void Sys_Init(void)
{
#ifdef _DEBUG
    sys_debugprint = Cvar_Get("sys_debugprint", "0", 0);
#endif

    if (!QueryPerformanceFrequency(&timer_freq))
        Sys_Error("QueryPerformanceFrequency failed");

    if (COM_DEDICATED)
        SetErrorMode(SEM_FAILCRITICALERRORS);

    // basedir <path>
    // allows the game to run from outside the data tree
    sys_basedir = Cvar_Get("basedir", ".", CVAR_NOSET);
    sys_libdir = Cvar_Get("libdir", ".", CVAR_NOSET);

    // homedir <path>
    // specifies per-user writable directory for demos, screenshots, etc
    sys_homedir = Cvar_Get("homedir", "", CVAR_NOSET);

    sys_exitonerror = Cvar_Get("sys_exitonerror", "0", 0);

#if USE_WINSVC
    Cmd_AddCommand("installservice", Sys_InstallService_f);
    Cmd_AddCommand("deleteservice", Sys_DeleteService_f);
#endif

#if USE_SYSCON
#if USE_CLIENT
    cvar_t *sys_viewlog = Cvar_Get("sys_viewlog", "0", CVAR_NOSET);

    if (dedicated->integer || sys_viewlog->integer)
#endif
        Sys_ConsoleInit();
#endif // USE_SYSCON

#if USE_DBGHELP
    // install our exception filter
    cvar_t *var = Cvar_Get("sys_disablecrashdump", "0", CVAR_NOSET);
    if (!var->integer)
        Sys_InstallExceptionFilter();
#endif
}

/*
========================================================================

DLL LOADING

========================================================================
*/

void Sys_FreeLibrary(void *handle)
{
    if (handle && !FreeLibrary(reinterpret_cast<HMODULE>(handle))) {
        Com_Error(ERR_FATAL, "FreeLibrary failed on %p", handle);
    }
}

void *Sys_LoadLibrary(const char *path, const char *sym, void **handle)
{
    HMODULE module;
    void    *entry;

    *handle = NULL;

    module = LoadLibraryA(path);
    if (!module) {
        Com_SetLastError(va("%s: LoadLibrary failed: %s",
                            path, Sys_ErrorString(GetLastError())));
        return NULL;
    }

    if (sym) {
        entry = reinterpret_cast<void *>(GetProcAddress(module, sym));
        if (!entry) {
            Com_SetLastError(va("%s: GetProcAddress(%s) failed: %s",
                                path, sym, Sys_ErrorString(GetLastError())));
            FreeLibrary(module);
            return NULL;
        }
    } else {
        entry = NULL;
    }

    *handle = reinterpret_cast<void *>(module);
    return entry;
}

void *Sys_GetProcAddress(void *handle, const char *sym)
{
    void    *entry;

    entry = reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(handle), sym));
    if (!entry)
        Com_SetLastError(va("GetProcAddress(%s) failed: %s",
                            sym, Sys_ErrorString(GetLastError())));

    return entry;
}

#if USE_MEMORY_TRACES
void Sys_BackTrace(void **output, size_t count, size_t offset)
{
    CaptureStackBackTrace(offset, count, output, NULL);
}
#endif

/*
========================================================================

FILESYSTEM

========================================================================
*/

/*
=================
Sys_ListFiles_r
=================
*/
void Sys_ListFiles_r(listfiles_t *list, const char *path, int depth)
{
    struct _finddatai64_t   data;
    intptr_t    handle;
    char        fullpath[MAX_OSPATH], *name;
    size_t      pathlen, len;
    unsigned    mask;
    void        *info;
    const char  *filter = list->filter;

    if (list->count >= MAX_LISTED_FILES) {
        return;
    }

    // optimize single extension search
    if (!(list->flags & (FS_SEARCH_BYFILTER | FS_SEARCH_RECURSIVE)) &&
        filter && !strchr(filter, ';')) {
        if (*filter == '.') {
            filter++;
        }
        len = Q_concat(fullpath, sizeof(fullpath), path, "\\*.", filter);
        filter = NULL; // do not check it later
    } else {
        len = Q_concat(fullpath, sizeof(fullpath), path, "\\*");
    }

    if (len >= sizeof(fullpath)) {
        return;
    }

    // format path to windows style
    // done on the first run only
    if (!depth) {
        FS_ReplaceSeparators(fullpath, '\\');
    }

    handle = _findfirsti64(fullpath, &data);
    if (handle == -1) {
        return;
    }

    // make it point right after the slash
    pathlen = strlen(path) + 1;

    do {
        if (!strcmp(data.name, ".") || !strcmp(data.name, "..")) {
            continue; // ignore special entries
        }

        if (data.attrib & (_A_HIDDEN | _A_SYSTEM)) {
            continue;
        }

        // construct full path
        len = strlen(data.name);
        if (pathlen + len >= sizeof(fullpath)) {
            continue;
        }

        memcpy(fullpath + pathlen, data.name, len + 1);

        if (data.attrib & _A_SUBDIR) {
            mask = FS_SEARCH_DIRSONLY;
        } else {
            mask = 0;
        }

        // pattern search implies recursive search
        if ((list->flags & (FS_SEARCH_BYFILTER | FS_SEARCH_RECURSIVE))
            && mask && depth < MAX_LISTED_DEPTH) {
            Sys_ListFiles_r(list, fullpath, depth + 1);

            // re-check count
            if (list->count >= MAX_LISTED_FILES) {
                break;
            }
        }

        // check type
        if ((list->flags & FS_SEARCH_DIRSONLY) != mask) {
            continue;
        }

        // check filter
        if (filter) {
            if (list->flags & FS_SEARCH_BYFILTER) {
                if (!FS_WildCmp(filter, fullpath + list->baselen)) {
                    continue;
                }
            } else {
                if (!FS_ExtCmp(filter, data.name)) {
                    continue;
                }
            }
        }

        // skip path
        name = fullpath + list->baselen;

        // reformat it back to quake filesystem style
        FS_ReplaceSeparators(name, '/');

        // strip extension
        if (list->flags & FS_SEARCH_STRIPEXT) {
            *COM_FileExtension(name) = 0;

            if (!*name) {
                continue;
            }
        }

        // copy info off
        if (list->flags & FS_SEARCH_EXTRAINFO) {
            info = FS_CopyInfo(name, data.size, data.time_create, data.time_write);
        } else {
            info = FS_CopyString(name);
        }

        list->files = FS_ReallocList(list->files, list->count + 1);
        list->files[list->count++] = info;
    } while (list->count < MAX_LISTED_FILES && _findnexti64(handle, &data) == 0);

    _findclose(handle);
}

/*
========================================================================

GAME PATH DETECTION

========================================================================
*/

#define QUAKE_II_GOG_CLASSIC_APP_ID     "1441704824"
#define QUAKE_II_GOG_RERELEASE_APP_ID   "1947927225"
#define QUAKE_II_XBOX_FAMILY_NAME       L"BethesdaSoftworks.ProjAthena_3275kfvn8vcwc"

bool Steam_GetInstallationPath(char *out_dir, size_t out_dir_length)
{
    DWORD path_length = out_dir_length;

#ifndef _WIN64
    LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam\\", "InstallPath", RRF_RT_REG_SZ, NULL, (PVOID) out_dir, &path_length);
#else
    LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam\\", "InstallPath", RRF_RT_REG_SZ, NULL, (PVOID) out_dir, &path_length);
#endif

    if (status != ERROR_SUCCESS) {
        if (status != ERROR_FILE_NOT_FOUND) // ERROR_FILE_NOT_FOUND may just mean Steam's not installed
            Com_WPrintf("Error %lu finding Steam installation.\n", GetLastError());
        return false;
    }

    return true;
}

static bool find_gog_installation_path(const char *app_id, char *out_dir, size_t out_dir_length)
{
    std::array<char, MAX_OSPATH> folder_path;
    DWORD folder_path_len = static_cast<DWORD>(folder_path.size());
    bool result = false;
    
#ifndef _WIN64
    LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, va("SOFTWARE\\GOG.com\\Games\\%s\\", app_id), "path", RRF_RT_REG_SZ, NULL, (PVOID)folder_path.data(), &folder_path_len);
#else
    LSTATUS status = RegGetValueA(HKEY_LOCAL_MACHINE, va("SOFTWARE\\WOW6432Node\\GOG.com\\Games\\%s\\", app_id), "path", RRF_RT_REG_SZ, NULL, (PVOID)folder_path.data(), &folder_path_len);
#endif

    if (status != ERROR_SUCCESS) {
        Com_WPrintf("Error %lu finding GOG installation.\n", GetLastError());
        return result;
    }

    Q_strlcpy(out_dir, folder_path.data(), out_dir_length);

    FS_NormalizePath(out_dir);

    return true;
}

static bool find_gog_installation_path_rr(rerelease_mode_t rr_mode, char *out_dir, size_t out_dir_length)
{
    if (com_rerelease->integer != RERELEASE_MODE_YES)
        return false;
    return find_gog_installation_path(QUAKE_II_GOG_RERELEASE_APP_ID, out_dir, out_dir_length);
}

static bool find_gog_installation_path_classic(rerelease_mode_t rr_mode, char *out_dir, size_t out_dir_length)
{
    if (com_rerelease->integer != RERELEASE_MODE_NO)
        return false;
    return find_gog_installation_path(QUAKE_II_GOG_CLASSIC_APP_ID, out_dir, out_dir_length);
}

static bool find_xbox_installation_path(rerelease_mode_t rr_mode, char *out_dir, size_t out_dir_length)
{
    if (com_rerelease->integer != RERELEASE_MODE_YES)
        return false;
    if (!IsWindows8Point1OrGreater())
        return false;

    const PCWSTR family_name = QUAKE_II_XBOX_FAMILY_NAME;

    std::array<WCHAR, MAX_PATH> buffer{};
    std::array<PWSTR, 1> packageNames{};
    uint32_t num_packages = 1;
    uint32_t buffer_length = static_cast<uint32_t>(buffer.size());
    LONG result = GetPackagesByPackageFamily(family_name, &num_packages, packageNames.data(), &buffer_length, buffer.data());

    if (result)
        return false;

    std::array<WCHAR, MAX_PATH> path{};
    uint32_t pathLength = static_cast<uint32_t>(path.size());
    result = GetPackagePathByFullName(packageNames[0], &pathLength, path.data());

    if (result)
        return false;

    WideCharToMultiByte(CP_ACP, 0, path.data(), pathLength, out_dir, out_dir_length, NULL, NULL);
    return true;
}

// Installation detection functions, called by FS_FindBaseDir in order
const sys_getinstalledgamepath_func_t gamepath_funcs[] = {
    &Steam_FindQuake2Path,
    &find_gog_installation_path_rr,
    &find_xbox_installation_path,
    &find_gog_installation_path_classic,
    NULL
};

// Locate rerelease home dir
bool Sys_GetRereleaseHomeDir(char *path, size_t path_length)
{
    bool result = false;
    PWSTR saved_games_path = NULL;
    HRESULT hr = SHGetKnownFolderPath(FOLDERID_SavedGames, KF_FLAG_CREATE | KF_FLAG_INIT, NULL, &saved_games_path);
    if (!SUCCEEDED(hr))
    {
        Com_WPrintf("Failed to retrieve Saved Games path (%.8lx)\n", (long)hr);
        goto done;
    }

    if (WideCharToMultiByte(CP_UTF8, 0, saved_games_path, -1, path, (int)path_length, NULL, NULL) == 0)
    {
        DWORD err = GetLastError();
        Com_WPrintf("Failed to convert Saved Games path (%lu)\n", err);
        goto done;
    }

    Q_strlcat(path, "\\NightDive Studios\\Quake II", path_length);
    result = true;

done:
    CoTaskMemFree(saved_games_path);
    return result;
}

/*
========================================================================

MAIN

========================================================================
*/

static void fix_current_directory(void)
{
    WCHAR buffer[MAX_PATH];
    DWORD ret = GetModuleFileNameW(NULL, buffer, MAX_PATH);

    if (ret < MAX_PATH)
        while (ret)
            if (buffer[--ret] == '\\')
                break;

    if (ret == 0)
        Sys_Error("Can't determine base directory");

    if (ret >= MAX_PATH - MAX_QPATH)
        Sys_Error("Base directory path too long. Move your " PRODUCT " installation to a shorter path.");

    buffer[ret] = 0;
    if (!SetCurrentDirectoryW(buffer))
        Sys_Error("SetCurrentDirectoryW failed");
}

#if (_MSC_VER >= 1400)
static void msvcrt_sucks(const wchar_t *expr, const wchar_t *func,
                         const wchar_t *file, unsigned int line, uintptr_t unused)
{
}
#endif

static int Sys_Main(int argc, char **argv)
{
#if USE_WINSVC
    if (statusHandle && setjmp(exitBuf))
        return 0;
#endif

    // fix current directory to point to the basedir
    fix_current_directory();

#if (_MSC_VER >= 1400)
    // work around strftime given invalid format string
    // killing the whole fucking process :((
    _set_invalid_parameter_handler(msvcrt_sucks);
#endif

#ifndef _WIN64
    HeapSetInformation(NULL, HeapEnableTerminationOnCorruption, NULL, 0);
#endif

    main_thread_id = GetCurrentThreadId();

    Qcommon_Init(argc, argv);

    // main program loop
    while (!atomic_load(&shouldExit))
        Qcommon_Frame();

    Com_Quit(NULL, ERR_DISCONNECT);
    return 0;   // never gets here
}

#if USE_CLIENT

/*
==================
WinMain

==================
*/
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // previous instances do not exist in Win32
    if (hPrevInstance) {
        return 1;
    }

    hGlobalInstance = hInstance;

    return Sys_Main(__argc, __argv);
}

#else // USE_CLIENT

#if USE_WINSVC

static char     **sys_argv;
static int      sys_argc;

static void WINAPI ServiceHandler(DWORD fdwControl)
{
    if (fdwControl == SERVICE_CONTROL_STOP) {
        atomic_store(&shouldExit, TRUE);
    }
}

static void WINAPI ServiceMain(DWORD argc, LPSTR *argv)
{
    SERVICE_STATUS    status;

    statusHandle = RegisterServiceCtrlHandlerA(APPLICATION, ServiceHandler);
    if (!statusHandle) {
        return;
    }

    memset(&status, 0, sizeof(status));
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_RUNNING;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    SetServiceStatus(statusHandle, &status);

    Sys_Main(sys_argc, sys_argv);

    status.dwCurrentState = SERVICE_STOPPED;
    status.dwControlsAccepted = 0;
    SetServiceStatus(statusHandle, &status);
}

static char serviceName[] = APPLICATION;

static const SERVICE_TABLE_ENTRYA serviceTable[] = {
    { serviceName, ServiceMain },
    { NULL, NULL }
};

#endif // USE_WINSVC

/*
==================
main

==================
*/
int main(int argc, char **argv)
{
    hGlobalInstance = GetModuleHandle(NULL);

#if USE_WINSVC
    if (argc > 1 && !strcmp(argv[1], "-service")) {
        sys_argc = argc - 1;
        sys_argv = argv + 1;
        if (StartServiceCtrlDispatcherA(serviceTable)) {
            return 0;
        }
        fprintf(stderr, "%s\n", Sys_ErrorString(GetLastError()));
        return 1;
    }
#endif

    return Sys_Main(argc, argv);
}

#endif // !USE_CLIENT
