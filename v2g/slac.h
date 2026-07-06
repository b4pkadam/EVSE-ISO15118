/*
 * SLAC - Signal Level Attenuation Characterization
 * ISO 15118-3 / HomePlug Green PHY (HPGP)
 *
 * Implements EV and EVSE roles for power line communication
 * Uses raw Ethernet PF_PACKET sockets for HomePlug AV/GP protocol
 */

#ifndef SLAC_H
#define SLAC_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* =========================================================================
 * Constants – ISO 15118-3 Table A.4 / HomePlug GP
 * ========================================================================= */

#define HOMEPLUG_ETHERTYPE      0x88E1   /* HomePlug AV/GP EtherType         */
#define BROADCAST_ADDR          {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}

/* Management Message Entry (MME) types (little-endian on wire)              */
#define MME_CM_SLAC_PARM_REQ    0x6064
#define MME_CM_SLAC_PARM_CNF    0x6065
#define MME_CM_START_ATTEN_IND  0x606A
#define MME_CM_MNBC_SOUND_IND   0x6074
#define MME_CM_ATTEN_CHAR_IND   0x606E
#define MME_CM_ATTEN_CHAR_RSP   0x606F
#define MME_CM_VALIDATE_REQ     0x6078
#define MME_CM_VALIDATE_CNF     0x6079
#define MME_CM_SLAC_MATCH_REQ   0x607C
#define MME_CM_SLAC_MATCH_CNF   0x607D

/* SLAC timing parameters (milliseconds)                                     */
#define TT_EVSE_SLAC_INIT_MS    20000   /* EVSE waits for first PARM.REQ    */
#define TT_EV_MATCH_MNBC_MS      400   /* EV sends M-SOUNDs within 400 ms  */
#define TT_EVSE_MATCH_MNBC_MS    600   /* EVSE waits for all M-SOUNDs      */
#define TT_MATCH_JOIN_MS        12000   /* Matching join timeout            */
#define C_EV_MATCH_RETRY          3    /* EV match retry count             */
#define C_SEQU_RETRY              3    /* Sequence retry count             */
#define NUM_SOUNDS                10   /* Number of M-SOUND bursts         */
#define NUM_GROUPS                58   /* OFDM carrier groups              */
#define SLAC_MSOUND_TARGET        10   /* Expected M-SOUNDs per round      */
#define APPLICATION_TYPE        0x00   /* EV-EVSE charging                 */
#define SECURITY_TYPE           0x00   /* No security                      */

/* =========================================================================
 * Data Structures
 * ========================================================================= */

/* Ethernet + HomePlug MME frame header */
typedef struct __attribute__((packed)) {
    uint8_t  dst[6];
    uint8_t  src[6];
    uint16_t ethertype;       /* 0x88E1 big-endian                          */
    uint8_t  mmv;             /* MME version = 0x01                         */
    uint16_t mmtype;          /* little-endian                              */
    uint8_t  fmi_fmsn;        /* fragmentation: 0x00 for single fragment    */
} EthMMEHeader;

/* CM_SLAC_PARM.REQ (EV → EVSE broadcast) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  run_id[8];
    uint8_t  cipher_suite_set[2];
} CM_SLAC_PARM_REQ;

/* CM_SLAC_PARM.CNF (EVSE → EV) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  m_sound_target[6];
    uint8_t  num_sounds;
    uint8_t  time_out;         /* unit = 100 ms, e.g. 6 = 600 ms           */
    uint8_t  resp_type;        /* 0x01 = other GP station                  */
    uint8_t  forwarding_sta[6];
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  run_id[8];
} CM_SLAC_PARM_CNF;

/* CM_START_ATTEN_CHAR.IND (EV → broadcast) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  num_sounds;
    uint8_t  time_out;
    uint8_t  resp_type;
    uint8_t  forwarding_sta[6];
    uint8_t  run_id[8];
} CM_START_ATTEN_IND;

/* CM_MNBC_SOUND.IND (EV → broadcast, repeated NUM_SOUNDS times) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  sender_id[17];
    uint8_t  cnt;              /* countdown: NUM_SOUNDS-1 … 0              */
    uint8_t  run_id[8];
    uint8_t  reserved[8];
} CM_MNBC_SOUND_IND;

/* CM_ATTEN_CHAR.IND (EVSE → EV) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  source_address[6];
    uint8_t  run_id[8];
    uint8_t  source_id[17];
    uint8_t  resp_id[17];
    uint8_t  num_sounds;
    uint8_t  num_groups;
    uint8_t  aag[NUM_GROUPS];  /* Average Attenuation per Group (dB)       */
} CM_ATTEN_CHAR_IND;

/* CM_ATTEN_CHAR.RSP (EV → EVSE) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  source_address[6];
    uint8_t  run_id[8];
    uint8_t  source_id[17];
    uint8_t  resp_id[17];
    uint8_t  result;           /* 0x00 = success                           */
} CM_ATTEN_CHAR_RSP;

