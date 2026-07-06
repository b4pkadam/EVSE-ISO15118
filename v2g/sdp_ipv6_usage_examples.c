/*
 * SDP (Service Discovery Protocol) IPv6 Usage Examples
 * ISO 15118-2 Standard Compliance
 * 
 * This file demonstrates how to use the updated SDP handler with IPv6 addresses
 */

#include <stdio.h>
#include <string.h>
#include "v2gtp_sdp_handler.h"

/* =========================================================================
 * Example 1: Build SDP Response with IPv6 Address
 * ========================================================================= */

void example_build_ipv6_response(void)
{
    uint8_t response_buf[64];
    sdp_response_t resp;
    
    /* Set security type */
    resp.security = TRANSPORT_TLS;  /* Use TLS transport */
    
    /* Set port for V2G communication */
    resp.port = 6363;  /* Standard V2G port */
    
    /* Set IPv6 address */
    /* Example: 2001:db8::1 */
    uint8_t ipv6_addr[16] = {
        0x20, 0x01, 0x0d, 0xb8,  /* 2001:db8 */
        0x00, 0x00, 0x00, 0x00,  /* :: */
        0x00, 0x00, 0x00, 0x00,  /* :: */
        0x00, 0x00, 0x00, 0x01   /* ::1 */
    };
    memcpy(resp.ip6_addr, ipv6_addr, 16);
    
    /* Build response packet */
    int packet_len = sdp_build_response(response_buf, sizeof(response_buf), &resp);
    if (packet_len > 0) {
        printf("SDP Response built successfully: %d bytes\n", packet_len);
        printf("Security: %s\n", resp.security == TRANSPORT_TLS ? "TLS" : "Non-TLS");
        printf("Port: %d\n", resp.port);
        printf("IPv6: ");
        for (int i = 0; i < 16; i++) {
            printf("%02x%s", resp.ip6_addr[i], (i % 2) ? ":" : "");
        }
        printf("\n");
    }
}

/* =========================================================================
 * Example 2: Build SDP Response with Localhost IPv6 (::1)
 * ========================================================================= */

void example_localhost_ipv6(void)
{
    uint8_t response_buf[64];
    sdp_response_t resp;
    
    resp.security = TRANSPORT_NON_TLS;  /* TCP only, no TLS */
    resp.port = 6363;
    
    /* IPv6 localhost: ::1 (0000:0000:0000:0000:0000:0000:0000:0001) */
    memset(resp.ip6_addr, 0, 16);
    resp.ip6_addr[15] = 0x01;  /* Last byte is 1 */
    
    int packet_len = sdp_build_response(response_buf, sizeof(response_buf), &resp);
    printf("IPv6 Localhost Response: %d bytes\n", packet_len);
}

/* =========================================================================
 * Example 3: Build SDP Response with Link-Local IPv6
 * ========================================================================= */

void example_link_local_ipv6(void)
{
    uint8_t response_buf[64];
    sdp_response_t resp;
    
    resp.security = TRANSPORT_TLS;
    resp.port = 6363;
    
    /* IPv6 Link-local: fe80::1 */
    uint8_t ipv6_addr[16] = {
        0xfe, 0x80, 0x00, 0x00,  /* fe80 */
        0x00, 0x00, 0x00, 0x00,  /* :: */
        0x00, 0x00, 0x00, 0x00,  /* :: */
        0x00, 0x00, 0x00, 0x01   /* ::1 */
    };
    memcpy(resp.ip6_addr, ipv6_addr, 16);
    
    int packet_len = sdp_build_response(response_buf, sizeof(response_buf), &resp);
    printf("IPv6 Link-Local Response: %d bytes\n", packet_len);
    
    /* Print IPv6 address */
    printf("Address: fe80::1\n");
}

/* =========================================================================
 * Example 4: Parse IPv6 from SDP Response Packet
 * ========================================================================= */

