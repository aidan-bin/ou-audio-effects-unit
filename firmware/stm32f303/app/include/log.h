#ifndef LOG_H
#define LOG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    LOG_LEVEL_OFF = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARN = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5,
} LogLevel;

typedef struct
{
    bool (*is_enabled)(void *context);
    uint8_t (*get_level)(void *context);
    bool (*write_line)(const char *line, void *context);
    void *context;
} LogOps;

void log_configure(const LogOps *ops);

bool log_write(LogLevel level, const char *message);
bool log_messagef(LogLevel level, const char *format, ...);

const char *log_level_name(LogLevel level);

#endif