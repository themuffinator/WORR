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

#define Q2PROTO_BUILD
#include "q2proto_internal_download.h"

#include "q2proto_internal_defs.h"

void q2proto_download_common_begin(q2proto_servercontext_t *context, size_t total_size,
                                   q2proto_server_download_state_t *state)
{
    memset(state, 0, sizeof(*state));
    state->context = context;
    state->total_size = total_size;
}

q2proto_error_t q2proto_download_common_complete_struct(q2proto_server_download_state_t *state,
                                                        size_t download_remaining, q2proto_svc_download_t *svc_download)
{
    state->transferred = state->total_size - download_remaining;
    svc_download->percent = ((100 * state->transferred) / state->total_size);
    svc_download->percent = CLAMP(svc_download->percent, 0, 100);
    return state->transferred == state->total_size ? Q2P_ERR_DOWNLOAD_COMPLETE : Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_download_common_data(q2proto_server_download_state_t *state, const uint8_t **data,
                                             size_t *remaining, size_t packet_remaining,
                                             q2proto_svc_download_t *svc_download)
{
    if (packet_remaining < 4)
        return Q2P_ERR_NOT_ENOUGH_PACKET_SPACE;

    size_t download_size = packet_remaining - 4;
    download_size = MIN(download_size, *remaining);
    download_size = MIN(download_size, INT16_MAX);

    memset(svc_download, 0, sizeof(*svc_download));
    svc_download->data = *data;
    svc_download->size = (int16_t)download_size;

    *data += download_size;
    *remaining -= download_size;
    return q2proto_download_common_complete_struct(state, *remaining, svc_download);
}

q2proto_error_t q2proto_download_common_finish(q2proto_server_download_state_t *state,
                                               q2proto_svc_download_t *svc_download)
{
    memset(svc_download, 0, sizeof(*svc_download));
    svc_download->percent = 100;
    svc_download->size = 0;
    return Q2P_ERR_SUCCESS;
}

q2proto_error_t q2proto_download_common_abort(q2proto_server_download_state_t *state,
                                              q2proto_svc_download_t *svc_download)
{
    memset(svc_download, 0, sizeof(*svc_download));
    svc_download->percent = (state && state->total_size) ? (100 * state->transferred) / state->total_size : 0;
    svc_download->size = -1;
    return Q2P_ERR_SUCCESS;
}
