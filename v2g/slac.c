/*
 * SLAC - Signal Level Attenuation Characterization Implementation
 * ISO 15118-3 / HomePlug Green PHY (HPGP)
 */

#include "slac.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <arpa/inet.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>

/* =========================================================================
 * Utilities
 * ========================================================================= */

static void get_timestamp(char *buf, size_t len) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    struct tm *tm_info = localtime(&tv.tv_sec);
    snprintf(buf, len, "%02d:%02d:%02d.%03ld",
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec,
             tv.tv_usec / 1000);
}

#define LOG(fmt, ...) do { \
    char _ts[32]; get_timestamp(_ts, sizeof(_ts)); \
    printf("[SLAC %s] " fmt "\n", _ts, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

static void print_mac(const char *label, const uint8_t *mac) {
    printf("  %-20s %02X:%02X:%02X:%02X:%02X:%02X\n", label,
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("  %-20s ", label);
    for (size_t i = 0; i < len; i++) printf("%02X ", data[i]);
    printf("\n");
}

static void rand_bytes(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++)
        buf[i] = (uint8_t)(rand() & 0xFF);
}

static uint64_t millis(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)(ts.tv_sec * 1000ULL + ts.tv_nsec / 1000000ULL);
}

static void ms_sleep(unsigned int ms) {
    struct timespec ts = { .tv_sec = ms / 1000,
                           .tv_nsec = (ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/* =========================================================================
 * Socket layer
 * ========================================================================= */

static int open_raw_socket(const char *iface, SlacContext *ctx) {
    int fd = socket(PF_PACKET, SOCK_RAW, htons(HOMEPLUG_ETHERTYPE));
    if (fd < 0) { 
        perror("socket");
        return -1; 
    }

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, iface, IFNAMSIZ - 1);

    if (ioctl(fd, SIOCGIFINDEX, &ifr) < 0) { 
        perror("SIOCGIFINDEX");
        close(fd);
        return -1;
    }
    ctx->iface_idx = ifr.ifr_ifindex;

    if (ioctl(fd, SIOCGIFHWADDR, &ifr) < 0) { 
        perror("SIOCGIFHWADDR");
        close(fd);
        return -1;
    }
    memcpy(ctx->local_mac, ifr.ifr_hwaddr.sa_data, 6);

    struct sockaddr_ll sll = {0};
    sll.sll_family   = AF_PACKET;
    sll.sll_protocol = htons(HOMEPLUG_ETHERTYPE);
    sll.sll_ifindex  = ctx->iface_idx;
    if (bind(fd, (struct sockaddr *)&sll, sizeof(sll)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    /* Set 500 ms receive timeout */
    struct timeval tv = { .tv_sec = 0, .tv_usec = 500000 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    LOG("Socket opened on %s  MAC=%02X:%02X:%02X:%02X:%02X:%02X",
        iface,
        ctx->local_mac[0], ctx->local_mac[1], ctx->local_mac[2],
        ctx->local_mac[3], ctx->local_mac[4], ctx->local_mac[5]);
    return fd;
}

static int send_frame(SlacContext *ctx, void *frame, size_t len) {
    struct sockaddr_ll sll = {0};
    sll.sll_family  = AF_PACKET;
    sll.sll_ifindex = ctx->iface_idx;
    sll.sll_halen   = 6;
    memcpy(sll.sll_addr, ((uint8_t *)frame), 6);  /* dst MAC */
    ssize_t sent = sendto(ctx->sock_fd, frame, len, 0,
                          (struct sockaddr *)&sll, sizeof(sll));
    if (sent < 0) { 
        perror("sendto");
        return -1; 
    }
    return 0;
}

static int recv_frame(SlacContext *ctx, uint8_t *buf, size_t buflen, uint16_t *mmtype) {
    ssize_t n = recv(ctx->sock_fd, buf, buflen, 0);
    if (n < (ssize_t)sizeof(EthMMEHeader)) return -1;
    EthMMEHeader *hdr = (EthMMEHeader *)buf;
    if (ntohs(hdr->ethertype) != HOMEPLUG_ETHERTYPE) return -1;
    *mmtype = le16toh(hdr->mmtype);
    return (int)n;
}

/* =========================================================================
 * Frame builders
 * ========================================================================= */

static void build_hdr(EthMMEHeader *hdr, const uint8_t *dst,
                      const uint8_t *src, uint16_t mmtype) {
    memcpy(hdr->dst, dst, 6);
    memcpy(hdr->src, src, 6);
    hdr->ethertype = htons(HOMEPLUG_ETHERTYPE);
    hdr->mmv       = 0x01;
    hdr->mmtype    = htole16(mmtype);
    hdr->fmi_fmsn  = 0x00;
}

/* =========================================================================
 * EV Role – Message Senders
 * ========================================================================= */

static int ev_send_parm_req(SlacContext *ctx) {
    CM_SLAC_PARM_REQ req;
    memset(&req, 0, sizeof(req));
    uint8_t bcast[] = BROADCAST_ADDR;
    build_hdr(&req.hdr, bcast, ctx->local_mac, MME_CM_SLAC_PARM_REQ);
    req.application_type = APPLICATION_TYPE;
    req.security_type    = SECURITY_TYPE;
    rand_bytes(ctx->run_id, 8);
    memcpy(req.run_id, ctx->run_id, 8);
    LOG("[EV] TX CM_SLAC_PARM.REQ  RUN_ID=%02X%02X%02X%02X...",
        ctx->run_id[0], ctx->run_id[1], ctx->run_id[2], ctx->run_id[3]);
    return send_frame(ctx, &req, sizeof(req));
}

static int ev_send_start_atten(SlacContext *ctx) {
    CM_START_ATTEN_IND ind;
    memset(&ind, 0, sizeof(ind));
    uint8_t bcast[] = BROADCAST_ADDR;
    build_hdr(&ind.hdr, bcast, ctx->local_mac, MME_CM_START_ATTEN_IND);
    ind.application_type = APPLICATION_TYPE;
    ind.security_type    = SECURITY_TYPE;
    ind.num_sounds       = NUM_SOUNDS;
    ind.time_out         = 6;   /* 600 ms */
    ind.resp_type        = 0x01;
    memcpy(ind.forwarding_sta, ctx->local_mac, 6);
    memcpy(ind.run_id, ctx->run_id, 8);
    LOG("[EV] TX CM_START_ATTEN_CHAR.IND  num_sounds=%d", NUM_SOUNDS);
    return send_frame(ctx, &ind, sizeof(ind));
}

static int ev_send_mnbc_sounds(SlacContext *ctx) {
    for (int i = NUM_SOUNDS - 1; i >= 0; i--) {
        CM_MNBC_SOUND_IND snd;
        memset(&snd, 0, sizeof(snd));
        uint8_t bcast[] = BROADCAST_ADDR;
        build_hdr(&snd.hdr, bcast, ctx->local_mac, MME_CM_MNBC_SOUND_IND);
        snd.application_type = APPLICATION_TYPE;
        snd.security_type    = SECURITY_TYPE;
        memcpy(snd.sender_id, ctx->local_mac, 6);
        snd.cnt = (uint8_t)i;
        memcpy(snd.run_id, ctx->run_id, 8);
        LOG("[EV] TX CM_MNBC_SOUND.IND  cnt=%d", i);
        if (send_frame(ctx, &snd, sizeof(snd)) < 0) return -1;
        ms_sleep(20);   /* 20 ms between sounds per spec */
    }
    return 0;
}

static int ev_send_atten_rsp(SlacContext *ctx, const uint8_t *evse_mac) {
    CM_ATTEN_CHAR_RSP rsp;
    memset(&rsp, 0, sizeof(rsp));
    build_hdr(&rsp.hdr, evse_mac, ctx->local_mac, MME_CM_ATTEN_CHAR_RSP);
    rsp.application_type = APPLICATION_TYPE;
    rsp.security_type    = SECURITY_TYPE;
    memcpy(rsp.source_address, evse_mac, 6);
    memcpy(rsp.run_id, ctx->run_id, 8);
    memcpy(rsp.source_id, evse_mac, 6);
    memcpy(rsp.resp_id, ctx->local_mac, 6);
    rsp.result = 0x00;
    LOG("[EV] TX CM_ATTEN_CHAR.RSP  result=OK");
    return send_frame(ctx, &rsp, sizeof(rsp));
}

static int ev_send_validate_req(SlacContext *ctx, const uint8_t *evse_mac) {
    CM_VALIDATE_REQ req;
    memset(&req, 0, sizeof(req));
    build_hdr(&req.hdr, evse_mac, ctx->local_mac, MME_CM_VALIDATE_REQ);
    req.signal_type = 0x00;
    req.timer       = 0x00;
    req.result      = 0x00;
    LOG("[EV] TX CM_VALIDATE.REQ");
    return send_frame(ctx, &req, sizeof(req));
}

static int ev_send_match_req(SlacContext *ctx, const uint8_t *evse_mac) {
    CM_SLAC_MATCH_REQ req;
    memset(&req, 0, sizeof(req));
    build_hdr(&req.hdr, evse_mac, ctx->local_mac, MME_CM_SLAC_MATCH_REQ);
    req.application_type = APPLICATION_TYPE;
    req.security_type    = SECURITY_TYPE;
    req.mv_length        = htole16(0x003E);
    memcpy(req.pev_mac,  ctx->local_mac, 6);
    memcpy(req.evse_mac, evse_mac, 6);
    memcpy(req.run_id,   ctx->run_id, 8);
    LOG("[EV] TX CM_SLAC_MATCH.REQ");
    return send_frame(ctx, &req, sizeof(req));
}

/* =========================================================================
 * EVSE Role – Message Senders
 * ========================================================================= */

static int evse_send_parm_cnf(SlacContext *ctx, const uint8_t *ev_mac,
                              const uint8_t *run_id) {
    CM_SLAC_PARM_CNF cnf;
    memset(&cnf, 0, sizeof(cnf));
    build_hdr(&cnf.hdr, ev_mac, ctx->local_mac, MME_CM_SLAC_PARM_CNF);
    memset(cnf.m_sound_target, 0xFF, 6);   /* broadcast */
    cnf.num_sounds       = NUM_SOUNDS;
    cnf.time_out         = 6;              /* 600 ms    */
    cnf.resp_type        = 0x01;
    memcpy(cnf.forwarding_sta, ctx->local_mac, 6);
    cnf.application_type = APPLICATION_TYPE;
    cnf.security_type    = SECURITY_TYPE;
    memcpy(cnf.run_id, run_id, 8);
    LOG("[EVSE] TX CM_SLAC_PARM.CNF");
    return send_frame(ctx, &cnf, sizeof(cnf));
}

static int evse_send_atten_char(SlacContext *ctx, const uint8_t *ev_mac,
                                const uint8_t *run_id) {
    CM_ATTEN_CHAR_IND ind;
    memset(&ind, 0, sizeof(ind));
    build_hdr(&ind.hdr, ev_mac, ctx->local_mac, MME_CM_ATTEN_CHAR_IND);
    ind.application_type = APPLICATION_TYPE;
    ind.security_type    = SECURITY_TYPE;
    memcpy(ind.source_address, ctx->local_mac, 6);
    memcpy(ind.run_id, run_id, 8);
    memcpy(ind.source_id, ctx->local_mac, 6);
    memcpy(ind.resp_id, ev_mac, 6);
    ind.num_sounds = ctx->sound_count;
    ind.num_groups = NUM_GROUPS;
    /* Average the accumulated group attenuations */
    for (int g = 0; g < NUM_GROUPS; g++)
        ind.aag[g] = (ctx->sound_count > 0)
                     ? ctx->aag[g] / ctx->sound_count : 0;
    LOG("[EVSE] TX CM_ATTEN_CHAR.IND  sounds_rx=%d", ctx->sound_count);
    return send_frame(ctx, &ind, sizeof(ind));
}

static int evse_send_validate_cnf(SlacContext *ctx, const uint8_t *ev_mac) {
    CM_VALIDATE_CNF cnf;
    memset(&cnf, 0, sizeof(cnf));
    build_hdr(&cnf.hdr, ev_mac, ctx->local_mac, MME_CM_VALIDATE_CNF);
    cnf.signal_type = 0x00;
    cnf.toggle_num  = 0x00;
    cnf.result      = 0x00;   /* success */
    LOG("[EVSE] TX CM_VALIDATE.CNF  result=OK");
    return send_frame(ctx, &cnf, sizeof(cnf));
}

static int evse_send_match_cnf(SlacContext *ctx, const uint8_t *ev_mac,
                               const uint8_t *run_id) {
    CM_SLAC_MATCH_CNF cnf;
    memset(&cnf, 0, sizeof(cnf));
    build_hdr(&cnf.hdr, ev_mac, ctx->local_mac, MME_CM_SLAC_MATCH_CNF);
    cnf.application_type = APPLICATION_TYPE;
    cnf.security_type    = SECURITY_TYPE;
    cnf.mv_length        = htole16(0x003E);
    memcpy(cnf.pev_mac,  ev_mac, 6);
    memcpy(cnf.evse_mac, ctx->local_mac, 6);
    memcpy(cnf.run_id,   run_id, 8);
    /* Generate random NMK and NID */
    rand_bytes(cnf.nmk, 16);
    rand_bytes(cnf.nid, 7);
    cnf.nid[6] &= 0xFC;   /* clear LS 2 bits per spec */
    memcpy(ctx->nmk, cnf.nmk, 16);
    memcpy(ctx->nid, cnf.nid, 7);
    LOG("[EVSE] TX CM_SLAC_MATCH.CNF");
    print_hex("NMK:", cnf.nmk, 16);
    print_hex("NID:", cnf.nid, 7);
    return send_frame(ctx, &cnf, sizeof(cnf));
}

/* =========================================================================
 * EV State Machine
 * ========================================================================= */

static int ev_step(SlacContext *ctx, uint8_t *buf, size_t bufsize, 
                   uint8_t *evse_mac) {
    uint16_t mmtype;
    int      rc;

    switch (ctx->state) {

    case SLAC_EV_SEND_PARM_REQ:
        if (ev_send_parm_req(ctx) < 0) { ctx->state = SLAC_EV_FAILED; break; }
        ctx->state = SLAC_EV_WAIT_PARM_CNF;
        ctx->last_time = millis();
        break;

    case SLAC_EV_WAIT_PARM_CNF:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_SLAC_PARM_CNF) {
            CM_SLAC_PARM_CNF *cnf = (CM_SLAC_PARM_CNF *)buf;
            memcpy(evse_mac, cnf->hdr.src, 6);
            LOG("[EV] RX CM_SLAC_PARM.CNF from %02X:%02X:%02X:%02X:%02X:%02X",
                evse_mac[0],evse_mac[1],evse_mac[2],
                evse_mac[3],evse_mac[4],evse_mac[5]);
            ctx->state = SLAC_EV_SEND_START_ATTEN;
        } else if (millis() - ctx->last_time > 400) {
            LOG("[EV] Timeout waiting for PARM.CNF (retry %d)", ++ctx->retry_count);
            if (ctx->retry_count >= C_EV_MATCH_RETRY) ctx->state = SLAC_EV_FAILED;
            else ctx->state = SLAC_EV_SEND_PARM_REQ;
        }
        break;

    case SLAC_EV_SEND_START_ATTEN:
        ev_send_start_atten(ctx);
        ctx->state = SLAC_EV_SEND_MNBC_SOUNDS;
        break;

    case SLAC_EV_SEND_MNBC_SOUNDS:
        ev_send_mnbc_sounds(ctx);
        ctx->state = SLAC_EV_WAIT_ATTEN_CHAR;
        ctx->last_time = millis();
        break;

    case SLAC_EV_WAIT_ATTEN_CHAR:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_ATTEN_CHAR_IND) {
            CM_ATTEN_CHAR_IND *ind = (CM_ATTEN_CHAR_IND *)buf;
            LOG("[EV] RX CM_ATTEN_CHAR.IND  num_sounds=%d  num_groups=%d",
                ind->num_sounds, ind->num_groups);
            printf("  AAG (first 10): ");
            for (int g = 0; g < 10 && g < ind->num_groups; g++)
                printf("%3d ", ind->aag[g]);
            printf("...\n");
            memcpy(ctx->aag, ind->aag, NUM_GROUPS);
            ctx->state = SLAC_EV_SEND_ATTEN_RSP;
        } else if (millis() - ctx->last_time > TT_EVSE_MATCH_MNBC_MS + 200) {
            LOG("[EV] Timeout waiting for ATTEN_CHAR.IND");
            ctx->state = SLAC_EV_FAILED;
        }
        break;

    case SLAC_EV_SEND_ATTEN_RSP:
        ev_send_atten_rsp(ctx, evse_mac);
        ctx->state = SLAC_EV_SEND_VALIDATE;
        break;

    case SLAC_EV_SEND_VALIDATE:
        ev_send_validate_req(ctx, evse_mac);
        ctx->state = SLAC_EV_WAIT_VALIDATE_CNF;
        ctx->last_time = millis();
        break;

    case SLAC_EV_WAIT_VALIDATE_CNF:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_VALIDATE_CNF) {
            CM_VALIDATE_CNF *cnf = (CM_VALIDATE_CNF *)buf;
            LOG("[EV] RX CM_VALIDATE.CNF  result=%02X", cnf->result);
            if (cnf->result == 0x00)
                ctx->state = SLAC_EV_SEND_MATCH_REQ;
            else
                ctx->state = SLAC_EV_FAILED;
        } else if (millis() - ctx->last_time > 1200) {
            LOG("[EV] Timeout waiting for VALIDATE.CNF");
            ctx->state = SLAC_EV_FAILED;
        }
        break;

    case SLAC_EV_SEND_MATCH_REQ:
        ev_send_match_req(ctx, evse_mac);
        ctx->state = SLAC_EV_WAIT_MATCH_CNF;
        ctx->last_time = millis();
        break;

    case SLAC_EV_WAIT_MATCH_CNF:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_SLAC_MATCH_CNF) {
            CM_SLAC_MATCH_CNF *cnf = (CM_SLAC_MATCH_CNF *)buf;
            memcpy(ctx->nmk, cnf->nmk, 16);
            memcpy(ctx->nid, cnf->nid, 7);
            LOG("[EV] RX CM_SLAC_MATCH.CNF");
            print_hex("NMK:", ctx->nmk, 16);
            print_hex("NID:", ctx->nid, 7);
            ctx->state = SLAC_EV_MATCHED;
        } else if (millis() - ctx->last_time > TT_MATCH_JOIN_MS) {
            LOG("[EV] Timeout waiting for MATCH.CNF");
            ctx->state = SLAC_EV_FAILED;
        }
        break;

    default: break;
    }

    return 0;
}

/* =========================================================================
 * EVSE State Machine
 * ========================================================================= */

static int evse_step(SlacContext *ctx, uint8_t *buf, size_t bufsize,
                     uint8_t *ev_mac, uint8_t *run_id) {
    uint16_t mmtype;
    int      rc;

    switch (ctx->state) {

    case SLAC_EVSE_WAIT_PARM_REQ:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_SLAC_PARM_REQ) {
            CM_SLAC_PARM_REQ *req = (CM_SLAC_PARM_REQ *)buf;
            memcpy(ev_mac, req->hdr.src, 6);
            memcpy(run_id, req->run_id, 8);
            LOG("[EVSE] RX CM_SLAC_PARM.REQ from %02X:%02X:%02X:%02X:%02X:%02X",
                ev_mac[0],ev_mac[1],ev_mac[2],ev_mac[3],ev_mac[4],ev_mac[5]);
            ctx->state = SLAC_EVSE_SEND_PARM_CNF;
        }
        break;

    case SLAC_EVSE_SEND_PARM_CNF:
        evse_send_parm_cnf(ctx, ev_mac, run_id);
        ctx->state = SLAC_EVSE_WAIT_START_ATTEN;
        ctx->last_time = millis();
        break;

    case SLAC_EVSE_WAIT_START_ATTEN:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_START_ATTEN_IND) {
            LOG("[EVSE] RX CM_START_ATTEN_CHAR.IND");
            ctx->state  = SLAC_EVSE_COLLECT_SOUNDS;
            ctx->sound_count = 0;
            ctx->last_time = millis();
        } else if (millis() - ctx->last_time > 800) {
            LOG("[EVSE] Timeout waiting for START_ATTEN");
            ctx->state = SLAC_EVSE_FAILED;
        }
        break;

    case SLAC_EVSE_COLLECT_SOUNDS:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_MNBC_SOUND_IND) {
            CM_MNBC_SOUND_IND *snd = (CM_MNBC_SOUND_IND *)buf;
            /* Simulate attenuation measurement: random dB per group */
            for (int g = 0; g < NUM_GROUPS; g++)
                ctx->aag[g] += (uint8_t)(20 + rand() % 20);
            ctx->sound_count++;
            LOG("[EVSE] RX CM_MNBC_SOUND.IND cnt=%d  (%d/%d)",
                snd->cnt, ctx->sound_count, NUM_SOUNDS);
            if (ctx->sound_count >= NUM_SOUNDS)
                ctx->state = SLAC_EVSE_SEND_ATTEN_CHAR;
        } else if (millis() - ctx->last_time > TT_EVSE_MATCH_MNBC_MS) {
            LOG("[EVSE] Sound collection timeout, got %d/%d sounds",
                ctx->sound_count, NUM_SOUNDS);
            if (ctx->sound_count > 0)
                ctx->state = SLAC_EVSE_SEND_ATTEN_CHAR;
            else
                ctx->state = SLAC_EVSE_FAILED;
        }
        break;

    case SLAC_EVSE_SEND_ATTEN_CHAR:
        evse_send_atten_char(ctx, ev_mac, run_id);
        ctx->state = SLAC_EVSE_WAIT_ATTEN_RSP;
        ctx->last_time = millis();
        break;

    case SLAC_EVSE_WAIT_ATTEN_RSP:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_ATTEN_CHAR_RSP) {
            CM_ATTEN_CHAR_RSP *rsp = (CM_ATTEN_CHAR_RSP *)buf;
            LOG("[EVSE] RX CM_ATTEN_CHAR.RSP  result=%02X", rsp->result);
            ctx->state = SLAC_EVSE_WAIT_VALIDATE_REQ;
            ctx->last_time = millis();
        } else if (millis() - ctx->last_time > 800) {
            LOG("[EVSE] Timeout waiting for ATTEN_CHAR.RSP");
            ctx->state = SLAC_EVSE_FAILED;
        }
        break;

    case SLAC_EVSE_WAIT_VALIDATE_REQ:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_VALIDATE_REQ) {
            LOG("[EVSE] RX CM_VALIDATE.REQ");
            ctx->state = SLAC_EVSE_SEND_VALIDATE_CNF;
        } else if (millis() - ctx->last_time > 1500) {
            LOG("[EVSE] Timeout waiting for VALIDATE.REQ");
            ctx->state = SLAC_EVSE_FAILED;
        }
        break;

    case SLAC_EVSE_SEND_VALIDATE_CNF:
        evse_send_validate_cnf(ctx, ev_mac);
        ctx->state = SLAC_EVSE_WAIT_MATCH_REQ;
        ctx->last_time = millis();
        break;

    case SLAC_EVSE_WAIT_MATCH_REQ:
        rc = recv_frame(ctx, buf, bufsize, &mmtype);
        if (rc > 0 && mmtype == MME_CM_SLAC_MATCH_REQ) {
            LOG("[EVSE] RX CM_SLAC_MATCH.REQ");
            ctx->state = SLAC_EVSE_SEND_MATCH_CNF;
        } else if (millis() - ctx->last_time > TT_MATCH_JOIN_MS) {
            LOG("[EVSE] Timeout waiting for MATCH.REQ");
            ctx->state = SLAC_EVSE_FAILED;
        }
        break;

    case SLAC_EVSE_SEND_MATCH_CNF:
        evse_send_match_cnf(ctx, ev_mac, run_id);
        ctx->state = SLAC_EVSE_MATCHED;
        break;

    default: break;
    }

    return 0;
}

