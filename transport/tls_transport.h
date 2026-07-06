#ifndef TLS_TRANSPORT_H
#define TLS_TRANSPORT_H

#include <stdint.h>
#include <netinet/in.h>


#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SDP_MULTICAST_ADDR   "FF02::1"
#define SDP_PORT             15118
#define V2GTP_SDP_REQUEST    0x9000
#define V2GTP_SDP_RESPONSE   0x9001
#define SECURITY_TLS         0x00
#define SECURITY_NONE        0x10

/* =========================
 * TLS Transport API
 * ========================= */

/* Initialize TCP server + accept EV connection (TLS or Non-TLS mode) */
int tls_server_init_and_accept_ex(int use_tls_mode);

/* Initialize TCP server + accept EV connection (default TLS mode) */
int tls_server_init_and_accept(void);

/* Perform TLS handshake (only for TLS mode) */
int tls_handshake(int client_fd);

/* Read data (handles both TLS and Non-TLS) */
int tls_read(int client_fd, unsigned char *buf, uint32_t len);

/* Write data (handles both TLS and Non-TLS) */
int tls_write(int client_fd, const unsigned char *buf, uint32_t len);

/* Close connection */
void tls_close(int client_fd);

/* =========================
 * UDP Transport API (for SDP)
 * ========================= */

/* Initialize UDP server and listen for SDP requests
 * Returns UDP socket fd, or -1 on error
 * Stores client address for responding */
int udp_sdp_server_init(void);

/* Receive SDP request from EV (IPv6)
 * Returns number of bytes read, or -1 on error */
int udp_sdp_recv(int udp_fd, uint8_t *buf, size_t buf_len, 
                 struct sockaddr_in6 *client_addr, socklen_t *client_addr_len);

/* Send SDP response to EV (IPv6)
 * Returns number of bytes sent, or -1 on error */
int udp_sdp_send(int udp_fd, const uint8_t *buf, size_t buf_len,
                 const struct sockaddr_in6 *client_addr);

/* Close UDP socket */
void udp_sdp_close(int udp_fd);

int secc_sdp_listen(uint16_t v2g_port, int same_pc);

#endif