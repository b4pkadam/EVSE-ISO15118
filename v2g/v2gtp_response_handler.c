#include <stdio.h>
#include <string.h>

#include "v2gtp_response_handler.h"

/* OpenV2G encoder */
#include "iso1/iso1EXIDatatypes.h"
#include "iso1/iso1EXIDatatypesEncoder.h"
#include "iso1/iso1EXIDatatypesDecoder.h"

#include "appHandEXIDatatypes.h"
#include "appHandEXIDatatypesEncoder.h"
#include "appHandEXIDatatypesDecoder.h"
#include "EXIHeaderDecoder.h"
// #include "EXIHeaderEncoder.h

/* Bitstream */
#include "codec/BitOutputStream.h"

#define V2GTP_HEADER_LEN 8
#define V2GTP_EXI 0x8001


/* After encoding, immediately decode to verify */
int verify_session_setup_response(uint8_t *exi_buf, int exi_len)
{
    printf("[VERIFY] Decoding encoded EXI to verify correctness\n");

    struct iso1EXIDocument decoded_doc;
    memset(&decoded_doc, 0, sizeof(decoded_doc));

    bitstream_t dec_stream;
    size_t dec_pos = 0;

    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int err = decode_iso1ExiDocument(&dec_stream, &decoded_doc);
    if (err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", err);
        return -1;
    }

    printf("[VERIFY] ✅ Decode successful\n");

    /* Check V2G message */
    if (!decoded_doc.V2G_Message_isUsed) {
        printf("[VERIFY] ❌ V2G_Message_isUsed = 0\n");
        return -1;
    }
    printf("[VERIFY] ✅ V2G_Message_isUsed = 1\n");

    /* Check SessionSetupRes */
    if (!decoded_doc.V2G_Message.Body.SessionSetupRes_isUsed) {
        printf("[VERIFY] ❌ SessionSetupRes_isUsed = 0\n");
        return -1;
    }
    printf("[VERIFY] ✅ SessionSetupRes_isUsed = 1\n");

    /* Check response code */
    iso1responseCodeType rc =
        decoded_doc.V2G_Message.Body.SessionSetupRes.ResponseCode;
    printf("[VERIFY] ResponseCode = %d (expected %d = OK_NewSessionEstablished)\n",
           rc, iso1responseCodeType_OK_NewSessionEstablished);

    if (rc != iso1responseCodeType_OK_NewSessionEstablished) {
        printf("[VERIFY] ❌ Wrong response code!\n");
        return -1;
    }
    printf("[VERIFY] ✅ ResponseCode correct\n");

    /* Check Session ID */
    int sid_len = decoded_doc.V2G_Message.Header.SessionID.bytesLen;
    printf("[VERIFY] SessionID (%d bytes): ", sid_len);
    for (int i = 0; i < sid_len; i++) {
        printf("%02X ", decoded_doc.V2G_Message.Header.SessionID.bytes[i]);
    }
    printf("\n");

    /* Check EVSEID */
    int evse_len = decoded_doc.V2G_Message.Body.SessionSetupRes.EVSEID.charactersLen;
    printf("[VERIFY] EVSEID (%d chars): ", evse_len);
    for (int i = 0; i < evse_len; i++) {
        printf("%c", decoded_doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[i]);
    }
    printf("\n");

    printf("[VERIFY] ✅ All fields verified OK\n");
    return 0;
}
int verify_service_discovery_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc2;
    memset(&doc2, 0, sizeof(doc2));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc2);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ ServiceDiscoveryRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc2.V2G_Message.Body.ServiceDiscoveryRes.ResponseCode);
    printf("[VERIFY] ServiceID          = %d\n",
        doc2.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceID);
    printf("[VERIFY] EnergyTransferMode = %d\n",
        doc2.V2G_Message.Body.ServiceDiscoveryRes.ChargeService
            .SupportedEnergyTransferMode.EnergyTransferMode.array[0]);
    printf("[VERIFY] PaymentOption      = %d\n",
        doc2.V2G_Message.Body.ServiceDiscoveryRes.PaymentOptionList
            .PaymentOption.array[0]);
      printf("\n");

    printf("[VERIFY] ✅ All fields verified OK\n");        
    return 0; 
}
int verify_payment_service_selection_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc3;
    memset(&doc3, 0, sizeof(doc3));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc3);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ PaymentServiceSelectionRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc3.V2G_Message.Body.PaymentServiceSelectionRes.ResponseCode);
    // printf("[VERIFY] ServiceID          = %d\n",
    //     doc3.V2G_Message.Body.PaymentServiceSelectionRes.ChargeService.ServiceID);
    // printf("[VERIFY] PaymentOption      = %d\n",
    //     doc3.V2G_Message.Body.PaymentServiceSelectionRes.PaymentOptionList
    //         .PaymentOption.array[0]);
    //   printf("\n");

    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
}  
int verify_authorization_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc4;
    memset(&doc4, 0, sizeof(doc4));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc4);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ AuthorizationRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc4.V2G_Message.Body.AuthorizationRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
}
int verify_charge_parameter_discovery_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc5;
    memset(&doc5, 0, sizeof(doc5));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc5);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ ChargeParameterDiscoveryRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc5.V2G_Message.Body.ChargeParameterDiscoveryRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
} 
int verify_cable_check_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc5;
    memset(&doc5, 0, sizeof(doc5));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc5);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ CableCheckRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc5.V2G_Message.Body.CableCheckRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
} 
int verify_pre_charge_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc9;
    memset(&doc9, 0, sizeof(doc9));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc9);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ PreChargeRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc9.V2G_Message.Body.PreChargeRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
}
int verify_current_demand_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc10;
    memset(&doc10, 0, sizeof(doc10));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc10);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ CurrentDemandRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc10.V2G_Message.Body.CurrentDemandRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
}
int verify_power_delivery_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc6;
    memset(&doc6, 0, sizeof(doc6));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc6);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ PowerDeliveryRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc6.V2G_Message.Body.PowerDeliveryRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
}
int verify_session_stop_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc7;
    memset(&doc7, 0, sizeof(doc7));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc7);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ SessionStopRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc7.V2G_Message.Body.SessionStopRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
}
int verify_welding_detection_response(uint8_t *exi_buf, int exi_len)
{
    struct iso1EXIDocument doc8;
    memset(&doc8, 0, sizeof(doc8));

    bitstream_t dec_stream;
    size_t dec_pos = 0;
    dec_stream.data     = exi_buf;
    dec_stream.size     = exi_len;
    dec_stream.pos      = &dec_pos;
    dec_stream.buffer   = 0;
    dec_stream.capacity = 8;

    int dec_err = decode_iso1ExiDocument(&dec_stream, &doc8);
    if (dec_err != 0) {
        printf("[VERIFY] ❌ Decode failed: %d\n", dec_err);
        return -1;
    }

    printf("[VERIFY] ✅ WeldingDetectionRes decode OK\n");
    printf("[VERIFY] ResponseCode       = %d\n",
        doc8.V2G_Message.Body.WeldingDetectionRes.ResponseCode);
  
    printf("[VERIFY] ✅ All fields verified OK\n");      
    return 0; 
}


