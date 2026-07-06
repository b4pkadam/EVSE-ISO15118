# SDP (SECC Discovery Protocol) Code Review
## Senior-Level Security & Best Practices Analysis
**Date:** 2026-06-20  
**Standard:** ISO 15118-2  
**Review Scope:** Memory Safety, Buffer Overflow Protection, MISRA-C Compliance

---

## Issue #1: Buffer Overflow in get_secc_ipv6() - iface_len Not Validated

### Location
`transport/tls_transport.c:629-630`

### Issue
```c
if (iface_out) strncpy(iface_out, "lo0", iface_len - 1);  // UNSAFE if iface_len == 0
```

### Risk
- **Risk Level:** HIGH  
- **Type:** Integer underflow → buffer overflow
- **Trigger:** If caller passes `iface_len=0` or caller passes NULL but iface_out is not NULL
- **Impact:** Stack overflow, memory corruption, crash, or code execution

### Root Cause
- No validation that `iface_len > 1` before subtracting 1
- Integer arithmetic without bounds checking
- Trust in caller to pass valid iface_len

### Recommended Fix
Check iface_len before strncpy operations:
```c
if (iface_out && iface_len > 1) {
    strncpy(iface_out, "lo0", iface_len - 1);
    iface_out[iface_len - 1] = '\0';  // Explicit null termination
}
```

### Runtime Behavior
**No change** - improves robustness by rejecting invalid input gracefully.

---

## Issue #2: Missing Return Value Checks on setsockopt()

### Location
`transport/tls_transport.c:708-710, 712-714`

### Issue
```c
int reuse = 1;
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));  // ❌ No check
setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));  // ❌ No check
int loop = 1;
setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));  // ❌ No check
```

### Risk
- **Risk Level:** MEDIUM  
- **Type:** Unvalidated system call
- **Scenario:** On embedded systems or restricted environments, setsockopt may fail silently
- **Impact:** Socket may not be configured correctly, leading to multicast reception failures

### Root Cause
- Assumption that setsockopt always succeeds
- No error propagation from system calls

### Recommended Fix
Check return values and propagate errors:
```c
if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    perror("setsockopt SO_REUSEADDR");
    close(sock);
    return -1;
}
// Repeat for other setsockopt calls
```

### Runtime Behavior
**No change** - fails gracefully instead of silently on systems where setsockopt fails.

---

## Issue #3: Type Mismatch - ssize_t vs int

### Location
`transport/tls_transport.c:732-735`

### Issue
```c
int n = recvfrom(sock, req, sizeof(req), 0,
                 (struct sockaddr *)&ev_addr, &ev_len);
if (n < 0) { ... }
if (n < 8)  { ... }
```

### Risk
- **Risk Level:** MEDIUM (on platforms where ssize_t differs from int)
- **Type:** Type mismatch
- **Scenario:** On 64-bit systems, ssize_t is signed long; could cause sign extension issues
- **Impact:** Potential for misinterpretation of return value on portability

### Root Cause
- recvfrom() returns ssize_t, not int
- Implicit conversion without explicit cast
- POSIX compliance issue

### Recommended Fix
```c
ssize_t n = recvfrom(sock, req, sizeof(req), 0,
                     (struct sockaddr *)&ev_addr, &ev_len);
if (n < 0) {
    perror("recvfrom");
    close(sock);
    return -1;
}
if (n < 8) { printf("Packet too short\n"); continue; }
```

### Runtime Behavior
**No change** - improves type safety and portability.

---

## Issue #4: Missing Bounds Check Before Buffer Access

### Location
`transport/tls_transport.c:745-750`

### Issue
```c
int n = recvfrom(...);
if (n < 0) { ... }
if (n < 8) { ... }

// Directly accessing req[] without checking n >= 4
if (req[0] != 0x01 || req[1] != 0xFE) continue;  // ❌ Bounds check insufficient
uint16_t ptype = ((uint16_t)req[2] << 8) | req[3];  // ❌ Could be out of bounds
```

### Risk
- **Risk Level:** HIGH  
- **Type:** Buffer over-read
- **Trigger:** Malformed SDP packet with n=4 to n=7
- **Impact:** Read beyond buffer boundary, information disclosure, potential crash

### Root Cause
- Check `n < 8` allows n=4-7 which is insufficient for accessing req[2] and req[3]
- No validation before accessing req[8] later (security field)

### Recommended Fix
```c
if (n < 10) {  // Minimum: 8 byte header + 2 byte payload
    printf("Packet too short (got %zd, need 10)\n", n);
    continue;
}
```

### Runtime Behavior
**No change** - rejects insufficiently sized packets earlier.

---

## Issue #5: Missing inet_pton() Return Value Validation

