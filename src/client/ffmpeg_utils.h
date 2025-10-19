/*
Copyright (C) 2024 Andrey Nazarov

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

#include <type_traits>

#include <libavutil/channel_layout.h>

static inline int Q_AVChannelLayoutDefault(AVChannelLayout *layout, int nb_channels)
{
    using ChannelLayoutDefaultFn = decltype(&av_channel_layout_default);

    if constexpr (std::is_same_v<ChannelLayoutDefaultFn, int (*)(AVChannelLayout *, int)>) {
        return av_channel_layout_default(layout, nb_channels);
    }

    av_channel_layout_default(layout, nb_channels);
    return 0;
}
