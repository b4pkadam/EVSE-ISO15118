#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "tls_transport.h"
#include "v2gtp_sdp_handler.h"

/* mbedTLS */
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

/* =========================
 * Static TLS context (server-side single session)
 * ========================= */
static mbedtls_net_context listen_fd;
static mbedtls_net_context client_fd;
static int plain_tcp_fd = -1;  /* For non-TLS mode */

static mbedtls_ssl_context ssl;
static mbedtls_ssl_config conf;
static mbedtls_x509_crt srvcert;
static mbedtls_pk_context pkey;
static mbedtls_entropy_context entropy;
static mbedtls_ctr_drbg_context ctr_drbg;

static const char *pers = "secc_tls";
static FILE *keylog_file = NULL;  /* SSLKEYLOGFILE for debugging */
static int use_tls = 1;  /* Flag to track TLS vs Non-TLS mode */

/* =========================
 * KEYLOG CALLBACK FOR mbedTLS
 * ========================= */
#ifdef MBEDTLS_SSL_EXPORT_KEYS
static int keylog_callback(void *p_expkey,
                           const unsigned char *ms,
                           const unsigned char *kb,
                           size_t maclen,
                           size_t keylen,
                           size_t ivlen,
                           const unsigned char client_random[32],
                           const unsigned char server_random[32],
                           mbedtls_tls_prf_types tls_prf_type)
{
    (void)p_expkey;
    (void)kb;
    (void)maclen;
    (void)keylen;
    (void)ivlen;
    (void)server_random;
    (void)tls_prf_type;

    if (!keylog_file)
        return 0;

    fprintf(keylog_file, "CLIENT_RANDOM ");
    for (size_t i = 0; i < 32; i++)
        fprintf(keylog_file, "%02x", client_random[i]);
    fprintf(keylog_file, " ");
    for (size_t i = 0; i < 48; i++)
        fprintf(keylog_file, "%02x", ms[i]);
    fprintf(keylog_file, "\n");
    fflush(keylog_file);

    return 0;
}
#endif

/* =========================
 * SETUP KEYLOG FILE
 * ========================= */
static int setup_keylog(void)
{
    printf("[DEBUG] setup_keylog called. keylog_file=%p\n", (void*)keylog_file);
    if (keylog_file)
        return 0;

    const char *keylog_path = getenv("SSLKEYLOGFILE");
    printf("[DEBUG] SSLKEYLOGFILE env: %s\n", keylog_path ? keylog_path : "NULL (falling back to keys.log)");
    if (!keylog_path)
    {
        keylog_path = "keys.log";
    }

    keylog_file = fopen(keylog_path, "a");
    if (!keylog_file)
    {
        printf("Warning: Failed to open SSLKEYLOGFILE: %s\n", keylog_path);
        return -1;
    }

    printf("[TLS] Keylog enabled: %s\n", keylog_path);
    return 0;
}

/* =========================
 * INIT + ACCEPT TCP (Both TLS and Non-TLS)
 * ========================= */
