# SECC Application - Feature Implementation Summary

## ✅ Completed Features

### 1. **TLS/NonTLS Selective Mode Based on SDP**
   - **Capability**: Server dynamically selects TLS or NonTLS based on EV's SDP message
   - **SDP Detection**: 
     - Listens first in plain TCP mode
     - Parses incoming SDP request from EV
     - Extracts supported transports (TLS/TCP bitmap)
     - Negotiates preferred transport
   - **Protocol**: ISO 15118-2 Secure Device Protocol (SDP)

### 2. **TLS Keylogging Support (SSLKEYLOGFILE)**
   - **Capability**: Export TLS session keys for Wireshark decryption
   - **Implementation**: mbedTLS keylog callback with SSLKEYLOGFILE environment variable
   - **Usage**: `SSLKEYLOGFILE=/tmp/keys.log ./secc_app`
   - **Format**: Standard NSS SSLKEYLOGFILE format (compatible with Wireshark, tcpdump)

---

## 📝 Modified/Created Files

### **1. Core Context** - `core/secc_context.h`
```c
typedef enum {
    TRANSPORT_MODE_TLS = 0,
    TRANSPORT_MODE_NON_TLS = 1
} transport_mode_t;

/* Added field to track TLS vs NonTLS mode */
transport_mode_t transport_mode;
```

### **2. Transport Layer** - `transport/tls_transport.c`
**Key Changes:**
- Added socket headers for non-TLS TCP support
- Keylog file handling and SSLKEYLOGFILE support
- Dual mode functions: `tls_server_init_and_accept_ex(use_tls_mode)`
- Unified read/write/close operations for both TLS and NonTLS
- Static flags to track current mode (`use_tls`, `plain_tcp_fd`, `keylog_file`)

**Functions Added:**
- `tls_server_init_and_accept_ex(int use_tls_mode)` - Accept in either mode
- `setup_keylog()` - Initialize SSLKEYLOGFILE
- Updated `tls_read()` - Handle both TLS and plain TCP reads
- Updated `tls_write()` - Handle both TLS and plain TCP writes
- Updated `tls_close()` - Clean up resources for both modes

### **3. Transport Header** - `transport/tls_transport.h`
```c
/* New function for mode selection */
int tls_server_init_and_accept_ex(int use_tls_mode);
```

### **4. SDP Handler** - `v2g/v2gtp_sdp_handler.h` & `v2g/v2gtp_sdp_handler.c`
**New Module**: Complete SDP message parsing and transport selection

**Key Structures:**
```c
typedef struct {
    uint8_t msg_type;
    uint32_t timestamp;
    uint8_t supported_transports;  /* Bitmask: bit0=TLS, bit1=TCP */
} sdp_request_t;
```

**Functions:**
- `sdp_parse_request()` - Parse SDP from EV
- `sdp_select_transport()` - Negotiate preferred transport (TLS preferred)

### **5. Session Initialization** - `core/secc_session.c`
**Key Changes:**
- Updated `secc_session_init()` to initialize transport mode
- Rewrote `secc_session_start()` for SDP detection:
  1. Accept connection in NonTLS mode
  2. Read SDP message from EV
  3. Parse SDP and select transport
  4. Perform TLS handshake if TLS mode selected
  5. Return with proper state

### **6. Main Application** - `app/main.c`
**Enhanced with:**
- Usage information and feature list
- Environment variable documentation
- Better logging and status messages

---

## 🔧 Architecture Flow

```
┌─ EV connects (TCP) ──┐
│                      │
│ SDP Message Exchange │
│      (plain TCP)     │
│                      │
└──────────────────────┘
         │
         ├─ TLS Support? ─────────────→ YES ──→ TLS Handshake
         │                              │
         └─ NonTLS Support? ────────────┘
                      │
                      └──→ NO ──→ Plain TCP (V2G messages)
                      
         │
         └──→ ISO 15118 Message Exchange
              (SessionSetup, ServiceDiscovery, etc.)
```

---

## 🚀 Usage

### Default (TLS Mode):
```bash
./secc_app
```

### With Keylogging (TLS debugging):
```bash
SSLKEYLOGFILE=/tmp/secc_keys.log ./secc_app
```

### Import keys into Wireshark:
1. Edit → Preferences → Protocols → TLS
2. (Pre)-Master-Secret log filename: `/tmp/secc_keys.log`
3. Restart Wireshark or reload captures

---

## 🔐 State Transitions

### TLS Mode Path:
```
IDLE → TCP_CONNECTED → TLS_HANDSHAKE → SESSION_SETUP → SERVICE_DISCOVERY → ...
```

### NonTLS Mode Path:
```
IDLE → TCP_CONNECTED → SESSION_SETUP → SERVICE_DISCOVERY → ...
(skips TLS_HANDSHAKE)
```

---

## 📊 Transport Decision Logic

**SDP supported_transports bitmap:**
- Bit 0 (0x01): TLS supported
- Bit 1 (0x02): NonTLS/TCP supported

**Selection Priority:**
1. TLS if both supported (preferred for security)
2. NonTLS if only TCP supported
3. Error if no common transport

---

## ✨ Key Features Summary

| Feature | Status | Implementation |
|---------|--------|-----------------|
| SDP Detection | ✅ | Parses EV SDP request |
| TLS Mode | ✅ | Full mbedTLS with handshake |
| NonTLS Mode | ✅ | Plain TCP socket I/O |
| Mode Selection | ✅ | Automatic negotiation |
| SSLKEYLOGFILE | ✅ | mbedTLS keylog export |
| Wireshark Support | ✅ | NSS format keys |
| ISO 15118-2 Response | ✅ | EXI encoding/wrapping |
| Message Routing | ✅ | Dynamic dispatch |

---

## 🧪 Testing Checklist

- [ ] Test TLS mode with valid cert/key
- [ ] Test NonTLS mode (plain TCP)
- [ ] Test SDP parsing with both transport options
- [ ] Verify keylog file generation (`SSLKEYLOGFILE`)
- [ ] Verify keylog compatibility with Wireshark
- [ ] Test ISO 15118-2 message responses
- [ ] Test session state transitions
- [ ] Test error handling for unsupported transports

---

## 📝 Notes

1. **SDP is mandatory first message** - Server awaits SDP before selecting transport
2. **TLS prefers security** - If EV supports both, TLS is selected
3. **Keylogging is optional** - Only active if SSLKEYLOGFILE environment variable is set
4. **Session tracking** - Transport mode is stored in context for consistency
5. **All transport I/O unified** - Read/write functions handle both modes transparently

---

## 🔄 Session Lifecycle

1. **Accept Connection** → Plain TCP
2. **Read SDP Message** → Parse requirements
3. **Negotiate Transport** → Select TLS or NonTLS
4. **Optional Handshake** → TLS only
5. **V2G Message Loop** → ISO 15118-2 protocol
6. **Session Terminate** → Clean cleanup