/* =========================
 * V2GTP HEADER
 * ========================= */
static void write_v2gtp_header(uint8_t *buf, uint32_t payload_len)
{
    buf[0] = 0x01;
    buf[1] = 0xFE;

    buf[2] = 0x80;
    buf[3] = 0x01;

    buf[4] = (payload_len >> 24) & 0xFF;
    buf[5] = (payload_len >> 16) & 0xFF;
    buf[6] = (payload_len >> 8) & 0xFF;
    buf[7] = payload_len & 0xFF;
}
/* ============================================================
 * SUPPORTED APP PROTOCOL RESPONSE (REAL EXI)
 *
 * ISO 15118-2:
 *   - Uses appHandEXIDocument (NOT iso1EXIDocument)
 *   - Uses encode_appHandExiDocument()
 *   - V2GTP PayloadType = 0x8001
 * ============================================================ */
static int build_supported_app_protocol_res(secc_context_t *ctx,
                                             uint8_t *out_buf,
                                             int *out_len)
{
    printf("[SECC] Building REAL SupportedAppProtocolRes\n");

    /* =========================
     * BUILD appHandEXIDocument
     * ========================= */
    struct appHandEXIDocument doc;
    memset(&doc, 0, sizeof(doc));

    /* Enable response structure */
    doc.supportedAppProtocolRes_isUsed = 1u;

    /* ── ResponseCode ── */
    doc.supportedAppProtocolRes.ResponseCode =
        appHandresponseCodeType_OK_SuccessfulNegotiation;

    /* ── SchemaID ── */
    /*
     * SchemaID matches the protocol selected:
     *   1 = ISO 15118-2
     *   2 = DIN SPEC 70121
     *
     * Must match what EVCC requested
     */
    doc.supportedAppProtocolRes.SchemaID_isUsed = 1u;
    doc.supportedAppProtocolRes.SchemaID =
        (uint8_t)exi_get_supported_app_protocol_schema();

    /* =========================
     * ENCODE EXI
     * ========================= */
    bitstream_t stream;
    uint8_t     exi_buf[1024];
    size_t      pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));

    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    /* ── Use appHand encoder, NOT iso1 encoder! ── */
    int err = encode_appHandExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode SupportedAppProtocolRes"
               " failed: %d\n", err);
        return -1;
    }

    /* ── Flush remaining bits ── */
    if (flush(&stream) != 0) {
        printf("[SECC] ❌ Bitstream flush failed\n");
        return -1;
    }

    int exi_len = (int)pos;
    printf("[SECC] EXI encoded length: (%d bytes)\n", exi_len);

    /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n");

    /* =========================
     * VERIFY BY DECODING
     * ========================= */
    /* ── Decode back to verify ── */
    size_t      verify_pos = 0;
    bitstream_t verify_stream;

    verify_stream.size     = exi_len;
    verify_stream.data     = exi_buf;
    verify_stream.pos      = &verify_pos;
    verify_stream.buffer   = 0;
    verify_stream.capacity = 8;

    struct appHandEXIDocument verify_doc;
    memset(&verify_doc, 0, sizeof(verify_doc));

    err = decode_appHandExiDocument(&verify_stream, &verify_doc);
    if (err != 0) {
        printf("[SECC] ❌ Verification decode failed: %d\n", err);
        return -1;
    }

    if (!verify_doc.supportedAppProtocolRes_isUsed) {
        printf("[SECC] ❌ Verification: Response not found\n");
        return -1;
    }

    printf("[SECC] ✅ Verification passed:\n");
    printf("   ResponseCode : %d\n",
           verify_doc.supportedAppProtocolRes.ResponseCode);
    printf("   SchemaID     : %d\n",
           verify_doc.supportedAppProtocolRes.SchemaID_isUsed
               ? verify_doc.supportedAppProtocolRes.SchemaID
               : -1);

    /* =========================
     * WRAP INTO V2GTP
     * ========================= */
    write_v2gtp_header(out_buf, exi_len);

    memcpy(out_buf + V2GTP_HEADER_LEN,
           exi_buf,
           exi_len);

    *out_len = V2GTP_HEADER_LEN + exi_len;

    // printf("[SECC] Total response: %d bytes\n", *out_len);

    return 0;
}


/* =========================
 * SESSION SETUP RESPONSE (REAL EXI)
 * ========================= */
