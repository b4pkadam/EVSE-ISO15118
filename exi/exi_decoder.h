#ifndef EXI_DECODER_H
#define EXI_DECODER_H

#include <stdint.h>

/* ISO15118 Message Types (internal SECC mapping) */
typedef enum {
    EXI_MSG_UNKNOWN = 0,
    EXI_MSG_SUPPORTED_APP_PROTOCOL = 1,
    EXI_MSG_SESSION_SETUP = 2,
    EXI_MSG_SERVICE_DISCOVERY = 3,
    EXI_MSG_SERVICE_DETAIL = 4,
    EXI_MSG_PAYMENT_SERVICE_SELECTION = 5,
    EXI_MSG_AUTHORIZATION = 6,
    EXI_MSG_CHARGE_PARAMETER_DISCOVERY = 7,
    EXI_MSG_CABLE_CHECK = 8,
    EXI_MSG_PRECHARGE = 9,
    EXI_MSG_POWER_DELIVERY = 10,
    EXI_MSG_CURRENT_DEMAND = 11,
    EXI_MSG_WELDING_DETECTION = 12,
    EXI_MSG_SESSION_STOP = 13
} exi_msg_type_t;

/* Decode EXI and return message type */
exi_msg_type_t exi_decode_message(uint8_t *data, int len);

/* Returns the last selected SupportedAppProtocol SchemaID from the most recently decoded request.
 * Defaults to ISO 15118-2 schema ID 1 when no explicit selection is available.
 */
int exi_get_supported_app_protocol_schema(void);

#endif