/* =========================================================================
 * Public API
 * ========================================================================= */

int slac_init(SlacContext *ctx, const char *iface, bool is_ev) {
    memset(ctx, 0, sizeof(*ctx));
    ctx->is_ev = is_ev;
    ctx->state = is_ev ? SLAC_EV_IDLE : SLAC_EVSE_IDLE;
    ctx->retry_count = 0;

    ctx->sock_fd = open_raw_socket(iface, ctx);
    if (ctx->sock_fd < 0) return -1;

    LOG("SLAC initialized as %s on %s", is_ev ? "EV" : "EVSE", iface);
    return 0;
}

int slac_step(SlacContext *ctx) {
    static uint8_t buf[2048];
    static uint8_t evse_mac[6];
    static uint8_t ev_mac[6];
    static uint8_t run_id[8];

    if (ctx->is_ev) {
        ev_step(ctx, buf, sizeof(buf), evse_mac);
    } else {
        evse_step(ctx, buf, sizeof(buf), ev_mac, run_id);
    }

    if (ctx->state == SLAC_EV_MATCHED || ctx->state == SLAC_EVSE_MATCHED) {
        return 1;  /* Success */
    }
    if (ctx->state == SLAC_EV_FAILED || ctx->state == SLAC_EVSE_FAILED) {
        return -1; /* Failure */
    }
    return 0;  /* Still processing */
}