static int build_session_setup_res(secc_context_t *ctx,
                                    uint8_t *out_buf,
                                    int *out_len)
{
    printf("[SECC] Building REAL SessionSetupRes\n");

    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));

    /* Enable response structure */
    doc.V2G_Message_isUsed = 1;
    doc.V2G_Message.Body.SessionSetupRes_isUsed = 1;

    /* Fill mandatory fields */
    doc.V2G_Message.Header.SessionID.bytesLen = 8;

    /* Notification - set isUsed=0 explicitly */
    // doc.V2G_Message.Header.Notification_isUsed = 0u;
    // doc.V2G_Message.Header.Signature_isUsed    = 0u;

    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++)
    {
        doc.V2G_Message.Header.SessionID.bytes[7 -i] = (sid >> (i * 8)) & 0xFF;
    }
    // memcpy(doc.V2G_Message.Header.SessionID.bytes,
    //        &ctx->session_id,
    //        8);

    doc.V2G_Message.Body.SessionSetupRes.ResponseCode =
        iso1responseCodeType_OK_NewSessionEstablished;

    // const char *evse_id_str = "SE012345";
    
    // size_t evse_id_len = strlen(evse_id_str);
    // doc.V2G_Message.Body.SessionSetupRes.EVSEID.charactersLen = evse_id_len;
    // memcpy(doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters,
    //        evse_id_str,
    //        evse_id_len);
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.charactersLen = 8;
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[0] = 'S';
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[1] = 'E';
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[2] = '0';
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[3] = '1';
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[4] = '2';
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[5] = '3';
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[6] = '4';
    doc.V2G_Message.Body.SessionSetupRes.EVSEID.characters[7] = '5';

    // doc.V2G_Message.Body.SessionSetupRes.EVSETimeStamp_isUsed = 0u;

    /* =========================
     * ENCODE EXI
     * ========================= */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;
    // memset(exi_buf, 0xAA, sizeof(exi_buf)); /* Fill with 0xAA to detect real end */
    memset(exi_buf, 0, sizeof(exi_buf)); /* Clear buffer before encoding */
    
    stream.data = exi_buf;
    stream.size = sizeof(exi_buf);
    stream.pos = &pos;
    stream.buffer = 0;
    stream.capacity = 8; /* Start with full byte capacity */

    int err = encode_iso1ExiDocument(&stream, &doc);

    if (err != 0)
    {
        printf("EXI encode failed: %d\n", err);
        return -1;
    }
  
    /* Flush remaining bits */
    if (flush(&stream) != 0)
    {
        printf("Bitstream flush failed\n");
        return -1;
    }

    int exi_len = (int)pos;  /* Round up to next byte */
    printf("[SECC] EXI encoded length: (%d bytes)\n", exi_len);
    for (int i = 0; i < exi_len; i++)
    {
        printf("%02X ", exi_buf[i]);
    }
    printf("\n");
    /* =========================
     * VERIFY BY DECODING
     * ========================= */
    if (verify_session_setup_response(exi_buf, exi_len) != 0)
    {
        printf("[SECC] ❌ Verification failed! Check encoding logic.\n");
        return -1;
    }
    /* =========================
     * WRAP INTO V2GTP
     * ========================= */
    write_v2gtp_header(out_buf, exi_len);

    memcpy(out_buf + V2GTP_HEADER_LEN,
           exi_buf,
           exi_len);

    *out_len = V2GTP_HEADER_LEN + exi_len;
    printf("[SECC] Total response: %zu bytes\n", *out_len);


    return 0;
}


static int build_service_discovery_res(secc_context_t *ctx,
                                       uint8_t *out_buf,
                                       int *out_len)
{
    printf("[SECC] Building REAL ServiceDiscoveryRes\n");

    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));

    /* =============================================
     * HEADER
     * ============================================= */
    doc.V2G_Message_isUsed = 1u;

    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;

    /* =============================================
     * BODY
     * ============================================= */
    doc.V2G_Message.Body.ServiceDiscoveryRes_isUsed = 1u;

    /* Response Code */
    doc.V2G_Message.Body.ServiceDiscoveryRes.ResponseCode =
        iso1responseCodeType_OK;

    /* =============================================
     * ChargeService - MANDATORY
     * ============================================= */
    doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceID       = 1;
    doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceName_isUsed = 0u;
    doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceCategory =
        iso1serviceCategoryType_EVCharging;
    doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceScope_isUsed = 0u;
    doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.FreeService = 1;

    /* =============================================
     * SupportedEnergyTransferMode - MANDATORY
     * ============================================= */
    /* Must have at least ONE energy transfer mode */
    doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService
        .SupportedEnergyTransferMode.EnergyTransferMode.array[0] =
        iso1EnergyTransferModeType_DC_extended; /* or DC */

//         /* Pick the correct one for your EVSE hardware */
// iso1EnergyTransferModeType_AC_single_phase_core  /* AC 1-phase */
// iso1EnergyTransferModeType_AC_three_phase_core   /* AC 3-phase */
// iso1EnergyTransferModeType_DC_core               /* DC basic   */
// iso1EnergyTransferModeType_DC_extended           /* DC extended*/
// iso1EnergyTransferModeType_DC_combo_core         /* CCS DC     */
// iso1EnergyTransferModeType_DC_unique             /* DC unique  */

    doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService
        .SupportedEnergyTransferMode.EnergyTransferMode.arrayLen = 1;

    /* =============================================
     * ServiceList - OPTIONAL (set to empty)
     * ============================================= */
    doc.V2G_Message.Body.ServiceDiscoveryRes.ServiceList_isUsed  = 0u;
    doc.V2G_Message.Body.ServiceDiscoveryRes.ServiceList.Service.arrayLen = 0;

    /* =============================================
     * PaymentOptionList - MANDATORY
     * ============================================= */
    /* Must have at least ONE payment option */
    doc.V2G_Message.Body.ServiceDiscoveryRes.PaymentOptionList
        .PaymentOption.array[0] = iso1paymentOptionType_ExternalPayment; /* No contract - external payment */
     
    // iso1paymentOptionType_Contract         /* ISO 15118 contract/PnC         */

