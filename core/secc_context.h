#ifndef SECC_CONTEXT_H
#define SECC_CONTEXT_H

#include <stdint.h>
#include "secc_state.h"
#include "secc_session.h"  /* For transport_mode_t */

typedef struct {
    int client_fd;
    secc_state_t state;
    uint32_t session_id;  /* Must match secc_session_t */
    uint8_t evcc_id[8];
    int evcc_id_len;
    
    /* Transport mode (TLS or Non-TLS) */
    transport_mode_t transport_mode;
    
    /* Receive buffer for handling multiple messages and partial reads */
    uint8_t recv_buffer[2048];
    int recv_buffer_len;

} secc_context_t;

#endif