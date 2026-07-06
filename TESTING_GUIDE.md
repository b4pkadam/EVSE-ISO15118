# SDP Bug Fix - Testing Guide

## Quick Start

### 1. Verify Build
```bash
cd /Users/bala/secc_1.2
ls -lh secc_app    # Should be ~1.2M
file secc_app      # Should show: Mach-O 64-bit executable arm64
```

### 2. Run with Wireshark Monitoring

**Terminal 1 - Start Wireshark:**
```bash
wireshark -i lo -f "udp port 15118 or tcp port 15118" &
```

**Terminal 2 - Start SECC Application:**
```bash
cd /Users/bala/secc_1.2
./secc_app
```

**Expected Output:**
```
==========================================
  SECC Application (ISO 15118-2)
==========================================

Features:
  [TLS/NonTLS] Selective mode based on SDP
  [Keylogging] SSLKEYLOGFILE support

Environment Variables:
  SSLKEYLOGFILE  - Path to save TLS key log
                   (use with Wireshark to decrypt TLS)

Usage:
  SSLKEYLOGFILE=/tmp/keys.log ./secc_app

==========================================

[INFO] Session started successfully
SECC listening on port 15118...
[SECC] Phase 1: Starting SDP discovery (UDP)...
[UDP] SDP listener initialized on port 15118
[SECC] Waiting for SDP request from EV (UDP)...
```

**Application is now waiting for EDP SDP request!**

### 3. Send Test SDP Request (Terminal 3)

#### Option A: Using Python (easiest)
```python
#!/usr/bin/env python3
import socket
import struct

# Create UDP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

# Build SDP Request
# Header: Version(0x01) | Inverted(0xFE) | Type(0x9000) | Length(6)
# Payload: MsgType(0x01) | Timestamp(4 bytes) | Transports(0x03=both)

sdp_request = bytearray([
    0x01, 0xFE,           # Protocol Version
    0x90, 0x00,           # Payload Type (SDP)
    0x00, 0x00, 0x00, 0x06,  # Payload Length
    0x01,                 # Message Type (Request)
    0x00, 0x00, 0x00, 0x42,  # Timestamp
    0x03                  # Supported Transports (TLS + TCP)
])

# Send to localhost
sock.sendto(sdp_request, ('127.0.0.1', 15118))
print(f"Sent {len(sdp_request)} byte SDP request")

# Receive response
data, addr = sock.recvfrom(1024)
print(f"Received {len(data)} bytes from {addr}")
print(f"Response: {data.hex()}")

sock.close()
```

#### Option B: Using bash/xxd
```bash
# Build request in hex and send
echo "01fe90000000000601000000004203" | xxd -r -p | \
  nc -u 127.0.0.1 15118 | xxd
```

#### Option C: Using socat (interactive)
```bash
socat - UDP:127.0.0.1:15118
# Type and send the SDP request (hex encoded)
```

### 4. Monitor Wireshark

**Key things to look for:**

✅ **UDP Packet 1** (Request - EV → SECC)
- Source: 127.0.0.1:random_port
- Destination: 127.0.0.1:15118
- Payload: 14 bytes
- First 4 bytes: `01 fe 90 00`

✅ **UDP Packet 2** (Response - SECC → EV)
- Source: 127.0.0.1:15118
- Destination: 127.0.0.1:random_port
- Payload: 14 bytes
- Byte 8: `02` (Response type)

### 5. Verify Console Output

**Terminal 2 (SECC app) should show:**
```
[UDP] Received SDP request from 127.0.0.1:XXXXX (14 bytes)
[SDP] Parsed SDP request: type=1, transports=0x03
[SDP] Selected: TLS transport
[SECC] Sending SDP response (UDP)...
[SDP] Built response packet: 14 bytes (type=0x02, transports=0x03)
[UDP] Sent SDP response to 127.0.0.1:XXXXX (14 bytes)
[SECC] Phase 1 complete: SDP discovery finished

[SECC] Phase 2: Starting V2G communication (TCP)...
[SECC] Accepting client connection (V2G communication)...
SECC listening on port 15118 (Non-TLS mode)...
[SECC] Waiting for next message...
```

---

## Expected Behavior Comparison

### BEFORE FIX ❌
```
[SECC] Accepting client connection (SDP detection mode)...
[SECC] Waiting for SDP message from EV...
[... blocks indefinitely ...]
[Wireshark] SDP request visible, but NOT processed
```

### AFTER FIX ✅
```
[SECC] Phase 1: Starting SDP discovery (UDP)...
[UDP] Received SDP request from 127.0.0.1:54321 (14 bytes)
[SDP] Parsed SDP request: type=1, transports=0x03
[UDP] Sent SDP response to 127.0.0.1:54321 (14 bytes)
[SECC] Phase 1 complete

[SECC] Phase 2: Starting V2G communication (TCP)...
SECC listening on port 15118 (Non-TLS mode)...
[Wireshark] SDP request AND response visible
```

