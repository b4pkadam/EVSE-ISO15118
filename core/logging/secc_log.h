#ifndef SECC_LOG_H
#define SECC_LOG_H

#include <stdint.h>
#include <time.h>

/* Log levels */
typedef enum {
    SECC_LOG_DEBUG = 0,
    SECC_LOG_INFO = 1,
    SECC_LOG_WARN = 2,
    SECC_LOG_ERROR = 3,
    SECC_LOG_CRITICAL = 4
} secc_log_level_t;

/* Log categories */
typedef enum {
    SECC_LOG_CAT_MAIN = 0,
    SECC_LOG_CAT_SESSION = 1,
    SECC_LOG_CAT_TRANSPORT = 2,
    SECC_LOG_CAT_V2G = 3,
    SECC_LOG_CAT_STATE_MACHINE = 4,
    SECC_LOG_CAT_EXI = 5,
    SECC_LOG_CAT_SIMULATOR = 6,
    SECC_LOG_CAT_SECURITY = 7
} secc_log_category_t;

/* Structured log entry */
typedef struct {
    uint64_t timestamp_ms;          /* Milliseconds since epoch */
    secc_log_level_t level;         /* Log level */
    secc_log_category_t category;   /* Log category */
    uint32_t session_id;            /* Session ID (0 if N/A) */
    char message[512];              /* Log message */
    char component[32];             /* Component name */
} secc_log_entry_t;

/* Initialize logging system */
int secc_log_init(void);

/* Close logging system */
void secc_log_close(void);

/* Main logging function */
void secc_log(secc_log_level_t level, secc_log_category_t category,
              uint32_t session_id, const char *component, const char *format, ...);

/* Convenience macros */
#define SECC_LOG_DEBUG(cat, sid, comp, fmt, ...) \
    secc_log(SECC_LOG_DEBUG, cat, sid, comp, fmt, ##__VA_ARGS__)

#define SECC_LOG_INFO(cat, sid, comp, fmt, ...) \
    secc_log(SECC_LOG_INFO, cat, sid, comp, fmt, ##__VA_ARGS__)

#define SECC_LOG_WARN(cat, sid, comp, fmt, ...) \
    secc_log(SECC_LOG_WARN, cat, sid, comp, fmt, ##__VA_ARGS__)

#define SECC_LOG_ERROR(cat, sid, comp, fmt, ...) \
    secc_log(SECC_LOG_ERROR, cat, sid, comp, fmt, ##__VA_ARGS__)

#define SECC_LOG_CRITICAL(cat, sid, comp, fmt, ...) \
    secc_log(SECC_LOG_CRITICAL, cat, sid, comp, fmt, ##__VA_ARGS__)

/* Get recent logs for web dashboard */
int secc_log_get_recent(secc_log_entry_t *entries, int max_entries);

/* Export current session state as JSON */
char *secc_log_export_state_json(uint32_t session_id);

/* Export session data to /tmp/secc_session.json for web dashboard */
void secc_log_write_session_file(uint32_t session_id, const char *state_name,
                                   const char *client_ip, int client_port,
                                   const char *transport_mode, const char *evcc_id,
                                   int messages_sent, int messages_recv);

/* Export charge parameters to /tmp/secc_params.json for web dashboard */
void secc_log_write_params_file(uint32_t session_id, float voltage_target,
                                 float voltage_actual, float current_target,
                                 float current_actual, float soc_target,
                                 float soc_actual, float energy_requested,
                                 float energy_charged);

/* Export charge parameters as JSON */
char *secc_log_export_params_json(uint32_t session_id);

#endif /* SECC_LOG_H */
