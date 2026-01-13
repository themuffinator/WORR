/*
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
 * Externally provided IO interface
 */
#ifndef Q2PROTO_IO_H_
#define Q2PROTO_IO_H_

#include "q2proto_defs.h"
#include "q2proto_error.h"
#include "q2proto_string.h"

#include <stdbool.h>
#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**\name I/O interface (needs to be provided externally)
 * @{ */
#if Q2PROTO_RETURN_IO_ERROR_CODES
/// Return error from last I/O operation.
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoio_get_error(uintptr_t io_arg);
#endif

/// Read an 8-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL uint8_t q2protoio_read_u8(uintptr_t io_arg);
/// Read a 16-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL uint16_t q2protoio_read_u16(uintptr_t io_arg);
/// Read a 32-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL uint32_t q2protoio_read_u32(uintptr_t io_arg);
/// Read a 64-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL uint64_t q2protoio_read_u64(uintptr_t io_arg);
/**
 * Read a string (consisting of length and chars).
 * The returned pointer must remain valid for the duration of processing the message!
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_string_t q2protoio_read_string(uintptr_t io_arg);
/**
 * Read raw data of the given size.
 * The returned pointer must remain valid for the duration of processing the message!
 * If \a readcount is non-NULL, it receives the number of bytes actually read;
 * reading less than the requested size is \em not an error.
 * If \a readcount is NULL, the \a size bytes must be read exactly. If less that that
 * is available treat it as an error.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL const void *q2protoio_read_raw(uintptr_t io_arg, size_t size, size_t *readcount);

/// Return how many bytes are still available to read.
Q2PROTO_EXTERNALLY_PROVIDED_DECL size_t q2protoio_read_available(uintptr_t io_arg);

/// Write an 8-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL void q2protoio_write_u8(uintptr_t io_arg, uint8_t x);
/// Write a 16-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL void q2protoio_write_u16(uintptr_t io_arg, uint16_t x);
/// Write a 32-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL void q2protoio_write_u32(uintptr_t io_arg, uint32_t x);
/// Write a 64-bit unsigned integer.
Q2PROTO_EXTERNALLY_PROVIDED_DECL void q2protoio_write_u64(uintptr_t io_arg, uint64_t x);
/// Reserve \a size bytes in the output buffer, return pointer to first byte
Q2PROTO_EXTERNALLY_PROVIDED_DECL void *q2protoio_write_reserve_raw(uintptr_t io_arg, size_t size);
/**
 * Write (up to) \a size bytes in the output buffer.
 * If \a written is \c NULL, will write exactly \a size bytes.
 * If \a written is not \c NULL, will write as much data, up to  \a size bytes,
 * as possible, with the amount of written bytes returned in \a written.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL void q2protoio_write_raw(uintptr_t io_arg, const void *data, size_t size,
                                                          size_t *written);

/**
 * Return a (conservative) limit on how many bytes can still be written to the output buffer.
 *
 * Even after writing the returned amount, more space may be available, if compression is enabled.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL size_t q2protoio_write_available(uintptr_t io_arg);

/// Opaque deflate options. Passed through to deflate functions
typedef struct q2protoio_deflate_args_s q2protoio_deflate_args_t;

#if Q2PROTO_COMPRESSION_DEFLATE
/// Specify presence or absence of zlib header & footer (per RFC 1950) in deflated data.
typedef enum q2proto_inflate_deflate_header_mode_e {
    /// Header/footer should not be expected (inflate) resp emitted (deflate)
    Q2P_INFL_DEFL_RAW = 0,
    /// Header/footer should be expected (inflate) resp emitted (deflate)
    Q2P_INFL_DEFL_HEADER = 1,
} q2proto_inflate_deflate_header_mode_t;

/**
 * Initialize inflate decompression.
 *
 * Intended usage:
 * - Inflation is initialized by providing the "I/O argument" to obtain compressed data.
 *   A new "I/O argument" is returned allowing access to the deflated data.
 * - Raw compressed data is provided via q2protoio_inflate_data().
 * - Inflated data is read via the earlier obtained new I/O argument.
 *   You can assume all inflated data will be consumed before new compressed data is provided.
 *   Inflated data not being completely consumed can be considered an error.
 * - If processing the inflated data was successful, q2protoio_inflate_end() is called.
 * \param io_arg "I/O argument" to source compressed data from.
 * \param header_mode Whether to expect a header in compressed data.
 * \param inflate_io_arg Receives I/O argument that will be used to retrieve the inflated data.
 * \returns Error code
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t
q2protoio_inflate_begin(uintptr_t io_arg, q2proto_inflate_deflate_header_mode_t header_mode, uintptr_t *inflate_io_arg);
/**
 * Inflate some data.
 * \param io_arg "I/O argument" to source compressed data from.
 * \param inflate_io_arg "I/O argument" to obtain uncompressed data from. Provided by q2protoio_inflate_begin().
 * \param compressed_size Size of compressed data. May be -1, in which case all the remaining "source" data should be consumed.
 * \param stream_end Returns whether the compressed stream ended after inflation.
 * \returns Error code
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoio_inflate_data(uintptr_t io_arg, uintptr_t inflate_io_arg,
                                                                        size_t compressed_size);
/**
 * Returns whether the stream of inflated data ended.
 * \param io_arg "I/O argument" to source compressed data from.
 * \param inflate_io_arg "I/O argument" to obtain uncompressed data from. Provided by q2protoio_inflate_begin().
 * \param stream_end Returns whether the compressed stream ended after inflation.
 * \returns Error code
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoio_inflate_stream_ended(uintptr_t inflate_io_arg,
                                                                                bool *stream_end);
/**
 * End inflation.
 * \param inflate_io_arg "I/O argument" to obtain uncompressed data from. Provided by q2protoio_inflate_begin().
 * \returns Error code
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoio_inflate_end(uintptr_t inflate_io_arg);

/**
 * Begin deflation.
 * \param deflate_args Deflation args. Passed through from functions providing deflation support.
 * \param max_deflated Maximum size of deflated data.
 * \param header_mode Whether to expect a header in compressed data.
 * \param deflate_io_arg Receives an "I/O argument" used for deflation operation.
 * \returns Error code
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t
q2protoio_deflate_begin(q2protoio_deflate_args_t *deflate_args, size_t max_deflated,
                        q2proto_inflate_deflate_header_mode_t header_mode, uintptr_t *deflate_io_arg);
/**
 * Retrieve deflated data.
 * This function should deflate the data previously written to the deflate "I/O argument".
 * The returned pointer to the data should be valid until the next time q2protoio_deflate_get_data() is called,
 * something is written to the deflate "I/O argument", or q2protoio_deflate_end() is called.
 * Consumers of the deflated data should be able to inflate all of the consumed input data without
 * requiring any more data beyond what was output.
 * \param deflate_io_arg Receives an "I/O argument" used for deflation operation.
 * \param in_size Optional. Receives size of input (uncompressed) data.
 * \param out Pointer to start of output data. Changed to point after the last written output byte.
 * \param out_size Amount of output data written.
 * \returns Error code
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoio_deflate_get_data(uintptr_t deflate_io_arg, size_t *in_size,
                                                                            const void **out, size_t *out_size);
/**
 * End deflation.
 * \param deflate_handle "I/O argument" used for deflation operation.
 * \returns Error code
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoio_deflate_end(uintptr_t deflate_io_arg);
#endif

#if Q2PROTO_SHOWNET
/**
 * 'shownet' output.
 * Returns whether 'shownet' output with \a level is shown.
 * Used for some optimizations.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL bool q2protodbg_shownet_check(uintptr_t io_arg, int level);
/**
 * 'shownet' output.
 * Should be treated as a single line.
 * \remarks This does _not_ contain the byte index of the message;
 *   must be added separately, if desired.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL void q2protodbg_shownet(uintptr_t io_arg, int level, int offset, const char *msg, ...);
#endif
/** @} */

#if Q2PROTO_ERROR_FEEDBACK
/**\name Error handling (needs to be provided externally)
 * @{ */
/**
 * Handle a "client read" error.
 * \param io_arg "I/O argument" as provided to q2proto function.
 * \param err Error code
 * \param msg Format string with additional information.
 * \returns Error code to return from q2proto function. Typically just \a err.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoerr_client_read(uintptr_t io_arg, q2proto_error_t err,
                                                                        const char *msg, ...);
/**
 * Handle a "client write" error.
 * \param io_arg "I/O argument" as provided to q2proto function.
 * \param err Error code
 * \param msg Format string with additional information.
 * \returns Error code to return from q2proto function. Typically just \a err.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoerr_client_write(uintptr_t io_arg, q2proto_error_t err,
                                                                         const char *msg, ...);
/**
 * Handle a "server write" error.
 * \param io_arg "I/O argument" as provided to q2proto function.
 * \param err Error code
 * \param msg Format string with additional information.
 * \returns Error code to return from q2proto function. Typically just \a err.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoerr_server_write(uintptr_t io_arg, q2proto_error_t err,
                                                                         const char *msg, ...);
/**
 * Handle a "server read" error.
 * \param io_arg "I/O argument" as provided to q2proto function.
 * \param err Error code
 * \param msg Format string with additional information.
 * \returns Error code to return from q2proto function. Typically just \a err.
 */
Q2PROTO_EXTERNALLY_PROVIDED_DECL q2proto_error_t q2protoerr_server_read(uintptr_t io_arg, q2proto_error_t err,
                                                                        const char *msg, ...);
/** @} */
#endif

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // Q2PROTO_IO_H_
