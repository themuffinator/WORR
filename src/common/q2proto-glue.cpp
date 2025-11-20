/*
Copyright (C) 1997-2001 Id Software, Inc.
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

#include "shared/shared.hpp"
#include "common/intreadwrite.hpp"
#include "common/msg.hpp"
#include "common/q2proto_shared.hpp"

#include "q2proto/q2proto.h"

#include <array>

#if USE_ZLIB
#include <zlib.h>

#define MAX_DEFLATED_SIZE   0x10000

// FIXME: need a mechanism to clean up the stream(s)?
static struct {
/// Buffer to receive inflated data
byte buffer[MAX_DEFLATED_SIZE];
/// zlib stream
z_stream z;
/// Whether the deflate stream ended
bool stream_end;
} io_inflate;

static bool io_inflate_initialized;

// Sizebuf with inflated data
static sizebuf_t msg_inflate;
// Sizebuf to buffer data to deflate
static sizebuf_t msg_deflate;
static byte deflate_buf[MAX_DEFLATED_SIZE];

#endif // USE_ZLIB

q2protoio_ioarg_t default_q2protoio_ioarg{&msg_read, &msg_write, 1384 /* conservative default */, nullptr};

#if USE_ZLIB
static q2protoio_ioarg_t inflate_q2protoio_ioarg{&msg_inflate, &msg_write, 0, nullptr};
static q2protoio_ioarg_t deflate_q2protoio_ioarg{nullptr, &msg_deflate, 0, nullptr};

// I/O arg: read from inflated data
#define IOARG_INFLATE      ((uintptr_t)&inflate_q2protoio_ioarg)
// I/O arg: write w/ deflate
#define IOARG_DEFLATE      ((uintptr_t)&deflate_q2protoio_ioarg)
#endif // USE_ZLIB

#if USE_ZLIB
/*
=============
clear_inflate_state

Release inflate stream resources and optionally clear the initialization flag.
=============
*/
static void clear_inflate_state(bool reset_initialized)
{
	if (!io_inflate_initialized)
		return;

	if (io_inflate.z.state)
	{
		int ret = inflateEnd(&io_inflate.z);
		if (ret != Z_OK && ret != Z_STREAM_ERROR)
		{
			Com_Error(ERR_DROP, "%s: inflateEnd() failed with error %d", __func__, ret);
		}
		memset(&io_inflate.z, 0, sizeof(io_inflate.z));
	}
	io_inflate.stream_end = false;
	SZ_Clear(&msg_inflate);
	if (reset_initialized)
		io_inflate_initialized = false;
}

/*
=============
clear_deflate_state

Release headered deflate stream resources.
=============
*/
static void clear_deflate_state(q2protoio_deflate_args_t *deflate_args)
{
	if (!deflate_args)
		return;

	if (deflate_args->z_header.state)
	{
		int ret = deflateEnd(&deflate_args->z_header);
		if (ret != Z_OK && ret != Z_STREAM_ERROR)
		{
			Com_Error(ERR_DROP, "%s: deflateEnd() failed with error %d", __func__, ret);
		}
		memset(&deflate_args->z_header, 0, sizeof(deflate_args->z_header));
	}
	if (deflate_args->z_current == &deflate_args->z_header)
		deflate_args->z_current = NULL;
}
#endif // USE_ZLIB

/*
=============
Q2Proto_IO_Init

Initialize inflate buffers and stream tracking.
=============
*/
void Q2Proto_IO_Init(void)
{
	memset(&io_inflate.z, 0, sizeof(io_inflate.z));
	io_inflate.stream_end = false;
	SZ_Init(&msg_inflate, io_inflate.buffer, sizeof(io_inflate.buffer), "msg_inflate");
	SZ_Clear(&msg_inflate);
	io_inflate_initialized = true;
}

/*
=============
Q2Proto_IO_ResetInflate

Reset inflate state and release any active zlib stream.
=============
*/
void Q2Proto_IO_ResetInflate(void)
{
	clear_inflate_state(false);
}

