#include <stdio.h>
#include <string.h>

#include "v2gtp_sdp_handler.h"

/* =========================
 * SDP REQUEST FORMAT (ISO 15118-2 Annex A):
 *
 * V2GTP Header (8 bytes):
 *   Byte 0:    Protocol Version         (0x01)
 *   Byte 1:    Protocol Version Inv     (0xFE)
 *   Byte 2-3:  Payload Type             (0x9000 = SDP Request)
 *   Byte 4-7:  Payload Length           (0x00000002, big-endian)
 *
 * SDP Request Payload (2 bytes only — no msg_type, no timestamp):
 *   Byte 8:    Security                 (0x00=TLS, 0x10=No security)
 *   Byte 9:    Transport Protocol       (0x00=TCP only)
 *
 * Total packet = 10 bytes
 * ========================= */

#define V2GTP_VERSION           0x01
#define V2GTP_VERSION_INV       0xFE
#define V2GTP_SDP_REQUEST       0x9000   /* FIX #5: named explicitly as Request */
#define V2GTP_SDP_RESPONSE      0x9001
#define V2GTP_HEADER_LEN        8

/* ISO 15118-2 Security field values */
#define SDP_SECURITY_TLS        0x00     /* FIX #3: was wrongly 0x01 */
#define SDP_SECURITY_NONE       0x10     /* FIX #3: was wrongly 0x00 */
#define SDP_TRANSPORT_TCP       0x00

int sdp_parse_request(const uint8_t *data, int len, sdp_request_t *req)
{
    if (!data || !req || len < (V2GTP_HEADER_LEN + 2))
        return -1;

    /* FIX #4: Check V2GTP version bytes */
    if (data[0] != V2GTP_VERSION || data[1] != V2GTP_VERSION_INV) {
        printf("[SDP] Invalid V2GTP version: 0x%02X 0x%02X\n", data[0], data[1]);
        return -1;
    }

    /* Check payload type = SDP Request */
    uint16_t payload_type = (data[2] << 8) | data[3];
    if (payload_type != V2GTP_SDP_REQUEST) {
        printf("[SDP] Invalid payload type: 0x%04X\n", payload_type);
        return -1;
    }

    /* Check payload length = exactly 2 */
    uint32_t payload_len = (data[4] << 24) |
                           (data[5] << 16) |
                           (data[6] <<  8) |
                            data[7];
    if (payload_len != 2) {
        printf("[SDP] Invalid payload length: %u (expected 2)\n", payload_len);
        return -1;
    }

    /* Extract SDP payload — starts at byte 8, exactly 2 fields */
    const uint8_t *sdp_data = data + V2GTP_HEADER_LEN;

    /* FIX #1 #2: sdp_data[0]=security, sdp_data[1]=transport — no msg_type field */
    req->security        = sdp_data[0];   /* 0x00=TLS, 0x10=No security */
    req->transport_proto = sdp_data[1];   /* 0x00=TCP                   */

    printf("[SDP] Parsed SDP request: security=0x%02X transport=0x%02X payload_len=%u\n",
           req->security, req->transport_proto, payload_len); /* */

    return 0;
}

int sdp_select_transport(const sdp_request_t *req)
{
    if (!req)
        return -1;

    /* FIX #3: Correct ISO 15118-2 security field values */
    if (req->security == SDP_SECURITY_TLS) {          /* 0x00 = TLS */
        printf("[SDP] Selected: TLS transport\n");
        return SDP_SECURITY_TLS;
    }
    else if (req->security == SDP_SECURITY_NONE) {    /* 0x10 = No security (TCP) */
        printf("[SDP] Selected: Non-TLS (TCP) transport\n");
        return SDP_SECURITY_NONE;
    }

    printf("[SDP] Unknown security value: 0x%02X\n", req->security);
    return -1;
}


 /* =========================
 * BUILD SDP RESPONSE (for UDP transmission)
 * ISO 15118-2 Annex A — SDP Response
 * =========================
 *
 * V2GTP Header (8 bytes):
 * Byte 0    : Protocol Version        (0x01)
 * Byte 1    : Inverse Version         (0xFE)
 * Byte 2-3  : Payload Type            (0x9001 = SDP Response)
 * Byte 4-7  : Payload Length          (0x00000014 = 20 bytes)
 *
 * SDP Payload (20 bytes):

 * Total Packet Size = 28 bytes
 * ================================================================== */
int sdp_build_response(uint8_t *buf, 
                        int buf_len, 
                        const uint8_t *ip6_addr,
                        uint16_t port,
                        uint8_t security,
                        const sdp_response_t *resp) /* FIX #4: added sdp_response_t struct for cleaner parameters */{
    if (!buf || buf_len < 28 || !ip6_addr)   /* FIX #5: was < 26 */
        return -1;

    /* ── V2GTP Header (8 bytes) ──────────────────────────── */
    buf[0] = 0x01;                       /* Protocol Version          */
    buf[1] = 0xFE;                       /* Protocol Version Inverted */
    buf[2] = 0x90;                       /* Payload Type High         */
    buf[3] = 0x01;                       /* 0x9001 = SDP Response */

    /* Payload length: 20 bytes (1 msg_type + 1 security + 2 port + 16 IPv6) */
    uint32_t payload_len = 20;           /* FIX #2: was 18 */
    buf[4] = (payload_len >> 24) & 0xFF;
    buf[5] = (payload_len >> 16) & 0xFF;
    buf[6] = (payload_len >>  8) & 0xFF;
    buf[7] =  payload_len        & 0xFF;

    /* ── SDP Payload (20 bytes) ──────────────────────────── */
    /* IPv6 Address (16 bytes) */
   /* Payload bytes 8-23 */
    memcpy(&buf[8], ip6_addr, 16);

    /* Payload bytes 24-25 */
    buf[24] = (port >> 8) & 0xFF;
    buf[25] = port & 0xFF;

    /* Payload byte 26 */
    buf[26] = security;     /* 0x00 TLS, 0x10 No TLS */

    /* Payload byte 27 */
    buf[27] = 0x00;         /* TCP */

    int packet_len = 28;                 /*  27 (8 header + 20 payload) */

    /* ── Debug: print all fields with byte positions ── */
    printf("[SDP] Response built:\n");
   // printf("  [8]     msg_type = 0x%02X\n",       buf[8]);
    printf("  [26]    security = 0x%02X (%s)\n",  buf[26],
           buf[26] == 0x00 ? "TLS" : "No Security");
    printf("  [24-25] port     = %d\n",            resp->port);
    printf("  [8-23]  IPv6     = ");
    for (int i = 0; i < 16; i++) {
        printf("%02X", resp->ip6_addr[i]);
        if (i % 2 == 1 && i != 15) printf(":");
    }
    printf("\n");

    return packet_len;
}