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

//
// win_dbg.c -- crash dump generation
//

#include "client.h"
#include <dbghelp.h>

typedef HINSTANCE (WINAPI *SHELLEXECUTEA)(HWND, LPCSTR, LPCSTR, LPCSTR, LPCSTR, INT);
typedef	BOOL (WINAPI *MINIDUMPWRITEDUMP)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                           CONST PMINIDUMP_EXCEPTION_INFORMATION,
                                           PMINIDUMP_USER_STREAM_INFORMATION,
                                           PMINIDUMP_CALLBACK_INFORMATION);

static SHELLEXECUTEA pShellExecuteA;
static MINIDUMPWRITEDUMP pMiniDumpWriteDump;

static volatile LONG exceptionEntered;
static LPTOP_LEVEL_EXCEPTION_FILTER prevExceptionFilter;

// write a minidump, which is actually useful
// compared to text info
static bool write_minidump(LPEXCEPTION_POINTERS exceptionInfo)
{
    // get base directory to save crash dump to
    char execdir[MAX_PATH] = { 0 };

    size_t len = GetModuleFileNameA(NULL, execdir, sizeof(execdir));
    if (!len || len >= sizeof(execdir))
        return EXCEPTION_CONTINUE_SEARCH;

    while (--len)
        if (execdir[len] == '\\')
            break;

    execdir[len] = 0;

    MINIDUMP_EXCEPTION_INFORMATION M = { 0 };

    M.ThreadId = GetCurrentThreadId();
    M.ExceptionPointers = exceptionInfo;
    
    char dumppath[MAX_PATH] = { 0 };
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    uint64_t *asU64 = (uint64_t *) &time;
    Q_strlcat(dumppath, execdir, sizeof(dumppath));
    Q_strlcat(dumppath, va("\\crashdump_%llu.dmp", *asU64), sizeof(dumppath));

    HANDLE dumpFile = CreateFile(dumppath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (dumpFile == INVALID_HANDLE_VALUE)
        return false;

    pMiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
        dumpFile, MiniDumpNormal,
        (exceptionInfo) ? &M : NULL, NULL, NULL);

    pShellExecuteA(NULL, "open", "explorer.exe", va("/select,\"%s\"", dumppath), NULL, SW_SHOW);

    CloseHandle(dumpFile);

    return true;
}

#define CRASH_TITLE PRODUCT " Unhandled Exception"

// be careful to avoid using any non-trivial C runtime functions here!
// C runtime structures may be already corrupted and unusable at this point.
static LONG WINAPI exception_filter(LPEXCEPTION_POINTERS exceptionInfo)
{
    int ret;
    HMODULE moduleHandle;
    LONG action;

    // give previous filter a chance to handle this exception
    if (prevExceptionFilter) {
        action = prevExceptionFilter(exceptionInfo);
        if (action != EXCEPTION_CONTINUE_SEARCH) {
            return action;
        }
    }

    // debugger present? not our business
    if (IsDebuggerPresent()) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // only enter once
    if (InterlockedCompareExchange(&exceptionEntered, 1, 0)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }

#if USE_CLIENT
    Win_Shutdown();
#endif

    ret = MessageBoxA(NULL,
                      PRODUCT " has encountered an unhandled "
                      "exception and needs to be terminated.\n"
                      "Would you like to generate a crash report?",
                      CRASH_TITLE,
                      MB_ICONERROR | MB_YESNO
#if USE_SERVER
                      | MB_SERVICE_NOTIFICATION
#endif
                      );

    if (ret == IDNO) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

#define LL(x)                                   \
    do {                                        \
        moduleHandle = LoadLibraryA(x);         \
        if (!moduleHandle) {                    \
            return EXCEPTION_CONTINUE_SEARCH;   \
        }                                       \
    } while(0)

#define GPA(x, y)                                   \
    do {                                            \
        p##y = (x)GetProcAddress(moduleHandle, #y); \
        if (!p##y) {                                \
            return EXCEPTION_CONTINUE_SEARCH;       \
        }                                           \
    } while(0)

    LL("dbghelp.dll");
    GPA(MINIDUMPWRITEDUMP, MiniDumpWriteDump);

    LL("shell32.dll");
    GPA(SHELLEXECUTEA, ShellExecuteA);

    if (!write_minidump(exceptionInfo)) {
        MessageBoxA(NULL,
                    "Couldn't create crash report. :(",
                    CRASH_TITLE,
                    MB_ICONERROR);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    MessageBoxA(NULL,
                "Crash report .dmp written; please submit to:\nhttps://github.com/Paril/q2pro/issues",
                CRASH_TITLE,
                MB_ICONERROR);

    return EXCEPTION_EXECUTE_HANDLER;
}

void Sys_InstallExceptionFilter(void)
{
    prevExceptionFilter = SetUnhandledExceptionFilter(exception_filter);
}
