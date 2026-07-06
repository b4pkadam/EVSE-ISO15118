# SLAC API Reference

## Header Files

### v2g/slac.h
Complete SLAC protocol definitions and public API.

## Public API Functions

### Initialization & Lifecycle

#### `int slac_init(SlacContext *ctx, const char *iface, bool is_ev)`
Initialize SLAC context and open raw socket.

**Parameters:**
- `ctx`: Pointer to uninitialized SlacContext
- `iface`: Interface name (e.g., "eth0")
- `is_ev`: true for EV role, false for EVSE role

**Returns:**
- `0`: Success
- `-1`: Error (check errno)

**Example:**
```c
SlacContext ctx;
if (slac_init(&ctx, "eth0", true) < 0) {
    perror("slac_init");
    return -1;
}
```

---

#### `int slac_run(SlacContext *ctx)`
Run complete SLAC handshake sequence (blocking).

Automatically processes all state transitions until success or failure.

**Parameters:**
- `ctx`: Initialized SlacContext

**Returns:**
- `0`: SLAC matching successful (NMK/NID available)
- `-1`: SLAC matching failed

**Time Complexity:** O(12-14 seconds)

**Example:**
```c
if (slac_run(&ctx) < 0) {
    fprintf(stderr, "SLAC matching failed\n");
    slac_cleanup(&ctx);
    return -1;
}

printf("SLAC Success!\n");
printf("NMK: ");
for (int i = 0; i < 16; i++) printf("%02X ", ctx.nmk[i]);
printf("\n");
```

---

#### `int slac_step(SlacContext *ctx)`
Process one SLAC state machine step (non-blocking).

Call in a loop with timing/sleep for non-blocking operation.

**Parameters:**
- `ctx`: Initialized SlacContext

**Returns:**
- `0`: Processing in progress
- `1`: SLAC matching successful (NMK/NID available)
- `-1`: SLAC matching failed

**Example:**
```c
while (1) {
    int result = slac_step(&ctx);
    
    if (result == 1) {
        printf("SLAC matched!\n");
        break;
    } else if (result == -1) {
        printf("SLAC failed\n");
        break;
    }
    
    usleep(100000);  // 100ms polling
}
```

---

#### `void slac_cleanup(SlacContext *ctx)`
Close raw socket and cleanup resources.

**Parameters:**
- `ctx`: Initialized SlacContext

**Example:**
```c
slac_cleanup(&ctx);
// ctx is now invalid, don't reuse
```

---

#### `const char *slac_state_to_string(SlacState state)`
Convert SLAC state enum to readable string.

**Parameters:**
- `state`: SlacState enum value

**Returns:**
- String representation (e.g., "EV_SEND_PARM_REQ")

**Example:**
```c
printf("Current state: %s\n", slac_state_to_string(ctx.state));
```

---

## Data Structures

### SlacContext
Main SLAC session context.

```c
typedef struct {
    int      sock_fd;           /* Raw packet socket */
    int      iface_idx;         /* Interface index */
    uint8_t  local_mac[6];      /* Local MAC address */
    uint8_t  peer_mac[6];       /* Peer MAC address */
    uint8_t  run_id[8];         /* Run ID (random) */
    uint8_t  nmk[16];           /* Network Membership Key */
    uint8_t  nid[7];            /* Network Identifier */
    uint8_t  aag[NUM_GROUPS];   /* Attenuation groups (58) */
    uint8_t  sound_count;       /* M-SOUND count */
    SlacState state;            /* Current state */
    bool     is_ev;             /* Role: true=EV, false=EVSE */
    int      retry_count;       /* Retry counter */
    uint64_t last_time;         /* Last timestamp */
} SlacContext;
```

**Fields Accessible After Success:**
- `ctx.nmk` - 16-byte Network Membership Key (for AES-CCM)
- `ctx.nid` - 7-byte Network Identifier
- `ctx.local_mac` - MAC address used in SLAC
- `ctx.peer_mac` - Matched peer MAC address

---

### Message Types

#### EthMMEHeader
Ethernet + HomePlug MME header (all messages).

