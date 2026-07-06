#ifndef V2GTP_SDP_HANDLER_H
#define V2GTP_SDP_HANDLER_H

#include <stdint.h>

/* SDP message types */
typedef enum {
    SDP_REQUEST = 0x01,
    SDP_RESPONSE = 0x02
} sdp_msg_type_t;

/* Transport protocol */
typedef enum {
    TRANSPORT_TLS = 0x00,
    TRANSPORT_TCP = 0x01
} sdp_transport_t;

/* SDP Request structure */
typedef struct {
    uint8_t msg_type;
    uint8_t transport_proto;
    uint8_t security;  /* bit0=TLS, bit1=TCP */
} sdp_request_t;

/* SDP Response structure (ISO 15118-2) */
typedef struct {
    uint8_t security;   /* bit0=TLS, bit1=TCP */
    uint16_t port;      /* Port number for V2G communication */
    uint8_t ip6_addr[16];/* IPv6 address for V2G communication (ISO 15118-2) */
} sdp_response_t;

/* Parse SDP request from EV */
int sdp_parse_request(const uint8_t *data, int len, sdp_request_t *req);

/* Determine which transport to use based on SDP */
int sdp_select_transport(const sdp_request_t *req);

/* Build SDP response packet (returns packet length, or -1 on error) */
int sdp_build_response(uint8_t *buf, int buf_len, 
                        const uint8_t *ip6_addr,
                        uint16_t port,
                        uint8_t security,   
                       const sdp_response_t *resp);

#endif
