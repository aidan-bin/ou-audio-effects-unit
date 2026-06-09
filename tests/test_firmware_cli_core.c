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
    uint32_t stepFailureCount;
    uint32_t stepFailureStreak;
    uint8_t streamBatchSize;
    uint8_t streamQueueCount;

    bool testInputModeEnabled;
    uint8_t testInputVector;
    uint16_t testInputFrequencyHz;
    uint16_t testInputAmplitude;

    bool testOutputModeEnabled;
    uint8_t testOutputVector;
    uint16_t testOutputFrequencyHz;
    uint16_t testOutputAmplitude;

    bool rebootCalled;
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

static bool log_set_stream_batch(uint8_t batchSize, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->streamBatchSize = batchSize;
    return !ctx->failService;
}

static bool log_reset_stats(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->frameCount = 0;
    ctx->failureCount = 0;
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
    statsOut->streamEnabled = ctx->logStreamEnabled;
    statsOut->level = ctx->logLevel;
    statsOut->frameCount = ctx->frameCount;
    statsOut->failureCount = ctx->failureCount;
    statsOut->stepFailureCount = ctx->stepFailureCount;
    statsOut->stepFailureStreak = ctx->stepFailureStreak;
    statsOut->frameTimeMinUs = 0;
    statsOut->frameTimeMaxUs = 0;
    statsOut->frameTimeTotalUs = 0;
    statsOut->measuredFrameCount = 0;
    statsOut->overrunCount = 0;
    statsOut->streamDropCount = 0;
    statsOut->streamBatchSize = ctx->streamBatchSize;
    statsOut->streamQueueCount = ctx->streamQueueCount;
    return !ctx->failService;
}

static bool test_set_input_mode(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testInputModeEnabled = enabled;
    return !ctx->failService;
}

static bool test_set_input_vector(uint8_t vector, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testInputVector = vector;
    return !ctx->failService;
}

static bool test_set_input_frequency_hz(uint16_t frequencyHz, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testInputFrequencyHz = frequencyHz;
    return !ctx->failService;
}

static bool test_set_input_amplitude(uint16_t amplitude, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testInputAmplitude = amplitude;
    return !ctx->failService;
}

static bool test_get_input_status(CliTestModeStatus *statusOut, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (statusOut == NULL)
    {
        return false;
    }

    statusOut->enabled = ctx->testInputModeEnabled;
    statusOut->vector = ctx->testInputVector;
    statusOut->frequencyHz = ctx->testInputFrequencyHz;
    statusOut->amplitude = ctx->testInputAmplitude;
    return !ctx->failService;
}

static bool test_set_output_mode(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testOutputModeEnabled = enabled;
    return !ctx->failService;
}

static bool test_set_output_vector(uint8_t vector, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testOutputVector = vector;
    return !ctx->failService;
}

static bool test_set_output_frequency_hz(uint16_t frequencyHz, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testOutputFrequencyHz = frequencyHz;
    return !ctx->failService;
}

static bool test_set_output_amplitude(uint16_t amplitude, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->testOutputAmplitude = amplitude;
    return !ctx->failService;
}

static bool test_get_output_status(CliTestModeStatus *statusOut, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (statusOut == NULL)
    {
        return false;
    }

    statusOut->enabled = ctx->testOutputModeEnabled;
    statusOut->vector = ctx->testOutputVector;
    statusOut->frequencyHz = ctx->testOutputFrequencyHz;
    statusOut->amplitude = ctx->testOutputAmplitude;
    return !ctx->failService;
}

static bool system_reboot(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->rebootCalled = true;
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
        .log_set_stream_batch = log_set_stream_batch,
        .log_get_stats = log_get_stats,
        .log_reset_stats = log_reset_stats,
        .test_set_input_mode = test_set_input_mode,
        .test_set_input_vector = test_set_input_vector,
        .test_set_input_frequency_hz = test_set_input_frequency_hz,
        .test_set_input_amplitude = test_set_input_amplitude,
        .test_get_input_status = test_get_input_status,

        .test_set_output_mode = test_set_output_mode,
        .test_set_output_vector = test_set_output_vector,
        .test_set_output_frequency_hz = test_set_output_frequency_hz,
        .test_set_output_amplitude = test_set_output_amplitude,
        .test_get_output_status = test_get_output_status,
        .system_reboot = system_reboot,
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
    ctx.logStreamEnabled = false;
    ctx.stepFailureCount = 0;
    ctx.stepFailureStreak = 0;
    ctx.streamBatchSize = 1;
    ctx.streamQueueCount = 0;
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stats", &services, &io),
                  "log stats succeeds");
    expect_true(strstr(ctx.output, "frames=11") != NULL, "log stats includes frame count");
    expect_true(strstr(ctx.output, "stream=0") != NULL, "log stats includes stream state");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stats timing", &services, &io),
                  "log stats timing succeeds");
    expect_true(strstr(ctx.output, "log timing") != NULL,
                "log stats timing emits timing line");
}

