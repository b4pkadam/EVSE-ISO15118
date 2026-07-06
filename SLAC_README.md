# SLAC Integration with SECC Application

## Overview

**SLAC** (Signal Level Attenuation Characterization) is a power line communication (PLC) protocol defined in **ISO 15118-3** that enables vehicle charging over power lines before establishing a TCP/TLS connection.

### Protocol Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ 1. SLAC Handshake (PLC - HomePlug Green PHY)                   │
│    ├─ EV broadcasts CM_SLAC_PARM.REQ                           │
│    ├─ EVSE responds with CM_SLAC_PARM.CNF                      │
│    ├─ EV sends M-SOUNDs (10 attenuation measurements)          │
│    ├─ EVSE measures attenuation and sends CM_ATTEN_CHAR.IND   │
│    ├─ EV validates and sends CM_SLAC_MATCH.REQ                │
│    └─ EVSE confirms with NMK/NID (CM_SLAC_MATCH.CNF)         │
├─────────────────────────────────────────────────────────────────┤
│ 2. Session Established                                          │
│    ├─ NMK: Network Membership Key (16 bytes)                   │
│    ├─ NID: Network Identifier (7 bytes)                        │
│    └─ Link ready for encrypted communication                   │
├─────────────────────────────────────────────────────────────────┤
│ 3. TCP/TLS Connection (over PLC)                               │
│    └─ Standard SECC charging protocol                          │
└─────────────────────────────────────────────────────────────────┘
```

## File Structure

### New SLAC Files

```
v2g/
├── slac.h                 # SLAC protocol definitions & API
├── slac.c                 # SLAC implementation (raw socket, state machine)
└── slac_integration.c     # Integration examples with SECC
```

### Modified SECC Files

```
core/
├── secc_state.h          # Added SLAC states to state machine
└── secc_session.h        # Added SLAC context and NMK/NID to session

v2g/
└── slac.h/c              # New files
```

## Building with SLAC

### Updated Compile Command

```bash
gcc \
  app/main.c \
  core/secc_session.c core/secc_state_machine.c core/logging/secc_log.c \
  transport/tcp_transport.c transport/tls_transport.c \
  v2g/v2gtp_response_handler.c v2g/v2gtp_handler.c v2g/v2gtp_parser.c \
  v2g/v2gtp_sdp_handler.c v2g/slac.c v2g/slac_integration.c \
  exi/exi_decoder.c \
  $(find exi/OpenV2G/src/codec -name "*.c") \
  $(find exi/OpenV2G/src/iso1 -name "*.c") \
  $(find exi/OpenV2G/src/appHandshake -name "*.c") \
  -I./mbedtls/include \
  -Icore -Icore/logging -Itransport -Iv2g -Iexi \
  -Iexi/OpenV2G/src \
  -Iexi/OpenV2G/src/codec \
  -Iexi/OpenV2G/src/appHandshake \
  -L./mbedtls/build/library \
  -lmbedtls -lmbedx509 -lmbedcrypto \
  -lpthread \
  -o secc_app
```

### Running with SLAC

**Note:** SLAC requires raw socket access and works on Ethernet interfaces with PLC modems

```bash
# EV mode (requires root for raw sockets)
sudo ./secc_app ev eth0

# EVSE mode
sudo ./secc_app evse eth0
```

## SLAC Message Types

### Standard Messages

| MME Type | Direction | Purpose |
|----------|-----------|---------|
| `CM_SLAC_PARM.REQ` | EV → EVSE | Request SLAC parameters |
| `CM_SLAC_PARM.CNF` | EVSE → EV | Confirm parameters, set expectations |
| `CM_START_ATTEN.IND` | EV → EVSE | Begin attenuation measurement |
| `CM_MNBC_SOUND.IND` | EV → EVSE | Send M-SOUND burst (10x) |
| `CM_ATTEN_CHAR.IND` | EVSE → EV | Report attenuation characteristics |
| `CM_ATTEN_CHAR.RSP` | EV → EVSE | Acknowledge attenuation data |
| `CM_VALIDATE.REQ` | EV → EVSE | Request link validation |
| `CM_VALIDATE.CNF` | EVSE → EV | Confirm link is valid |
| `CM_SLAC_MATCH.REQ` | EV → EVSE | Request final matching |
| `CM_SLAC_MATCH.CNF` | EVSE → EV | Confirm matching + deliver NMK/NID |

### Frame Format

```c
/* Ethernet header + HomePlug MME header */
typedef struct {
    uint8_t  dst[6];              /* Destination MAC */
    uint8_t  src[6];              /* Source MAC */
    uint16_t ethertype;           /* 0x88E1 (HomePlug Green PHY) */
    uint8_t  mmv;                 /* MME version = 0x01 */
    uint16_t mmtype;              /* Message type (little-endian) */
    uint8_t  fmi_fmsn;            /* Fragmentation (0x00 = single) */
} EthMMEHeader;

/* Attenuation characteristics: 58 OFDM carrier groups */
typedef struct {
    EthMMEHeader hdr;
    uint8_t  application_type;    /* 0x00 = charging */
    uint8_t  security_type;       /* 0x00 = no security */
    uint8_t  source_address[6];   /* EVSE MAC */
    uint8_t  run_id[8];           /* Session identifier */
    uint8_t  source_id[17];       /* EVSE identifier */
    uint8_t  resp_id[17];         /* EV identifier */
    uint8_t  num_sounds;          /* Number of M-SOUNDs received */
    uint8_t  num_groups;          /* 58 for HomePlug GP */
    uint8_t  aag[58];             /* Average attenuation per group */
} CM_ATTEN_CHAR_IND;
```

## SLAC State Machine

### EV States
```
IDLE
  ↓