int tls_server_init_and_accept_ex(int use_tls_mode)
{
    int ret;
    use_tls = use_tls_mode;

    if (use_tls)
    {
        mbedtls_net_init(&listen_fd);
        mbedtls_net_init(&client_fd);

        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_x509_crt_init(&srvcert);
        mbedtls_pk_init(&pkey);
        mbedtls_entropy_init(&entropy);
        mbedtls_ctr_drbg_init(&ctr_drbg);

        /* RNG */
        if ((ret = mbedtls_ctr_drbg_seed(
                &ctr_drbg,
                mbedtls_entropy_func,
                &entropy,
                (const unsigned char *)pers,
                strlen(pers))) != 0)
        {
            printf("DRBG seed failed: -0x%x\n", -ret);
            return -1;
        }

        
        /* Load cert + key */
        if ((ret = mbedtls_x509_crt_parse_file(&srvcert, "tls/server.pem")) != 0)
        {
            printf("Cert load failed: -0x%x\n", -ret);
            return -1;
        }

        if ((ret = mbedtls_pk_parse_keyfile(&pkey, "tls/server.key", NULL)) != 0)
        {
            printf("Key load failed: -0x%x\n", -ret);
            return -1;
        }

        /* Setup keylog */
        setup_keylog();

        /* Bind TCP */
        if ((ret = mbedtls_net_bind(&listen_fd, NULL, "15118",
                                    MBEDTLS_NET_PROTO_TCP)) != 0)
        {
            printf("Bind failed: -0x%x\n", -ret);
            return -1;
        }

        printf("SECC listening on port 15118 (TLS mode)...\n");

        /* Accept client */
        if ((ret = mbedtls_net_accept(&listen_fd, &client_fd,
                                      NULL, 0, NULL)) != 0)
        {
            printf("Accept failed: -0x%x\n", -ret);
            return -1;
        }

        return client_fd.fd;
    }
    else
    {
        /* Non-TLS mode: plain TCP */
        int listen_sock = socket(AF_INET, SOCK_STREAM, 0);
        if (listen_sock < 0)
        {
            printf("Socket creation failed\n");
            return -1;
        }

        int opt = 1;
        if (setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        {
            printf("Setsockopt failed\n");
            close(listen_sock);
            return -1;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(15118);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        {
            printf("Bind failed\n");
            close(listen_sock);
            return -1;
        }

        if (listen(listen_sock, 1) < 0)
        {
            printf("Listen failed\n");
            close(listen_sock);
            return -1;
        }

        printf("SECC listening on port 15118 (Non-TLS mode)...\n");

        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        plain_tcp_fd = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);
        close(listen_sock);

        if (plain_tcp_fd < 0)
        {
            printf("Accept failed\n");
            return -1;
        }

        return plain_tcp_fd;
    }
}

/* Backward compatibility */
int tls_server_init_and_accept(void)
{
    return tls_server_init_and_accept_ex(1);  /* TLS mode by default */
}

/* =========================
 * TLS HANDSHAKE
 * ========================= */
int tls_handshake(int client_fd_int)
{
    printf("[DEBUG] tls_handshake called.\n");
#ifdef MBEDTLS_SSL_EXPORT_KEYS
    printf("[DEBUG] MBEDTLS_SSL_EXPORT_KEYS is defined.\n");
#else
    printf("[DEBUG] MBEDTLS_SSL_EXPORT_KEYS is NOT defined.\n");
#endif

    int ret;

    mbedtls_ssl_config_defaults(
        &conf,
        MBEDTLS_SSL_IS_SERVER,
        MBEDTLS_SSL_TRANSPORT_STREAM,
        MBEDTLS_SSL_PRESET_DEFAULT);

    mbedtls_ssl_conf_rng(&conf,
                         mbedtls_ctr_drbg_random,
                         &ctr_drbg);

    mbedtls_ssl_conf_authmode(&conf,
                              MBEDTLS_SSL_VERIFY_NONE);

    mbedtls_ssl_conf_ca_chain(&conf,
                              srvcert.next,
                              NULL);

    mbedtls_ssl_conf_own_cert(&conf,
                              &srvcert,
                              &pkey);

    /* Setup keylog if environment variable is set */
    setup_keylog();
#ifdef MBEDTLS_SSL_EXPORT_KEYS
    if (keylog_file)
    {
        mbedtls_ssl_conf_export_keys_ext_cb(&conf, keylog_callback, NULL);
    }
#endif

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        printf("SSL setup failed: -0x%x\n", -ret);
        return -1;
    }

    mbedtls_ssl_set_bio(&ssl,
                        &client_fd,
                        mbedtls_net_send,
                        mbedtls_net_recv,
                        NULL);

    /* handshake loop */
    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
    {
        if (ret != MBEDTLS_ERR_SSL_WANT_READ &&
            ret != MBEDTLS_ERR_SSL_WANT_WRITE)
        {
            printf("Handshake failed: -0x%x\n", -ret);
            return -1;
        }
    }

    return 0;
}

/* =========================
 * READ DATA (TLS or Non-TLS)
 * ========================= */