```c
typedef struct __attribute__((packed)) {
    uint8_t  dst[6];            /* Destination MAC */
    uint8_t  src[6];            /* Source MAC */
    uint16_t ethertype;         /* 0x88E1 */
    uint8_t  mmv;               /* Version = 0x01 */
    uint16_t mmtype;            /* Message type (LE) */
    uint8_t  fmi_fmsn;          /* Fragmentation */
} EthMMEHeader;
```

#### CM_SLAC_PARM_REQ
Parameter request (EV → EVSE broadcast).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;  /* 0x00 */
    uint8_t  security_type;     /* 0x00 */
    uint8_t  run_id[8];         /* Random session ID */
    uint8_t  cipher_suite_set[2];
} CM_SLAC_PARM_REQ;
```

#### CM_SLAC_PARM_CNF
Parameter confirmation (EVSE → EV).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  m_sound_target[6]; /* Broadcast MAC */
    uint8_t  num_sounds;        /* 10 for GP */
    uint8_t  time_out;          /* 6 = 600ms */
    uint8_t  resp_type;         /* 0x01 */
    uint8_t  forwarding_sta[6];
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  run_id[8];
} CM_SLAC_PARM_CNF;
```

#### CM_MNBC_SOUND_IND
M-SOUND measurement burst (EV → broadcast, 10x).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  sender_id[17];     /* Sender identifier */
    uint8_t  cnt;               /* Countdown: 9→0 */
    uint8_t  run_id[8];
    uint8_t  reserved[8];
} CM_MNBC_SOUND_IND;
```

#### CM_ATTEN_CHAR_IND
Attenuation characteristics (EVSE → EV).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  source_address[6]; /* EVSE MAC */
    uint8_t  run_id[8];
    uint8_t  source_id[17];
    uint8_t  resp_id[17];
    uint8_t  num_sounds;
    uint8_t  num_groups;        /* 58 */
    uint8_t  aag[58];           /* Attenuation per group (dB) */
} CM_ATTEN_CHAR_IND;
```

#### CM_ATTEN_CHAR_RSP
Attenuation response (EV → EVSE).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint8_t  source_address[6];
    uint8_t  run_id[8];
    uint8_t  source_id[17];
    uint8_t  resp_id[17];
    uint8_t  result;            /* 0x00 = OK */
} CM_ATTEN_CHAR_RSP;
```

#### CM_VALIDATE_REQ
Validation request (EV → EVSE).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  signal_type;       /* 0x00 = PLC */
    uint8_t  timer;
    uint8_t  result;
} CM_VALIDATE_REQ;
```

#### CM_VALIDATE_CNF
Validation confirmation (EVSE → EV).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  signal_type;
    uint8_t  toggle_num;
    uint8_t  result;            /* 0x00 = OK */
} CM_VALIDATE_CNF;
```

#### CM_SLAC_MATCH_REQ
Matching request (EV → EVSE).

```c
typedef struct __attribute__((packed)) {
    EthMMEHeader hdr;
    uint8_t  application_type;
    uint8_t  security_type;
    uint16_t mv_length;         /* 0x003E */
    uint8_t  pev_id[17];
    uint8_t  pev_mac[6];
    uint8_t  evse_id[17];
    uint8_t  evse_mac[6];
    uint8_t  run_id[8];
    uint8_t  reserved[8];
} CM_SLAC_MATCH_REQ;
```

#### CM_SLAC_MATCH_CNF
Matching confirmation (EVSE → EV) - **Final message with NMK/NID**.

```c
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
    uint8_t  nmk[16];           /* ← Network Membership Key */
    uint8_t  nid[7];            /* ← Network Identifier */
} CM_SLAC_MATCH_CNF;
```

---

## Constants

### Message Types
```c
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
```

### Protocol Parameters
```c
#define HOMEPLUG_ETHERTYPE      0x88E1      /* EtherType */
#define BROADCAST_ADDR          {0xFF,...}  /* FF:FF:FF:FF:FF:FF */
#define NUM_SOUNDS              10          /* M-SOUND bursts */
#define NUM_GROUPS              58          /* OFDM groups */
#define APPLICATION_TYPE        0x00        /* Charging */
#define SECURITY_TYPE           0x00        /* No security */
```

### Timing Parameters (milliseconds)
```c
#define TT_EVSE_SLAC_INIT_MS    20000   /* EVSE startup wait */
#define TT_EV_MATCH_MNBC_MS      400    /* EV M-SOUND timeout */
#define TT_EVSE_MATCH_MNBC_MS    600    /* EVSE collection timeout */
#define TT_MATCH_JOIN_MS        12000   /* Final matching timeout */
#define C_EV_MATCH_RETRY          3     /* Retry count */
```

---

## Integration Examples

### Basic Usage (Blocking)

```c
#include "v2g/slac.h"

