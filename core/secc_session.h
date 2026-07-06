#ifndef SECC_SESSION_H
#define SECC_SESSION_H

#include <stdint.h>

/* =========================
 * SECC State Machine
 * ========================= */
#include "secc_state.h"

/* Forward declare SLAC context */
typedef struct SlacContext SlacContext;

/* Transport mode */
typedef enum {
    TRANSPORT_MODE_SLAC = 0,    /* Power Line Communication */
    TRANSPORT_MODE_TLS = 1,     /* TCP + TLS (over PLC or Ethernet) */
    TRANSPORT_MODE_NON_TLS = 2  /* TCP only */
} transport_mode_t;

/* =========================
 * Session Context
 * ========================= */
typedef struct {
    int client_fd;
    secc_state_t state;
    uint32_t session_id;
    uint8_t evcc_id[8];
    int evcc_id_len;
    
    /* Transport mode (SLAC, TLS, or Non-TLS) */
    transport_mode_t transport_mode;
    
    /* SLAC phase context */
    SlacContext *slac_ctx;
    char slac_iface[16];        /* e.g., "eth0" for PLC interface */
    uint8_t slac_nmk[16];       /* Network Membership Key from SLAC */
    uint8_t slac_nid[7];        /* Network Identifier from SLAC */
    
    /* Receive buffer for handling multiple messages and partial reads */
    uint8_t recv_buffer[2048];
    int recv_buffer_len;
} secc_session_t;

/* =========================
 * API
 * ========================= */
void secc_session_init(secc_session_t *sess);

int  secc_session_start(secc_session_t *sess);

void secc_session_run(secc_session_t *sess);

void secc_session_stop(secc_session_t *sess);

#endif