int tls_read(int client_fd_int, unsigned char *buf, uint32_t len)
{
    if (use_tls)
    {
        /* ============================================
         * TLS Read: mbedTLS handles fragmented data internally
         * This path handles TLS-encrypted V2GTP messages
         * mbedTLS's ssl_read() will buffer and reassemble fragments
         * ============================================ */
        int ret = mbedtls_ssl_read(&ssl, buf, len);
        if (ret < 0)
        {
            printf("[TLS] ⚠️  Read error: -0x%x\n", -ret);
            
            /* Log specific error types */
            if (ret == MBEDTLS_ERR_SSL_WANT_READ)
                printf("[TLS] Want read - no data available yet\n");
            else if (ret == MBEDTLS_ERR_SSL_WANT_WRITE)
                printf("[TLS] Want write - internal buffer issue\n");
            else if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
                printf("[TLS] Peer closed connection\n");
            
            return -1;
        }
        
        /* Log successful TLS read */
        printf("[TLS] ✅ Received %d bytes (encrypted V2GTP)\n", ret);
        printf("[TLS] Decrypted data (hex): ");
        for (int i = 0; i < ret; i++)
        {
            printf("%02x ", buf[i]);
        }
        printf("\n");
        
        return ret;
    }
    else
    {
        /* ============================================
         * FIX: Handle incomplete TCP reads for V2GTP messages
         * Issue: TCP recv() might not return complete message in one call,
         * especially for large messages like charge parameter request
         * after authorization response (46 bytes expected, 21 bytes received).
         * 
         * Solution: Read V2GTP header first to determine total length,
         * then keep reading until complete message is received.
         * ============================================ */
        
        uint32_t total_bytes_read = 0;
        uint32_t bytes_needed = len;
        uint32_t header_len = 8;  /* V2GTP header is 8 bytes */
        
        /* First, read at least the V2GTP header (8 bytes) */
        while (total_bytes_read < header_len && total_bytes_read < len)
        {
            int ret = recv(plain_tcp_fd, buf + total_bytes_read, 
                          len - total_bytes_read, 0);
            
            if (ret <= 0)
            {
                if (ret == 0)
                {
                    printf("[TCP] Connection closed by peer (received %d bytes so far)\n", 
                           total_bytes_read);
                }
                else
                {
                    printf("[TCP] Read error after %d bytes\n", total_bytes_read);
                }
                
                /* Return what we have if at least header received */
                if (total_bytes_read > 0)
                    return total_bytes_read;
                return -1;
            }
            
            total_bytes_read += ret;
            printf("[TCP] Partial read: %d bytes (total: %d/%u)\n", 
                   ret, total_bytes_read, len);
        }
        
        /* Now parse V2GTP header to determine complete message length */
        if (total_bytes_read >= header_len)
        {
            /* Extract payload length from V2GTP header (bytes 4-7) */
            uint32_t payload_length = 
                (buf[4] << 24) | (buf[5] << 16) | (buf[6] << 8) | buf[7];
            
            uint32_t total_message_len = header_len + payload_length;
            
            // printf("[TCP] V2GTP Header parsed - Payload length: %u bytes\n, "
            //        "Total message: %u bytes\n", payload_length, total_message_len);
            
            /* Keep reading until we have the complete message */
            while (total_bytes_read < total_message_len && 
                   total_bytes_read < len)
            {
                int ret = recv(plain_tcp_fd, buf + total_bytes_read, 
                              len - total_bytes_read, 0);
                
                if (ret <= 0)
                {
                    if (ret == 0)
                    {
                        printf("[TCP] ⚠️  Connection closed - incomplete message! "
                               "Expected %u bytes, got %u bytes\n", 
                               total_message_len, total_bytes_read);
                    }
                    else
                    {
                        printf("[TCP] ⚠️  Read error during payload reception\n");
                    }
                    /* Return incomplete message for now, but log warning */
                    return total_bytes_read;
                }
                
                total_bytes_read += ret;
                printf("[TCP] Partial read: %d bytes (total: %u/%u)\n", 
                       ret, total_bytes_read, total_message_len);
            }
        }
        
        /* Print source address */
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        getpeername(plain_tcp_fd, (struct sockaddr*)&addr, &addr_len);
        printf("[TCP] ✅ Complete V2G message received from %s:%d "
               "(%u bytes total)\n", inet_ntoa(addr.sin_addr), 
               ntohs(addr.sin_port), total_bytes_read);
        
        // /* Print received data in hex */
        // printf("[TCP] Received data (hex): ");
        // for (uint32_t i = 0; i < total_bytes_read; i++)
        // {
        //     printf("%02x ", buf[i]);
        // }
        // printf("\n");
        
        return total_bytes_read;
    }
}