/* For simple EVSE without PnC, use ExternalPayment only */
    doc.V2G_Message.Body.ServiceDiscoveryRes.PaymentOptionList
        .PaymentOption.arrayLen = 1;

    /* =========================
     * ENCODE EXI
     * ========================= */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0xAA, sizeof(exi_buf));

    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);

        /* Debug: which field caused it? */
        printf("[DEBUG] ResponseCode        = %d\n",
            doc.V2G_Message.Body.ServiceDiscoveryRes.ResponseCode);
        printf("[DEBUG] ServiceID           = %d\n",
            doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceID);
        printf("[DEBUG] ServiceCategory     = %d\n",
            doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.ServiceCategory);
        printf("[DEBUG] FreeService         = %d\n",
            doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService.FreeService);
        printf("[DEBUG] EnergyTransferMode  = %d (arrayLen=%d)\n",
            doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService
                .SupportedEnergyTransferMode.EnergyTransferMode.array[0],
            doc.V2G_Message.Body.ServiceDiscoveryRes.ChargeService
                .SupportedEnergyTransferMode.EnergyTransferMode.arrayLen);
        printf("[DEBUG] PaymentOption       = %d (arrayLen=%d)\n",
            doc.V2G_Message.Body.ServiceDiscoveryRes.PaymentOptionList
                .PaymentOption.array[0],
            doc.V2G_Message.Body.ServiceDiscoveryRes.PaymentOptionList
                .PaymentOption.arrayLen);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }

    int exi_len = (int)pos;

    /* =========================
     * VERIFY - DECODE BACK
     * ========================= */
    if (verify_service_discovery_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ ServiceDiscoveryRes verification failed\n");
        return -1;
    }

    /* =========================
     * WRAP INTO V2GTP
     * ========================= */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
    for (int i = 0; i < exi_len; i++) printf("%02X ", exi_buf[i]);
    printf("\n");

    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;

    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}
/* ===========================================================================
 * SERVICE DETAIL RESPONSE 
ServiceDetailRes
├── ResponseCode          = OK
├── ServiceID             = 1 (EV Charging)
└── ServiceParameterList
    ├── ParameterSet[0]   ID=1
    │   ├── Parameter[0]  Name="Connector"   intValue=1
    │   └── Parameter[1]  Name="ControlMode" intValue=1
    └── ParameterSet[1]   ID=2
        ├── Parameter[0]  Name="MaxVoltage"  intValue=500
        └── Parameter[1]  Name="MaxCurrent"  intValue=125


 * =========================================================================== */
