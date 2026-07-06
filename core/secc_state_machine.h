#ifndef SECC_STATE_MACHINE_H
#define SECC_STATE_MACHINE_H

#include "secc_context.h"

/* State transition */
void secc_state_transition(secc_context_t *ctx, secc_state_t new_state);

/* Message router */
void secc_handle_v2g_message(secc_context_t *ctx, int msg_type);

#endif