/* =========================
 * WRITE DATA (TLS or Non-TLS)
 * ========================= */
int tls_write(int client_fd_int, const unsigned char *buf, uint32_t len)
{
    if (use_tls)
    {
        int ret = mbedtls_ssl_write(&ssl, buf, len);
        if (ret < 0)
        {
            printf("TLS write error: -0x%x\n", -ret);
            return -1;
        }
        return ret;
    }
    else
    {
        int ret = send(plain_tcp_fd, buf, len, 0);
        printf("[TCP] ✅ Sending Response data (%d bytes): \n", ret);
        for (int i = 0; i < len; i++)
        {
            // printf("%02x ", buf[i]);
            printf("%02X ", buf[i]);
            if ((i + 1) % 16 == 0)
            printf("\n");

        }
        printf("\n");
        
        if (ret < 0)
        {
            printf("TCP write error: %x\n", ret);
            return ret;
        }
        return ret;
    }
}

/* =========================
 * CLOSE CONNECTION
 * ========================= */
void tls_close(int client_fd_int)
{
    if (use_tls)
    {
        mbedtls_ssl_close_notify(&ssl);
        mbedtls_net_free(&client_fd);
        mbedtls_net_free(&listen_fd);
        mbedtls_x509_crt_free(&srvcert);
        mbedtls_pk_free(&pkey);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);

        if (keylog_file)
        {
            fclose(keylog_file);
            keylog_file = NULL;
        }
    }
    else
    {
        if (plain_tcp_fd >= 0)
        {
            close(plain_tcp_fd);
            plain_tcp_fd = -1;
        }
    }
}

/* =========================
 * UDP SDP FUNCTIONS
 * ========================= */

/* SDP protocol constants */
#define SDP_MAX_PACKET_SIZE     256
#define SDP_REQUEST_MIN_LEN     10
#define SDP_RESPONSE_LEN        28
#define V2GTP_HEADER_LEN        8
#define IPV6_ADDR_LEN           16