static void test_test_input_mode_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input mode 1", &services, &io),
                  "test input mode command succeeds");
    expect_true(ctx.testInputModeEnabled, "test input mode enabled state stored");
    expect_true(ctx.outputUsed == 0, "test input mode emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input vector lut", &services, &io),
                  "test input vector command succeeds");
    expect_eq_u8(1, ctx.testInputVector, "test input vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input vector sweep", &services, &io),
                  "test input sweep vector command succeeds");
    expect_eq_u8(2, ctx.testInputVector, "test input sweep vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input freq 1234", &services, &io),
                  "test input freq command succeeds");
    expect_eq_u16(1234, ctx.testInputFrequencyHz, "test input frequency parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input amp 4000", &services, &io),
                  "test input amp command succeeds");
    expect_eq_u16(4000, ctx.testInputAmplitude, "test input amplitude parsed");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input status", &services, &io),
                  "test input status command succeeds");
    expect_true(strstr(ctx.output, "vector=sweep") != NULL, "test input status prints sweep vector");
}

static void test_test_output_mode_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output mode 1", &services, &io),
                  "test output mode command succeeds");
    expect_true(ctx.testOutputModeEnabled, "test output mode enabled state stored");
    expect_true(ctx.outputUsed == 0, "test output mode emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output vector lut", &services, &io),
                  "test output vector command succeeds");
    expect_eq_u8(1, ctx.testOutputVector, "test output vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output vector sweep", &services, &io),
                  "test output sweep vector command succeeds");
    expect_eq_u8(2, ctx.testOutputVector, "test output sweep vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output freq 5678", &services, &io),
                  "test output freq command succeeds");
    expect_eq_u16(5678, ctx.testOutputFrequencyHz, "test output frequency parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output amp 8000", &services, &io),
                  "test output amp command succeeds");
    expect_eq_u16(8000, ctx.testOutputAmplitude, "test output amplitude parsed");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output status", &services, &io),
                  "test output status command succeeds");
    expect_true(strstr(ctx.output, "mode=1") != NULL, "test output status prints mode");
    expect_true(strstr(ctx.output, "vector=sweep") != NULL, "test output status prints sweep vector");
    expect_true(strstr(ctx.output, "freq_hz=5678") != NULL, "test output status prints freq");
    expect_true(strstr(ctx.output, "amp=8000") != NULL, "test output status prints amp");
}

static void test_log_stream_batch_command(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream 1 batch 5", &services, &io),
                  "log stream batch 5 succeeds");
    expect_true(ctx.logStreamEnabled, "log stream enabled with batch");
    expect_eq_u8(5, ctx.streamBatchSize, "batch size 5 parsed");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream 1 batch 1", &services, &io),
                  "log stream batch 1 succeeds");
    expect_eq_u8(1, ctx.streamBatchSize, "batch size 1 parsed");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream batch 10", &services, &io),
                  "log stream batch 10 without bool prefix succeeds");
    expect_true(ctx.logStreamEnabled, "log stream enabled without bool prefix");
    expect_eq_u8(10, ctx.streamBatchSize, "batch size 10 parsed");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_PARSE_ERROR,
                  cli_core_process_line("log stream 1 batch 0", &services, &io),
                  "log stream batch 0 rejected as invalid");

    ctx.outputUsed = 0;
    ctx.output[0] = '\0';
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream 0", &services, &io),
                  "log stream off works independently");
    expect_false(ctx.logStreamEnabled, "log stream disabled");
    expect_true(strstr(ctx.output, "logging disabled") != NULL,
                "log stream off explains logging is disabled");
}

static void test_log_stats_reset(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);
    ctx.frameCount = 42;
    ctx.failureCount = 5;

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stats reset", &services, &io),
                  "log stats reset succeeds");
    expect_eq_u32(0, ctx.frameCount, "frame count reset");
    expect_eq_u32(0, ctx.failureCount, "failure count reset");
}

static void test_reboot_command(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("reboot", &services, &io),
                  "reboot returns ok");
    expect_true(ctx.rebootCalled, "reboot service called");
    expect_true(strstr(ctx.output, "reboot ok") != NULL, "reboot writes confirmation");
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
    test_log_stream_batch_command();
    test_log_stats_reset();
    test_test_input_mode_commands();
    test_test_output_mode_commands();
    test_reboot_command();
    test_error_paths();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli core tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli core tests passed");
    return 0;
}