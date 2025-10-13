/*
Copyright (C) 2023 Andrey Nazarov

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

#ifdef __cplusplus
#include <atomic>

struct atomic_int {
    constexpr atomic_int() noexcept = default;
    constexpr explicit atomic_int(int desired) noexcept
        : value(desired) {}

    atomic_int(const atomic_int &) = delete;
    atomic_int &operator=(const atomic_int &) = delete;

    std::atomic<int> value{0};
};

inline int atomic_load(const atomic_int *p) noexcept
{
    return p->value.load(std::memory_order_acquire);
}

template <typename T>
inline void atomic_store(atomic_int *p, T desired) noexcept
{
    p->value.store(static_cast<int>(desired), std::memory_order_release);
}

#else
#include <stdatomic.h>
#endif