/* Initialize UDP server for SDP on port 15118 (IPv6) */
int udp_sdp_server_init(void)
{
    /* Use AF_INET6 instead of AF_INET for IPv6 support (ISO 15118-2) */
    int udp_fd = socket(AF_INET6, SOCK_DGRAM, 0);
    if (udp_fd < 0)
    {
        fprintf(stderr, "[UDP] Socket creation failed: %s\n", strerror(errno));
        return -1;
    }

    /* Allow reuse of the port */
    int opt = 1;
    if (setsockopt(udp_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        fprintf(stderr, "[UDP] Setsockopt SO_REUSEADDR failed: %s\n", strerror(errno));
        close(udp_fd);
        return -1;
    }

    /* FIX: Use struct sockaddr_in6 for IPv6 addressing */
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(15118);
    addr.sin6_addr = in6addr_any;  /* FIX: in6addr_any for IPv6 wildcard */

    if (bind(udp_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        printf("[UDP] Bind failed (port 15118)\n");
        close(udp_fd);
        return -1;
    }

    printf("[UDP] SDP listener initialized on port 15118 (IPv6)\n");
    return udp_fd;
}

/* Receive SDP request from EV via UDP (IPv6) */
int udp_sdp_recv(int udp_fd, uint8_t *buf, size_t buf_len, 
                 struct sockaddr_in6 *client_addr, socklen_t *client_addr_len)
{
    /* Validate all input parameters */
    if (udp_fd < 0 || !buf || buf_len == 0 || !client_addr_len)
        return -1;

    /* Initialize output parameter to prevent use of uninitialized value */
    *client_addr_len = 0;

    /* Use ssize_t for proper type checking */
    socklen_t addr_len = sizeof(struct sockaddr_in6);
    ssize_t ret = recvfrom(udp_fd, buf, buf_len, 0, 
                           (struct sockaddr *)client_addr, &addr_len);
    
    if (ret < 0)
    {
        fprintf(stderr, "[UDP] Receive failed: %s\n", strerror(errno));
        return -1;
    }

    /* Update output parameter only on success */
    *client_addr_len = addr_len;

    /* Use inet_ntop for IPv6 address formatting */
    char addr_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &client_addr->sin6_addr, addr_str, sizeof(addr_str));
    
    printf("[UDP] Received SDP request from [%s]:%d (%zd bytes)\n",
           addr_str, ntohs(client_addr->sin6_port), ret);
    
    return (int)ret;
}

/* Send SDP response to EV via UDP (IPv6) */
int udp_sdp_send(int udp_fd, const uint8_t *buf, size_t buf_len,
                 const struct sockaddr_in6 *client_addr)
{
    /* Validate all input parameters */
    if (udp_fd < 0 || !buf || buf_len == 0 || !client_addr)
        return -1;

    /* Use ssize_t for proper type checking */
    ssize_t ret = sendto(udp_fd, buf, buf_len, 0,
                         (const struct sockaddr *)client_addr, 
                         sizeof(struct sockaddr_in6));
    
    if (ret < 0)
    {
        fprintf(stderr, "[UDP] Send failed: %s\n", strerror(errno));
        return -1;
    }

    /* Verify all bytes were sent (UDP may fragment) */
    if ((size_t)ret != buf_len)
    {
        fprintf(stderr, "[UDP] Partial send: %zd of %zu bytes\n", ret, buf_len);
        return -1;
    }

    /* Use inet_ntop for IPv6 address formatting */
    char addr_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &client_addr->sin6_addr, addr_str, sizeof(addr_str));
    
    printf("[UDP] Sent SDP response to [%s]:%d (%zd bytes)\n",
           addr_str, ntohs(client_addr->sin6_port), ret);
    
    return (int)ret;
}

/* Close UDP socket */
void udp_sdp_close(int udp_fd)
{
    if (udp_fd >= 0)
    {
        close(udp_fd);
    }
}


/* ================================================================
 * get_secc_ipv6()
 * Fills evse_ip6[16] with the correct address based on test mode.
 *
 * same_pc = 1  →  ::1           EV and EVSE on same machine
 * same_pc = 0  →  fe80::xxxx   real PLC hardware
 * ================================================================ */
static int get_secc_ipv6(uint8_t *ip6_out,
                          char    *iface_out,
                          size_t   iface_len,
                          int      same_pc)
{
    /* Validate buffer pointers */
    if (!ip6_out)
        return -1;

    memset(ip6_out, 0, IPV6_ADDR_LEN);

    if (same_pc) {
        /* ::1 — IPv6 loopback for same-PC testing */
        ip6_out[15] = 0x01;
        
        /* Safely copy interface name with bounds checking */
        if (iface_out && iface_len > 1) {
            strncpy(iface_out, "lo0", iface_len - 1);
            iface_out[iface_len - 1] = '\0';  /* Ensure null termination */
        }
        printf("[SECC] Test mode: using ::1 (same-PC loopback)\n");
        return 0;
    }

    /* Real hardware — find link-local fe80:: on active interface */
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) < 0) { perror("getifaddrs"); return -1; }

    for (ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) continue;
        if (ifa->ifa_addr->sa_family != AF_INET6) continue;
        if (ifa->ifa_flags & IFF_LOOPBACK) continue;
        if (!(ifa->ifa_flags & IFF_UP))      continue;
        if (!(ifa->ifa_flags & IFF_RUNNING)) continue;

        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)ifa->ifa_addr;
        if (IN6_IS_ADDR_LOOPBACK(&s6->sin6_addr))   continue;

        /* Prefer link-local (fe80::) — required by ISO 15118-2 */
        if (!IN6_IS_ADDR_LINKLOCAL(&s6->sin6_addr))  continue;

        memcpy(ip6_out, s6->sin6_addr.s6_addr, IPV6_ADDR_LEN);
        
        /* Safely copy interface name with bounds checking */
        if (iface_out && iface_len > 1) {
            strncpy(iface_out, ifa->ifa_name, iface_len - 1);
            iface_out[iface_len - 1] = '\0';
        }

        char str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, ip6_out, str, sizeof(str));
        printf("[SECC] Real mode: using %s on %s\n", str, ifa->ifa_name);

        freeifaddrs(ifaddr);
        return 0;
    }

    freeifaddrs(ifaddr);
    printf("❌ No fe80:: link-local address found\n");
    return -1;
}