---

## Debugging

### Enable More Verbose Output
Modify code if needed and recompile with debug flags:
```bash
gcc ... -DDEBUG -g -o secc_app
```

### Check UDP Socket on macOS
```bash
# List listening sockets
netstat -an | grep 15118

# Should show:
# udp4       0      0  127.0.0.1.15118        *.*
# tcp4       0      0  127.0.0.1.15118        *.*
```

### Capture to PCAP File
```bash
tcpdump -i lo -w secc_capture.pcap 'port 15118'
```

Then open `secc_capture.pcap` in Wireshark for analysis.

### Analyze V2GTP Packets
In Wireshark, add display filter:
```
frame.len == 14 && (udp.port == 15118 || tcp.port == 15118)
```

---

## Test Scenarios

### Scenario 1: SDP Only (Success Path)
1. Start SECC app
2. Send valid SDP request
3. **Verify:** Response received, Phase 1 completes
4. **Expected:** Output shows "Phase 1 complete"

### Scenario 2: Invalid SDP
1. Start SECC app
2. Send malformed packet (wrong header)
3. **Verify:** Error message shown, graceful handling
4. **Expected:** No crash, proper error logging

### Scenario 3: SDP + TCP Connection
1. Start SECC app
2. Send SDP request via UDP
3. Connect via TCP to port 15118
4. **Verify:** Both phases complete successfully
5. **Expected:** Can establish TCP connection after SDP response

### Scenario 4: Wireshark Decryption
1. Set `SSLKEYLOGFILE=/tmp/secc_keys.log`
2. Run SECC app
3. Perform TLS mode handshake
4. Open `/tmp/secc_keys.log` in Wireshark for decryption
5. **Verify:** TLS traffic becomes readable

---

## Common Issues & Fixes

### Issue: Port Already in Use
```
Error: Bind failed (port 15118)
```
**Fix:**
```bash
# Kill existing process
lsof -i :15118 | grep secc_app | awk '{print $2}' | xargs kill -9

# Or use different port (modify code if needed)
```

### Issue: Permission Denied
```
Error: Bind failed
```
**Fix:** On macOS, ports < 1024 may require sudo (but 15118 doesn't)
```bash
sudo ./secc_app
```

### Issue: No Response Received
**Check:**
1. Is SECC app running? `ps aux | grep secc_app`
2. Is firewall blocking UDP? Check System Preferences > Security
3. Is correct IP/port? Use `127.0.0.1:15118` for localhost

### Issue: Wireshark Not Showing Packets
**Check:**
1. Capture on `lo` (loopback): `wireshark -i lo`
2. Use correct filter: `port 15118`
3. Or use tcpdump: `tcpdump -i lo port 15118`

---

## Key Files to Review

1. **SDP_BUG_FIX_REPORT.md** - Detailed technical explanation
2. **transport/tls_transport.c** - UDP implementation (lines ~350-450)
3. **core/secc_session.c** - Two-phase flow (lines ~28-130)
4. **v2g/v2gtp_sdp_handler.c** - SDP response building (lines ~45-100)

---

## Success Criteria

✅ **All the following must pass:**

- [ ] SECC app compiles without errors
- [ ] `[UDP] SDP listener initialized` message appears
- [ ] Wireshark shows incoming UDP SDP request
- [ ] `[UDP] Received SDP request` message appears
- [ ] `[SDP] Parsed SDP request` message appears
- [ ] `[UDP] Sent SDP response` message appears
- [ ] Wireshark shows outgoing UDP SDP response
- [ ] `Phase 1 complete` message appears
- [ ] `Phase 2: Starting V2G communication` message appears
- [ ] Application transitions from UDP to TCP

**If all pass: ✅ BUG FIXED!**

---

## Post-Fix Verification Script

Create `test_sdp.sh`:
```bash
#!/bin/bash

echo "=== SDP Bug Fix Verification ==="
echo ""

# Check binary exists
if [ ! -f ./secc_app ]; then
    echo "❌ secc_app not found"
    exit 1
fi
echo "✅ Binary exists: $(ls -lh secc_app | awk '{print $5}')"

# Check key files modified
for file in transport/tls_transport.h v2g/v2gtp_sdp_handler.h core/secc_session.c; do
    if grep -q "udp_sdp" "$file"; then
        echo "✅ $file has UDP functions"
    else
        echo "❌ $file missing UDP functions"
    fi
done

echo ""
echo "Ready to test! Run: ./secc_app"
echo "Then send SDP from another terminal"
```

```bash
chmod +x test_sdp.sh
./test_sdp.sh
```

---

**Next Step:** Run `./secc_app` and observe the output!

