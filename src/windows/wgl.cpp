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

#include "client.h"

#include <GL/gl.h>
#include <GL/wglext.h>

#include <array>

static struct {
    HGLRC       context;        // handle to GL rendering context
    HMODULE     handle;         // handle to GL library
    unsigned    extensions;
    PFNWGLCREATECONTEXTATTRIBSARBPROC   CreateContextAttribsARB;
    PFNWGLCHOOSEPIXELFORMATARBPROC      ChoosePixelFormatARB;
    PFNWGLSWAPINTERVALEXTPROC           SwapIntervalEXT;
} wgl;

static cvar_t   *gl_allow_software;

enum {
    QWGL_ARB_create_context             = BIT(0),
    QWGL_ARB_multisample                = BIT(1),
    QWGL_ARB_pixel_format               = BIT(2),
    QWGL_EXT_create_context_es_profile  = BIT(3),
    QWGL_EXT_swap_control               = BIT(4),
    QWGL_EXT_swap_control_tear          = BIT(5),
};

static unsigned wgl_parse_extension_string(const char *s)
{
    static const char *const extnames[] = {
        "WGL_ARB_create_context",
        "WGL_ARB_multisample",
        "WGL_ARB_pixel_format",
        "WGL_EXT_create_context_es_profile",
        "WGL_EXT_swap_control",
        "WGL_EXT_swap_control_tear",
        NULL
    };

    return Com_ParseExtensionString(s, extnames);
}

static void wgl_shutdown(void)
{
    wglMakeCurrent(NULL, NULL);

    if (wgl.context) {
        wglDeleteContext(wgl.context);
        wgl.context = NULL;
    }

    Win_Shutdown();

    memset(&wgl, 0, sizeof(wgl));
}

static void print_error(const char *what)
{
    Com_EPrintf("%s failed: %s\n", what, Sys_ErrorString(GetLastError()));
}

#define FAIL_OK     0
#define FAIL_SOFT   -1
#define FAIL_HARD   -2

