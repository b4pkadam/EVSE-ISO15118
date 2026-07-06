#include <stdio.h>
#include "secc_context.h"
#include "../exi/exi_decoder.h"

/* External handlers */
#include "v2gtp_handler.h"

/* =========================
 * STATE TRANSITION ENGINE
 * ========================= */
void secc_state_transition(secc_context_t *ctx, secc_state_t new_state)
{
    printf("[SECC_STATE_MACHINE] State change: %d → %d\n",
           ctx->state, new_state);

    ctx->state = new_state;
}

/* =========================
 * MESSAGE ROUTER
 * ========================= */
void secc_handle_v2g_message(secc_context_t *ctx, int msg_type)
{
    switch (msg_type)
    {
        case EXI_MSG_SESSION_SETUP:
            secc_state_transition(ctx, SECC_STATE_SESSION_SETUP);
            v2g_handle_session_setup(ctx);
            break;

        case EXI_MSG_SERVICE_DISCOVERY:
            secc_state_transition(ctx, SECC_STATE_SERVICE_DISCOVERY);
            v2g_handle_service_discovery(ctx);
            break;

        case EXI_MSG_PAYMENT_SERVICE_SELECTION:
            secc_state_transition(ctx, SECC_STATE_PAYMENT_SELECTION);
            v2g_handle_payment_selection(ctx);
            break;

        case EXI_MSG_AUTHORIZATION:
            secc_state_transition(ctx, SECC_STATE_AUTHORIZATION);
            v2g_handle_authorization(ctx);
            break;

        case EXI_MSG_CHARGE_PARAMETER_DISCOVERY:
            secc_state_transition(ctx, SECC_STATE_CHARGE_PARAMETER);
            v2g_handle_charge_parameter(ctx);
            break;
            
        case EXI_MSG_CABLE_CHECK:
            secc_state_transition(ctx, SECC_STATE_CABLE_CHECK);
            v2g_handle_cable_check(ctx);
            break;

        case EXI_MSG_PRECHARGE:
            secc_state_transition(ctx, SECC_STATE_PRECHARGE);
            v2g_handle_precharge(ctx);
            break;
    
        case EXI_MSG_POWER_DELIVERY:
            secc_state_transition(ctx, SECC_STATE_POWER_DELIVERY);
            v2g_handle_power_delivery(ctx);
            break;

        case EXI_MSG_CURRENT_DEMAND:
            secc_state_transition(ctx, SECC_STATE_CURRENT_DEMAND);
            v2g_handle_current_demand(ctx);
            break;
            
        case EXI_MSG_WELDING_DETECTION:
            secc_state_transition(ctx, SECC_STATE_WELDING_DETECTION);
            v2g_handle_welding_detection(ctx);
            break;
            
        case EXI_MSG_SESSION_STOP:
            secc_state_transition(ctx, SECC_STATE_SESSION_STOP);
            v2g_handle_session_stop(ctx);
            break;    

        default:
            printf("[SECC_STATE_MACHINE] Unknown message\n");
            ctx->state = SECC_STATE_ERROR;
            break;
    }
}