int slac_run(SlacContext *ctx) {
    if (ctx->is_ev) {
        ctx->state = SLAC_EV_SEND_PARM_REQ;
        LOG("=== EV SLAC sequence started ===");
    } else {
        ctx->state = SLAC_EVSE_WAIT_PARM_REQ;
        ctx->sound_count = 0;
        memset(ctx->aag, 0, sizeof(ctx->aag));
        LOG("=== EVSE SLAC listening ===");
    }

    while (ctx->state != SLAC_EV_MATCHED && ctx->state != SLAC_EV_FAILED &&
           ctx->state != SLAC_EVSE_MATCHED && ctx->state != SLAC_EVSE_FAILED) {
        slac_step(ctx);
    }

    if (ctx->state == SLAC_EV_MATCHED) {
        LOG("=== EV SLAC MATCHED SUCCESSFULLY ===");
        return 0;
    } else if (ctx->state == SLAC_EVSE_MATCHED) {
        LOG("=== EVSE SLAC MATCHED SUCCESSFULLY ===");
        return 0;
    }

    LOG("=== SLAC FAILED ===");
    return -1;
}

void slac_cleanup(SlacContext *ctx) {
    if (ctx->sock_fd >= 0) {
        close(ctx->sock_fd);
        ctx->sock_fd = -1;
    }
    LOG("SLAC cleanup complete");
}

