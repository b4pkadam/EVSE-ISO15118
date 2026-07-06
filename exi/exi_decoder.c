#include <stdio.h>
#include <string.h>
#include <limits.h>

#include "exi_decoder.h"

/* OpenV2G */
#include "iso1/iso1EXIDatatypes.h"
#include "iso1/iso1EXIDatatypesDecoder.h"
#include "codec/BitInputStream.h"

/* AppHand */
#include "appHandEXIDatatypes.h"
#include "appHandEXIDatatypesDecoder.h"

/* =========================
 * EXI DECODER → MESSAGE CLASSIFIER
 * ========================= */
static int selected_supported_app_protocol_schema = 1;
static int selected_supported_app_protocol_valid = 0;

static void print_app_protocol_entry(int index,
    const struct appHandAppProtocolType *protocol)
{
    printf("   AppProtocol[%d]\n", index);
    printf("      ProtocolNamespace: ");
    for (int i = 0; i < protocol->ProtocolNamespace.charactersLen; i++) {
        putchar((char)protocol->ProtocolNamespace.characters[i]);
    }
    printf("\n");
    printf("      VersionNumber: %u.%u\n",
           protocol->VersionNumberMajor,
           protocol->VersionNumberMinor);
    printf("      SchemaID: %u\n", protocol->SchemaID);
    printf("      Priority: %u\n", protocol->Priority);
}

static int choose_supported_app_protocol_schema(
    const struct appHandEXIDocument *doc)
{
    if (!doc->supportedAppProtocolReq_isUsed ||
        doc->supportedAppProtocolReq.AppProtocol.arrayLen == 0)
    {
        return 1;
    }

    int selected_schema = doc->supportedAppProtocolReq.AppProtocol.array[0].SchemaID;
    unsigned int best_priority = UINT_MAX;

    for (int i = 0; i < doc->supportedAppProtocolReq.AppProtocol.arrayLen; i++) {
        const struct appHandAppProtocolType *protocol =
            &doc->supportedAppProtocolReq.AppProtocol.array[i];

        if (protocol->Priority < best_priority) {
            best_priority = protocol->Priority;
            selected_schema = protocol->SchemaID;
        }
    }

    return selected_schema;
}

int exi_get_supported_app_protocol_schema(void)
{
    return selected_supported_app_protocol_valid
               ? selected_supported_app_protocol_schema
               : 1;
}

