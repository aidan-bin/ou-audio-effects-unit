#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "log.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    bool enabled;
    uint8_t level;
    uint32_t writeCalls;
    char lastLine[128];
} LogTestContext;

static bool log_is_enabled(void *context)
{
    LogTestContext *ctx = (LogTestContext *)context;
    return ctx != NULL && ctx->enabled;
}

static uint8_t log_get_level(void *context)
{
    LogTestContext *ctx = (LogTestContext *)context;
    return ctx == NULL ? 0U : ctx->level;
}

static bool log_write_line(const char *line, void *context)
{
    LogTestContext *ctx = (LogTestContext *)context;
    if (ctx == NULL || line == NULL)
    {
        return false;
    }

    ctx->writeCalls++;
    (void)strncpy(ctx->lastLine, line, sizeof(ctx->lastLine) - 1U);
    ctx->lastLine[sizeof(ctx->lastLine) - 1U] = '\0';
    return true;
}

static void test_log_filters_by_enabled_and_level(void)
{
    LogTestContext context = {
        .enabled = false,
        .level = (uint8_t)LOG_LEVEL_TRACE,
        .writeCalls = 0,
    };

    LogOps ops = {
        .is_enabled = log_is_enabled,
        .get_level = log_get_level,
        .write_line = log_write_line,
        .context = &context,
    };

    log_configure(&ops);

    expect_false(log_write(LOG_LEVEL_INFO, "disabled path"), "disabled log is filtered");
    expect_eq_u32(0, context.writeCalls, "disabled log does not write");

    context.enabled = true;
    context.level = (uint8_t)LOG_LEVEL_WARN;

    expect_false(log_write(LOG_LEVEL_INFO, "below threshold"),
                 "below threshold log is filtered");
    expect_eq_u32(0, context.writeCalls, "below threshold log does not write");

    expect_true(log_write(LOG_LEVEL_ERROR, "critical event"),
                "error level log is emitted");
    expect_eq_u32(1, context.writeCalls, "error level increments writes");
    expect_true(strstr(context.lastLine, "[error] critical event") != NULL,
                "error line contains level prefix");
}

static void test_logf_formats_with_prefix(void)
{
    LogTestContext context = {
        .enabled = true,
        .level = (uint8_t)LOG_LEVEL_DEBUG,
        .writeCalls = 0,
    };

    LogOps ops = {
        .is_enabled = log_is_enabled,
        .get_level = log_get_level,
        .write_line = log_write_line,
        .context = &context,
    };

    log_configure(&ops);

    expect_true(log_messagef(LOG_LEVEL_DEBUG, "value=%u", 17U),
                "formatted debug log emitted");
    expect_eq_u32(1, context.writeCalls, "formatted log increments writes");
    expect_true(strstr(context.lastLine, "[debug] value=17") != NULL,
                "formatted log contains message and prefix");
}

static void test_log_config_reset_disables_output(void)
{
    LogTestContext context = {
        .enabled = true,
        .level = (uint8_t)LOG_LEVEL_TRACE,
        .writeCalls = 0,
    };

    LogOps ops = {
        .is_enabled = log_is_enabled,
        .get_level = log_get_level,
        .write_line = log_write_line,
        .context = &context,
    };

    log_configure(&ops);
    expect_true(log_write(LOG_LEVEL_INFO, "before reset"), "log writes before reset");

    log_configure(NULL);
    expect_false(log_write(LOG_LEVEL_INFO, "after reset"), "log disabled after reset");
    expect_eq_u32(1, context.writeCalls, "reset prevents additional writes");
}

int main(void)
{
    test_log_filters_by_enabled_and_level();
    test_logf_formats_with_prefix();
    test_log_config_reset_disables_output();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware app log tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware app log tests passed");
    return 0;
}
