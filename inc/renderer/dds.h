/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "shared/shared.h"

typedef void *(*r_dds_alloc_fn)(size_t size);

int R_DecodeDDS(const byte *rawdata, size_t rawlen,
                int *out_width, int *out_height,
                byte **out_rgba, bool *out_has_alpha,
                r_dds_alloc_fn alloc_fn);