/*
=============
Q2Proto_IO_Shutdown

Tear down inflate buffers and prevent reuse of stale state.
=============
*/
void Q2Proto_IO_Shutdown(void)
{
	clear_inflate_state(true);
}

/*
=============
Q2Proto_IO_ShutdownDeflate

Release any initialized deflate streams bound to the provided arguments.
=============
*/
void Q2Proto_IO_ShutdownDeflate(q2protoio_deflate_args_t *deflate_args)
{
	clear_deflate_state(deflate_args);
}

/*
=============
io_read_data

Read a specified amount of data from the provided IO buffer.
=============
*/
static byte* io_read_data(uintptr_t io_arg, size_t len, size_t *readcount)
{
#if !USE_ZLIB
	Q_assert(io_arg == _Q2PROTO_IOARG_DEFAULT);
#else
	Q_assert(io_arg == _Q2PROTO_IOARG_DEFAULT || io_arg == IOARG_INFLATE);
#endif
	sizebuf_t *sz = ((q2protoio_ioarg_t*)io_arg)->sz_read;

	if (readcount) {
	    len = min(len, sz->cursize - sz->readcount);
	    *readcount = len;
	    return static_cast<byte*>(SZ_ReadData(sz, len));
	} else
	    return static_cast<byte*>(SZ_ReadData(sz, len));
}

/*
=============
q2protoio_read_u8

Read an unsigned 8-bit value from the IO buffer.
=============
*/
uint8_t q2protoio_read_u8(uintptr_t io_arg)
{
	byte *buf = io_read_data(io_arg, 1, NULL);
	return buf ? (uint8_t)buf[0] : (uint8_t)-1;
}

/*
=============
q2protoio_read_u16

Read an unsigned 16-bit value from the IO buffer.
=============
*/
uint16_t q2protoio_read_u16(uintptr_t io_arg)
{
	byte *buf = io_read_data(io_arg, 2, NULL);
	return buf ? (uint16_t)RL16(buf) : (uint16_t)-1;
}

/*
=============
q2protoio_read_u32

Read an unsigned 32-bit value from the IO buffer.
=============
*/
uint32_t q2protoio_read_u32(uintptr_t io_arg)
{
	byte *buf = io_read_data(io_arg, 4, NULL);
	return buf ? (uint32_t)RL32(buf) : (uint32_t)-1;
}

/*
=============
q2protoio_read_u64

Read an unsigned 64-bit value from the IO buffer.
=============
*/
uint64_t q2protoio_read_u64(uintptr_t io_arg)
{
	byte *buf = io_read_data(io_arg, 8, NULL);
	return buf ? (uint64_t)RL64(buf) : (uint64_t)-1;
}

/*
=============
q2protoio_read_string

Read a null-terminated string from the IO buffer.
=============
*/
q2proto_string_t q2protoio_read_string(uintptr_t io_arg)
{
	q2proto_string_t str{nullptr, 0};
	str.str = reinterpret_cast<const char*>(io_read_data(io_arg, 0, NULL));
	while (1) {
	    byte *c = io_read_data(io_arg, 1, NULL);
	    if (!c || *c == 0) {
	        break;
	    }
	    str.len++;
	}
	return str;
}

/*
=============
q2protoio_read_raw

Read a raw block of bytes from the IO buffer.
=============
*/
const void* q2protoio_read_raw(uintptr_t io_arg, size_t size, size_t* readcount)
{
	return io_read_data(io_arg, size, readcount);
}

/*
=============
q2protoio_read_available

Return remaining readable bytes in the IO buffer.
=============
*/
size_t q2protoio_read_available(uintptr_t io_arg)
{
	const q2protoio_ioarg_t *io_data = (const q2protoio_ioarg_t *)io_arg;
	sizebuf_t *sz = io_data->sz_read;
	return sz->cursize - sz->readcount;
}

