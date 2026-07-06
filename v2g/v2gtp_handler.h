#ifndef V2GTP_HANDLER_H
#define V2GTP_HANDLER_H

#include <stdint.h>
#include "secc_context.h"

/* Process one V2G message from TLS stream */
int v2g_process_message(int client_fd, secc_context_t *ctx);

/* Request handlers */
void v2g_handle_supported_app_protocol(secc_context_t *ctx);
void v2g_handle_session_setup(secc_context_t *ctx);
void v2g_handle_service_discovery(secc_context_t *ctx);
void v2g_handle_service_detail(secc_context_t *ctx);
void v2g_handle_payment_selection(secc_context_t *ctx);
void v2g_handle_authorization(secc_context_t *ctx);
void v2g_handle_charge_parameter(secc_context_t *ctx);
void v2g_handle_cable_check(secc_context_t *ctx);
void v2g_handle_precharge(secc_context_t *ctx);
void v2g_handle_current_demand(secc_context_t *ctx);
void v2g_handle_welding_detection(secc_context_t *ctx);
void v2g_handle_session_stop(secc_context_t *ctx);
void v2g_handle_power_delivery(secc_context_t *ctx);


#endif