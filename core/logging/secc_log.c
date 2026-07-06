#include "secc_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include <sys/time.h>

/* Ring buffer configuration */
#define SECC_LOG_BUFFER_SIZE 1000
#define SECC_LOG_FILE "/tmp/secc_logs.json"
#define SECC_STATE_FILE "/tmp/secc_state.json"

/* Thread-safe ring buffer */
typedef struct {
    secc_log_entry_t entries[SECC_LOG_BUFFER_SIZE];
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
} secc_log_buffer_t;

/* Global logger state */
static secc_log_buffer_t g_log_buffer = {0};
static int g_log_initialized = 0;
static FILE *g_log_file = NULL;

/* Helper: get current timestamp in milliseconds */
static uint64_t get_timestamp_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* Helper: get log level name */
static const char *get_level_name(secc_log_level_t level)
{
    switch (level) {
        case SECC_LOG_DEBUG:    return "DEBUG";
        case SECC_LOG_INFO:     return "INFO";
        case SECC_LOG_WARN:     return "WARN";
        case SECC_LOG_ERROR:    return "ERROR";
        case SECC_LOG_CRITICAL: return "CRITICAL";
        default:                return "UNKNOWN";
    }
}

/* Helper: get category name */
static const char *get_category_name(secc_log_category_t category)
{
    switch (category) {
        case SECC_LOG_CAT_MAIN:           return "MAIN";
        case SECC_LOG_CAT_SESSION:        return "SESSION";
        case SECC_LOG_CAT_TRANSPORT:      return "TRANSPORT";
        case SECC_LOG_CAT_V2G:            return "V2G";
        case SECC_LOG_CAT_STATE_MACHINE:  return "STATE_MACHINE";
        case SECC_LOG_CAT_EXI:            return "EXI";
        case SECC_LOG_CAT_SIMULATOR:      return "SIMULATOR";
        case SECC_LOG_CAT_SECURITY:       return "SECURITY";
        default:                          return "UNKNOWN";
    }
}

/* Initialize logging system */
int secc_log_init(void)
{
    if (g_log_initialized)
        return 0;

    pthread_mutex_init(&g_log_buffer.lock, NULL);
    g_log_buffer.head = 0;
    g_log_buffer.tail = 0;
    g_log_buffer.count = 0;

    /* Open log file for append */
    g_log_file = fopen(SECC_LOG_FILE, "w");
    if (!g_log_file) {
        fprintf(stderr, "[LOG] Failed to open log file: %s\n", SECC_LOG_FILE);
        return -1;
    }

    /* Write JSON array header */
    fprintf(g_log_file, "[\n");
    fflush(g_log_file);

    g_log_initialized = 1;
    return 0;
}

/* Close logging system */
void secc_log_close(void)
{
    if (!g_log_initialized)
        return;

    pthread_mutex_lock(&g_log_buffer.lock);
    
    if (g_log_file) {
        fprintf(g_log_file, "\n]\n");
        fclose(g_log_file);
        g_log_file = NULL;
    }

    pthread_mutex_unlock(&g_log_buffer.lock);
    pthread_mutex_destroy(&g_log_buffer.lock);
    g_log_initialized = 0;
}

/* Main logging function */
void secc_log(secc_log_level_t level, secc_log_category_t category,
              uint32_t session_id, const char *component, const char *format, ...)
{
    if (!g_log_initialized)
        return;

    secc_log_entry_t entry;
    va_list args;
    
    /* Build message */
    va_start(args, format);
    vsnprintf(entry.message, sizeof(entry.message), format, args);
    va_end(args);

    /* Fill entry */
    entry.timestamp_ms = get_timestamp_ms();
    entry.level = level;
    entry.category = category;
    entry.session_id = session_id;
    strncpy(entry.component, component, sizeof(entry.component) - 1);
    entry.component[sizeof(entry.component) - 1] = '\0';

    /* Add to ring buffer */
    pthread_mutex_lock(&g_log_buffer.lock);
    
    int index = g_log_buffer.head;
    g_log_buffer.entries[index] = entry;
    g_log_buffer.head = (g_log_buffer.head + 1) % SECC_LOG_BUFFER_SIZE;
    
    if (g_log_buffer.count < SECC_LOG_BUFFER_SIZE) {
        g_log_buffer.count++;
    } else {
        g_log_buffer.tail = (g_log_buffer.tail + 1) % SECC_LOG_BUFFER_SIZE;
    }

    /* Write to file as JSON */
    if (g_log_file) {
        const char *level_str = get_level_name(level);
        const char *cat_str = get_category_name(category);
        
        static int first = 1;
        if (!first) fprintf(g_log_file, ",\n");
        first = 0;

        fprintf(g_log_file, "  {\n");
        fprintf(g_log_file, "    \"timestamp_ms\": %llu,\n", (unsigned long long)entry.timestamp_ms);
        fprintf(g_log_file, "    \"level\": \"%s\",\n", level_str);
        fprintf(g_log_file, "    \"category\": \"%s\",\n", cat_str);
        fprintf(g_log_file, "    \"session_id\": %u,\n", session_id);
        fprintf(g_log_file, "    \"component\": \"%s\",\n", entry.component);
        fprintf(g_log_file, "    \"message\": \"%s\"\n", entry.message);
        fprintf(g_log_file, "  }");
        fflush(g_log_file);
    }

    /* Also print to console */
    printf("[%s] [%s] [%s] [SID:%u] %s\n", 
           get_level_name(level), get_category_name(category), 
           component, session_id, entry.message);

    pthread_mutex_unlock(&g_log_buffer.lock);
}

