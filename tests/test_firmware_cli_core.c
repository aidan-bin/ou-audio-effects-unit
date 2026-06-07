#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cli_core.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    char output[512];
    size_t outputUsed;

    uint8_t lastPotIndex;
    uint32_t lastPotValue;
    uint8_t lastSwitchIndex;
    bool lastSwitchEnabled;
    char lastConfigKey[32];
    int32_t lastConfigValue;

    bool failService;
    uint8_t romReadBytes[4];
    size_t romReadSize;

    bool logEnabled;
    bool logStreamEnabled;
    uint8_t logLevel;
    uint32_t frameCount;
    uint32_t failureCount;

    bool testModeEnabled;
    uint8_t testVector;
    uint16_t testFrequencyHz;
    uint16_t testAmplitude;
} CliCoreTestContext;

static bool test_write(const char *text, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    size_t length = strlen(text);
    if (ctx->outputUsed + length >= sizeof(ctx->output))
    {
        return false;
    }

    memcpy(&ctx->output[ctx->outputUsed], text, length);
    ctx->outputUsed += length;
    ctx->output[ctx->outputUsed] = '\0';
    return true;
}

static bool set_pot_override(uint8_t potIndex, uint32_t value, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->lastPotIndex = potIndex;
    ctx->lastPotValue = value;
    return !ctx->failService;
}

static bool clear_pot_override(uint8_t potIndex, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->lastPotIndex = potIndex;
    return !ctx->failService;
}

static bool set_switch_override(uint8_t switchIndex, bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->lastSwitchIndex = switchIndex;
    ctx->lastSwitchEnabled = enabled;
    return !ctx->failService;
}

static bool clear_switch_override(uint8_t switchIndex, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->lastSwitchIndex = switchIndex;
    return !ctx->failService;
}

static bool clear_all_overrides(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->failService;
}

static bool config_set(const char *key, int32_t value, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    strncpy(ctx->lastConfigKey, key, sizeof(ctx->lastConfigKey) - 1);
    ctx->lastConfigValue = value;
    return !ctx->failService;
}

static bool config_get(const char *key, int32_t *valueOut, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    strncpy(ctx->lastConfigKey, key, sizeof(ctx->lastConfigKey) - 1);
    *valueOut = 42;
    return !ctx->failService;
}

static bool rom_save_state(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->failService;
}

static bool rom_load_state(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->failService;
}

static bool rom_read_raw(uint16_t address, uint16_t length, uint8_t *payloadOut,
                         size_t payloadCapacity, size_t *payloadSizeOut, void *context)
{
    (void)address;
    (void)length;
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->failService)
    {
        return false;
    }

    if (ctx->romReadSize > payloadCapacity)
    {
        return false;
    }

    memcpy(payloadOut, ctx->romReadBytes, ctx->romReadSize);
    *payloadSizeOut = ctx->romReadSize;
    return true;
}

static bool rom_write_raw(uint16_t address, const uint8_t *payload, size_t payloadSize,
                          void *context)
{
    (void)address;
    (void)payload;
    (void)payloadSize;
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->failService;
}

static bool log_set_level(uint8_t level, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->logLevel = level;
    return !ctx->failService;
}

static bool log_set_enabled(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->logEnabled = enabled;
    return !ctx->failService;
}

static bool log_set_stream(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->logStreamEnabled = enabled;
    ctx->logEnabled = enabled;
    return !ctx->failService;
}

static bool log_get_stats(CliLogStats *statsOut, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (statsOut == NULL)
    {
        return false;
    }

    statsOut->enabled = ctx->logEnabled;
    statsOut->level = ctx->logLevel;
    statsOut->frameCount = ctx->frameCount;
    statsOut->failureCount = ctx->failureCount;
    return !ctx->failService;
}

static bool test_set_mode(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testModeEnabled = enabled;
    return !ctx->failService;
}

static bool test_set_vector(uint8_t vector, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testVector = vector;
    return !ctx->failService;
}

static bool test_set_frequency_hz(uint16_t frequencyHz, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testFrequencyHz = frequencyHz;
    return !ctx->failService;
}

static bool test_set_amplitude(uint16_t amplitude, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testAmplitude = amplitude;
    return !ctx->failService;
}

static bool test_get_status(CliTestModeStatus *statusOut, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (statusOut == NULL)
    {
        return false;
    }

    statusOut->enabled = ctx->testModeEnabled;
    statusOut->vector = ctx->testVector;
    statusOut->frequencyHz = ctx->testFrequencyHz;
    statusOut->amplitude = ctx->testAmplitude;
    return !ctx->failService;
}