### Location
`transport/tls_transport.c:723-724`

### Issue
```c
struct ipv6_mreq mreq;
memset(&mreq, 0, sizeof(mreq));
inet_pton(AF_INET6, SDP_MULTICAST_ADDR, &mreq.ipv6mr_multiaddr);  // ❌ No error check
```

### Risk
- **Risk Level:** MEDIUM  
- **Type:** Unvalidated conversion
- **Scenario:** SDP_MULTICAST_ADDR could be invalid, inet_pton returns 0 or -1
- **Impact:** Multicast group join fails silently with invalid address

### Root Cause
- inet_pton() can return: 1 (success), 0 (invalid format), -1 (error)
- No validation of return value

### Recommended Fix
```c
if (inet_pton(AF_INET6, SDP_MULTICAST_ADDR, &mreq.ipv6mr_multiaddr) <= 0) {
    fprintf(stderr, "Invalid multicast address: %s\n", SDP_MULTICAST_ADDR);
    close(sock);
    return -1;
}
```

### Runtime Behavior
**No change** - fails gracefully if address is invalid.

---

## Issue #6: if_nametoindex() Return Value Not Validated

### Location
`transport/tls_transport.c:719-720`

### Issue
```c
unsigned int iface_idx = if_nametoindex(
    same_pc ? "lo0" : iface_name);  // ❌ Returns 0 on error, unchecked
```

### Risk
- **Risk Level:** MEDIUM  
- **Type:** Unvalidated system call
- **Scenario:** Interface doesn't exist, iface_name is garbage
- **Impact:** IPv6_JOIN_GROUP with interface 0 (invalid), multicast join fails silently

### Root Cause
- if_nametoindex() returns 0 on error, not -1
- No validation before use

### Recommended Fix
```c
unsigned int iface_idx = if_nametoindex(same_pc ? "lo0" : iface_name);
if (iface_idx == 0) {
    fprintf(stderr, "Invalid interface: %s\n", same_pc ? "lo0" : iface_name);
    close(sock);
    return -1;
}
```

### Runtime Behavior
**No change** - fails gracefully if interface doesn't exist.

---

## Issue #7: Missing const Correctness

### Location
`transport/tls_transport.c:518-521, 545-549`

### Issue
```c
int udp_sdp_recv(int udp_fd, uint8_t *buf, int buf_len,  // ❌ buf should be const for read params
                 struct sockaddr_in6 *client_addr, socklen_t *client_addr_len)

int udp_sdp_send(int udp_fd, const uint8_t *buf, int buf_len,  // ✓ Correctly const
                 const struct sockaddr_in6 *client_addr)
```

### Risk
- **Risk Level:** LOW  
- **Type:** API design inconsistency
- **Impact:** Can't pass const buffer to recvfrom (though recvfrom expects non-const)

### Root Cause
- recv buffer needs to be non-const (system call limitation)
- But output parameters like client_addr should be const in function signature when not modified

### Recommended Fix
Keep as-is for recvfrom compatibility. Note: System call limitation prevents marking recv buffer as const.

### Runtime Behavior
**No change** - system call limitation (recvfrom signature).

---

## Issue #8: No Bounds Check on buf_len Parameter

### Location
`transport/tls_transport.c:520-521, 557-559, 488-491`

### Issue
```c
int udp_sdp_recv(int udp_fd, uint8_t *buf, int buf_len, ...)
{
    if (udp_fd < 0 || !buf || buf_len <= 0)  // ❌ Checks buf_len > 0, but could still be INT_MAX
        return -1;
    
    int ret = recvfrom(udp_fd, buf, buf_len, 0, ...);  // ❌ buf_len as size_t could overflow
}
```

### Risk
- **Risk Level:** MEDIUM  
- **Type:** Signed/unsigned mismatch
- **Scenario:** Caller passes negative buf_len (after underflow), or extremely large positive value
- **Impact:** Buffer overflow in recvfrom, reading beyond allocated memory

### Root Cause
- buf_len is signed int, but recvfrom expects size_t (unsigned)
- Check `buf_len <= 0` catches negatives but doesn't prevent huge values
- Implicit conversion from int to size_t

### Recommended Fix
```c
int udp_sdp_recv(int udp_fd, uint8_t *buf, size_t buf_len,  // Change to size_t
                 struct sockaddr_in6 *client_addr, socklen_t *client_addr_len)
{
    if (udp_fd < 0 || !buf || buf_len == 0)
        return -1;
    
    ssize_t ret = recvfrom(udp_fd, buf, buf_len, 0, ...);  // Now type-safe
```

### Runtime Behavior
**No change** - improves type safety.

---

## Issue #9: Missing Lock Release on Error Path

