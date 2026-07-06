# Detailed Changes Documentation

## 1. Core Context (`core/secc_context.h`)

### Added
```c
/* Transport mode enum */
typedef enum {
    TRANSPORT_MODE_TLS = 0,
    TRANSPORT_MODE_NON_TLS = 1
} transport_mode_t;

/* New field in secc_context_t struct */
transport_mode_t transport_mode;
```

**Purpose**: Track which transport mode (TLS or NonTLS) is active for this session

---

## 2. Session Initialization (`core/secc_session.c`)

### Added Include
```c
#include "v2gtp_sdp_handler.h"
#include <stdlib.h>
```

### Modified `secc_session_init()`
**Before**: Only initialized basic fields
**After**: Also initializes `transport_mode` to default (TLS)

### Completely Rewritten `secc_session_start()`
**Before**: Direct TLS acceptance
**After**:
1. Accept connection in NonTLS mode (for SDP)
2. Read SDP message from EV
3. Parse SDP request
4. Negotiate transport (TLS preferred)
5. Update context with mode
6. Perform TLS handshake if TLS selected
7. Return success with proper state

**New Function Signature**: Same but different flow

---

## 3. Transport Layer (`transport/tls_transport.c`)

### System Headers Added
```c
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
```

### New Static Variables
```c
static int plain_tcp_fd = -1;           /* For non-TLS mode */
static FILE *keylog_file = NULL;        /* SSLKEYLOGFILE */
static int use_tls = 1;                 /* Current mode flag */
```

### New Keylog Callback
```c
#ifdef MBEDTLS_SSL_EXPORT_KEYS_EXT_CB
static void keylog_callback(...)  /* Exports master secret to file */
#endif
```

### New Function: `setup_keylog()`
- Reads `SSLKEYLOGFILE` environment variable
- Opens keylog file in append mode
- Sets up callback for key export

### New Function: `tls_server_init_and_accept_ex(int use_tls_mode)`
- Core negotiation function
- Accepts in either TLS or NonTLS mode
- Handles both cert loading and plain socket creation
- Backward compatible wrapper added

### Modified `tls_read()`, `tls_write()`, `tls_close()`
- All check `use_tls` flag
- Branch to appropriate implementation
- TLS uses mbedTLS functions
- NonTLS uses send/recv syscalls

---

## 4. Transport Header (`transport/tls_transport.h`)

### Added Function Declaration
```c
int tls_server_init_and_accept_ex(int use_tls_mode);
```

### Updated Comments
- Clarified that read/write handle both modes
- Noted keylogging support

---

## 5. SDP Handler (NEW FILES)

### `v2g/v2gtp_sdp_handler.h`
```c
/* Enums for SDP message types */
typedef enum {
    SDP_REQUEST = 0x01,
    SDP_RESPONSE = 0x02
} sdp_msg_type_t;

/* Transport protocol selection */
typedef enum {
    TRANSPORT_TLS = 0x00,
    TRANSPORT_TCP = 0x01
} sdp_transport_t;

/* SDP request structure */
typedef struct {
    uint8_t msg_type;
    uint32_t timestamp;
    uint8_t supported_transports;  /* Bitmask */
} sdp_request_t;

/* Public functions */
int sdp_parse_request(const uint8_t *data, int len, sdp_request_t *req);
int sdp_select_transport(const sdp_request_t *req);
```

### `v2g/v2gtp_sdp_handler.c`
- Parses V2GTP header (8 bytes)
- Extracts SDP payload
- Decodes message type, timestamp, transport bitmap
- Implements selection logic (TLS preferred)
- Returns transport mode decision

---

## 6. Main Application (`app/main.c`)

### Added Features
```c
/* Command-line help text */
- Feature list (TLS/NonTLS, Keylogging)
- Environment variable documentation
- Usage examples with SSLKEYLOGFILE

/* Better logging */
- Session initialization status
- Session closure status
- Error messages
```

### Structure
- Enhanced banner with features
- Environment variable hints
- Example command with SSLKEYLOGFILE
- Clear success/failure logging

---

## Feature Matrix

| Feature | Location | Status |
|---------|----------|--------|
| SDP Parsing | `v2gtp_sdp_handler.c` | ✅ New |
| Mode Detection | `secc_session.c` | ✅ Updated |
| TLS Mode | `tls_transport.c` | ✅ Enhanced |
| NonTLS Mode | `tls_transport.c` | ✅ New |
| Keylogging | `tls_transport.c` | ✅ New |
| Context Tracking | `secc_context.h` | ✅ Updated |
| Transport Selection | `secc_session.c` | ✅ Updated |

---

## State Machine Enhancements

### New States Usage
- `TCP_CONNECTED` - After plain TCP accept
- `TLS_HANDSHAKE` - Only if TLS mode (skipped for NonTLS)
- `SESSION_SETUP` - After mode decision

### Conditional State Paths

**TLS Path**:
```
TCP_CONNECTED → TLS_HANDSHAKE → SESSION_SETUP → ...
```

**NonTLS Path**:
```
TCP_CONNECTED → SESSION_SETUP → ...
(TLS_HANDSHAKE skipped)
```

---

## Backward Compatibility

✅ **Maintained**:
- `tls_server_init_and_accept()` still works (defaults to TLS)
- All existing functions preserve signatures
- New functionality is optional (defaults to TLS if no SDP)
- Existing code can still compile without SDP handler

---

## Configuration

### Environment Variables
```bash
SSLKEYLOGFILE=/path/to/keylog   # Enable TLS keylogging
```

### Runtime Behavior
- First message must be SDP (for mode detection)
- Falls back to NonTLS if SDP invalid
- TLS mode requires cert.pem + key.pem
- NonTLS mode requires no certificates

---

## Error Handling

### New Validation
- SDP payload type checking
- Transport bitmap validation
- Keylog file open failures (warning, not error)
- Mode decision failures (error, connection closed)

### Fallback Behavior
- Invalid SDP → NonTLS mode
- No common transport → Error
- Keylog file unavailable → Continue without logging

