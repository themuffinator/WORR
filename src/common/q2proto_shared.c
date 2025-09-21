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

#include "shared/shared.h"
#include "common/msg.h"
#include "common/q2proto_shared.h"

static const q2proto_packed_entity_state_t nullServerEntityState;

bool Q2PROTO_MakeEntityDelta(q2proto_servercontext_t *context, q2proto_entity_state_delta_t *delta, const q2proto_packed_entity_state_t *from, const q2proto_packed_entity_state_t *to, msgEsFlags_t flags)
{
    if (!from)
        from = &nullServerEntityState;

    bool write_old_origin =
        ((flags & MSG_ES_NEWENTITY) && !VectorCompare(to->old_origin, from->origin))
        || ((to->renderfx & RF_FRAMELERP) && !VectorCompare(to->old_origin, from->origin))
        || ((to->renderfx & RF_BEAM) && (!(flags & MSG_ES_BEAMORIGIN) || !VectorCompare(to->old_origin, from->old_origin)));
    q2proto_server_make_entity_state_delta(context, from, to, !(flags & MSG_ES_FIRSTPERSON) && write_old_origin, delta);
    if (flags & MSG_ES_FIRSTPERSON)
    {
        memcpy(&delta->origin.write.current, &delta->origin.write.prev, sizeof(delta->origin.write.current));
        delta->angle.delta_bits = 0;
    }
    return (delta->delta_bits != 0) || (memcmp(&delta->origin.write.current, &delta->origin.write.prev, sizeof(delta->origin.write.current)) != 0) || (delta->angle.delta_bits != 0);
}