#if USE_ZLIB
/*
=============
q2protoio_inflate_begin

Begin inflating compressed data into a working buffer.
=============
*/
q2proto_error_t q2protoio_inflate_begin(uintptr_t io_arg, q2proto_inflate_deflate_header_mode_t header_mode, uintptr_t* inflate_io_arg)
{
	if (io_arg != _Q2PROTO_IOARG_DEFAULT) {
		Com_Error(ERR_DROP, "%s: recursively entered", __func__);
}

	if (!io_inflate_initialized)
		Q2Proto_IO_Init();
	else
		Q2Proto_IO_ResetInflate();

	int window_bits = header_mode == Q2P_INFL_DEFL_RAW ? -MAX_WBITS : MAX_WBITS;
	int ret = inflateInit2(&io_inflate.z, window_bits);
	if (ret != Z_OK) {
		Com_Error(ERR_DROP, "%s: inflate initialization failed with error %d", __func__, ret);
	}
	io_inflate.stream_end = false;

	*inflate_io_arg = IOARG_INFLATE;
	return Q2P_ERR_SUCCESS;
}

/*
=============
q2protoio_inflate_data

Inflate compressed data chunk into the buffer.
=============
*/
q2proto_error_t q2protoio_inflate_data(uintptr_t io_arg, uintptr_t inflate_io_arg, size_t compressed_size)
{
	Q_assert(io_arg == _Q2PROTO_IOARG_DEFAULT);
	Q_assert(inflate_io_arg == IOARG_INFLATE);

	byte *in_data;
	if (compressed_size == (size_t)-1)
		in_data = io_read_data(io_arg, SIZE_MAX, &compressed_size);
	else
		in_data = io_read_data(io_arg, compressed_size, NULL);
	io_inflate.z.next_in = in_data;
	io_inflate.z.avail_in = compressed_size;
	io_inflate.z.next_out = io_inflate.buffer;
	io_inflate.z.avail_out = sizeof(io_inflate.buffer);
	int ret = inflate(&io_inflate.z, Z_SYNC_FLUSH);
	if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
	clear_inflate_state(true);
		Com_Error(ERR_DROP, "%s: inflate() failed with error %d", __func__, ret);
	}

	io_inflate.stream_end = ret == Z_STREAM_END;

	SZ_InitRead(&msg_inflate, io_inflate.buffer, sizeof(io_inflate.buffer));
	msg_inflate.cursize = sizeof(io_inflate.buffer) - io_inflate.z.avail_out;

	return Q2P_ERR_SUCCESS;
}

/*
=============
q2protoio_inflate_stream_ended

Check whether the inflate stream has finished.
=============
*/
q2proto_error_t q2protoio_inflate_stream_ended(uintptr_t inflate_io_arg, bool *stream_end)
{
	Q_assert(inflate_io_arg == IOARG_INFLATE);
	*stream_end = io_inflate.stream_end;
	return Q2P_ERR_SUCCESS;
}

/*
=============
q2protoio_inflate_end

Finish inflating and return status based on remaining data.
=============
*/
q2proto_error_t q2protoio_inflate_end(uintptr_t inflate_io_arg)
{
	Q_assert(inflate_io_arg == IOARG_INFLATE);
	int ret = inflateEnd(&io_inflate.z);
	if (ret != Z_OK && ret != Z_STREAM_END) {
		Com_Error(ERR_DROP, "%s: inflateEnd() failed with error %d", __func__, ret);
	}
	q2proto_error_t result = msg_inflate.readcount < msg_inflate.cursize ? Q2P_ERR_MORE_DATA_DEFLATED : Q2P_ERR_SUCCESS;
	clear_inflate_state(true);
	return result;
}

/*
=============
reset_deflate_input

Prepare deflate buffers for a fresh write.
=============
*/
static void reset_deflate_input(q2protoio_deflate_args_t* deflate_args)
{
	deflate_args->z_current->next_in = (Byte*)deflate_buf;
	deflate_args->z_current->next_out = deflate_args->z_buffer;
	deflate_args->z_current->avail_out = deflate_args->z_buffer_size;
	deflate_args->z_current->total_in = 0;
	deflate_args->z_current->total_out = 0;
}