void example_parse_ipv6_from_packet(const uint8_t *packet, int packet_len)
{
    if (packet_len < 26) {
        printf("Packet too short for SDP IPv6 response\n");
        return;
    }
    
    /* Extract security type (byte 8) */
    uint8_t security = packet[8];
    
    /* Extract port (bytes 9-10) */
    uint16_t port = (packet[9] << 8) | packet[10];
    
    /* Extract IPv6 address (bytes 11-26) */
    uint8_t ipv6[16];
    memcpy(ipv6, &packet[11], 16);
    
    printf("Parsed SDP Response:\n");
    printf("  Security: %s\n", security == TRANSPORT_TLS ? "TLS" : "Non-TLS");
    printf("  Port: %d\n", port);
    printf("  IPv6: ");
    for (int i = 0; i < 16; i++) {
        printf("%02x%s", ipv6[i], (i % 2) ? ":" : "");
    }
    printf("\n");
}

/* =========================================================================
 * Example 5: Convert IPv6 String to Bytes
 * (Simple implementation - does not handle all IPv6 formats)
 * ========================================================================= */

int ipv6_from_string(const char *str, uint8_t *addr)
{
    if (!str || !addr) return -1;
    
    /* Simple approach: parse colon-separated groups */
    memset(addr, 0, 16);
    
    /* Example: Handle common formats like "2001:db8::1" */
    int groups[8] = {0};
    int group_count = 0;
    char temp[64];
    strncpy(temp, str, sizeof(temp) - 1);
    
    /* Very simplified parser - production code should use inet_pton() */
    const char *delim = ":";
    char *token = strtok(temp, delim);
    
    while (token && group_count < 8) {
        if (strlen(token) > 0) {
            sscanf(token, "%x", &groups[group_count]);
        }
        group_count++;
        token = strtok(NULL, delim);
    }
    
    /* Convert to byte array (big-endian) */
    for (int i = 0; i < group_count; i++) {
        addr[i*2] = (groups[i] >> 8) & 0xFF;
        addr[i*2 + 1] = groups[i] & 0xFF;
    }
    
    return 0;
}

/* =========================================================================
 * Example 6: IPv6 Address Structure (Recommended)
 * ========================================================================= */

typedef struct {
    uint16_t group[8];  /* 8 groups of 16-bit values */
} ipv6_addr_t;

void example_ipv6_structure(void)
{
    ipv6_addr_t addr;
    
    /* Set address to 2001:db8::1 */
    addr.group[0] = 0x2001;
    addr.group[1] = 0x0db8;
    addr.group[2] = 0x0000;
    addr.group[3] = 0x0000;
    addr.group[4] = 0x0000;
    addr.group[5] = 0x0000;
    addr.group[6] = 0x0000;
    addr.group[7] = 0x0001;
    
    printf("IPv6 Address: ");
    for (int i = 0; i < 8; i++) {
        printf("%04x%s", addr.group[i], i < 7 ? ":" : "\n");
    }
}

/* =========================================================================
 * Example 7: Common IPv6 Addresses for Testing
 * ========================================================================= */

void example_common_addresses(void)
{
    printf("Common IPv6 Addresses for SDP:\n\n");
    
    printf("1. Loopback (::1):\n");
    printf("   Bytes: 00:00:00:00:00:00:00:00:00:00:00:00:00:00:00:01\n\n");
    
    printf("2. Link-Local (fe80::1):\n");
    printf("   Bytes: fe:80:00:00:00:00:00:00:00:00:00:00:00:00:00:01\n\n");
    
    printf("3. Unique Local (fc00::1):\n");
    printf("   Bytes: fc:00:00:00:00:00:00:00:00:00:00:00:00:00:00:01\n\n");
    
    printf("4. Documentation (2001:db8::1):\n");
    printf("   Bytes: 20:01:0d:b8:00:00:00:00:00:00:00:00:00:00:00:01\n\n");
}

/* =========================================================================
 * Example 8: SDP Response Construction with Different Transports
 * ========================================================================= */

void example_transport_options(void)
{
    uint8_t buf[64];
    sdp_response_t resp;
    
    resp.port = 6363;
    memset(resp.ip6_addr, 0, 16);
    resp.ip6_addr[15] = 1;  /* ::1 */
    
    printf("SDP Response Options:\n\n");
    
    /* Option 1: TLS Transport */
    resp.security = TRANSPORT_TLS;
    int len1 = sdp_build_response(buf, sizeof(buf), &resp);
    printf("1. TLS Transport: %d bytes, Security=%d\n", len1, resp.security);
    
    /* Option 2: Non-TLS Transport */
    resp.security = TRANSPORT_TCP;
    int len2 = sdp_build_response(buf, sizeof(buf), &resp);
    printf("2. Non-TLS Transport: %d bytes, Security=%d\n", len2, resp.security);
}

