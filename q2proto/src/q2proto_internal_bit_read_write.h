/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2003-2011 Richard Stanway
Copyright (C) 2003-2024 Andrey Nazarov
Copyright (C) 2024 Frank Richter

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

/**\file
 * Helpers to read/write individual bit sequences from messages.
 */
#ifndef Q2PROTO_INTERNAL_BIT_READ_WRITE_H_
#define Q2PROTO_INTERNAL_BIT_READ_WRITE_H_

typedef struct bitwriter_s {
    uintptr_t io_arg;
    uint32_t buf;
    uint32_t left;
} bitwriter_t;

static inline void bitwriter_init(bitwriter_t *bitwriter, uintptr_t io_arg)
{
    bitwriter->io_arg = io_arg;
    bitwriter->buf = 0;
    bitwriter->left = 32;
}

static inline q2proto_error_t bitwriter_write(bitwriter_t *bitwriter, int value, int bits)
{
    if (bits == 0 || bits < -31 || bits > 31) // FIXME?: assert
        return Q2P_ERR_BAD_DATA;

    if (bits < 0) {
        bits = -bits;
    }

    uint32_t bits_buf = bitwriter->buf;
    uint32_t bits_left = bitwriter->left;
    uint32_t v = value & ((1U << bits) - 1);

    bits_buf |= v << (32 - bits_left);
    if (bits >= bits_left) {
        WRITE_CHECKED(client_write, bitwriter->io_arg, u32, bits_buf);
        bits_buf = v >> bits_left;
        bits_left += 32;
    }
    bits_left -= bits;

    bitwriter->buf = bits_buf;
    bitwriter->left = bits_left;
    return Q2P_ERR_SUCCESS;
}

static inline q2proto_error_t bitwriter_flush(bitwriter_t *bitwriter)
{
    uint32_t bits_buf = bitwriter->buf;
    uint32_t bits_left = bitwriter->left;

    while (bits_left < 32) {
        WRITE_CHECKED(client_write, bitwriter->io_arg, u8, bits_buf & 255);
        bits_buf >>= 8;
        bits_left += 8;
    }

    bitwriter->buf = 0;
    bitwriter->left = 32;
    return Q2P_ERR_SUCCESS;
}

typedef struct bitreader_s {
    uintptr_t io_arg;
    uint32_t buf;
    uint32_t left;
} bitreader_t;

static inline void bitreader_init(bitreader_t *bitreader, uintptr_t io_arg)
{
    bitreader->io_arg = io_arg;
    bitreader->buf = 0;
    bitreader->left = 0;
}

static inline int32_t sign_extend(uint32_t v, int bits) { return (int32_t)(v << (32 - bits)) >> (32 - bits); }

// positive bits: read unsigned. negative bits: read signed.
static inline q2proto_error_t bitreader_read(bitreader_t *bitreader, int bits, int *value)
{
    bool sgn = false;

    if (bits == 0 || bits < -25 || bits > 25) // FIXME?: assert
        return Q2P_ERR_BAD_DATA;

    if (bits < 0) {
        bits = -bits;
        sgn = true;
    }

    uint32_t bits_buf = bitreader->buf;
    uint32_t bits_left = bitreader->left;

    while (bits > bits_left) {
        uint8_t new_byte;
        READ_CHECKED(server_read, bitreader->io_arg, new_byte, u8);
        bits_buf |= new_byte << bits_left;
        bits_left += 8;
    }

    *value = bits_buf & ((1U << bits) - 1);

    bitreader->buf = bits_buf >> bits;
    bitreader->left = bits_left - bits;

    if (sgn)
        *value = sign_extend(*value, bits);

    return Q2P_ERR_SUCCESS;
}

#endif // Q2PROTO_INTERNAL_BIT_READ_WRITE_H_