const char *slac_state_to_string(SlacState state) {
    switch (state) {
    case SLAC_EV_IDLE: return "EV_IDLE";
    case SLAC_EV_SEND_PARM_REQ: return "EV_SEND_PARM_REQ";
    case SLAC_EV_WAIT_PARM_CNF: return "EV_WAIT_PARM_CNF";
    case SLAC_EV_SEND_START_ATTEN: return "EV_SEND_START_ATTEN";
    case SLAC_EV_SEND_MNBC_SOUNDS: return "EV_SEND_MNBC_SOUNDS";
    case SLAC_EV_WAIT_ATTEN_CHAR: return "EV_WAIT_ATTEN_CHAR";
    case SLAC_EV_SEND_ATTEN_RSP: return "EV_SEND_ATTEN_RSP";
    case SLAC_EV_SEND_VALIDATE: return "EV_SEND_VALIDATE";
    case SLAC_EV_WAIT_VALIDATE_CNF: return "EV_WAIT_VALIDATE_CNF";
    case SLAC_EV_SEND_MATCH_REQ: return "EV_SEND_MATCH_REQ";
    case SLAC_EV_WAIT_MATCH_CNF: return "EV_WAIT_MATCH_CNF";
    case SLAC_EV_MATCHED: return "EV_MATCHED";
    case SLAC_EV_FAILED: return "EV_FAILED";
    case SLAC_EVSE_IDLE: return "EVSE_IDLE";
    case SLAC_EVSE_WAIT_PARM_REQ: return "EVSE_WAIT_PARM_REQ";
    case SLAC_EVSE_SEND_PARM_CNF: return "EVSE_SEND_PARM_CNF";
    case SLAC_EVSE_WAIT_START_ATTEN: return "EVSE_WAIT_START_ATTEN";
    case SLAC_EVSE_COLLECT_SOUNDS: return "EVSE_COLLECT_SOUNDS";
    case SLAC_EVSE_SEND_ATTEN_CHAR: return "EVSE_SEND_ATTEN_CHAR";
    case SLAC_EVSE_WAIT_ATTEN_RSP: return "EVSE_WAIT_ATTEN_RSP";
    case SLAC_EVSE_WAIT_VALIDATE_REQ: return "EVSE_WAIT_VALIDATE_REQ";
    case SLAC_EVSE_SEND_VALIDATE_CNF: return "EVSE_SEND_VALIDATE_CNF";
    case SLAC_EVSE_WAIT_MATCH_REQ: return "EVSE_WAIT_MATCH_REQ";
    case SLAC_EVSE_SEND_MATCH_CNF: return "EVSE_SEND_MATCH_CNF";
    case SLAC_EVSE_MATCHED: return "EVSE_MATCHED";
    case SLAC_EVSE_FAILED: return "EVSE_FAILED";
    default: return "UNKNOWN";
    }
}