/* =========================================================================
 * Example 9: Full IPv6 SDP Handshake
 * ========================================================================= */

void example_full_handshake(void)
{
    printf("Full IPv6 SDP Handshake:\n");
    printf("========================\n\n");
    
    /* Step 1: EV sends SDP request */
    printf("Step 1: EV sends SDP Request (broadcast UDP)\n");
    printf("  - Uses UDP port 15118\n");
    printf("  - Broadcast to ff02::1 (all nodes)\n\n");
    
    /* Step 2: EVSE builds response with IPv6 */
    printf("Step 2: EVSE builds SDP Response with IPv6\n");
    uint8_t response_buf[64];
    sdp_response_t resp;
    
    resp.security = TRANSPORT_TLS;
    resp.port = 6363;
    
    /* EVSE's IPv6 address on the local link */
    uint8_t evse_ipv6[16] = {
        0xfe, 0x80, 0x00, 0x00,  /* Link-local */
        0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xff,
        0xfe, 0x00, 0x12, 0x34   /* EVSE ID from MAC */
    };
    memcpy(resp.ip6_addr, evse_ipv6, 16);
    
    int packet_len = sdp_build_response(response_buf, sizeof(response_buf), &resp);
    printf("  - Response packet: %d bytes\n", packet_len);
    printf("  - Security: TLS\n");
    printf("  - Port: %d\n", resp.port);
    printf("  - IPv6: fe80::ff:fe00:1234\n\n");
    
    /* Step 3: EV connects via IPv6 */
    printf("Step 3: EV establishes TCP/TLS connection\n");
    printf("  - Connect to [fe80::ff:fe00:1234]:6363\n");
    printf("  - Perform TLS handshake\n");
    printf("  - Begin V2G protocol\n");
}

/* =========================================================================
 * Example 10: Migration from IPv4 to IPv6
 * ========================================================================= */

void example_migration_guide(void)
{
    printf("Migration from IPv4 to IPv6:\n");
    printf("=============================\n\n");
    
    printf("Old Code (IPv4):\n");
    printf("  sdp_response_t resp;\n");
    printf("  resp.ip = 0xc0a80001;  // 192.168.0.1\n");
    printf("  resp.port = 6363;\n\n");
    
    printf("New Code (IPv6):\n");
    printf("  sdp_response_t resp;\n");
    printf("  // fe80::1 (link-local)\n");
    printf("  uint8_t ipv6[16] = {0xfe,0x80,0,0,0,0,0,0,0,0,0,0,0,0,0,1};\n");
    printf("  memcpy(resp.ip6_addr, ipv6, 16);\n");
    printf("  resp.port = 6363;\n\n");
    
    printf("Key Changes:\n");
    printf("  - uint32_t ip  → uint8_t ip6_addr[16]\n");
    printf("  - IPv4 4 bytes → IPv6 16 bytes\n");
    printf("  - Response size: 15 bytes → 27 bytes\n");
    printf("  - Payload size: 7 bytes → 18 bytes\n");
}

/* =========================================================================
 * Main Test Function
 * ========================================================================= */

int main(void)
{
    printf("SDP IPv6 Usage Examples\n");
    printf("=======================\n\n");
    
    printf("--- Example 1: Build IPv6 Response ---\n");
    example_build_ipv6_response();
    printf("\n");
    
    printf("--- Example 2: Localhost IPv6 ---\n");
    example_localhost_ipv6();
    printf("\n");
    
    printf("--- Example 3: Link-Local IPv6 ---\n");
    example_link_local_ipv6();
    printf("\n");
    
    printf("--- Example 7: Common Addresses ---\n");
    example_common_addresses();
    printf("\n");
    
    printf("--- Example 8: Transport Options ---\n");
    example_transport_options();
    printf("\n");
    
    printf("--- Example 9: Full Handshake ---\n");
    example_full_handshake();
    printf("\n");
    
    printf("--- Example 10: Migration Guide ---\n");
    example_migration_guide();
    
    return 0;
}