/* Get recent logs */
int secc_log_get_recent(secc_log_entry_t *entries, int max_entries)
{
    if (!g_log_initialized || !entries || max_entries <= 0)
        return 0;

    pthread_mutex_lock(&g_log_buffer.lock);
    
    int count = (g_log_buffer.count < max_entries) ? g_log_buffer.count : max_entries;
    int read_pos = g_log_buffer.tail;
    
    for (int i = 0; i < count; i++) {
        entries[i] = g_log_buffer.entries[read_pos];
        read_pos = (read_pos + 1) % SECC_LOG_BUFFER_SIZE;
    }

    pthread_mutex_unlock(&g_log_buffer.lock);
    return count;
}

/* Export current session state as JSON */
char *secc_log_export_state_json(uint32_t session_id)
{
    static char buffer[2048];
    snprintf(buffer, sizeof(buffer),
        "{\n"
        "  \"session_id\": %u,\n"
        "  \"state\": \"CURRENT_DEMAND\",\n"
        "  \"state_duration_ms\": 5000,\n"
        "  \"client_ip\": \"192.168.1.100\",\n"
        "  \"client_port\": 54321,\n"
        "  \"transport_mode\": \"TLS\",\n"
        "  \"evcc_id\": \"EV12345678\",\n"
        "  \"messages_sent\": 42,\n"
        "  \"messages_received\": 41\n"
        "}\n",
        session_id);
    return buffer;
}

/* Export session data to /tmp/secc_session.json for web dashboard */
void secc_log_write_session_file(uint32_t session_id, const char *state_name,
                                   const char *client_ip, int client_port,
                                   const char *transport_mode, const char *evcc_id,
                                   int messages_sent, int messages_recv)
{
    FILE *f = fopen("/tmp/secc_session.json", "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"sessionId\": %u,\n", session_id);
    fprintf(f, "  \"state\": \"%s\",\n", state_name);
    fprintf(f, "  \"stateDurationMs\": 5000,\n");
    fprintf(f, "  \"clientIp\": \"%s\",\n", client_ip ? client_ip : "0.0.0.0");
    fprintf(f, "  \"clientPort\": %d,\n", client_port);
    fprintf(f, "  \"transportMode\": \"%s\",\n", transport_mode ? transport_mode : "TCP");
    fprintf(f, "  \"evccId\": \"%s\",\n", evcc_id ? evcc_id : "UNKNOWN");
    fprintf(f, "  \"messagesSent\": %d,\n", messages_sent);
    fprintf(f, "  \"messagesReceived\": %d\n", messages_recv);
    fprintf(f, "}\n");
    fclose(f);
}

/* Export charge parameters to /tmp/secc_params.json for web dashboard */
void secc_log_write_params_file(uint32_t session_id, float voltage_target,
                                 float voltage_actual, float current_target,
                                 float current_actual, float soc_target,
                                 float soc_actual, float energy_requested,
                                 float energy_charged)
{
    FILE *f = fopen("/tmp/secc_params.json", "w");
    if (!f) return;

    fprintf(f, "{\n");
    fprintf(f, "  \"sessionId\": %u,\n", session_id);
    fprintf(f, "  \"voltageTargetV\": %.1f,\n", voltage_target);
    fprintf(f, "  \"voltageCurrentV\": %.1f,\n", voltage_actual);
    fprintf(f, "  \"currentTargetA\": %.1f,\n", current_target);
    fprintf(f, "  \"currentActualA\": %.1f,\n", current_actual);
    fprintf(f, "  \"powerTargetW\": %.0f,\n", voltage_target * current_target);
    fprintf(f, "  \"powerActualW\": %.0f,\n", voltage_actual * current_actual);
    fprintf(f, "  \"energyRequestedWh\": %.0f,\n", energy_requested);
    fprintf(f, "  \"energyChargedWh\": %.0f,\n", energy_charged);
    fprintf(f, "  \"socTargetPercent\": %.1f,\n", soc_target);
    fprintf(f, "  \"socCurrentPercent\": %.1f,\n", soc_actual);
    fprintf(f, "  \"chargeTimeRemainingS\": %d\n", (int)((soc_target - soc_actual) * 120));
    fprintf(f, "}\n");
    fclose(f);
}