static int wgl_setup_gl(r_opengl_config_t cfg)
{
    auto soft_failure = []() {
        Win_Shutdown();
        return FAIL_SOFT;
    };
    auto hard_failure = []() {
        Win_Shutdown();
        return FAIL_HARD;
    };

    PIXELFORMATDESCRIPTOR pfd{};
    int pixelformat;

    // create the main window
    Win_Init();

    // choose pixel format
    if (wgl.ChoosePixelFormatARB && cfg.multisamples) {
        const int attr[] = {
            WGL_DRAW_TO_WINDOW_ARB, TRUE,
            WGL_SUPPORT_OPENGL_ARB, TRUE,
            WGL_DOUBLE_BUFFER_ARB, TRUE,
            WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
            WGL_COLOR_BITS_ARB, cfg.colorbits,
            WGL_DEPTH_BITS_ARB, cfg.depthbits,
            WGL_STENCIL_BITS_ARB, cfg.stencilbits,
            WGL_SAMPLE_BUFFERS_ARB, 1,
            WGL_SAMPLES_ARB, cfg.multisamples,
            0
        };
        UINT num_formats;

        if (!wgl.ChoosePixelFormatARB(win.dc, attr, NULL, 1, &pixelformat, &num_formats)) {
            print_error("wglChoosePixelFormatARB");
            return soft_failure();
        }
        if (num_formats == 0) {
            Com_EPrintf("No suitable OpenGL pixelformat found for %d multisamples\n", cfg.multisamples);
            return soft_failure();
        }
    } else {
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = static_cast<BYTE>(cfg.colorbits);
        pfd.cDepthBits = static_cast<BYTE>(cfg.depthbits);
        pfd.cStencilBits = static_cast<BYTE>(cfg.stencilbits);
        pfd.iLayerType = PFD_MAIN_PLANE;

        if (!(pixelformat = ChoosePixelFormat(win.dc, &pfd))) {
            print_error("ChoosePixelFormat");
            return soft_failure();
        }
    }

    // set pixel format
    if (!DescribePixelFormat(win.dc, pixelformat, sizeof(pfd), &pfd)) {
        print_error("DescribePixelFormat");
        return soft_failure();
    }

    if (!SetPixelFormat(win.dc, pixelformat, &pfd)) {
        print_error("SetPixelFormat");
        return soft_failure();
    }

    // check for software emulation
    if (pfd.dwFlags & PFD_GENERIC_FORMAT) {
        if (!gl_allow_software->integer) {
            Com_EPrintf("No hardware OpenGL acceleration detected\n");
            return soft_failure();
        }
        Com_WPrintf("Using software emulation\n");
    } else if (pfd.dwFlags & PFD_GENERIC_ACCELERATED) {
        Com_DPrintf("MCD acceleration found\n");
    } else {
        Com_DPrintf("ICD acceleration found\n");
    }

    // startup the OpenGL subsystem by creating a context and making it current
    if (wgl.CreateContextAttribsARB && (cfg.debug || cfg.profile)) {
        std::array<int, 9> attr{};
        int i = 0;

        if (cfg.profile) {
            attr[i++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
            attr[i++] = cfg.major_ver;
            attr[i++] = WGL_CONTEXT_MINOR_VERSION_ARB;
            attr[i++] = cfg.minor_ver;
        }
        if (cfg.profile == QGL_PROFILE_ES) {
            attr[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
            attr[i++] = WGL_CONTEXT_ES_PROFILE_BIT_EXT;
        } else if (cfg.profile == QGL_PROFILE_CORE) {
            attr[i++] = WGL_CONTEXT_PROFILE_MASK_ARB;
            attr[i++] = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
        }
        if (cfg.debug) {
            attr[i++] = WGL_CONTEXT_FLAGS_ARB;
            attr[i++] = WGL_CONTEXT_DEBUG_BIT_ARB;
        }
        attr[i] = 0;

        if (!(wgl.context = wgl.CreateContextAttribsARB(win.dc, NULL, attr.data()))) {
            print_error("wglCreateContextAttribsARB");
            return soft_failure();
        }
    } else {
        if (!(wgl.context = wglCreateContext(win.dc))) {
            print_error("wglCreateContext");
            return hard_failure();
        }
    }

    if (!wglMakeCurrent(win.dc, wgl.context)) {
        print_error("wglMakeCurrent");
        wglDeleteContext(wgl.context);
        wgl.context = NULL;
        return hard_failure();
    }

    return FAIL_OK;
}

#define GPA(x)  (void *)wglGetProcAddress(x)

static unsigned get_fake_window_extensions(void)
{
    static const char wndClass[] = PRODUCT " FAKE WINDOW CLASS";
    static const char name[] = PRODUCT " FAKE WINDOW NAME";
    unsigned extensions = 0;

    WNDCLASSEXA wc{};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = DefWindowProc;
    wc.hInstance = hGlobalInstance;
    wc.lpszClassName = wndClass;

    bool class_registered = RegisterClassExA(&wc) != 0;
    if (!class_registered)
        return 0;

    HWND wnd = CreateWindowA(wndClass, name, 0, 0, 0, 0, 0,
                             NULL, NULL, hGlobalInstance, NULL);
    HDC dc = NULL;
    HGLRC rc = NULL;
    PIXELFORMATDESCRIPTOR pfd{};
    int pixelformat = 0;
    bool made_current = false;

    do {
        if (!wnd)
            break;

        dc = GetDC(wnd);
        if (!dc)
            break;

        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cDepthBits = 24;
        pfd.iLayerType = PFD_MAIN_PLANE;

        pixelformat = ChoosePixelFormat(dc, &pfd);
        if (!pixelformat)
            break;

        if (!SetPixelFormat(dc, pixelformat, &pfd))
            break;

        rc = wglCreateContext(dc);
        if (!rc)
            break;

        if (!wglMakeCurrent(dc, rc))
            break;

        made_current = true;

        auto wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(
            GPA("wglGetExtensionsStringARB"));
        if (!wglGetExtensionsStringARB)
            break;

        extensions = wgl_parse_extension_string(wglGetExtensionsStringARB(dc));

        if (extensions & QWGL_ARB_create_context) {
            wgl.CreateContextAttribsARB = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
                GPA("wglCreateContextAttribsARB"));
        }

        if (extensions & QWGL_ARB_pixel_format) {
            wgl.ChoosePixelFormatARB = reinterpret_cast<PFNWGLCHOOSEPIXELFORMATARBPROC>(
                GPA("wglChoosePixelFormatARB"));
        }
    } while (false);

    if (made_current)
        wglMakeCurrent(NULL, NULL);
    if (rc)
        wglDeleteContext(rc);
    if (dc)
        ReleaseDC(wnd, dc);
    if (wnd)
        DestroyWindow(wnd);
    if (class_registered)
        UnregisterClassA(wndClass, hGlobalInstance);

    return extensions;
}

static bool wgl_init(void)
{
    const char *extensions = NULL;
    unsigned fake_extensions = 0;
    r_opengl_config_t cfg;
    int ret;

    gl_allow_software = Cvar_Get("gl_allow_software", "0", 0);

    wgl.handle = GetModuleHandle("opengl32");
    if (!wgl.handle) {
        print_error("GetModuleHandle");
        return false;
    }

    cfg = R_GetGLConfig();

    // check for extensions by creating a fake window
    if (cfg.multisamples || cfg.debug || cfg.profile)
        fake_extensions = get_fake_window_extensions();

    if (cfg.multisamples) {
        if (fake_extensions & QWGL_ARB_multisample) {
            if (!wgl.ChoosePixelFormatARB) {
                Com_WPrintf("Ignoring WGL_ARB_multisample, WGL_ARB_pixel_format not found\n");
                cfg.multisamples = 0;
            }
        } else {
            Com_WPrintf("WGL_ARB_multisample not found for %d multisamples\n", cfg.multisamples);
            cfg.multisamples = 0;
        }
    }

    if ((cfg.debug || cfg.profile) && !wgl.CreateContextAttribsARB) {
        Com_WPrintf("WGL_ARB_create_context not found\n");
        cfg.debug = false;
        cfg.profile = QGL_PROFILE_NONE;
    }

    if (cfg.profile == QGL_PROFILE_ES && !(fake_extensions & QWGL_EXT_create_context_es_profile)) {
        Com_WPrintf("WGL_EXT_create_context_es_profile not found\n");
        cfg.profile = QGL_PROFILE_NONE;
    }

    // create window, choose PFD, setup OpenGL context
    ret = wgl_setup_gl(cfg);

    // attempt to recover
    if (ret == FAIL_SOFT) {
        Com_Printf("Falling back to failsafe config\n");
        r_opengl_config_t failsafe{};
        failsafe.colorbits = 24;
        failsafe.depthbits = 24;
        ret = wgl_setup_gl(failsafe);
    }

    if (ret) {
        // it failed, clean up
        memset(&wgl, 0, sizeof(wgl));
        return false;
    }

    // initialize WGL extensions
    auto wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(
        GPA("wglGetExtensionsStringARB"));
    if (wglGetExtensionsStringARB)
        extensions = wglGetExtensionsStringARB(win.dc);

    // fall back to GL_EXTENSIONS for legacy drivers
    if (!extensions || !*extensions)
        extensions = (const char *)glGetString(GL_EXTENSIONS);

    wgl.extensions = wgl_parse_extension_string(extensions);

    if (wgl.extensions & QWGL_EXT_swap_control) {
        wgl.SwapIntervalEXT = reinterpret_cast<PFNWGLSWAPINTERVALEXTPROC>(
            GPA("wglSwapIntervalEXT"));
    }

    if (!wgl.SwapIntervalEXT)
        Com_WPrintf("WGL_EXT_swap_control not found\n");

    return true;
}

static void *wgl_get_proc_addr(const char *sym)
{
    void *entry = wglGetProcAddress(sym);

    if (entry)
        return entry;

    return GetProcAddress(wgl.handle, sym);
}

static void wgl_swap_buffers(void)
{
    SwapBuffers(win.dc);
}

static void wgl_swap_interval(int val)
{
    if (val < 0 && !(wgl.extensions & QWGL_EXT_swap_control_tear)) {
        Com_Printf("Negative swap interval is not supported on this system.\n");
        val = -val;
    }

    if (wgl.SwapIntervalEXT && !wgl.SwapIntervalEXT(val))
        print_error("wglSwapIntervalEXT");
}

static bool wgl_probe(void)
{
    return true;
}

const vid_driver_t vid_win32wgl = {
    "win32wgl",
    wgl_probe,
    wgl_init,
    wgl_shutdown,
    nullptr,
    Win_PumpEvents,
    Win_GetModeList,
    Win_GetDpiScale,
    Win_SetMode,
    Win_UpdateGamma,
    wgl_get_proc_addr,
    wgl_swap_buffers,
    wgl_swap_interval,
    nullptr,
    Win_GetClipboardData,
    Win_SetClipboardData,
    Win_InitMouse,
    Win_ShutdownMouse,
    Win_GrabMouse,
    Win_WarpMouse,
    Win_GetMouseMotion,
};