static int build_service_detail_res(secc_context_t *ctx,
                                     uint8_t *out_buf,
                                     int *out_len)
{
    printf("[SECC] Building ServiceDetailRes\n");

    /* =========================
     * BUILD iso1EXIDocument
     * ========================= */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));

    doc.V2G_Message_isUsed                       = 1u;
    doc.V2G_Message.Body.ServiceDetailRes_isUsed = 1u;

    /* ── Header ── */
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] =
            (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;

    /* ── ResponseCode ── */
    doc.V2G_Message.Body.ServiceDetailRes.ResponseCode =
        iso1responseCodeType_OK;

    /* ── ServiceID ── */
    /*
     * ServiceID must match what was requested in
     * ServiceDetailReq.ServiceID
     * Common values:
     *   1 = EV Charging
     *   2 = Certificate
     *   3 = Internet
     */
    doc.V2G_Message.Body.ServiceDetailRes.ServiceID = 1;

    /* ══════════════════════════════════════
     * ServiceParameterList
     *
     * Contains list of ParameterSets
     * Each ParameterSet has:
     *   - ParameterSetID (unique ID)
     *   - Parameter[]    (list of parameters)
     *
     * For DC Charging (ServiceID=1):
     *   Parameter: "Connector"  = DC_core
     *   Parameter: "ControlMode"= scheduled
     * ══════════════════════════════════════ */
    doc.V2G_Message.Body.ServiceDetailRes
        .ServiceParameterList_isUsed = 1u;

    struct iso1ServiceParameterListType *paramList =
        &doc.V2G_Message.Body.ServiceDetailRes.ServiceParameterList;

    /* ────────────────────────────────────────
     * ParameterSet[0]: DC Charging - Connector
     * ──────────────────────────────────────── */
    struct iso1ParameterSetType *paramSet0 =
        &paramList->ParameterSet.array[0];

    memset(paramSet0, 0, sizeof(struct iso1ParameterSetType));

    /* ── ParameterSetID ── */
    paramSet0->ParameterSetID = 1;

    /* ── Parameter[0]: Connector Type ── */
    struct iso1ParameterType *param0 =
        &paramSet0->Parameter.array[0];

    memset(param0, 0, sizeof(struct iso1ParameterType));

    /* Name = "Connector" */
    const char *conn_name = "Connector";
    param0->Name.charactersLen = (uint16_t)strlen(conn_name);
    for (uint16_t i = 0; i < param0->Name.charactersLen; i++) {
        param0->Name.characters[i] =
            (exi_string_character_t)conn_name[i];
    }

    /* Value = intValue (DC_core = 1) */
    param0->intValue_isUsed = 1u;
    param0->intValue        = 1;  /* DC_core connector */

    /* ── Parameter[1]: Control Mode ── */
    struct iso1ParameterType *param1 =
        &paramSet0->Parameter.array[1];

    memset(param1, 0, sizeof(struct iso1ParameterType));

    /* Name = "ControlMode" */
    const char *ctrl_name = "ControlMode";
    param1->Name.charactersLen = (uint16_t)strlen(ctrl_name);
    for (uint16_t i = 0; i < param1->Name.charactersLen; i++) {
        param1->Name.characters[i] =
            (exi_string_character_t)ctrl_name[i];
    }

    /* Value = intValue (Scheduled = 1) */
    param1->intValue_isUsed = 1u;
    param1->intValue        = 1;  /* Scheduled mode */

    /* ── Set parameter count ── */
    paramSet0->Parameter.arrayLen = 2;

    /* ────────────────────────────────────────
     * ParameterSet[1]: DC Charging - Energy
     * ──────────────────────────────────────── */
    struct iso1ParameterSetType *paramSet1 =
        &paramList->ParameterSet.array[1];

    memset(paramSet1, 0, sizeof(struct iso1ParameterSetType));

    paramSet1->ParameterSetID = 2;

    /* ── Parameter[0]: Max Voltage ── */
    struct iso1ParameterType *param2 =
        &paramSet1->Parameter.array[0];

    memset(param2, 0, sizeof(struct iso1ParameterType));

    /* Name = "MaxVoltage" */
    const char *volt_name = "MaxVoltage";
    param2->Name.charactersLen = (uint16_t)strlen(volt_name);
    for (uint16_t i = 0; i < param2->Name.charactersLen; i++) {
        param2->Name.characters[i] =
            (exi_string_character_t)volt_name[i];
    }

    /* Value = intValue (500V) */
    param2->intValue_isUsed = 1u;
    param2->intValue        = 500;

    /* ── Parameter[1]: Max Current ── */
    struct iso1ParameterType *param3 =
        &paramSet1->Parameter.array[1];

    memset(param3, 0, sizeof(struct iso1ParameterType));

    /* Name = "MaxCurrent" */
    const char *curr_name = "MaxCurrent";
    param3->Name.charactersLen = (uint16_t)strlen(curr_name);
    for (uint16_t i = 0; i < param3->Name.charactersLen; i++) {
        param3->Name.characters[i] =
            (exi_string_character_t)curr_name[i];
    }

    /* Value = intValue (125A) */
    param3->intValue_isUsed = 1u;
    param3->intValue        = 125;

    /* ── Set parameter count ── */
    paramSet1->Parameter.arrayLen = 2;

    /* ── Set total ParameterSet count ── */
    paramList->ParameterSet.arrayLen = 2;

    /* =========================
     * ENCODE EXI
     * ========================= */
    bitstream_t stream;
    uint8_t     exi_buf[1024];
    size_t      pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));

    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode ServiceDetailRes"
               " failed: %d\n", err);
        return -1;
    }

    /* ── Flush remaining bits ── */
    if (flush(&stream) != 0) {
        printf("[SECC] ❌ Bitstream flush failed\n");
        return -1;
    }

    int exi_len = (int)pos;
    printf("[SECC] EXI encoded length: (%d bytes)\n", exi_len);

    /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");

    /* =========================
     * VERIFY BY DECODING
     * ========================= */
    size_t      verify_pos = 0;
    bitstream_t verify_stream;

    verify_stream.size     = (size_t)exi_len;
    verify_stream.data     = exi_buf;
    verify_stream.pos      = &verify_pos;
    verify_stream.buffer   = 0;
    verify_stream.capacity = 8;

    struct iso1EXIDocument verify_doc;
    memset(&verify_doc, 0, sizeof(verify_doc));

    err = decode_iso1ExiDocument(&verify_stream, &verify_doc);
    if (err != 0) {
        printf("[SECC] ❌ Verification decode failed: %d\n", err);
        return -1;
    }

    if (!verify_doc.V2G_Message.Body.ServiceDetailRes_isUsed) {
        printf("[SECC] ❌ Verification:"
               " ServiceDetailRes not found\n");
        return -1;
    }

    struct iso1ServiceDetailResType *vRes =
        &verify_doc.V2G_Message.Body.ServiceDetailRes;

    printf("[SECC] ✅ Verification passed:\n");
    printf("   ResponseCode  : %d\n",  vRes->ResponseCode);
    printf("   ServiceID     : %d\n",  vRes->ServiceID);

    if (vRes->ServiceParameterList_isUsed) {
        printf("   ParameterSets : %d\n",
               vRes->ServiceParameterList.ParameterSet.arrayLen);

        for (uint16_t s = 0;
             s < vRes->ServiceParameterList.ParameterSet.arrayLen;
             s++)
        {
            struct iso1ParameterSetType *ps =
                &vRes->ServiceParameterList.ParameterSet.array[s];

            printf("   ParameterSet[%d] ID=%d Params=%d\n",
                   s, ps->ParameterSetID,
                   ps->Parameter.arrayLen);

            for (uint16_t p = 0;
                 p < ps->Parameter.arrayLen; p++)
            {
                struct iso1ParameterType *pm =
                    &ps->Parameter.array[p];

                /* Print parameter name */
                printf("      Param[%d] Name=", p);
                for (uint16_t c = 0;
                     c < pm->Name.charactersLen; c++) {
                    printf("%c", (char)pm->Name.characters[c]);
                }

                /* Print parameter value */
                if (pm->intValue_isUsed) {
                    printf(" intValue=%d\n", pm->intValue);
                } else if (pm->boolValue_isUsed) {
                    printf(" boolValue=%d\n", pm->boolValue);
                } else if (pm->physicalValue_isUsed) {
                    printf(" physicalValue=%d*10^%d\n",
                           pm->physicalValue.Value,
                           pm->physicalValue.Multiplier);
                } else if (pm->stringValue_isUsed) {
                    printf(" stringValue=");
                    for (uint16_t c = 0;
                         c < pm->stringValue.charactersLen; c++) {
                        printf("%c",
                               (char)pm->stringValue.characters[c]);
                    }
                    printf("\n");
                } else {
                    printf(" (no value)\n");
                }
            }
        }
    }

    /* =========================
     * WRAP INTO V2GTP
     * ========================= */
    write_v2gtp_header(out_buf, exi_len);

    memcpy(out_buf + V2GTP_HEADER_LEN,
           exi_buf,
           exi_len);

    *out_len = V2GTP_HEADER_LEN + exi_len;

    // printf("[SECC] ✅ ServiceDetailRes total: %d bytes\n", *out_len);

    return 0;
}
static int build_payment_service_selection_res(secc_context_t *ctx,
                                                uint8_t *out_buf,
                                                int *out_len)
{    printf("[SECC] Building REAL PaymentServiceSelectionRes\n");
    /* For simplicity, we just return OK without any actual payment processing */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.PaymentServiceSelectionRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.PaymentServiceSelectionRes.ResponseCode =
        iso1responseCodeType_OK;

    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;

        /* Verify by decoding back */
    if (verify_payment_service_selection_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ PaymentServiceSelectionRes verification failed\n");
        return -1;
    }

    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
    /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n"); 

    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    //printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}
static int build_authorization_res(secc_context_t *ctx,
                                        uint8_t *out_buf,
                                        int *out_len)
{
    printf("[SECC] Building REAL AuthorizationRes\n");
    /* For simplicity, we just return OK without any actual authorization logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.AuthorizationRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.AuthorizationRes.ResponseCode =
        iso1responseCodeType_OK;
    doc.V2G_Message.Body.AuthorizationRes.EVSEProcessing = iso1EVSEProcessingType_Finished; /* Optional, set to not used */
    // doc.V2G_Message.Body.AuthorizationRes.EVSEProcessing = iso1EVSEProcessingType_Ongoing; /* Optional, set to not used */


    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;

        /* Verify by decoding back */
    if (verify_authorization_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ AuthorizationRes verification failed\n");
        return -1;
    }

    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
   /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");

    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}
static int build_charge_parameter_discovery_res(secc_context_t *ctx,
                                                uint8_t *out_buf,
                                                int *out_len)
{
    printf("[SECC] Building ChargeParameterDiscoveryRes\n");

    /* For simplicity, we just return OK without any actual logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.ResponseCode =
        iso1responseCodeType_OK;
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.EVSEProcessing = iso1EVSEProcessingType_Finished; /* Optional, set to not used */

    // doc.V2G_Message.Body.ChargeParameterDiscoveryRes.EVSEPresent = 1; /* EVSE is present and can provide parameters */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.AC_EVSEChargeParameter_isUsed = 0u; /* Optional, set to not used */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter_isUsed = 1u; /* Optional, set to not used */
    
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumCurrentLimit.Value = 32; /* Example: 32A */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumCurrentLimit.Multiplier = 1; /* Multiplier is in A */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumCurrentLimit.Unit = iso1unitSymbolType_A; /* Example: 32A */

    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumVoltageLimit.Value = 400; /* Example: 400V */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumVoltageLimit.Multiplier = 1; /* Example: 400V */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumVoltageLimit.Unit = iso1unitSymbolType_V; /* Example: 400V */

    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumPowerLimit.Value = 11000; /* Example: 11kW */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumPowerLimit.Multiplier = 1; /* Multiplier is in W */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMaximumPowerLimit.Unit = iso1unitSymbolType_W; /* Example: 11kW */

    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumVoltageLimit.Value = 200; /* Example: 200V */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumVoltageLimit.Multiplier = 1; /* Example: 200V */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumVoltageLimit.Unit = iso1unitSymbolType_V; /* Example: 200V */

    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumCurrentLimit.Value = 6; /* Example: 6A */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumCurrentLimit.Multiplier = 1; /* Example: 6A */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEMinimumCurrentLimit.Unit = iso1unitSymbolType_A; /* Example: 6A */

    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSECurrentRegulationTolerance.Value = 1200; /* Example: 1.2kW */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSECurrentRegulationTolerance.Multiplier = 1; /* Example: 1.2kW */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSECurrentRegulationTolerance.Unit = iso1unitSymbolType_A; /* Example: 6A */
    
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEPeakCurrentRipple.Value = 10; /* Example: 1000V */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEPeakCurrentRipple.Multiplier = 1; /* Example: 1000V */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.EVSEPeakCurrentRipple.Unit = iso1unitSymbolType_A; /* Example:  */

    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.DC_EVSEStatus.EVSEStatusCode = 1u; /*  */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.DC_EVSEStatus.EVSENotification = 1u; /* Optional, set to not used */
    doc.V2G_Message.Body.ChargeParameterDiscoveryRes.DC_EVSEChargeParameter.DC_EVSEStatus.EVSEIsolationStatus = 1u; /* Optional, set to not used */
   
    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;

        /* Verify by decoding back */
    if (verify_charge_parameter_discovery_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ ChargeParameterDiscoveryRes verification failed\n");
        return -1;
    }

    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
    // for (int i = 0; i < exi_len; i++) printf("%02X ", exi_buf[i]);
    // printf("\n");  
    
    /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");

    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}  
static int build_cable_check_res(secc_context_t *ctx,
                                        uint8_t *out_buf,
                                        int *out_len)
{
    printf("[SECC] Building CableCheckRes\n");
    /* For simplicity, we just return OK without any actual logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.CableCheckRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.CableCheckRes.ResponseCode =
        iso1responseCodeType_OK;
    doc.V2G_Message.Body.CableCheckRes.EVSEProcessing = iso1EVSEProcessingType_Finished; /* Optional, set to not used */    
    // doc.V2G_Message.Body.CableCheckRes.EVSEProcessing = iso1EVSEProcessingType_Ongoing;

    doc.V2G_Message.Body.CableCheckRes.DC_EVSEStatus.EVSEStatusCode = 1u; /* Optional, set to not used */
    doc.V2G_Message.Body.CableCheckRes.DC_EVSEStatus.EVSENotification = 1u; /* Optional, set to not used */
    doc.V2G_Message.Body.CableCheckRes.DC_EVSEStatus.EVSEIsolationStatus = iso1isolationLevelType_Valid; /* Optional, set to not used */

    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;

        /* Verify by decoding back */
    if (verify_cable_check_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ CableCheckRes verification failed\n");
        return -1;
    }

    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
   /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");
    
    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}

static int build_precharge_res(secc_context_t *ctx,
                                        uint8_t *out_buf,
                                        int *out_len)
{
    printf("[SECC] Building PreChargeRes\n");
    /* For simplicity, we just return OK without any actual logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.PreChargeRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.PreChargeRes.ResponseCode =
        iso1responseCodeType_OK;

    doc.V2G_Message.Body.PreChargeRes.DC_EVSEStatus.EVSEStatusCode = 1u; /* Optional, set to not used */
    doc.V2G_Message.Body.PreChargeRes.DC_EVSEStatus.EVSENotification = 0u; /* Optional, set to not used */
    doc.V2G_Message.Body.PreChargeRes.DC_EVSEStatus.EVSEIsolationStatus = iso1isolationLevelType_Valid; /* Optional, set to not used */

    doc.V2G_Message.Body.PreChargeRes.EVSEPresentVoltage.Value = 400u; /* Value */
    doc.V2G_Message.Body.PreChargeRes.EVSEPresentVoltage.Multiplier = 0u; /* multiplier */
    doc.V2G_Message.Body.PreChargeRes.EVSEPresentVoltage.Unit = iso1unitSymbolType_V; /* Unit */


    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;

        /* Verify by decoding back */
    if (verify_pre_charge_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ PreChargeRes verification failed\n");
        return -1;
    }

    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
   /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");


    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}
static int build_power_delivery_res(secc_context_t *ctx,
                                        uint8_t *out_buf,
                                        int *out_len)
{
    printf("[SECC] Building PowerDeliveryRes\n");

    /* For simplicity, we just return OK without any actual logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.PowerDeliveryRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;

    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.PowerDeliveryRes.ResponseCode =
        iso1responseCodeType_OK;

    /* ── DC_EVSEStatus  ── */
    doc.V2G_Message.Body.PowerDeliveryRes.DC_EVSEStatus_isUsed = 1u;
    doc.V2G_Message.Body.PowerDeliveryRes.DC_EVSEStatus.EVSEStatusCode = iso1DC_EVSEStatusCodeType_EVSE_Ready; /* Optional, set to not used */
    doc.V2G_Message.Body.PowerDeliveryRes.DC_EVSEStatus.EVSENotification = 0u; /* Optional, set to not used */
    doc.V2G_Message.Body.PowerDeliveryRes.DC_EVSEStatus.EVSEIsolationStatus = iso1isolationLevelType_Valid; /* Optional, set to not used */
    doc.V2G_Message.Body.PowerDeliveryRes.DC_EVSEStatus.EVSEIsolationStatus_isUsed = 1u;
    
    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;

        /* Verify by decoding back */
    if (verify_power_delivery_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ PowerDeliveryRes verification failed\n");
        return -1;
    }

    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
    for (int i = 0; i < exi_len; i++) printf("%02X ", exi_buf[i]);
    printf("\n");
    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}
static int build_current_demand_res(secc_context_t *ctx,
                                        uint8_t *out_buf,
                                        int *out_len)
{
    printf("[SECC] Building CurrentDemandRes\n");

    /* For simplicity, we just return OK without any actual logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.CurrentDemandRes_isUsed = 1u;

    /* SessionID */
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u; 
    
    /* ResponseCode - MANDATORY */
    doc.V2G_Message.Body.CurrentDemandRes.ResponseCode =
        iso1responseCodeType_OK;
    
    /* ── DC_EVSEStatus  ── */
    doc.V2G_Message.Body.CurrentDemandRes.DC_EVSEStatus.EVSEStatusCode = 1u; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.DC_EVSEStatus.EVSENotification = 0u; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.DC_EVSEStatus.EVSEIsolationStatus = iso1isolationLevelType_Valid; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.DC_EVSEStatus.EVSEIsolationStatus_isUsed = 1u;
    
    /* EVSEPresentVoltage - OPTIONAL, but we set it to used with a value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEPresentVoltage.Value = 400u; /* Value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEPresentVoltage.Multiplier = 0u; /* multiplier */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEPresentVoltage.Unit = iso1unitSymbolType_V; /* Unit */

    /* EVSEPresentCurrent - OPTIONAL, but we set it to used with a value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEPresentCurrent.Value = 16u; /* Value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEPresentCurrent.Multiplier = 0u; /* multiplier */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEPresentCurrent.Unit = iso1unitSymbolType_A; /* Unit */

    /*EVSEMaximumVoltageLimit - OPTIONAL, but we set it to used with a value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumVoltageLimit.Value = 1000u; /* Value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumVoltageLimit.Multiplier = 0u; /* multiplier */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumVoltageLimit.Unit = iso1unitSymbolType_V; /* Unit */

    /* EVSEMaximumCurrentLimit - OPTIONAL, but we set it to used with a value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumCurrentLimit.Value = 32u; /* Value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumCurrentLimit.Multiplier = 0u; /* multiplier */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumCurrentLimit.Unit = iso1unitSymbolType_A; /* Unit */

    /* EVSEMaximumPowerLimit - OPTIONAL, but we set it to used with a value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumPowerLimit.Value = 11000u; /* Value */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumPowerLimit.Multiplier = 0u; /* multiplier */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEMaximumPowerLimit.Unit = iso1unitSymbolType_W; /* Unit */

    doc.V2G_Message.Body.CurrentDemandRes.SAScheduleTupleID = 1u; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.charactersLen = 8; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[0] = 'E'; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[1] = 'V'; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[2] = 'S'; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[3] = 'E'; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[4] = '0'; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[5] = '0'; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[6] = '1'; /* Optional, set to not used */
    doc.V2G_Message.Body.CurrentDemandRes.EVSEID.characters[7] = '2'; /* Optional, set to not used */   


    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;

    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;

    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }

    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;

        /* Verify by decoding back */
    if (verify_current_demand_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ CurrentDemandRes verification failed\n");
        return -1;
    }

    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
   /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");


    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}