/*
=============
q2protoio_deflate_begin

Initialize deflate state for writing compressed data.
=============
*/
q2proto_error_t q2protoio_deflate_begin(q2protoio_deflate_args_t* deflate_args, size_t max_deflated, q2proto_inflate_deflate_header_mode_t header_mode, uintptr_t *deflate_io_arg)
{
	Q_assert(!deflate_q2protoio_ioarg.deflate);

	if (header_mode == Q2P_INFL_DEFL_RAW)
	{
	    deflateReset(deflate_args->z_raw);
	    deflate_args->z_current = deflate_args->z_raw;
	}
	else
	{
	    if (!deflate_args->z_header.state)
	        Q_assert(deflateInit2(&deflate_args->z_header, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
	                 MAX_WBITS, 9, Z_DEFAULT_STRATEGY) == Z_OK);
	    else
	        deflateReset(&deflate_args->z_header);
	    deflate_args->z_current = &deflate_args->z_header;
	}
	reset_deflate_input(deflate_args);

	SZ_InitWrite(&msg_deflate, deflate_buf, MAX_MSGLEN);

	deflate_q2protoio_ioarg.max_msg_len = max_deflated;
	deflate_q2protoio_ioarg.deflate = deflate_args;
	*deflate_io_arg = IOARG_DEFLATE;

	return Q2P_ERR_SUCCESS;
}

#define DEFLATE_OUTPUT_MARGIN   16

/*
=============
q2protoio_deflate_get_data

Retrieve compressed data after deflating current input.
=============
*/
q2proto_error_t q2protoio_deflate_get_data(uintptr_t deflate_io_arg, size_t* in_size, const void **out, size_t *out_size)
{
	q2protoio_ioarg_t *io_data = (q2protoio_ioarg_t *)deflate_io_arg;
	q2protoio_deflate_args_t *deflate_args = io_data->deflate;

	deflate_args->z_current->avail_in = msg_deflate.cursize - deflate_args->z_current->total_in;
	int ret = deflate(deflate_args->z_current, Z_FINISH);
	if (ret != Z_OK && ret != Z_STREAM_END) {
	    deflateEnd(deflate_args->z_current);
	    Com_Error(ERR_DROP, "%s: deflate() failed with error %d", __func__, ret);
	}

	if (in_size)
	    *in_size = deflate_args->z_current->total_in;
	*out = deflate_args->z_buffer;
	*out_size = deflate_args->z_current->total_out;
	SZ_Clear(&msg_deflate);
	reset_deflate_input(deflate_args);

	return Q2P_ERR_SUCCESS;
}

/*
=============
q2protoio_deflate_end

Tear down deflate state and release ownership.
=============
*/
q2proto_error_t q2protoio_deflate_end(uintptr_t deflate_io_arg)
{
	q2protoio_ioarg_t *io_data = (q2protoio_ioarg_t *)deflate_io_arg;

	clear_deflate_state(io_data->deflate);
	io_data->deflate = NULL;

	return Q2P_ERR_SUCCESS;
}
#endif // USE_ZLIB

/*
=============
io_reserve_data

Reserve space in the write buffer.
=============
*/
static void* io_reserve_data(uintptr_t io_arg, size_t size)
{
	sizebuf_t *sz = ((q2protoio_ioarg_t*)io_arg)->sz_write;
	return SZ_GetSpace(sz, size);
}

/*
=============
q2protoio_write_u8

Write an unsigned 8-bit value to the IO buffer.
=============
*/
void q2protoio_write_u8(uintptr_t io_arg, uint8_t x)
{
	auto* buf = static_cast<byte*>(io_reserve_data(io_arg, 1));
	buf[0] = x;
}

/*
=============
q2protoio_write_u16

Write an unsigned 16-bit value to the IO buffer.
=============
*/
void q2protoio_write_u16(uintptr_t io_arg, uint16_t x)
{
	auto* buf = static_cast<byte*>(io_reserve_data(io_arg, 2));
	WL16(buf, x);
}