static CliServices make_services(CliCoreTestContext *ctx)
{
    CliServices services = {
        .set_pot_override = set_pot_override,
        .clear_pot_override = clear_pot_override,
        .set_switch_override = set_switch_override,
        .clear_switch_override = clear_switch_override,
        .clear_all_overrides = clear_all_overrides,
        .config_set = config_set,
        .config_get = config_get,
        .rom_save_state = rom_save_state,
        .rom_load_state = rom_load_state,
        .rom_read_raw = rom_read_raw,
        .rom_write_raw = rom_write_raw,
        .log_set_level = log_set_level,
        .log_set_enabled = log_set_enabled,
        .log_set_stream = log_set_stream,
        .log_get_stats = log_get_stats,
        .test_set_mode = test_set_mode,
        .test_set_vector = test_set_vector,
        .test_set_frequency_hz = test_set_frequency_hz,
        .test_set_amplitude = test_set_amplitude,
        .test_get_status = test_get_status,
        .context = ctx,
    };
    return services;
}

static CliIo make_io(CliCoreTestContext *ctx)
{
    CliIo io = {
        .write = test_write,
        .context = ctx,
    };
    return io;
}

static void test_ping_and_help(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("ping", &services, &io),
                  "ping returns ok");
    expect_true(strstr(ctx.output, "pong") != NULL, "ping writes pong");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("help", &services, &io),
                  "help returns ok");
    expect_true(strstr(ctx.output, "commands") != NULL, "help writes command list");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("help config", &services, &io),
                  "help <command> returns ok");
    expect_true(strstr(ctx.output, "config set|get") != NULL, "help command writes usage");
}

static void test_override_and_config_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("override pot set 2 1536", &services, &io),
                  "override pot set succeeds");
    expect_eq_u8(2, ctx.lastPotIndex, "pot index parsed");
    expect_eq_u32(1536, ctx.lastPotValue, "pot value parsed");
    expect_true(ctx.outputUsed == 0, "override pot set emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("override switch set 1 1", &services, &io),
                  "override switch set succeeds");
    expect_eq_u8(1, ctx.lastSwitchIndex, "switch index parsed");
    expect_true(ctx.lastSwitchEnabled, "switch enabled parsed");
    expect_true(ctx.outputUsed == 0, "override switch set emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("config set overdrive.level 77", &services, &io),
                  "config set succeeds");
    expect_true(strcmp("overdrive.level", ctx.lastConfigKey) == 0, "config key parsed");
    expect_eq_u32(77, (uint32_t)ctx.lastConfigValue, "config value parsed");
    expect_true(ctx.outputUsed == 0, "config set emits no success output");
}

static void test_rom_and_log_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);
    ctx.romReadBytes[0] = 0xAA;
    ctx.romReadBytes[1] = 0xBB;
    ctx.romReadSize = 2;

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("rom read 0x40 2", &services, &io),
                  "rom read succeeds");
    expect_true(strstr(ctx.output, "AABB") != NULL, "rom bytes formatted in output");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream", &services, &io),
                  "log stream succeeds");
    expect_true(ctx.logStreamEnabled, "log stream parsed");
    expect_true(ctx.logEnabled, "log stream enables logging");
    expect_true(strstr(ctx.output, "log stream active") != NULL,
                "log stream emits active status");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream 0", &services, &io),
                  "log stream off succeeds");
    expect_false(ctx.logStreamEnabled, "log stream disabled parsed");
    expect_true(strstr(ctx.output, "log stream stopped") != NULL,
                "log stream off emits stopped status");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    ctx.logLevel = 3;
    ctx.frameCount = 11;
    ctx.failureCount = 2;
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stats", &services, &io),
                  "log stats succeeds");
    expect_true(strstr(ctx.output, "frames=11") != NULL, "log stats includes frame count");
}

static void test_test_mode_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test mode 1", &services, &io),
                  "test mode command succeeds");
    expect_true(ctx.testModeEnabled, "test mode enabled state stored");
    expect_true(ctx.outputUsed == 0, "test mode emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test vector lut", &services, &io),
                  "test vector command succeeds");
    expect_eq_u8(1, ctx.testVector, "test vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test freq 1234", &services, &io),
                  "test freq command succeeds");
    expect_eq_u16(1234, ctx.testFrequencyHz, "test frequency parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test amp 4000", &services, &io),
                  "test amp command succeeds");
    expect_eq_u16(4000, ctx.testAmplitude, "test amplitude parsed");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test status", &services, &io),
                  "test status command succeeds");
    expect_true(strstr(ctx.output, "vector=lut") != NULL, "test status prints vector");
}

static void test_error_paths(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_UNKNOWN_COMMAND,
                  cli_core_process_line("does-not-exist", &services, &io),
                  "unknown command status");
    expect_true(strstr(ctx.output, "err unknown-command") != NULL,
                "unknown command error output");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_PARSE_ERROR,
                  cli_core_process_line("rom read nope 2", &services, &io),
                  "parse error status");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    ctx.failService = true;
    expect_eq_u32(CLI_STATUS_SERVICE_ERROR,
                  cli_core_process_line("rom save-state", &services, &io),
                  "service error status");
}

int main(void)
{
    test_ping_and_help();
    test_override_and_config_commands();
    test_rom_and_log_commands();
    test_test_mode_commands();
    test_error_paths();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli core tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli core tests passed");
    return 0;
}