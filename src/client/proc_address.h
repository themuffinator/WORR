#pragma once

#include <bit>
#include <type_traits>

#include "system/system.h"

// Helper to convert untyped procedure addresses to typed function pointers.
template <typename Func>
[[nodiscard]] inline Func Sys_CastFunctionAddress(void *address)
{
    static_assert(std::is_pointer_v<Func>, "Func must be a pointer type");

    if (!address) {
        return nullptr;
    }

    static_assert(sizeof(Func) == sizeof(address), "Function pointer size mismatch");
    return std::bit_cast<Func>(address);
}

// Helper to retrieve typed function pointers from dynamic libraries.
template <typename Func>
[[nodiscard]] inline Func Sys_GetFunctionAddress(void *handle, const char *symbol)
{
    return Sys_CastFunctionAddress<Func>(Sys_GetProcAddress(handle, symbol));
}