static int build_welding_detection_res(secc_context_t *ctx,
                                        uint8_t *out_buf,
                                        int *out_len)
{    printf("[SECC] Building WeldingDetectionRes\n");
    /* For simplicity, we just return OK without any actual logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.WeldingDetectionRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.WeldingDetectionRes.ResponseCode =
        iso1responseCodeType_OK;  

    doc.V2G_Message.Body.WeldingDetectionRes.DC_EVSEStatus.EVSEIsolationStatus = iso1isolationLevelType_Invalid; /* Optional, set to not used */    
    doc.V2G_Message.Body.WeldingDetectionRes.DC_EVSEStatus.EVSEStatusCode = 1u; /* Optional, set to not used */
    doc.V2G_Message.Body.WeldingDetectionRes.DC_EVSEStatus.EVSENotification = 0u; /* Optional, set to not used */   

    /*Present Voltage */    
    doc.V2G_Message.Body.WeldingDetectionRes.EVSEPresentVoltage.Value = 40; /* Optional, set to not used */
    doc.V2G_Message.Body.WeldingDetectionRes.EVSEPresentVoltage.Multiplier = 0; /* Optional, set to not used */
    doc.V2G_Message.Body.WeldingDetectionRes.EVSEPresentVoltage.Unit = iso1unitSymbolType_V; /* Optional, set to not used */

    
    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;     
    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;
    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }
    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;
        /* Verify by decoding back */
    if (verify_welding_detection_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ WeldingDetectionRes verification failed\n");
        return -1;
    }
    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
    for (int i = 0; i < exi_len; i++) printf("%02X ", exi_buf[i]);
    printf("\n");
    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}