### Location
`transport/tls_transport.c:632-643`

### Issue
```c
if (get_secc_ipv6(evse_ip6, iface_name, sizeof(iface_name), same_pc) < 0)
    return -1;

int sock = socket(AF_INET6, SOCK_DGRAM, 0);
if (sock < 0) { perror("socket"); return -1; }

int reuse = 1;
setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));  // Error ignored
setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));  // Error ignored
// ... more setup ...
if (bind(sock, ...) < 0) {  // ✓ Closes socket
    perror("bind");
    close(sock);
    return -1;
}
```

### Risk
- **Risk Level:** LOW  
- **Type:** Resource leak on error path
- **Scenario:** setsockopt fails (unlikely but possible on restricted systems)
- **Impact:** File descriptor leak, resource exhaustion on repeated failures

### Root Cause
- setsockopt failures not checked before proceeding
- Subsequent bind might fail, but socket is already misconfigured

### Recommended Fix
Check all system calls and close socket on any error:
```c
if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    perror("SO_REUSEADDR");
    close(sock);
    return -1;
}
// Repeat for other critical setsockopt calls
```

### Runtime Behavior
**No change** - fails faster and cleaner.

---

## Issue #10: Magic Numbers Without Symbolic Constants

### Location
Multiple locations: `transport/tls_transport.c`

### Issue
```c
uint8_t req[256];  // Magic number 256
if (n < 8) { ... }  // Magic number 8
if (n != 10) { ... }  // Magic number 10
ssize_t sent = sendto(sock, resp, 28, 0, ...);  // Magic number 28
memcpy(&buf[8], ...)  // Magic number 8
```

### Risk
- **Risk Level:** MEDIUM  
- **Type:** Code maintainability and consistency
- **Impact:** Hard to understand packet structure, prone to off-by-one errors

### Root Cause
- No symbolic constants for packet structure offsets
- Magic numbers scattered throughout code

### Recommended Fix
Add to tls_transport.h:
```c
#define SDP_MAX_PACKET_SIZE     256
#define V2GTP_HEADER_LEN        8
#define SDP_REQUEST_MIN_LEN     10
#define SDP_RESPONSE_LEN        28
#define IPV6_ADDR_LEN           16
```

Then use:
```c
uint8_t req[SDP_MAX_PACKET_SIZE];
if (n < SDP_REQUEST_MIN_LEN) { ... }
ssize_t sent = sendto(sock, resp, SDP_RESPONSE_LEN, 0, ...);
memcpy(&buf[V2GTP_HEADER_LEN], ...);
```

### Runtime Behavior
**No change** - improves readability and maintainability.

---

## Issue #11: Incomplete Error Messages with Non-ASCII Characters

### Location
`transport/tls_transport.c:707, 715, 722, 731, 756`

### Issue
```c
if (sock < 0) { perror("❌ socket"); return -1; }  // ❌ Emoji may not be portable
if (bind(...) < 0) { perror("❌ bind"); close(sock); return -1; }
```

### Risk
- **Risk Level:** LOW  
- **Type:** Portability and log parsing
- **Impact:** Emoji may not display correctly on all terminals, breaks log parsing scripts

### Root Cause
- Unicode emoji used in error messages for visual clarity
- Not portable to all log aggregation systems

### Recommended Fix
```c
if (sock < 0) { perror("socket"); return -1; }
if (bind(...) < 0) { perror("bind"); close(sock); return -1; }
```

Then add structured logging:
```c
fprintf(stderr, "[ERROR] socket: %s\n", strerror(errno));
```

### Runtime Behavior
**No change** - improves portability.

---

## Issue #12: No Validation of Output Parameter Initialization

### Location
`transport/tls_transport.c:521`

### Issue
```c
int udp_sdp_recv(int udp_fd, uint8_t *buf, int buf_len, 
                 struct sockaddr_in6 *client_addr, socklen_t *client_addr_len)
{
    if (udp_fd < 0 || !buf || buf_len <= 0)
        return -1;  // ❌ Returns without initializing *client_addr_len
    
    *client_addr_len = sizeof(struct sockaddr_in6);  // Only set on success path
}
```

### Risk
- **Risk Level:** MEDIUM  
- **Type:** Uninitialized output parameter
- **Scenario:** Error return means caller's socklen_t is uninitialized
- **Impact:** Caller might use uninitialized value in subsequent calls

### Root Cause
- Output parameter only set on success path
- Caller might not check return value before using output

