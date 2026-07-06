#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "secc_session.h"
#include "secc_state.h"

/* transport + v2g layer */
#include "tls_transport.h"
#include "v2gtp_handler.h"
#include "v2gtp_sdp_handler.h"

/* =========================
 * INIT SESSION
 * ========================= */
void secc_session_init(secc_session_t *sess)
{
    memset(sess, 0, sizeof(secc_session_t));

    sess->client_fd = -1;
    sess->state = SECC_STATE_IDLE;
    sess->session_id = 1;   // can be randomized later
    
    /* Initialize transport mode */
    ((secc_context_t *)sess)->transport_mode = TRANSPORT_MODE_TLS;  /* Default to TLS */
}

/* =========================
 * START LISTENER + ACCEPT + SDP HANDLING
 * ========================= */
int secc_session_start(secc_session_t *sess)
{
    printf("SECC listening on port 15118...\n");

    sess->state = SECC_STATE_IDLE;

    /* ===== PHASE 1: SDP DISCOVERY (UDP) ===== */
    printf("[SECC] Phase 1: Starting SDP discovery (UDP)...\n");
    
    /* secc_sdp_listen() handles the COMPLETE SDP exchange:
     * - Opens IPv6 socket
     * - Listens for SDP request from EV
     * - Sends SDP response back
     * - Returns when done (socket already closed)
     */
    if (secc_sdp_listen(15118, 0) < 0)
    {
        printf("[ERROR] SDP discovery failed\n");
        sess->state = SECC_STATE_ERROR;
        return -1;
    }

    printf("[SECC] Phase 1 complete: SDP discovery finished\n\n");

    /* ===== PHASE 2: V2G COMMUNICATION (TCP) ===== */
    printf("[SECC] Phase 2: Starting V2G communication (TCP)...\n");
    
    /* Update transport mode based on what was negotiated in SDP */
    ((secc_context_t *)sess)->transport_mode = TRANSPORT_MODE_TLS;
    printf("[SECC] Configured for TLS mode\n");
    
    sess->state = SECC_STATE_TCP_CONNECTED;

    /* Accept TCP connection for V2G communication */
    printf("[SECC] Accepting client connection (V2G communication)...\n");
    sess->client_fd = tls_server_init_and_accept_ex(((secc_context_t *)sess)->transport_mode);  /* Accept in Non-TLS mode first */

    if (sess->client_fd < 0) {
        printf("[ERROR] Failed to accept TCP connection\n");
        sess->state = SECC_STATE_ERROR;
        return -1;
    }

    printf("[SECC] TCP connection accepted\n");

    /* Perform TLS handshake (always TLS after SDP negotiation) */
    printf("[SECC] Waiting for EV TLS handshake...\n");
    sess->state = SECC_STATE_TLS_HANDSHAKE;

    if (tls_handshake(sess->client_fd) != 0) {
        printf("[ERROR] TLS handshake failed\n");
        sess->state = SECC_STATE_ERROR;
        tls_close(sess->client_fd);
        return -1;
    }

    printf("[SECC] TLS handshake complete\n");

    sess->state = SECC_STATE_SESSION_SETUP;
    printf("[SECC] Phase 2 complete: V2G communication established\n");
    return 0;
}

/* =========================
 * MAIN SESSION LOOP
 * ========================= */
void secc_session_run(secc_session_t *sess)
{
    uint8_t running = 1;

    sess->state = SECC_STATE_SERVICE_DISCOVERY;

    while (running)
    {
        int ret = v2g_process_message(sess->client_fd, (secc_context_t *)sess);

        if (ret < 0) {
            printf("V2G processing error\n");
            sess->state = SECC_STATE_ERROR;
            break;
        }

        /* Continue to next message */
        printf("[SECC] Waiting for next message...\n");
    }
}

/* =========================
 * STOP SESSION
 * ========================= */
void secc_session_stop(secc_session_t *sess)
{
    printf("Closing SECC session\n");

    sess->state = SECC_STATE_IDLE;

    if (sess->client_fd >= 0) {
        tls_close(sess->client_fd);
        sess->client_fd = -1;
    }
}