/*
=============
q2protoio_write_u32

Write an unsigned 32-bit value to the IO buffer.
=============
*/
void q2protoio_write_u32(uintptr_t io_arg, uint32_t x)
{
	auto* buf = static_cast<byte*>(io_reserve_data(io_arg, 4));
	WL32(buf, x);
}

/*
=============
q2protoio_write_u64

Write an unsigned 64-bit value to the IO buffer.
=============
*/
void q2protoio_write_u64(uintptr_t io_arg, uint64_t x)
{
	auto* buf = static_cast<byte*>(io_reserve_data(io_arg, 8));
	WL64(buf, x);
}

/*
=============
q2protoio_write_reserve_raw

Reserve a raw block for writing to the IO buffer.
=============
*/
void* q2protoio_write_reserve_raw(uintptr_t io_arg, size_t size)
{
	return io_reserve_data(io_arg, size);
}

/*
=============
q2protoio_write_raw

Write raw data into the IO buffer, optionally reporting bytes written.
=============
*/
void q2protoio_write_raw(uintptr_t io_arg, const void* data, size_t size, size_t *written)
{
	q2protoio_ioarg_t *io_data = (q2protoio_ioarg_t *)io_arg;
	sizebuf_t *sz = io_data->sz_write;

	if (io_data->deflate && written)
	{
	    // Deflating as much as possble: write data in a loop
	    const byte *data_bytes = static_cast<const byte*>(data);
	    size_t in_remaining = size;
	    size_t out_remaining = q2protoio_write_available(io_arg);
	    size_t in_consumed = 0;
	    while (in_remaining > 0 && out_remaining > 0)
	    {
	        size_t chunk_size = min(in_remaining, out_remaining);
	        void* p = SZ_GetSpace(sz, chunk_size);
	        memcpy(p, data_bytes, chunk_size);
	        in_consumed += chunk_size;
	        data_bytes += chunk_size;
	        in_remaining -= chunk_size;
	        out_remaining = q2protoio_write_available(io_arg);
	    }
	    *written = in_consumed;
	}
	else
	{
	    // Simple case: not deflating, or requiring all data to be deflated
	    size_t buf_remaining = sz->maxsize - sz->cursize;
	    size_t write_size = written ? min(buf_remaining, size) : size;
	    void* p = SZ_GetSpace(sz, write_size);
	    memcpy(p, data, write_size);
	    if (written)
	        *written = write_size;
	}
}

#if USE_ZLIB
/*
=============
compress_accumulated

Compress data accumulated in the deflate buffer.
=============
*/
static void compress_accumulated(q2protoio_deflate_args_t *deflate_args)
{
	// Compress data accumulated in deflate_buf
	deflate_args->z_current->avail_in = msg_deflate.cursize - deflate_args->z_current->total_in;

	int ret = deflate(deflate_args->z_current, Z_PARTIAL_FLUSH);
	if (ret != Z_OK && ret != Z_STREAM_END) {
	    deflateEnd(deflate_args->z_current);
	    Com_Error(ERR_DROP, "%s: deflate() failed with error %d", __func__, ret);
	}
}
#endif // USE_ZLIB

/*
=============
q2protoio_write_available

Calculate remaining writable space accounting for deflate buffering.
=============
*/
size_t q2protoio_write_available(uintptr_t io_arg)
{
	const q2protoio_ioarg_t *io_data = (const q2protoio_ioarg_t *)io_arg;
	sizebuf_t *sz = io_data->sz_write;
#if USE_ZLIB
	if (io_data->deflate)
	{
	    size_t already_compressed = io_data->deflate->z_current->total_out;
	    size_t yet_uncompressed = sz->cursize - io_data->deflate->z_current->total_in;

	    size_t used_size = already_compressed + deflateBound(io_data->deflate->z_current, yet_uncompressed);
	    size_t max_msg_len = io_data->max_msg_len - DEFLATE_OUTPUT_MARGIN;
	    size_t write_available = max_msg_len - min(used_size, max_msg_len);
	    if (write_available == 0 && yet_uncompressed > 0)
	    {
	        // Actually compress yet-uncompressed data to get an "available" number closer to reality
	        compress_accumulated(io_data->deflate);
	        already_compressed = io_data->deflate->z_current->total_out;
	        write_available = max_msg_len - min(already_compressed, max_msg_len);
	    }
	    return write_available;
	}
	else
#endif // USE_ZLIB
	{
	    return io_data->max_msg_len - min(sz->cursize, io_data->max_msg_len);
	}
}

