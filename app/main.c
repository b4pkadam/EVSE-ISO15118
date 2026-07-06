#include <stdio.h>
#include "secc_session.h"

int main(int argc, char *argv[])
{
    // printf("\n");
    // printf("==========================================\n");
    // printf("  SECC Application (ISO 15118-2)\n");
    // printf("==========================================\n");
    // printf("\n");
    // printf("Features:\n");
    // printf("  [TLS/NonTLS] Selective mode based on SDP\n");
    // printf("  [Keylogging] SSLKEYLOGFILE support\n");
    // printf("\n");
    // printf("Environment Variables:\n");
    // printf("  SSLKEYLOGFILE  - Path to save TLS key log\n");
    // printf("                   (use with Wireshark to decrypt TLS)\n");
    // printf("\n");
    // printf("Usage:\n");
    // printf("  SSLKEYLOGFILE=/tmp/keys.log ./secc_app\n");
    // printf("\n");
    // printf("==========================================\n\n");

    secc_session_t session;

    secc_session_init(&session);
    
    if (secc_session_start(&session) != 0) {
        printf("[ERROR] Failed to start session\n");
        return -1;
    }
    
    printf("[INFO] Session started successfully\n");
    secc_session_run(&session);
    secc_session_stop(&session);

    printf("[INFO] Session closed\n");
    return 0;
}

/* ISO15118-2 DC EIM
EVCC                                        SECC
  │                                           │
  │──── SupportedAppProtocolReq ─────────────►│
  │◄─── SupportedAppProtocolRes ──────────────│
  │                                           │
  │──── SessionSetupReq ──────────────────────►│
  │◄─── SessionSetupRes ───────────────────────│  save session_id
  │                                           │
  │──── ServiceDiscoveryReq ──────────────────►│
  │◄─── ServiceDiscoveryRes ───────────────────│
  │                                           │
  │──── PaymentServiceSelectionReq ───────────►│
  │◄─── PaymentServiceSelectionRes ────────────│
  │                                           │
  │──── AuthorizationReq ─────────────────────►│
  │◄─── AuthorizationRes ──────────────────────│  (retry if Ongoing)
  │                                           │
  │──── ChargeParameterDiscoveryReq ──────────►│
  │◄─── ChargeParameterDiscoveryRes ───────────│  (retry if Ongoing)
  │                                           │
  │──── CableCheckReq ────────────────────────►│
  │◄─── CableCheckRes ─────────────────────────│  (retry if Ongoing)
  │                                           │
  │──── PreChargeReq ─────────────────────────►│
  │◄─── PreChargeRes ──────────────────────────│  (retry if Ongoing)
  │                                           │
  │──── PowerDeliveryReq (Start) ─────────────►│
  │◄─── PowerDeliveryRes ──────────────────────│
  │                                           │
  │──── CurrentDemandReq ─────────────────────►│  ┐
  │◄─── CurrentDemandRes ──────────────────────│  │ loop until
  │──── CurrentDemandReq ─────────────────────►│  │ charging
  │◄─── CurrentDemandRes ──────────────────────│  │ complete
  │         ...                               │  ┘
  │                                           │
  │──── PowerDeliveryReq (Stop) ──────────────►│
  │◄─── PowerDeliveryRes ──────────────────────│
  │                                           │
  │──── WeldingDetectionReq ──────────────────►│
  │◄─── WeldingDetectionRes ───────────────────│  (retry if Ongoing)
  │                                           │
  │──── SessionStopReq ───────────────────────►│
  │◄─── SessionStopRes ────────────────────────│
  │                                           │
  ✅ Session Complete*/