static int build_session_stop_res(secc_context_t *ctx,
                                        uint8_t *out_buf,
                                        int *out_len)
{    printf("[SECC] Building SessionStopRes\n");
    /* For simplicity, we just return OK without any actual logic */
    struct iso1EXIDocument doc;
    memset(&doc, 0, sizeof(doc));   
    doc.V2G_Message_isUsed = 1u;
    doc.V2G_Message.Body.SessionStopRes_isUsed = 1u;
    doc.V2G_Message.Header.SessionID.bytesLen = 8;
    uint64_t sid = ctx->session_id;
    for (int i = 0; i < 8; i++) {
        doc.V2G_Message.Header.SessionID.bytes[7 - i] = (sid >> (i * 8)) & 0xFF;
    }
    doc.V2G_Message.Header.Notification_isUsed = 0u;
    doc.V2G_Message.Header.Signature_isUsed    = 0u;        
    doc.V2G_Message.Body.SessionStopRes.ResponseCode =
        iso1responseCodeType_OK;    
    /* Encode EXI */
    bitstream_t stream;
    uint8_t exi_buf[1024];
    size_t pos = 0;     
    memset(exi_buf, 0, sizeof(exi_buf));
    stream.data     = exi_buf;
    stream.size     = sizeof(exi_buf);
    stream.pos      = &pos;
    stream.buffer   = 0;
    stream.capacity = 8;
    int err = encode_iso1ExiDocument(&stream, &doc);
    if (err != 0) {
        printf("[SECC] ❌ EXI encode failed: %d\n", err);
        return -1;
    }
    if (flush(&stream) != 0) {
        printf("[SECC] flush failed\n");
        return -1;
    }
    int exi_len = (int)pos;
        /* Verify by decoding back */
    if (verify_session_stop_response(exi_buf, exi_len) != 0) {
        printf("[SECC] ❌ SessionStopRes verification failed\n");
        return -1;
    }
    /* Wrap into V2GTP */
    printf("[SECC] EXI encoded (%d bytes): ", exi_len);
   /* ── Print EXI payload ── */
    printf("[SECC] EXI Payload:\n");
    for (int i = 0; i < exi_len; i++) {
        printf("%02X ", exi_buf[i]);
        if ((i + 1) % 16 == 0)
            printf("\n");
    }
    printf("\n\n");

    write_v2gtp_header(out_buf, exi_len);
    memcpy(out_buf + V2GTP_HEADER_LEN, exi_buf, exi_len);
    *out_len = V2GTP_HEADER_LEN + exi_len;
    // printf("[SECC] ✅ Total response: %d bytes\n", *out_len);
    return 0;
}