/* CM_VALIDATE.REQ (EV → EVSE) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  signal_type;      /* 0x00 = PLC                               */
    uint8_t  timer;
    uint8_t  result;
} CM_VALIDATE_REQ;

/* CM_VALIDATE.CNF (EVSE → EV) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  signal_type;
    uint8_t  toggle_num;
    uint8_t  result;           /* 0x00 = success                           */
} CM_VALIDATE_CNF;

/* CM_SLAC_MATCH.REQ (EV → EVSE) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint16_t mv_length;        /* length of variable part = 0x003E         */
    uint8_t  pev_id[17];
    uint8_t  pev_mac[6];
    uint8_t  evse_id[17];
    uint8_t  evse_mac[6];
    uint8_t  run_id[8];
    uint8_t  reserved[8];
} CM_SLAC_MATCH_REQ;

/* CM_SLAC_MATCH.CNF (EVSE → EV) */
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint16_t mv_length;
    uint8_t  pev_id[17];
    uint8_t  pev_mac[6];
    uint8_t  evse_id[17];
    uint8_t  evse_mac[6];
    uint8_t  run_id[8];
    uint8_t  reserved[8];
    uint8_t  nmk[16];          /* Network Membership Key                   */
    uint8_t  nid[7];           /* Network Identifier                       */
} CM_SLAC_MATCH_CNF;

/* =========================================================================
 * SLAC State Machine
 * ========================================================================= */

typedef enum {
    /* EV states */
    SLAC_EV_IDLE = 0,
    SLAC_EV_SEND_PARM_REQ,
    SLAC_EV_WAIT_PARM_CNF,
    SLAC_EV_SEND_START_ATTEN,
    SLAC_EV_SEND_MNBC_SOUNDS,
    SLAC_EV_WAIT_ATTEN_CHAR,
    SLAC_EV_SEND_ATTEN_RSP,
    SLAC_EV_SEND_VALIDATE,
    SLAC_EV_WAIT_VALIDATE_CNF,
    SLAC_EV_SEND_MATCH_REQ,
    SLAC_EV_WAIT_MATCH_CNF,
    SLAC_EV_MATCHED,
    SLAC_EV_FAILED,

    /* EVSE states */
    SLAC_EVSE_IDLE = 100,
    SLAC_EVSE_WAIT_PARM_REQ,
    SLAC_EVSE_SEND_PARM_CNF,
    SLAC_EVSE_WAIT_START_ATTEN,
    SLAC_EVSE_COLLECT_SOUNDS,
    SLAC_EVSE_SEND_ATTEN_CHAR,
    SLAC_EVSE_WAIT_ATTEN_RSP,
    SLAC_EVSE_WAIT_VALIDATE_REQ,
    SLAC_EVSE_SEND_VALIDATE_CNF,
    SLAC_EVSE_WAIT_MATCH_REQ,
    SLAC_EVSE_SEND_MATCH_CNF,
    SLAC_EVSE_MATCHED,
    SLAC_EVSE_FAILED
} SlacState;

typedef struct {
    int      sock_fd;           /* Raw packet socket                        */
    int      iface_idx;         /* Interface index                          */
    uint8_t  local_mac[6];      /* Local MAC address                        */
    uint8_t  peer_mac[6];       /* Peer MAC address                         */
    uint8_t  run_id[8];         /* Run ID for session                       */
    uint8_t  nmk[16];           /* Network Membership Key                   */
    uint8_t  nid[7];            /* Network Identifier                       */
    uint8_t  aag[NUM_GROUPS];   /* Accumulated attenuation groups           */
    uint8_t  sound_count;       /* M-SOUND count                            */
    SlacState state;            /* Current SLAC state                       */
    bool     is_ev;             /* true = EV role, false = EVSE role        */
    int      retry_count;       /* Retry counter                            */
    uint64_t last_time;         /* Last timestamp for timeout detection     */
} SlacContext;

/* =========================================================================
 * Public API
 * ========================================================================= */

/**
 * Initialize SLAC context
 * @param ctx: SLAC context
 * @param iface: Interface name (e.g., "eth0")
 * @param is_ev: true for EV role, false for EVSE role
 * @return 0 on success, -1 on error
 */
int slac_init(SlacContext *ctx, const char *iface, bool is_ev);

/**
 * Run SLAC state machine
 * @param ctx: SLAC context
 * @return 0 on success, -1 on failure
 */
int slac_run(SlacContext *ctx);

/**
 * Process SLAC state machine one step
 * @param ctx: SLAC context
 * @return 0 if processing, -1 on failure, 1 on success (matched)
 */
int slac_step(SlacContext *ctx);

/**
 * Cleanup SLAC context
 * @param ctx: SLAC context
 */
void slac_cleanup(SlacContext *ctx);

/**
 * Get current SLAC state name
 * @param state: SLAC state
 * @return State name string
 */
const char *slac_state_to_string(SlacState state);

#endif /* SLAC_H */