exi_msg_type_t exi_decode_message(uint8_t *data, int len)
{
    if (!data || len <= 0)
        return EXI_MSG_UNKNOWN;

    /* -------------------------
     * Try SupportedAppProtocol request first (AppHand format)
     * ------------------------- */
    {
        bitstream_t app_stream;
        size_t app_pos = 0;
        struct appHandEXIDocument app_doc;

        app_stream.data = data;
        app_stream.size = len;
        app_stream.pos = &app_pos;
        app_stream.buffer = 0;
        app_stream.capacity = 8;

        memset(&app_doc, 0, sizeof(app_doc));
        int app_err = decode_appHandExiDocument(&app_stream, &app_doc);
        if (app_err == 0 && app_doc.supportedAppProtocolReq_isUsed)
        {
            printf("[EXI] SupportedAppProtocolReq detected\n");
            printf("[EXI] SupportedAppProtocolReq entries: %d\n",
                   app_doc.supportedAppProtocolReq.AppProtocol.arrayLen);

            for (int i = 0;
                 i < app_doc.supportedAppProtocolReq.AppProtocol.arrayLen;
                 i++)
            {
                print_app_protocol_entry(i,
                    &app_doc.supportedAppProtocolReq.AppProtocol.array[i]);
            }

            selected_supported_app_protocol_schema =
                choose_supported_app_protocol_schema(&app_doc);
            selected_supported_app_protocol_valid = 1;

            printf("[EXI] Selected supported app protocol SchemaID: %d\n",
                   selected_supported_app_protocol_schema);
            return EXI_MSG_SUPPORTED_APP_PROTOCOL;
        }

        selected_supported_app_protocol_valid = 0;
    }

    /* -------------------------
     * Bitstream setup
     * ------------------------- */
    bitstream_t stream;
    size_t pos = 0;

    stream.data = data;
    stream.size = len;
    stream.pos = &pos;
    stream.buffer = 0;
    stream.capacity = 8;

    /* -------------------------
     * Decode document
     * ------------------------- */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));
  
    int err = decode_iso1ExiDocument(&stream, &doc);

    if (err != 0)
    {
        printf("EXI decode error: %d\n", err);
        return EXI_MSG_UNKNOWN;
    }

    /* -------------------------
     * CLASSIFY MESSAGE TYPE
     * ------------------------- */

    if (doc.V2G_Message_isUsed)
    {
        /* SessionSetup */
        if (doc.V2G_Message.Body.SessionSetupReq_isUsed)
        {
            printf("[EXI] SessionSetupReq detected\n");
            return EXI_MSG_SESSION_SETUP;
        }

        /* Service Discovery */
        if (doc.V2G_Message.Body.ServiceDiscoveryReq_isUsed)
        {
            printf("[EXI] ServiceDiscoveryReq detected\n");
            return EXI_MSG_SERVICE_DISCOVERY;
        }

        /* Service Detail */
        if (doc.V2G_Message.Body.ServiceDetailReq_isUsed)
        {
            printf("[EXI] ServiceDetailReq detected\n");
            return EXI_MSG_SERVICE_DETAIL;
        }        

        /* Payment Selection */
        if (doc.V2G_Message.Body.PaymentServiceSelectionReq_isUsed)
        {
            printf("[EXI] PaymentServiceSelectionReq detected\n");
            return EXI_MSG_PAYMENT_SERVICE_SELECTION;
        }

        /* Authorization */
        if (doc.V2G_Message.Body.AuthorizationReq_isUsed)
        {
            printf("[EXI] AuthorizationReq detected\n");
            return EXI_MSG_AUTHORIZATION;
        }

        /* Charge Parameter */
        if (doc.V2G_Message.Body.ChargeParameterDiscoveryReq_isUsed)
        {
            printf("[EXI] ChargeParameterDiscoveryReq detected\n");
            return EXI_MSG_CHARGE_PARAMETER_DISCOVERY;
        }

        /* Cable Check */
        if (doc.V2G_Message.Body.CableCheckReq_isUsed)
        {
            printf("[EXI] CableCheckReq detected\n");
            return EXI_MSG_CABLE_CHECK;
        }

        /* PreCharge */
        if (doc.V2G_Message.Body.PreChargeReq_isUsed)
        {
            printf("[EXI] PreChargeReq detected\n");
            return EXI_MSG_PRECHARGE;
        }

        /* Current Demand */
        if (doc.V2G_Message.Body.CurrentDemandReq_isUsed)
        {
            printf("[EXI] CurrentDemandReq detected\n");
            return EXI_MSG_CURRENT_DEMAND;
        }   


        /* Power Delivery */
        if (doc.V2G_Message.Body.PowerDeliveryReq_isUsed)
        {
            printf("[EXI] PowerDeliveryReq detected\n");
            return EXI_MSG_POWER_DELIVERY;
        }

        /* Welding Detection */
        if (doc.V2G_Message.Body.WeldingDetectionReq_isUsed)
        {
            printf("[EXI] WeldingDetectionReq detected\n");
            return EXI_MSG_WELDING_DETECTION;   
        }   

        /* Session Stop */  
        if (doc.V2G_Message.Body.SessionStopReq_isUsed)
        {
            printf("[EXI] SessionStopReq detected\n");
            return EXI_MSG_SESSION_STOP;    
        }
    }

    return EXI_MSG_UNKNOWN;
}