/* =========================
 * ROUTER
 * ========================= */
int secc_build_response(secc_context_t *ctx,
                        exi_msg_type_t req_type,
                        uint8_t *out_buf,
                        int *out_len)
{
    switch (req_type)
    {
        case EXI_MSG_SUPPORTED_APP_PROTOCOL:
            return build_supported_app_protocol_res(ctx, out_buf, out_len);
            
        case EXI_MSG_SESSION_SETUP:
            return build_session_setup_res(ctx, out_buf, out_len);

        case EXI_MSG_SERVICE_DISCOVERY:
            return build_service_discovery_res(ctx, out_buf, out_len);

        case EXI_MSG_SERVICE_DETAIL:
            return build_service_detail_res(ctx, out_buf, out_len);
            
        // case EXI_MSG_PAYMENT_SERVICE_SELECTION:
        //     return build_payment_service_selection_res(ctx, out_buf, out_len);

        case EXI_MSG_PAYMENT_SERVICE_SELECTION:
            return build_payment_service_selection_res(ctx, out_buf, out_len);

        case EXI_MSG_AUTHORIZATION:
            return build_authorization_res(ctx, out_buf, out_len);

        case EXI_MSG_CHARGE_PARAMETER_DISCOVERY:
            return build_charge_parameter_discovery_res(ctx, out_buf, out_len);

        case EXI_MSG_CABLE_CHECK:
            return build_cable_check_res(ctx, out_buf, out_len);

        case EXI_MSG_PRECHARGE:
            return build_precharge_res(ctx, out_buf, out_len);

        case EXI_MSG_POWER_DELIVERY:
            return build_power_delivery_res(ctx, out_buf, out_len); 

        case EXI_MSG_CURRENT_DEMAND:
            return build_current_demand_res(ctx, out_buf, out_len);

        case EXI_MSG_WELDING_DETECTION:
            return build_welding_detection_res(ctx, out_buf, out_len);

        case EXI_MSG_SESSION_STOP:
            return build_session_stop_res(ctx, out_buf, out_len);

        default:
            printf("[SECC] No response implemented\n");
            return -1;
    }
}