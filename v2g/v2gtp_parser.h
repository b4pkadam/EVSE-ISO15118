#ifndef V2GTP_PARSER_H
#define V2GTP_PARSER_H

#include <stdint.h>

/* =========================
 * V2GTP Header Structure
 * ========================= */
typedef struct {
    uint16_t payload_type;
    uint32_t payload_length;
} v2gtp_header_t;

/* Parse raw V2GTP header */
int v2gtp_parse_header(v2gtp_header_t *hdr, const uint8_t *buf);

#endif