bool nonfatal_client_read_errors = false;

/*
=============
q2protoerr_client_read

Report a client read error, respecting the nonfatal flag.
=============
*/
q2proto_error_t q2protoerr_client_read(uintptr_t io_arg, q2proto_error_t err, const char *msg, ...)
{
	std::array<char, 256> buf;
	va_list argptr;

	va_start(argptr, msg);
	Q_vsnprintf(buf.data(), buf.size(), msg, argptr);
	va_end(argptr);

	if (nonfatal_client_read_errors)
	    Com_WPrintf("%s\n", buf.data());
	else
	    Com_Error(ERR_DROP, "%s", buf.data());
	return err;
}

/*
=============
q2protoerr_client_write

Report a client write error.
=============
*/
q2proto_error_t q2protoerr_client_write(uintptr_t io_arg, q2proto_error_t err, const char *msg, ...)
{
	std::array<char, 256> buf;
	va_list argptr;

	va_start(argptr, msg);
	Q_vsnprintf(buf.data(), buf.size(), msg, argptr);
	va_end(argptr);

	Com_EPrintf("client write error: %s\n", buf.data());
	return err;
}

/*
=============
q2protoerr_server_write

Report a server write error.
=============
*/
q2proto_error_t q2protoerr_server_write(uintptr_t io_arg, q2proto_error_t err, const char *msg, ...)
{
	std::array<char, 256> buf;
	va_list argptr;

	va_start(argptr, msg);
	Q_vsnprintf(buf.data(), buf.size(), msg, argptr);
	va_end(argptr);

	Com_EPrintf("server write error: %s\n", buf.data());
	return err;
}

/*
=============
q2protoerr_server_read

Report a server read error.
=============
*/
q2proto_error_t q2protoerr_server_read(uintptr_t io_arg, q2proto_error_t err, const char *msg, ...)
{
	std::array<char, 256> buf;
	va_list argptr;

	va_start(argptr, msg);
	Q_vsnprintf(buf.data(), buf.size(), msg, argptr);
	va_end(argptr);

	Com_EPrintf("server read error: %s\n", buf.data());
	return err;
}

#if Q2PROTO_SHOWNET
extern cvar_t   *cl_shownet;

/*
=============
q2protodbg_shownet_check

Return whether shownet debugging exceeds the provided level.
=============
*/
bool q2protodbg_shownet_check(uintptr_t io_arg, int level)
{
	return cl_shownet->integer > level;
}

/*
=============
q2protodbg_shownet

Emit debugging information about network traffic when enabled.
=============
*/
void q2protodbg_shownet(uintptr_t io_arg, int level, int offset, const char *msg, ...)
{
	if (cl_shownet->integer > level)
	{
	    q2protoio_ioarg_t *io_data = (q2protoio_ioarg_t *)io_arg;
	    std::array<char, 256> buf;
	    va_list argptr;

	#if USE_ZLIB
	    bool is_deflate = io_arg == IOARG_INFLATE;
	#else
	    bool is_deflate = false;
	#endif
	    const char *offset_suffix = is_deflate ? "[z]" : "";

	    va_start(argptr, msg);
	    Q_vsnprintf(buf.data(), buf.size(), msg, argptr);
	    va_end(argptr);

	    Com_LPrintf(PRINT_DEVELOPER, "%3u%s:%s\n", io_data->sz_read->readcount + offset, offset_suffix, buf.data());
	}
}
#endif
