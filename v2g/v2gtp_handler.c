#include <stdio.h>
#include <string.h>

#include "v2gtp_handler.h"
#include "v2gtp_parser.h"
#include "v2gtp_response_handler.h"
#include "tls_transport.h"

/* EXI layer */
#include "exi_decoder.h"
#include "secc_state_machine.h"
#include "secc_context.h"

/* =========================
 * V2G CONSTANTS
 * ========================= */
#define V2GTP_HEADER_LEN 8
#define BUFFER_SIZE 2048

/* Example payload type (ISO15118-2) */
#define V2GTP_EXI 0x8001
int err;
uint8_t response[BUFFER_SIZE];
int response_len;

/* =========================
 * MAIN MESSAGE HANDLER
 * ========================= */
int v2g_process_message(int client_fd, secc_context_t *ctx)
{
    uint8_t temp_buffer[BUFFER_SIZE];
    
    /* Read new TLS data */
    int len = tls_read(client_fd, temp_buffer, sizeof(temp_buffer));
    if (len <= 0)
    {
        return -1;
    }

    printf("[V2G] Received V2G message: data (%d bytes): \n", len);
    for (int i = 0; i < len; i++){
        // printf("%02x ", temp_buffer[i]);

            printf("%02X ", temp_buffer[i]);
            if ((i + 1) % 16 == 0)
            printf("\n");

    }
      printf("\n");
    /* Append new data to receive buffer */
    if (ctx->recv_buffer_len + len > BUFFER_SIZE)
    {
        printf("[V2G] Receive buffer overflow\n");
        ctx->recv_buffer_len = 0;
        return -1;
    }
    
    memcpy(&ctx->recv_buffer[ctx->recv_buffer_len], temp_buffer, len);
    ctx->recv_buffer_len += len;

    /* Process all complete V2GTP messages in buffer */
    int offset = 0;
    
    while (offset + V2GTP_HEADER_LEN <= ctx->recv_buffer_len)
    {
        /* Parse V2GTP header */
        v2gtp_header_t hdr;
        int err = v2gtp_parse_header(&hdr, &ctx->recv_buffer[offset]);

        if (err != 0)
        {
            printf("[V2G] V2GTP header parse failed\n");
            ctx->recv_buffer_len = 0;
            return -1;
        }

        /* Check if complete message is in buffer */
        int message_len = V2GTP_HEADER_LEN + hdr.payload_length;
        if (offset + message_len > ctx->recv_buffer_len)
        {
            /* Incomplete message - wait for more data */
            printf("[V2G] Incomplete message in buffer (%d/%d bytes)\n", 
                   ctx->recv_buffer_len - offset, message_len);
            break;
        }

        printf("[V2G] Payload Type: 0x%04X\n", hdr.payload_type);
        printf("[V2G] Payload Length: %d\n", hdr.payload_length);

        /* Only EXI payload supported for now */
        if (hdr.payload_type != V2GTP_EXI)
        {
            printf("[V2G] Unsupported V2GTP type\n");
            offset += message_len;
            continue;
        }

        /* Extract EXI payload */
        uint8_t *exi_data = &ctx->recv_buffer[offset + V2GTP_HEADER_LEN];

        /* Decode EXI */
        exi_msg_type_t msg_type = (exi_msg_type_t)exi_decode_message(exi_data, hdr.payload_length);
        
        /* Handle request and build response */
        if (msg_type == EXI_MSG_SUPPORTED_APP_PROTOCOL) {
            v2g_handle_supported_app_protocol(ctx);
        } else {
            secc_handle_v2g_message(ctx, msg_type);
        }

        /* Build and send response */
        uint8_t response[BUFFER_SIZE];
        int response_len = 0;
        
        if (secc_build_response(ctx, msg_type, response, &response_len) == 0)
        {
            if (tls_write(client_fd, response, response_len) < 0)
            {
                printf("[V2G] TLS write failed\n");
                ctx->recv_buffer_len = 0;
                return -1;
            }
        }
        else
        {
            printf("[V2G] Failed to build response\n");
            ctx->recv_buffer_len = 0;
            return -1;
        }

        /* Move to next message */
        offset += message_len;
    }

    /* Shift remaining data to beginning of buffer */
    if (offset > 0 && offset < ctx->recv_buffer_len)
    {
        ctx->recv_buffer_len -= offset;
        memmove(ctx->recv_buffer, &ctx->recv_buffer[offset], ctx->recv_buffer_len);
    }
    else if (offset >= ctx->recv_buffer_len)
    {
        ctx->recv_buffer_len = 0;
    }

    return 0;
}


int handle_v2g_response(int client_fd, secc_context_t *ctx, uint8_t *response, int *response_len) {
    if (secc_build_response(ctx, EXI_MSG_SESSION_SETUP, response, response_len) == 0) {
        tls_write(client_fd, response, *response_len);
    }
    return 0;
}

/* =========================
 * MESSAGE HANDLERS (STUBS)
 * ========================= */
void v2g_handle_supported_app_protocol(secc_context_t *ctx)
{
    printf("[V2G] SupportedAppProtocol handler\n");
}

void v2g_handle_session_setup(secc_context_t *ctx)
{
    printf("[V2G] Session setup handler\n");
}

void v2g_handle_service_discovery(secc_context_t *ctx)
{
    printf("[V2G] Service discovery handler\n");
}

void v2g_handle_service_detail(secc_context_t *ctx)
{
    printf("[V2G] Service detail handler\n");
}

void v2g_handle_payment_selection(secc_context_t *ctx)
{
    printf("[V2G] Payment selection handler\n");
}

void v2g_handle_authorization(secc_context_t *ctx)
{
    printf("[V2G] Authorization handler\n");
}

void v2g_handle_charge_parameter(secc_context_t *ctx)
{
    printf("[V2G] Charge parameter handler\n");
}

void v2g_handle_cable_check(secc_context_t *ctx)
{
    printf("[V2G] Cable check handler\n");
}

void v2g_handle_precharge(secc_context_t *ctx)
{
    printf("[V2G] Pre-charge handler\n");
}

void v2g_handle_current_demand(secc_context_t *ctx)
{
    printf("[V2G] Current demand handler\n");
}

void v2g_handle_welding_detection(secc_context_t *ctx)
{
    printf("[V2G] Welding detection handler\n");
}

void v2g_handle_session_stop(secc_context_t *ctx)
{
    printf("[V2G] Session stop handler\n");
}


void v2g_handle_power_delivery(secc_context_t *ctx)
{
    printf("[V2G] Power delivery handler\n");
}
