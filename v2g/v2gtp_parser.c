#include "v2gtp_parser.h"

/* =========================
 * V2GTP HEADER FORMAT:
 * Byte 0-1 : Protocol Version + Inverse
 * Byte 2-3 : Payload Type
 * Byte 4-7 : Payload Length
 * ========================= */

int v2gtp_parse_header(v2gtp_header_t *hdr, const uint8_t *buf)
{
    if (!hdr || !buf)
        return -1;

    hdr->payload_type =
        (buf[2] << 8) | buf[3];

    hdr->payload_length =
        (buf[4] << 24) |
        (buf[5] << 16) |
        (buf[6] << 8)  |
        (buf[7]);

    return 0;
}