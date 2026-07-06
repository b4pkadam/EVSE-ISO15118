#ifndef SECC_RESPONSE_H
#define SECC_RESPONSE_H

#include <stdint.h>
#include "secc_context.h"
#include "exi_decoder.h"

int secc_build_response(secc_context_t *ctx,
                        exi_msg_type_t req_type,
                        uint8_t *out_buf,
                        int *out_len);

#endif