SlacContext ctx;
const char *plc_interface = "eth0";

// Initialize
if (slac_init(&ctx, plc_interface, true) < 0) {
    fprintf(stderr, "Failed to initialize SLAC\n");
    return -1;
}

// Run complete sequence (blocking)
if (slac_run(&ctx) < 0) {
    fprintf(stderr, "SLAC matching failed\n");
    slac_cleanup(&ctx);
    return -1;
}

// Extract keys
printf("NMK: ");
for (int i = 0; i < 16; i++) printf("%02X ", ctx.nmk[i]);
printf("\nNID: ");
for (int i = 0; i < 7; i++) printf("%02X ", ctx.nid[i]);
printf("\n");

// Cleanup
slac_cleanup(&ctx);
return 0;
```

### Non-Blocking Usage (Event Loop)

```c
#include "v2g/slac.h"

SlacContext ctx;
if (slac_init(&ctx, "eth0", true) < 0) return -1;

// In main event loop
while (keep_running) {
    int result = slac_step(&ctx);
    
    if (result == 1) {
        printf("SLAC success\n");
        // Use ctx.nmk and ctx.nid
        break;
    } else if (result == -1) {
        printf("SLAC failed\n");
        break;
    }
    
    // Poll every 100ms
    usleep(100000);
}

slac_cleanup(&ctx);
```

### SECC Integration

```c
#include "core/secc_session.h"
#include "v2g/slac_integration.c"

secc_session_t session;

// Initialize with SLAC
if (secc_session_with_slac_init(&session, "eth0") < 0) {
    return -1;
}

// Run SLAC handshake
if (secc_session_slac_run(&session) < 0) {
    return -1;
}

// Now session.slac_nmk and session.slac_nid are available
// Proceed to TCP/TLS connection
```

---

## Error Handling

### Common Errors

| Error | Cause | Solution |
|-------|-------|----------|
| `EACCES` | Permission denied (raw socket) | Run with `sudo` |
| `ENODEV` | Interface not found | Check interface exists |
| `ENETDOWN` | Network interface down | `ifconfig eth0 up` |
| Timeout | No EVSE on network | Power on EVSE, check PLC |
| Wrong EtherType | Raw socket misconfiguration | Verify socket creation |

### Logging

Enable debug output by modifying the LOG macro in slac.c:

```c
#define LOG(fmt, ...) do { \
    fprintf(stderr, "[SLAC] " fmt "\n", ##__VA_ARGS__); \
    fflush(stderr); \
} while(0)
```

---

## Performance

- **Memory**: ~4 KB per SlacContext
- **CPU**: Minimal (event-driven)
- **Latency**: 12-14 seconds for complete SLAC
- **Bandwidth**: ~1-2 KB/sec during matching

---

## Thread Safety

⚠️ **NOT thread-safe**. Use one SlacContext per thread or add mutex protection.

```c
// Example: Thread-safe wrapper
pthread_mutex_t slac_mutex = PTHREAD_MUTEX_INITIALIZER;

int thread_safe_slac_step(SlacContext *ctx) {
    pthread_mutex_lock(&slac_mutex);
    int result = slac_step(ctx);
    pthread_mutex_unlock(&slac_mutex);
    return result;
}
```

---

## Related Documentation

- [SLAC_README.md](SLAC_README.md) - Protocol overview & usage
- [SLAC_IMPLEMENTATION_SUMMARY.md](SLAC_IMPLEMENTATION_SUMMARY.md) - Implementation details
- ISO 15118-3:2015 - PLC communication standard
- HomePlug Green PHY specification
