#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_LINE_MAX 96U

static LogOps g_log_ops = {0};

const char *log_level_name(LogLevel level)
{
    switch (level)
    {
    case LOG_LEVEL_ERROR:
        return "error";
    case LOG_LEVEL_WARN:
        return "warn";
    case LOG_LEVEL_INFO:
        return "info";
    case LOG_LEVEL_DEBUG:
        return "debug";
    case LOG_LEVEL_TRACE:
        return "trace";
    case LOG_LEVEL_OFF:
    default:
        return "off";
    }
}

void log_configure(const LogOps *ops)
{
    if (ops == NULL)
    {
        (void)memset(&g_log_ops, 0, sizeof(g_log_ops));
        return;
    }

    g_log_ops = *ops;
}

static bool log_should_emit(LogLevel level)
{
    if (level <= LOG_LEVEL_OFF || g_log_ops.write_line == NULL)
    {
        return false;
    }

    if (g_log_ops.is_enabled != NULL && !g_log_ops.is_enabled(g_log_ops.context))
    {
        return false;
    }

    uint8_t configured_level = (uint8_t)LOG_LEVEL_TRACE;
    if (g_log_ops.get_level != NULL)
    {
        configured_level = g_log_ops.get_level(g_log_ops.context);
    }

    return configured_level >= (uint8_t)level;
}

bool log_write(LogLevel level, const char *message)
{
    if (message == NULL || !log_should_emit(level))
    {
        return false;
    }

    char line[LOG_LINE_MAX] = {0};
    (void)snprintf(line, sizeof(line), "[%s] %s", log_level_name(level), message);

    return g_log_ops.write_line(line, g_log_ops.context);
}

bool log_messagef(LogLevel level, const char *format, ...)
{
    if (format == NULL || !log_should_emit(level))
    {
        return false;
    }

    char message[LOG_LINE_MAX] = {0};
    va_list args;
    va_start(args, format);
    (void)vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    return log_write(level, message);
}