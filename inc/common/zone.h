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

#pragma once

#define Z_CopyString(string)    Z_TagCopyString(string, TAG_GENERAL)
#define Z_CopyStruct(ptr)       memcpy(Z_Malloc(sizeof(*ptr)), ptr, sizeof(*ptr))

#ifdef __cplusplus
struct z_allocation {
    void *value;

    constexpr z_allocation(void *p = nullptr) noexcept
        : value(p) {}

    template <typename T>
    constexpr operator T *() const noexcept
    {
        return static_cast<T *>(value);
    }

    constexpr explicit operator bool() const noexcept
    {
        return value != nullptr;
    }
};

using z_pointer = z_allocation;
#else
typedef void *z_pointer;
#endif

// memory tags to allow dynamic memory to be cleaned up
// game DLL has separate tag namespace starting at TAG_MAX
typedef enum {
    TAG_FREE,       // should have never been set
    TAG_STATIC,

    TAG_GENERAL,
    TAG_CMD,
    TAG_CVAR,
    TAG_FILESYSTEM,
    TAG_RENDERER,
    TAG_UI,
    TAG_SERVER,
    TAG_MVD,
    TAG_SOUND,
    TAG_CMODEL,
    TAG_NAV,
    TAG_MAPDB,

    TAG_MAX
} memtag_t;

#ifdef __cplusplus
namespace zone_c_api {
extern "C" {
#endif

#if defined(_MSC_VER)
// MSVC's __declspec(allocator) applies only to pointer or reference return types.
// The custom z_pointer wrapper used in C++ builds does not satisfy this requirement,
// so skip the attribute for these functions on that compiler.
#define q_zone_allocator
#else
#define q_zone_allocator q_malloc
#endif

void    Z_Init(void);
void    Z_Free(void *ptr);
void    Z_Freep(void *ptr);
void   *Z_Realloc(void *ptr, size_t size);
void   *Z_ReallocArray(void *ptr, size_t nmemb, size_t size, memtag_t tag);
q_zone_allocator
void   *Z_Malloc(size_t size);
q_zone_allocator
void   *Z_Mallocz(size_t size);
q_zone_allocator
void   *Z_TagMalloc(size_t size, memtag_t tag);
q_zone_allocator
void   *Z_TagMallocz(size_t size, memtag_t tag);
q_malloc
char    *Z_TagCopyString(const char *in, memtag_t tag);
#undef q_zone_allocator
void    Z_FreeTags(memtag_t tag);
void    Z_LeakTest(memtag_t tag);
void    Z_Stats_f(void);

// may return pointer to static memory
char    *Z_CvarCopyString(const char *in);

}

static inline z_allocation Z_Realloc_allocation(void *ptr, size_t size) noexcept
{
    return z_allocation{zone_c_api::Z_Realloc(ptr, size)};
}

static inline z_allocation Z_ReallocArray_allocation(void *ptr, size_t nmemb, size_t size, memtag_t tag) noexcept
{
    return z_allocation{zone_c_api::Z_ReallocArray(ptr, nmemb, size, tag)};
}

static inline z_allocation Z_Malloc_allocation(size_t size) noexcept
{
    return z_allocation{zone_c_api::Z_Malloc(size)};
}

static inline z_allocation Z_Mallocz_allocation(size_t size) noexcept
{
    return z_allocation{zone_c_api::Z_Mallocz(size)};
}

static inline z_allocation Z_TagMalloc_allocation(size_t size, memtag_t tag) noexcept
{
    return z_allocation{zone_c_api::Z_TagMalloc(size, tag)};
}

static inline z_allocation Z_TagMallocz_allocation(size_t size, memtag_t tag) noexcept
{
    return z_allocation{zone_c_api::Z_TagMallocz(size, tag)};
}

#define Z_Realloc(ptr, size) (Z_Realloc_allocation((ptr), (size)))
#define Z_ReallocArray(ptr, nmemb, size, tag) (Z_ReallocArray_allocation((ptr), (nmemb), (size), (tag)))
#define Z_Malloc(size) (Z_Malloc_allocation((size)))
#define Z_Mallocz(size) (Z_Mallocz_allocation((size)))
#define Z_TagMalloc(size, tag) (Z_TagMalloc_allocation((size), (tag)))
#define Z_TagMallocz(size, tag) (Z_TagMallocz_allocation((size), (tag)))

#endif