### Recommended Fix
Initialize output parameters on entry:
```c
int udp_sdp_recv(int udp_fd, uint8_t *buf, int buf_len, 
                 struct sockaddr_in6 *client_addr, socklen_t *client_addr_len)
{
    if (!client_addr_len) return -1;
    
    *client_addr_len = 0;  // Initialize immediately
    
    if (udp_fd < 0 || !buf || buf_len <= 0)
        return -1;
    
    *client_addr_len = sizeof(struct sockaddr_in6);  // Set real value
    // ... rest of function ...
}
```

### Runtime Behavior
**No change** - improves API robustness.

---

## Issue #13: Dead Code - Infinite Loop with No Exit Condition

### Location
`transport/tls_transport.c:731-765`

### Issue
```c
while (1) {  // ❌ Infinite loop with exits via return/close
    uint8_t req[256];
    // ... receive and process ...
    close(sock);
    return 0;  // Only way out
}
```

### Risk
- **Risk Level:** LOW  
- **Type:** Code structure and maintainability
- **Impact:** Loop only serves as wrapper, confusing to readers

### Root Cause
- Function designed to handle single SDP exchange then exit
- Loop structure suggests multiple iterations possible

### Recommended Fix
Remove loop since function exits after first successful exchange:
```c
uint8_t req[256];
struct sockaddr_in6 ev_addr;
socklen_t ev_len = sizeof(ev_addr);

ssize_t n = recvfrom(...);
// ... process single request ...
close(sock);
return 0;
```

### Runtime Behavior
**No change** - same behavior, clearer intent.

---

## Issue #14: sendto() Return Value Not Checked for Partial Sends

### Location
`transport/tls_transport.c:755-758`

### Issue
```c
ssize_t sent = sendto(sock, resp, 28, 0,
                      (struct sockaddr *)&ev_addr, ev_len);
if (sent < 0) { perror("sendto"); continue; }
// ❌ Never checks if sent < 28 (partial send)
```

### Risk
- **Risk Level:** MEDIUM  
- **Type:** Incomplete data transmission
- **Scenario:** UDP to wireless interface might fragment, sendto returns < 28
- **Impact:** Incomplete SDP response sent to EV, causes SDP timeout

### Root Cause
- UDP sendto can return fewer bytes than requested if MTU is small
- No validation that all bytes were sent

### Recommended Fix
```c
ssize_t sent = sendto(sock, resp, SDP_RESPONSE_LEN, 0,
                      (struct sockaddr *)&ev_addr, ev_len);
if (sent < 0) {
    perror("sendto");
    close(sock);
    return -1;
}
if (sent != SDP_RESPONSE_LEN) {
    fprintf(stderr, "Partial send: %zd of %d bytes\n", sent, SDP_RESPONSE_LEN);
    close(sock);
    return -1;
}
```

### Runtime Behavior
**No change for typical networks** - adds validation for edge cases.

---

## Summary of Findings

| # | Issue | Severity | Type | Fix Impact |
|---|-------|----------|------|-----------|
| 1 | iface_len not validated | HIGH | Buffer Overflow | API robustness |
| 2 | setsockopt() errors ignored | MEDIUM | Resource Config | Error handling |
| 3 | ssize_t vs int mismatch | MEDIUM | Type Safety | Portability |
| 4 | Insufficient bounds check | HIGH | Buffer Over-read | Security |
| 5 | inet_pton() not validated | MEDIUM | Conversion Error | Robustness |
| 6 | if_nametoindex() not validated | MEDIUM | System Call | Robustness |
| 7 | Const correctness | LOW | Design | Maintainability |
| 8 | buf_len signed without bounds | MEDIUM | Type Safety | Robustness |
| 9 | Incomplete error cleanup | LOW | Resource Leak | Edge cases |
| 10 | Magic numbers | MEDIUM | Maintainability | Code clarity |
| 11 | Non-ASCII in error messages | LOW | Portability | Log parsing |
| 12 | Uninitialized output param | MEDIUM | API Safety | Caller safety |
| 13 | Infinite loop structure | LOW | Code clarity | Intent clarity |
| 14 | Partial UDP send not checked | MEDIUM | Data Integrity | Edge cases |

---

## Recommended Action Plan

### Critical (Fix First)
- Issue #1: Buffer overflow in get_secc_ipv6()
- Issue #4: Insufficient bounds check

### High Priority (Fix Before Production)
- Issue #2: setsockopt() error checking
- Issue #5: inet_pton() validation
- Issue #6: if_nametoindex() validation
- Issue #14: Partial send validation

### Medium Priority (Improve Code Quality)
- Issue #3: Type safety (ssize_t vs int)
- Issue #8: buf_len bounds
- Issue #10: Magic number constants
- Issue #12: Output parameter initialization

### Low Priority (Nice to Have)
- Issue #7: Const correctness
- Issue #9: Error cleanup
- Issue #11: Portable error messages
- Issue #13: Loop structure
