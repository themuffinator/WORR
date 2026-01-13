/*
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
 * Default packing functions
 */
#ifndef Q2PROTO_INTERNAL_PACKING_H_
#define Q2PROTO_INTERNAL_PACKING_H_

#include "q2proto/q2proto.h"
#include "q2proto_internal_defs.h"

static MAYBE_UNUSED const q2proto_packed_entity_state_t q2proto_null_packed_entity_state;
static MAYBE_UNUSED const q2proto_packed_player_state_t q2proto_null_packed_player_state;

/**
 * Compute delta message from changes between two entity states.
 * Vanilla, R1Q2, Q2PRO, Q2PRO extended are relatively similar and can be handled with a single function.
 * \param from From/old/previous entity state.
 * \param to To/new/current entity state.
 * \param write_old_origin Whether to include old_origin in the delta.
 * \param extended_state Whether to consider Q2PRO extended fields.
 * \param delta Receives delta message.
 */
Q2PROTO_PRIVATE_API void q2proto_packing_make_entity_state_delta(const q2proto_packed_entity_state_t *from,
                                                                 const q2proto_packed_entity_state_t *to,
                                                                 bool write_old_origin, bool extended_state,
                                                                 q2proto_entity_state_delta_t *delta);
/**
 * Compute delta message from changes between two player states.
 * Vanilla, R1Q2, Q2PRO, Q2PRO extended are relatively similar and can be handled with a single function.
 * \param from From/old/previous player state.
 * \param to To/new/current player state.
 * \param delta Receives delta message.
 */
Q2PROTO_PRIVATE_API void q2proto_packing_make_player_state_delta(const q2proto_packed_player_state_t *from,
                                                                 const q2proto_packed_player_state_t *to,
                                                                 q2proto_svc_playerstate_t *delta);

#endif // Q2PROTO_INTERNAL_PACKING_H_