/* ================================================================
 * secc_sdp_listen()
 * Listens for EV SDP Request, sends SDP Response.
 *
 * same_pc=1  →  use loopback ::1       (development/macOS)
 * same_pc=0  →  use fe80:: link-local  (real PLC hardware)
 * ================================================================ */
int secc_sdp_listen(uint16_t v2g_port, int same_pc)
{
    char    iface_name[IF_NAMESIZE];
    uint8_t evse_ip6[16];
    sdp_request_t sdp_req;
    sdp_response_t sdp_resp;

    if (get_secc_ipv6(evse_ip6, iface_name, sizeof(iface_name), same_pc) < 0)
        return -1;

    /* Create IPv6 UDP socket for SDP */
    int sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        fprintf(stderr, "[ERROR] socket: %s\n", strerror(errno));
        return -1;
    }

    int reuse = 1;
    
    /* Enable address reuse */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "[ERROR] SO_REUSEADDR: %s\n", strerror(errno));
        close(sock);
        return -1;
    }
    
    /* Enable port reuse */
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse)) < 0) {
        fprintf(stderr, "[ERROR] SO_REUSEPORT: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    /* Keep multicast loop ON — needed for same-PC testing */
    int loop = 1;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
        fprintf(stderr, "[ERROR] IPV6_MULTICAST_LOOP: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    /* Bind to SDP port on all IPv6 interfaces */
    struct sockaddr_in6 bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin6_family = AF_INET6;
    bind_addr.sin6_port   = htons(SDP_PORT);
    bind_addr.sin6_addr   = in6addr_any;   /* :: equivalent for IPv6 */
    
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        fprintf(stderr, "[ERROR] bind: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    /* Resolve interface name to index */
    const char *bind_iface = same_pc ? "lo0" : iface_name;
    unsigned int iface_idx = if_nametoindex(bind_iface);
    if (iface_idx == 0) {
        fprintf(stderr, "[ERROR] Invalid interface: %s\n", bind_iface);
        close(sock);
        return -1;
    }

    /* Join FF02::1 multicast group */
    struct ipv6_mreq mreq;
    memset(&mreq, 0, sizeof(mreq));
    
    /* Parse multicast address */
    if (inet_pton(AF_INET6, SDP_MULTICAST_ADDR, &mreq.ipv6mr_multiaddr) <= 0) {
        fprintf(stderr, "[ERROR] Invalid multicast address: %s\n", SDP_MULTICAST_ADDR);
        close(sock);
        return -1;
    }
    
    mreq.ipv6mr_interface = iface_idx;
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
                   &mreq, sizeof(mreq)) < 0) {
        fprintf(stderr, "[ERROR] IPV6_JOIN_GROUP: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    printf("[SECC] Listening for SDP on [FF02::1]:%d\n", SDP_PORT);

    /* Receive single SDP request */
    uint8_t req[SDP_MAX_PACKET_SIZE];
    struct sockaddr_in6 ev_addr;
    socklen_t ev_len = sizeof(ev_addr);

    /* Receive SDP request from EV */
    ssize_t n = recvfrom(sock, req, sizeof(req), 0,
                         (struct sockaddr *)&ev_addr, &ev_len);
    if (n < 0) {
        fprintf(stderr, "[ERROR] recvfrom: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    /* Validate minimum packet size for V2GTP header + SDP payload */
    if (n < SDP_REQUEST_MIN_LEN) {
        fprintf(stderr, "[ERROR] Packet too short: %zd bytes (need %d)\n", n, SDP_REQUEST_MIN_LEN);
        close(sock);
        return -1;
    }

    /* Validate V2GTP version bytes */
    if (req[0] != 0x01 || req[1] != 0xFE) {
        fprintf(stderr, "[ERROR] Invalid V2GTP version: 0x%02X 0x%02X\n", req[0], req[1]);
        close(sock);
        return -1;
    }

    uint16_t ptype = ((uint16_t)req[2] << 8) | req[3];

    /* Reject SDP response (multicast echo) */
    if (ptype == V2GTP_SDP_RESPONSE) {
        printf("[SECC] Ignoring SDP Response echo\n");
        close(sock);
        return -1;
    }

    if (ptype != V2GTP_SDP_REQUEST) {
        fprintf(stderr, "[ERROR] Invalid payload type: 0x%04X\n", ptype);
        close(sock);
        return -1;
    }

    if (n != SDP_REQUEST_MIN_LEN) {
        fprintf(stderr, "[ERROR] Invalid SDP request length: %zd (expected %d)\n", n, SDP_REQUEST_MIN_LEN);
        close(sock);
        return -1;
    }

    /* Format and log IPv6 source address */
    char ev_str[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ev_addr.sin6_addr, ev_str, sizeof(ev_str));
    printf("[SECC] SDP Request from EV [%s]:%d\n", ev_str, ntohs(ev_addr.sin6_port));

    /* Parse SDP request */
    if (sdp_parse_request(req, (int)n, &sdp_req) < 0) {
        fprintf(stderr, "[ERROR] Invalid SDP request format\n");
        close(sock);
        return -1;
    }

    /* Select transport based on EV request */
    int transport = sdp_select_transport(&sdp_req);
    if (transport < 0) {
        fprintf(stderr, "[ERROR] Unsupported security/transport\n");
        close(sock);
        return -1;
    }

    uint8_t chosen = (uint8_t)transport;
    uint8_t resp[SDP_RESPONSE_LEN];
    
    /* Initialize SDP response structure for debug output in sdp_build_response() */
    memset(&sdp_resp, 0, sizeof(sdp_resp));
    sdp_resp.security = chosen;
    sdp_resp.port = v2g_port;
    memcpy(sdp_resp.ip6_addr, evse_ip6, IPV6_ADDR_LEN);

    /* Build SDP response packet */
    if (sdp_build_response(resp, sizeof(resp),
                          evse_ip6, v2g_port, chosen, &sdp_resp) < 0) {
        fprintf(stderr, "[ERROR] Failed to build SDP response\n");
        close(sock);
        return -1;
    }

    /* Send SDP response back to EV */
    ssize_t sent = sendto(sock, resp, SDP_RESPONSE_LEN, 0,
                          (struct sockaddr *)&ev_addr, ev_len);
    if (sent < 0) {
        fprintf(stderr, "[ERROR] sendto: %s\n", strerror(errno));
        close(sock);
        return -1;
    }

    /* Verify all bytes were sent */
    if (sent != SDP_RESPONSE_LEN) {
        fprintf(stderr, "[ERROR] Partial SDP response: %zd of %d bytes\n", sent, SDP_RESPONSE_LEN);
        close(sock);
        return -1;
    }

    printf("[UDP] Sent SDP response to [%s]:%d (%zd bytes)\n",
           ev_str, ntohs(ev_addr.sin6_port), sent);

    printf("[UDP] SDP Response (hex): ");
    for (int i = 0; i < SDP_RESPONSE_LEN; i++) {
        printf("%02X ", resp[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    printf("\n");

    close(sock);
    return 0;
}


/*---------------------------------------------*/