SEND_PARM_REQ → WAIT_PARM_CNF
  ↓
SEND_START_ATTEN → SEND_MNBC_SOUNDS
  ↓
WAIT_ATTEN_CHAR → SEND_ATTEN_RSP
  ↓
SEND_VALIDATE → WAIT_VALIDATE_CNF
  ↓
SEND_MATCH_REQ → WAIT_MATCH_CNF
  ↓
MATCHED (NMK/NID delivered)
```

### EVSE States
```
WAIT_PARM_REQ
  ↓
SEND_PARM_CNF → WAIT_START_ATTEN
  ↓
COLLECT_SOUNDS (up to 10 M-SOUNDs)
  ↓
SEND_ATTEN_CHAR → WAIT_ATTEN_RSP
  ↓
WAIT_VALIDATE_REQ → SEND_VALIDATE_CNF
  ↓
WAIT_MATCH_REQ → SEND_MATCH_CNF
  ↓
MATCHED (NMK/NID generated)
```

## Using SLAC in SECC Application

### 1. Initialize Session with SLAC

```c
#include "core/secc_session.h"
#include "v2g/slac.h"

secc_session_t session;
const char *plc_interface = "eth0";

// Initialize with SLAC
secc_session_with_slac_init(&session, plc_interface);
```

### 2. Run SLAC Handshake

```c
// Blocking approach
if (secc_session_slac_run(&session) < 0) {
    printf("SLAC failed\n");
    return -1;
}
printf("SLAC matched! NMK and NID established.\n");
```

Or non-blocking:

```c
// Non-blocking approach
session.state = SECC_STATE_SLAC_MATCHING;
while (session.state == SECC_STATE_SLAC_MATCHING) {
    int result = secc_session_slac_step(&session);
    if (result == 1) {
        printf("SLAC matched\n");
        break;
    } else if (result == -1) {
        printf("SLAC failed\n");
        break;
    }
    usleep(100000);  // Poll every 100ms
}
```

### 3. Access Session Keys

```c
// After successful SLAC
uint8_t *nmk = session.slac_nmk;   // 16 bytes
uint8_t *nid = session.slac_nid;   // 7 bytes

printf("NMK: ");
for (int i = 0; i < 16; i++) printf("%02X", nmk[i]);
printf("\n");

printf("NID: ");
for (int i = 0; i < 7; i++) printf("%02X", nid[i]);
printf("\n");
```

## Key Timing Parameters

```c
#define TT_EVSE_SLAC_INIT_MS    20000   /* EVSE waits for first PARM.REQ */
#define TT_EV_MATCH_MNBC_MS      400    /* EV sends M-SOUNDs within 400ms */
#define TT_EVSE_MATCH_MNBC_MS    600    /* EVSE waits for all M-SOUNDs */
#define TT_MATCH_JOIN_MS        12000   /* Matching join timeout */
#define NUM_SOUNDS               10     /* Number of M-SOUND bursts */
#define NUM_GROUPS               58     /* OFDM carrier groups */
```

## Raw Ethernet Socket Details

SLAC uses **raw PF_PACKET sockets** with HomePlug EtherType `0x88E1`:

```c
// Create raw socket
int fd = socket(PF_PACKET, SOCK_RAW, htons(0x88E1));

// Bind to interface
struct sockaddr_ll sll;
sll.sll_family = AF_PACKET;
sll.sll_protocol = htons(0x88E1);
sll.sll_ifindex = if_index;
bind(fd, (struct sockaddr *)&sll, sizeof(sll));

// Send frames with destination MAC
sendto(fd, frame, len, 0, (struct sockaddr *)&sll, sizeof(sll));
```

## Debugging SLAC

Enable detailed logging in `slac.c`:

```c
#define LOG(fmt, ...) do { \
    printf("[SLAC] " fmt "\n", ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)
```

Monitor raw packets:

```bash
# Capture HomePlug frames
sudo tcpdump -i eth0 -n 'ether proto 0x88e1' -v

# Show only MME types
sudo tcpdump -i eth0 -n 'ether proto 0x88e1' -A | grep -E 'MME|6064|6065|606A'
```

## Integration with V2G Protocol

After SLAC succeeds:

1. **Extract NMK/NID** from `session.slac_nmk` and `session.slac_nid`
2. **Establish TCP** over PLC interface (use NID for proper addressing)
3. **TLS Handshake** (with optional AES-CCM using NMK if needed)
4. **V2G Charging** (standard protocol)

## Support & Standards

- **ISO 15118-3:2015** - PLC communication for charging
- **HomePlug Green PHY** - Power line modem specification
- **IEEE 1901** - Powerline Communications Standard

## Troubleshooting

### Raw Socket Permission Error
```bash
# Solution: Run with sudo
sudo ./secc_app ev eth0
```

### No PLC Modem Found
```
Error: ioctl SIOCGIFHWADDR failed
```
Solution: Ensure `eth0` has a HomePlug Green PHY modem attached

### SLAC Timeout
```
[SLAC] Timeout waiting for PARM.CNF
```
Solutions:
- Check EVSE is powered on and listening
- Verify PLC network is functional
- Check interface with: `ifconfig eth0`

### Invalid EtherType
```
recv_frame: EtherType mismatch
```
Solution: Ensure raw socket is created with `htons(0x88E1)`

## Future Enhancements

- [ ] AES-CCM encryption using derived NMK
- [ ] Support for multi-vendor PLC modems
- [ ] Link quality monitoring
- [ ] Retry with backoff strategies
- [ ] Integration with network management protocol
