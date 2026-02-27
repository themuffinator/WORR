/*
Copyright (C) 2026

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "renderer/dds.h"

#include "common/common.h"
#include "common/error.h"
#include "common/intreadwrite.h"

#include <stdint.h>
#include <string.h>

#define DDS_MAX_TEXTURE_SIZE 4096

#define DDS_MAGIC 0x20534444u

#define DDS_FOURCC      0x00000004u
#define DDS_RGB         0x00000040u
#define DDS_RGBA        0x00000041u
#define DDS_LUMINANCE   0x00020000u
#define DDS_ALPHA       0x00000002u
#define DDS_ALPHAPIXELS 0x00000001u

#define DDS_CUBEMAP     0x00000200u
#define DDS_FLAGS_VOLUME 0x00200000u

#define DDS_RESOURCE_DIMENSION_TEXTURE2D 3u
#define DDS_RESOURCE_MISC_TEXTURECUBE    0x4u

#define DDS_MAKEFOURCC(ch0, ch1, ch2, ch3) \
    ((uint32_t)(uint8_t)(ch0) | \
    ((uint32_t)(uint8_t)(ch1) << 8) | \
    ((uint32_t)(uint8_t)(ch2) << 16) | \
    ((uint32_t)(uint8_t)(ch3) << 24))

#define DDS_FOURCC_DXT1 DDS_MAKEFOURCC('D', 'X', 'T', '1')
#define DDS_FOURCC_DXT3 DDS_MAKEFOURCC('D', 'X', 'T', '3')
#define DDS_FOURCC_DXT5 DDS_MAKEFOURCC('D', 'X', 'T', '5')
#define DDS_FOURCC_DX10 DDS_MAKEFOURCC('D', 'X', '1', '0')

typedef struct {
    uint32_t size;
    uint32_t flags;
    uint32_t fourCC;
    uint32_t RGBBitCount;
    uint32_t RBitMask;
    uint32_t GBitMask;
    uint32_t BBitMask;
    uint32_t ABitMask;
} dds_pixel_format_t;

typedef struct {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitchOrLinearSize;
    uint32_t depth;
    uint32_t mipMapCount;
    uint32_t reserved1[11];
    dds_pixel_format_t ddspf;
    uint32_t caps;
    uint32_t caps2;
    uint32_t caps3;
    uint32_t caps4;
    uint32_t reserved2;
} dds_header_t;

typedef struct {
    uint32_t dxgi_format;
    uint32_t resource_dimension;
    uint32_t misc_flag;
    uint32_t array_size;
    uint32_t misc_flags2;
} dds_header_dxt10_t;

_Static_assert(sizeof(dds_pixel_format_t) == 32, "DDS pixel format size mismatch");
_Static_assert(sizeof(dds_header_t) == 128, "DDS header size mismatch");
_Static_assert(sizeof(dds_header_dxt10_t) == 20, "DDS DX10 header size mismatch");

typedef struct {
    uint32_t bits_per_pixel;
    uint32_t r_mask;
    uint32_t g_mask;
    uint32_t b_mask;
    uint32_t a_mask;
    bool luminance;
    bool alpha_only;
    bool force_opaque_alpha;
} dds_uncompressed_desc_t;

typedef enum {
    DDS_CODEC_NONE = 0,
    DDS_CODEC_UNCOMPRESSED,
    DDS_CODEC_BC1,
    DDS_CODEC_BC2,
    DDS_CODEC_BC3
} dds_codec_t;

static uint16_t dds_read_u16(const byte *data)
{
    return (uint16_t)(data[0] | (data[1] << 8));
}

static uint32_t dds_read_u32(const byte *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint32_t dds_mask_for_bits(uint32_t bits)
{
    if (!bits)
        return 0;
    if (bits >= 32)
        return 0xffffffffu;
    return (1u << bits) - 1u;
}

static uint8_t dds_extract_component(uint32_t packed, uint32_t mask)
{
    if (!mask)
        return 0;

    uint32_t shift = 0;
    while (((mask >> shift) & 1u) == 0u)
        shift++;

    uint32_t value = (packed & mask) >> shift;
    uint32_t max_value = mask >> shift;
    if (!max_value)
        return 0;

    return (uint8_t)((value * 255u + (max_value / 2u)) / max_value);
}

static void dds_decode_565(uint16_t color, uint8_t out_rgb[4])
{
    uint8_t r = (uint8_t)((color >> 11) & 0x1f);
    uint8_t g = (uint8_t)((color >> 5) & 0x3f);
    uint8_t b = (uint8_t)(color & 0x1f);

    out_rgb[0] = (uint8_t)((r << 3) | (r >> 2));
    out_rgb[1] = (uint8_t)((g << 2) | (g >> 4));
    out_rgb[2] = (uint8_t)((b << 3) | (b >> 2));
    out_rgb[3] = 255;
}

static void dds_build_bc_palette(uint16_t c0, uint16_t c1, bool allow_transparent,
                                 uint8_t colors[4][4])
{
    dds_decode_565(c0, colors[0]);
    dds_decode_565(c1, colors[1]);

    if (allow_transparent && c0 <= c1) {
        colors[2][0] = (uint8_t)((colors[0][0] + colors[1][0]) / 2);
        colors[2][1] = (uint8_t)((colors[0][1] + colors[1][1]) / 2);
        colors[2][2] = (uint8_t)((colors[0][2] + colors[1][2]) / 2);
        colors[2][3] = 255;

        colors[3][0] = 0;
        colors[3][1] = 0;
        colors[3][2] = 0;
        colors[3][3] = 0;
    } else {
        colors[2][0] = (uint8_t)((2 * colors[0][0] + colors[1][0]) / 3);
        colors[2][1] = (uint8_t)((2 * colors[0][1] + colors[1][1]) / 3);
        colors[2][2] = (uint8_t)((2 * colors[0][2] + colors[1][2]) / 3);
        colors[2][3] = 255;

        colors[3][0] = (uint8_t)((colors[0][0] + 2 * colors[1][0]) / 3);
        colors[3][1] = (uint8_t)((colors[0][1] + 2 * colors[1][1]) / 3);
        colors[3][2] = (uint8_t)((colors[0][2] + 2 * colors[1][2]) / 3);
        colors[3][3] = 255;
    }
}

static bool dds_calc_rgba_size(uint32_t width, uint32_t height, size_t *out_size)
{
    uint64_t size = (uint64_t)width * (uint64_t)height * 4u;
    if (size > SIZE_MAX)
        return false;

    *out_size = (size_t)size;
    return true;
}

static bool dds_calc_block_size(uint32_t width, uint32_t height, uint32_t block_bytes, size_t *out_size)
{
    uint64_t blocks_x = ((uint64_t)width + 3u) / 4u;
    uint64_t blocks_y = ((uint64_t)height + 3u) / 4u;
    uint64_t size = blocks_x * blocks_y * block_bytes;
    if (size > SIZE_MAX)
        return false;

    *out_size = (size_t)size;
    return true;
}

static void dds_decode_bc1(const byte *src, uint32_t width, uint32_t height,
                           byte *dst, bool *has_alpha)
{
    uint32_t blocks_x = (width + 3u) / 4u;
    uint32_t blocks_y = (height + 3u) / 4u;

    for (uint32_t by = 0; by < blocks_y; by++) {
        for (uint32_t bx = 0; bx < blocks_x; bx++) {
            const byte *block = src + ((by * blocks_x + bx) * 8u);
            uint16_t c0 = dds_read_u16(block + 0);
            uint16_t c1 = dds_read_u16(block + 2);
            uint8_t colors[4][4];
            dds_build_bc_palette(c0, c1, true, colors);

            uint32_t indices = dds_read_u32(block + 4);
            for (uint32_t py = 0; py < 4; py++) {
                uint32_t y = by * 4u + py;
                for (uint32_t px = 0; px < 4; px++) {
                    uint32_t color_idx = indices & 0x3u;
                    indices >>= 2;

                    uint32_t x = bx * 4u + px;
                    if (x >= width || y >= height)
                        continue;

                    byte *out = dst + (((size_t)y * width + x) * 4u);
                    out[0] = colors[color_idx][0];
                    out[1] = colors[color_idx][1];
                    out[2] = colors[color_idx][2];
                    out[3] = colors[color_idx][3];
                    if (colors[color_idx][3] < 255)
                        *has_alpha = true;
                }
            }
        }
    }
}

static void dds_decode_bc2(const byte *src, uint32_t width, uint32_t height,
                           byte *dst, bool *has_alpha)
{
    uint32_t blocks_x = (width + 3u) / 4u;
    uint32_t blocks_y = (height + 3u) / 4u;

    for (uint32_t by = 0; by < blocks_y; by++) {
        for (uint32_t bx = 0; bx < blocks_x; bx++) {
            const byte *block = src + ((by * blocks_x + bx) * 16u);
            uint8_t alpha_values[16];

            for (uint32_t row = 0; row < 4; row++) {
                uint16_t alpha_row = dds_read_u16(block + row * 2u);
                for (uint32_t col = 0; col < 4; col++) {
                    uint8_t a4 = (uint8_t)((alpha_row >> (col * 4u)) & 0x0fu);
                    alpha_values[row * 4u + col] = (uint8_t)(a4 * 17u);
                }
            }

            const byte *color_block = block + 8;
            uint16_t c0 = dds_read_u16(color_block + 0);
            uint16_t c1 = dds_read_u16(color_block + 2);
            uint8_t colors[4][4];
            dds_build_bc_palette(c0, c1, false, colors);

            uint32_t indices = dds_read_u32(color_block + 4);
            for (uint32_t py = 0; py < 4; py++) {
                uint32_t y = by * 4u + py;
                for (uint32_t px = 0; px < 4; px++) {
                    uint32_t color_idx = indices & 0x3u;
                    indices >>= 2;

                    uint32_t x = bx * 4u + px;
                    if (x >= width || y >= height)
                        continue;

                    uint8_t alpha = alpha_values[py * 4u + px];
                    byte *out = dst + (((size_t)y * width + x) * 4u);
                    out[0] = colors[color_idx][0];
                    out[1] = colors[color_idx][1];
                    out[2] = colors[color_idx][2];
                    out[3] = alpha;
                    if (alpha < 255)
                        *has_alpha = true;
                }
            }
        }
    }
}

static void dds_decode_bc3(const byte *src, uint32_t width, uint32_t height,
                           byte *dst, bool *has_alpha)
{
    uint32_t blocks_x = (width + 3u) / 4u;
    uint32_t blocks_y = (height + 3u) / 4u;

    for (uint32_t by = 0; by < blocks_y; by++) {
        for (uint32_t bx = 0; bx < blocks_x; bx++) {
            const byte *block = src + ((by * blocks_x + bx) * 16u);
            uint8_t alpha_palette[8];
            uint8_t a0 = block[0];
            uint8_t a1 = block[1];
            alpha_palette[0] = a0;
            alpha_palette[1] = a1;

            if (a0 > a1) {
                alpha_palette[2] = (uint8_t)((6u * a0 + 1u * a1 + 3u) / 7u);
                alpha_palette[3] = (uint8_t)((5u * a0 + 2u * a1 + 3u) / 7u);
                alpha_palette[4] = (uint8_t)((4u * a0 + 3u * a1 + 3u) / 7u);
                alpha_palette[5] = (uint8_t)((3u * a0 + 4u * a1 + 3u) / 7u);
                alpha_palette[6] = (uint8_t)((2u * a0 + 5u * a1 + 3u) / 7u);
                alpha_palette[7] = (uint8_t)((1u * a0 + 6u * a1 + 3u) / 7u);
            } else {
                alpha_palette[2] = (uint8_t)((4u * a0 + 1u * a1 + 2u) / 5u);
                alpha_palette[3] = (uint8_t)((3u * a0 + 2u * a1 + 2u) / 5u);
                alpha_palette[4] = (uint8_t)((2u * a0 + 3u * a1 + 2u) / 5u);
                alpha_palette[5] = (uint8_t)((1u * a0 + 4u * a1 + 2u) / 5u);
                alpha_palette[6] = 0;
                alpha_palette[7] = 255;
            }

            uint64_t alpha_indices = 0;
            for (uint32_t i = 0; i < 6; i++)
                alpha_indices |= ((uint64_t)block[2 + i]) << (8u * i);

            const byte *color_block = block + 8;
            uint16_t c0 = dds_read_u16(color_block + 0);
            uint16_t c1 = dds_read_u16(color_block + 2);
            uint8_t colors[4][4];
            dds_build_bc_palette(c0, c1, false, colors);

            uint32_t color_indices = dds_read_u32(color_block + 4);
            for (uint32_t py = 0; py < 4; py++) {
                uint32_t y = by * 4u + py;
                for (uint32_t px = 0; px < 4; px++) {
                    uint32_t color_idx = color_indices & 0x3u;
                    color_indices >>= 2;

                    uint32_t alpha_idx = (uint32_t)(alpha_indices & 0x7u);
                    alpha_indices >>= 3;

                    uint32_t x = bx * 4u + px;
                    if (x >= width || y >= height)
                        continue;

                    uint8_t alpha = alpha_palette[alpha_idx];
                    byte *out = dst + (((size_t)y * width + x) * 4u);
                    out[0] = colors[color_idx][0];
                    out[1] = colors[color_idx][1];
                    out[2] = colors[color_idx][2];
                    out[3] = alpha;
                    if (alpha < 255)
                        *has_alpha = true;
                }
            }
        }
    }
}

static int dds_decode_uncompressed(const byte *src, size_t src_len,
                                   uint32_t width, uint32_t height,
                                   const dds_uncompressed_desc_t *desc,
                                   byte **out_rgba, bool *out_has_alpha,
                                   r_dds_alloc_fn alloc_fn)
{
    if (!desc->bits_per_pixel || (desc->bits_per_pixel % 8u) != 0u) {
        Com_SetLastError("DDS: unsupported bits-per-pixel");
        return Q_ERR_INVALID_FORMAT;
    }

    uint32_t bytes_per_pixel = desc->bits_per_pixel / 8u;
    if (bytes_per_pixel == 0u || bytes_per_pixel > 4u) {
        Com_SetLastError("DDS: unsupported pixel stride");
        return Q_ERR_INVALID_FORMAT;
    }

    size_t row_bytes = (((size_t)width * desc->bits_per_pixel) + 7u) / 8u;
    if (row_bytes == 0 || src_len < row_bytes * (size_t)height) {
        Com_SetLastError("DDS: truncated uncompressed pixel data");
        return Q_ERR_FILE_TOO_SMALL;
    }

    size_t rgba_size = 0;
    if (!dds_calc_rgba_size(width, height, &rgba_size)) {
        Com_SetLastError("DDS: image is too large");
        return Q_ERR_INVALID_FORMAT;
    }

    byte *rgba = alloc_fn(rgba_size);
    if (!rgba) {
        Com_SetLastError("DDS: out of memory");
        return Q_ERR(ENOMEM);
    }

    bool has_alpha = false;
    for (uint32_t y = 0; y < height; y++) {
        const byte *row = src + row_bytes * (size_t)y;
        for (uint32_t x = 0; x < width; x++) {
            const byte *in = row + x * bytes_per_pixel;
            uint32_t packed = 0;
            for (uint32_t b = 0; b < bytes_per_pixel; b++)
                packed |= ((uint32_t)in[b]) << (8u * b);

            uint8_t r, g, b, a;
            if (desc->alpha_only) {
                uint32_t alpha_mask = desc->a_mask ? desc->a_mask : dds_mask_for_bits(desc->bits_per_pixel);
                a = dds_extract_component(packed, alpha_mask);
                r = g = b = 255;
            } else if (desc->luminance) {
                uint32_t lum_mask = desc->r_mask ? desc->r_mask : dds_mask_for_bits(desc->bits_per_pixel);
                uint8_t lum = dds_extract_component(packed, lum_mask);
                r = g = b = lum;
                a = desc->a_mask ? dds_extract_component(packed, desc->a_mask) : 255;
            } else {
                r = dds_extract_component(packed, desc->r_mask);
                g = dds_extract_component(packed, desc->g_mask);
                b = dds_extract_component(packed, desc->b_mask);
                a = desc->a_mask ? dds_extract_component(packed, desc->a_mask) : 255;
            }

            if (desc->force_opaque_alpha)
                a = 255;

            if (a < 255)
                has_alpha = true;

            byte *out = rgba + (((size_t)y * width + x) * 4u);
            out[0] = r;
            out[1] = g;
            out[2] = b;
            out[3] = a;
        }
    }

    *out_rgba = rgba;
    *out_has_alpha = has_alpha;
    return Q_ERR_SUCCESS;
}

static bool dds_validate_dimensions(uint32_t width, uint32_t height)
{
    return width >= 1 &&
           height >= 1 &&
           width <= DDS_MAX_TEXTURE_SIZE &&
           height <= DDS_MAX_TEXTURE_SIZE;
}

int R_DecodeDDS(const byte *rawdata, size_t rawlen,
                int *out_width, int *out_height,
                byte **out_rgba, bool *out_has_alpha,
                r_dds_alloc_fn alloc_fn)
{
    if (!rawdata || !rawlen || !out_width || !out_height || !out_rgba || !out_has_alpha || !alloc_fn)
        return Q_ERR(EINVAL);

    *out_width = 0;
    *out_height = 0;
    *out_rgba = NULL;
    *out_has_alpha = false;

    if (rawlen < sizeof(dds_header_t)) {
        Com_SetLastError("DDS: file too small");
        return Q_ERR_FILE_TOO_SMALL;
    }

    dds_header_t header;
    memcpy(&header, rawdata, sizeof(header));

    uint32_t magic = LittleLong(header.magic);
    uint32_t header_size = LittleLong(header.size);
    uint32_t width = LittleLong(header.width);
    uint32_t height = LittleLong(header.height);
    uint32_t depth = LittleLong(header.depth);
    uint32_t caps2 = LittleLong(header.caps2);

    uint32_t pf_size = LittleLong(header.ddspf.size);
    uint32_t pf_flags = LittleLong(header.ddspf.flags);
    uint32_t pf_fourcc = LittleLong(header.ddspf.fourCC);
    uint32_t pf_rgb_bits = LittleLong(header.ddspf.RGBBitCount);
    uint32_t pf_r_mask = LittleLong(header.ddspf.RBitMask);
    uint32_t pf_g_mask = LittleLong(header.ddspf.GBitMask);
    uint32_t pf_b_mask = LittleLong(header.ddspf.BBitMask);
    uint32_t pf_a_mask = LittleLong(header.ddspf.ABitMask);

    if (magic != DDS_MAGIC)
        return Q_ERR_UNKNOWN_FORMAT;

    if (header_size != 124u || pf_size != 32u) {
        Com_SetLastError("DDS: malformed header");
        return Q_ERR_INVALID_FORMAT;
    }

    if (!dds_validate_dimensions(width, height)) {
        Com_SetLastError("DDS: invalid image dimensions");
        return Q_ERR_INVALID_FORMAT;
    }

    if ((caps2 & DDS_CUBEMAP) || (caps2 & DDS_FLAGS_VOLUME) || depth > 1u) {
        Com_SetLastError("DDS: cubemap and volume textures are not supported");
        return Q_ERR_INVALID_FORMAT;
    }

    dds_codec_t codec = DDS_CODEC_NONE;
    dds_uncompressed_desc_t uncompressed = { 0 };
    size_t data_offset = sizeof(dds_header_t);

    if ((pf_flags & DDS_FOURCC) && pf_fourcc == DDS_FOURCC_DX10) {
        if (rawlen < data_offset + sizeof(dds_header_dxt10_t)) {
            Com_SetLastError("DDS: truncated DX10 header");
            return Q_ERR_FILE_TOO_SMALL;
        }

        dds_header_dxt10_t dx10;
        memcpy(&dx10, rawdata + data_offset, sizeof(dx10));
        uint32_t dxgi_format = LittleLong(dx10.dxgi_format);
        uint32_t resource_dimension = LittleLong(dx10.resource_dimension);
        uint32_t misc_flag = LittleLong(dx10.misc_flag);
        uint32_t array_size = LittleLong(dx10.array_size);

        data_offset += sizeof(dds_header_dxt10_t);

        if (resource_dimension != DDS_RESOURCE_DIMENSION_TEXTURE2D) {
            Com_SetLastError("DDS: only 2D DX10 textures are supported");
            return Q_ERR_INVALID_FORMAT;
        }

        if (array_size != 1u || (misc_flag & DDS_RESOURCE_MISC_TEXTURECUBE)) {
            Com_SetLastError("DDS: texture arrays and cubemaps are not supported");
            return Q_ERR_INVALID_FORMAT;
        }

        switch (dxgi_format) {
        case 28: // DXGI_FORMAT_R8G8B8A8_UNORM
        case 29: // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            codec = DDS_CODEC_UNCOMPRESSED;
            uncompressed.bits_per_pixel = 32;
            uncompressed.r_mask = 0x000000ffu;
            uncompressed.g_mask = 0x0000ff00u;
            uncompressed.b_mask = 0x00ff0000u;
            uncompressed.a_mask = 0xff000000u;
            break;
        case 87: // DXGI_FORMAT_B8G8R8A8_UNORM
        case 91: // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
            codec = DDS_CODEC_UNCOMPRESSED;
            uncompressed.bits_per_pixel = 32;
            uncompressed.r_mask = 0x00ff0000u;
            uncompressed.g_mask = 0x0000ff00u;
            uncompressed.b_mask = 0x000000ffu;
            uncompressed.a_mask = 0xff000000u;
            break;
        case 88: // DXGI_FORMAT_B8G8R8X8_UNORM
        case 93: // DXGI_FORMAT_B8G8R8X8_UNORM_SRGB
            codec = DDS_CODEC_UNCOMPRESSED;
            uncompressed.bits_per_pixel = 32;
            uncompressed.r_mask = 0x00ff0000u;
            uncompressed.g_mask = 0x0000ff00u;
            uncompressed.b_mask = 0x000000ffu;
            uncompressed.a_mask = 0;
            uncompressed.force_opaque_alpha = true;
            break;
        case 71: // DXGI_FORMAT_BC1_UNORM
        case 72: // DXGI_FORMAT_BC1_UNORM_SRGB
            codec = DDS_CODEC_BC1;
            break;
        case 74: // DXGI_FORMAT_BC2_UNORM
        case 75: // DXGI_FORMAT_BC2_UNORM_SRGB
            codec = DDS_CODEC_BC2;
            break;
        case 77: // DXGI_FORMAT_BC3_UNORM
        case 78: // DXGI_FORMAT_BC3_UNORM_SRGB
            codec = DDS_CODEC_BC3;
            break;
        default:
            Com_SetLastError("DDS: unsupported DX10 format");
            return Q_ERR_INVALID_FORMAT;
        }
    } else if (pf_flags & DDS_FOURCC) {
        switch (pf_fourcc) {
        case DDS_FOURCC_DXT1:
            codec = DDS_CODEC_BC1;
            break;
        case DDS_FOURCC_DXT3:
            codec = DDS_CODEC_BC2;
            break;
        case DDS_FOURCC_DXT5:
            codec = DDS_CODEC_BC3;
            break;
        default:
            Com_SetLastError("DDS: unsupported FOURCC compression");
            return Q_ERR_INVALID_FORMAT;
        }
    } else if (pf_flags & (DDS_RGB | DDS_LUMINANCE | DDS_ALPHA)) {
        codec = DDS_CODEC_UNCOMPRESSED;
        uncompressed.bits_per_pixel = pf_rgb_bits;
        uncompressed.r_mask = pf_r_mask;
        uncompressed.g_mask = pf_g_mask;
        uncompressed.b_mask = pf_b_mask;
        uncompressed.a_mask = pf_a_mask;
        uncompressed.luminance = (pf_flags & DDS_LUMINANCE) != 0;
        uncompressed.alpha_only = (pf_flags & DDS_ALPHA) && !(pf_flags & (DDS_RGB | DDS_LUMINANCE));

        if (uncompressed.alpha_only && !uncompressed.a_mask)
            uncompressed.a_mask = dds_mask_for_bits(uncompressed.bits_per_pixel);

        if (!uncompressed.luminance && !uncompressed.alpha_only &&
            !uncompressed.r_mask && !uncompressed.g_mask && !uncompressed.b_mask) {
            Com_SetLastError("DDS: missing RGB channel masks");
            return Q_ERR_INVALID_FORMAT;
        }

        if (!(pf_flags & DDS_ALPHAPIXELS))
            uncompressed.a_mask = 0;
    } else {
        Com_SetLastError("DDS: unsupported pixel format");
        return Q_ERR_INVALID_FORMAT;
    }

    if (data_offset >= rawlen) {
        Com_SetLastError("DDS: missing pixel data");
        return Q_ERR_FILE_TOO_SMALL;
    }

    const byte *pixel_data = rawdata + data_offset;
    size_t pixel_len = rawlen - data_offset;

    if (codec == DDS_CODEC_UNCOMPRESSED) {
        int ret = dds_decode_uncompressed(pixel_data, pixel_len,
                                          width, height, &uncompressed,
                                          out_rgba, out_has_alpha, alloc_fn);
        if (ret < 0)
            return ret;
    } else {
        size_t needed = 0;
        uint32_t block_bytes = (codec == DDS_CODEC_BC1) ? 8u : 16u;
        if (!dds_calc_block_size(width, height, block_bytes, &needed)) {
            Com_SetLastError("DDS: compressed texture size overflow");
            return Q_ERR_INVALID_FORMAT;
        }

        if (pixel_len < needed) {
            Com_SetLastError("DDS: truncated compressed pixel data");
            return Q_ERR_FILE_TOO_SMALL;
        }

        size_t rgba_size = 0;
        if (!dds_calc_rgba_size(width, height, &rgba_size)) {
            Com_SetLastError("DDS: image is too large");
            return Q_ERR_INVALID_FORMAT;
        }

        byte *rgba = alloc_fn(rgba_size);
        if (!rgba) {
            Com_SetLastError("DDS: out of memory");
            return Q_ERR(ENOMEM);
        }

        bool has_alpha = false;
        switch (codec) {
        case DDS_CODEC_BC1:
            dds_decode_bc1(pixel_data, width, height, rgba, &has_alpha);
            break;
        case DDS_CODEC_BC2:
            dds_decode_bc2(pixel_data, width, height, rgba, &has_alpha);
            break;
        case DDS_CODEC_BC3:
            dds_decode_bc3(pixel_data, width, height, rgba, &has_alpha);
            break;
        default:
            Q_assert(!"Unexpected DDS codec");
            break;
        }

        *out_rgba = rgba;
        *out_has_alpha = has_alpha;
    }

    *out_width = (int)width;
    *out_height = (int)height;
    return Q_ERR_SUCCESS;
}

