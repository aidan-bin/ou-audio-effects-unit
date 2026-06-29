#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cli_core.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    size_t output_used;
    size_t rom_read_size;
    size_t i2c_scanned_count;
    size_t i2c_transfer_tx_len;
    size_t i2c_transfer_rx_len;
    size_t slot_count;

    uint32_t last_pot_value;
    int32_t last_config_value;
    uint32_t frame_count;
    uint32_t failure_count;
    uint32_t step_failure_count;
    uint32_t step_failure_streak;

    uint16_t test_input_frequency_hz;
    uint16_t test_input_amplitude;
    uint16_t test_output_frequency_hz;
    uint16_t test_output_amplitude;

    uint8_t last_pot_index;
    uint8_t last_switch_index;
    bool last_switch_enabled;
    bool fail_service;
    bool log_enabled;
    bool log_stream_enabled;
    uint8_t log_level;
    uint8_t stream_batch_size;
    uint8_t stream_queue_count;
    bool test_input_mode_enabled;
    uint8_t test_input_vector;
    bool test_output_mode_enabled;
    uint8_t test_output_vector;
    uint8_t i2c_last_address;
    bool reboot_called;
    uint8_t active_slot;

    uint8_t rom_read_bytes[4];
    uint8_t slots[8];
    uint8_t slot_enabled[8];
    char last_config_key[32];
    uint8_t i2c_transfer_tx_buf[64];
    uint8_t i2c_transfer_rx_buf[64];
    uint8_t i2c_scanned_addrs[128];
    char output[512];
} CliCoreTestContext;

static void reset_output(CliCoreTestContext *ctx)
{
    ctx->output_used = 0;
    ctx->output[0] = '\0';
}

static bool test_write(const char *text, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    size_t length = strlen(text);
    if (ctx->output_used + length >= sizeof(ctx->output))
    {
        return false;
    }

    memcpy(&ctx->output[ctx->output_used], text, length);
    ctx->output_used += length;
    ctx->output[ctx->output_used] = '\0';
    return true;
}

static bool set_pot_override(uint8_t pot_index, uint32_t value, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->last_pot_index = pot_index;
    ctx->last_pot_value = value;
    return !ctx->fail_service;
}

static bool clear_pot_override(uint8_t pot_index, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->last_pot_index = pot_index;
    return !ctx->fail_service;
}

static bool set_switch_override(uint8_t switch_index, bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->last_switch_index = switch_index;
    ctx->last_switch_enabled = enabled;
    return !ctx->fail_service;
}

static bool clear_switch_override(uint8_t switch_index, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->last_switch_index = switch_index;
    return !ctx->fail_service;
}

static bool clear_all_overrides(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->fail_service;
}

static bool config_set(const char *key, int32_t value, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    strncpy(ctx->last_config_key, key, sizeof(ctx->last_config_key) - 1);
    ctx->last_config_value = value;
    return !ctx->fail_service;
}

static bool config_get(const char *key, int32_t *value_out, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    strncpy(ctx->last_config_key, key, sizeof(ctx->last_config_key) - 1);
    *value_out = 42;
    return !ctx->fail_service;
}

static bool slot_get(uint8_t *slots_out, uint8_t *enabled_out, size_t capacity, size_t *count_out,
                     void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->slot_count > capacity)
    {
        return false;
    }
    memcpy(slots_out, ctx->slots, ctx->slot_count);
    memcpy(enabled_out, ctx->slot_enabled, ctx->slot_count);
    *count_out = ctx->slot_count;
    return !ctx->fail_service;
}

static bool slot_assign(uint8_t pos, uint8_t effect, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->fail_service || pos >= ctx->slot_count)
    {
        return false;
    }
    for (size_t i = 0; i < ctx->slot_count; i++)
    {
        if (i != pos && ctx->slots[i] == effect)
        {
            ctx->slots[i] = 0xFFu;
            ctx->slot_enabled[i] = 0;
        }
    }
    ctx->slots[pos] = effect;
    return true;
}

static bool slot_clear(uint8_t pos, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->fail_service || pos >= ctx->slot_count)
    {
        return false;
    }
    ctx->slots[pos] = 0xFFu;
    ctx->slot_enabled[pos] = 0;
    return true;
}

static bool slot_swap(uint8_t a, uint8_t b, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->fail_service || a >= ctx->slot_count || b >= ctx->slot_count)
    {
        return false;
    }
    uint8_t ts = ctx->slots[a];
    ctx->slots[a] = ctx->slots[b];
    ctx->slots[b] = ts;
    uint8_t te = ctx->slot_enabled[a];
    ctx->slot_enabled[a] = ctx->slot_enabled[b];
    ctx->slot_enabled[b] = te;
    return true;
}

static bool slot_set_enabled(uint8_t pos, bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->fail_service || pos >= ctx->slot_count)
    {
        return false;
    }
    ctx->slot_enabled[pos] = enabled ? 1 : 0;
    return true;
}

static bool slot_get_active(uint8_t *pos_out, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    *pos_out = ctx->active_slot;
    return !ctx->fail_service;
}

static bool slot_set_active(uint8_t pos, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->fail_service || pos >= ctx->slot_count)
    {
        return false;
    }
    ctx->active_slot = pos;
    return true;
}

static bool rom_save_state(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->fail_service;
}

static bool rom_load_state(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->fail_service;
}

static bool rom_read_raw(uint16_t address, uint16_t length, uint8_t *payload_out,
                         size_t payload_capacity, size_t *payload_size_out, void *context)
{
    (void)address;
    (void)length;
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (ctx->fail_service)
    {
        return false;
    }

    if (ctx->rom_read_size > payload_capacity)
    {
        return false;
    }

    memcpy(payload_out, ctx->rom_read_bytes, ctx->rom_read_size);
    *payload_size_out = ctx->rom_read_size;
    return true;
}

static bool rom_write_raw(uint16_t address, const uint8_t *payload, size_t payload_size,
                          void *context)
{
    (void)address;
    (void)payload;
    (void)payload_size;
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    return !ctx->fail_service;
}

static bool log_set_level(uint8_t level, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->log_level = level;
    return !ctx->fail_service;
}

static bool log_set_enabled(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->log_enabled = enabled;
    return !ctx->fail_service;
}

static bool log_set_stream(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->log_stream_enabled = enabled;
    ctx->log_enabled = enabled;
    return !ctx->fail_service;
}

static bool log_set_stream_batch(uint8_t batch_size, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->stream_batch_size = batch_size;
    return !ctx->fail_service;
}

static bool log_reset_stats(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->frame_count = 0;
    ctx->failure_count = 0;
    return !ctx->fail_service;
}

static bool log_get_stats(CliLogStats *stats_out, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (stats_out == NULL)
    {
        return false;
    }

    stats_out->enabled = ctx->log_enabled;
    stats_out->stream_enabled = ctx->log_stream_enabled;
    stats_out->level = ctx->log_level;
    stats_out->frame_count = ctx->frame_count;
    stats_out->failure_count = ctx->failure_count;
    stats_out->step_failure_count = ctx->step_failure_count;
    stats_out->step_failure_streak = ctx->step_failure_streak;
    stats_out->frame_time_min_us = 0;
    stats_out->frame_time_max_us = 0;
    stats_out->frame_time_total_us = 0;
    stats_out->measured_frame_count = 0;
    stats_out->overrun_count = 0;
    stats_out->stream_drop_count = 0;
    stats_out->stream_batch_size = ctx->stream_batch_size;
    stats_out->stream_queue_count = ctx->stream_queue_count;
    return !ctx->fail_service;
}

static bool test_set_input_mode(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_input_mode_enabled = enabled;
    return !ctx->fail_service;
}

static bool test_set_input_vector(uint8_t vector, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_input_vector = vector;
    return !ctx->fail_service;
}

static bool test_set_input_frequency_hz(uint16_t frequency_hz, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_input_frequency_hz = frequency_hz;
    return !ctx->fail_service;
}

static bool test_set_input_amplitude(uint16_t amplitude, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_input_amplitude = amplitude;
    return !ctx->fail_service;
}

static bool test_get_input_status(CliTestModeStatus *status_out, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (status_out == NULL)
    {
        return false;
    }

    status_out->enabled = ctx->test_input_mode_enabled;
    status_out->vector = ctx->test_input_vector;
    status_out->frequency_hz = ctx->test_input_frequency_hz;
    status_out->amplitude = ctx->test_input_amplitude;
    return !ctx->fail_service;
}

static bool test_set_output_mode(bool enabled, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_output_mode_enabled = enabled;
    return !ctx->fail_service;
}

static bool test_set_output_vector(uint8_t vector, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_output_vector = vector;
    return !ctx->fail_service;
}

static bool test_set_output_frequency_hz(uint16_t frequency_hz, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_output_frequency_hz = frequency_hz;
    return !ctx->fail_service;
}

static bool test_set_output_amplitude(uint16_t amplitude, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->test_output_amplitude = amplitude;
    return !ctx->fail_service;
}

static bool test_get_output_status(CliTestModeStatus *status_out, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    if (status_out == NULL)
    {
        return false;
    }

    status_out->enabled = ctx->test_output_mode_enabled;
    status_out->vector = ctx->test_output_vector;
    status_out->frequency_hz = ctx->test_output_frequency_hz;
    status_out->amplitude = ctx->test_output_amplitude;
    return !ctx->fail_service;
}

static bool i2c_ping_mock(uint8_t address, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->i2c_last_address = address;
    return !ctx->fail_service;
}

static bool i2c_transfer_mock(uint8_t address, const uint8_t *tx_data, size_t tx_len,
                              uint8_t *rx_data, size_t *rx_len, uint32_t timeout_ms,
                              void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    (void)timeout_ms;
    ctx->i2c_last_address = address;
    if (tx_data != NULL && tx_len > 0)
    {
        size_t copy = tx_len < sizeof(ctx->i2c_transfer_tx_buf) ? tx_len : sizeof(ctx->i2c_transfer_tx_buf);
        memcpy(ctx->i2c_transfer_tx_buf, tx_data, copy);
        ctx->i2c_transfer_tx_len = copy;
    }
    else
    {
        ctx->i2c_transfer_tx_len = 0;
    }
    if (rx_data != NULL && rx_len != NULL && *rx_len > 0)
    {
        size_t copy = *rx_len < sizeof(ctx->i2c_transfer_rx_buf) ? *rx_len : sizeof(ctx->i2c_transfer_rx_buf);
        memcpy(rx_data, ctx->i2c_transfer_rx_buf, copy);
        *rx_len = copy;
    }
    return !ctx->fail_service;
}

static bool i2c_scan_mock(uint8_t start_addr, uint8_t end_addr,
                          uint8_t *found_addrs, size_t *count,
                          size_t max_count, void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    size_t to_copy = ctx->i2c_scanned_count < max_count ? ctx->i2c_scanned_count : max_count;
    memcpy(found_addrs, ctx->i2c_scanned_addrs, to_copy);
    *count = to_copy;
    return !ctx->fail_service;
}

static bool system_reboot(void *context)
{
    CliCoreTestContext *ctx = (CliCoreTestContext *)context;
    ctx->reboot_called = true;
    return !ctx->fail_service;
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
        .slot_get = slot_get,
        .slot_assign = slot_assign,
        .slot_clear = slot_clear,
        .slot_swap = slot_swap,
        .slot_set_enabled = slot_set_enabled,
        .slot_get_active = slot_get_active,
        .slot_set_active = slot_set_active,
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
        .i2c_ping = i2c_ping_mock,
        .i2c_transfer = i2c_transfer_mock,
        .i2c_scan = i2c_scan_mock,
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

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("help", &services, &io),
                  "help returns ok");
    expect_true(strstr(ctx.output, "commands") != NULL, "help writes command list");

    reset_output(&ctx);
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
    expect_eq_u8(2, ctx.last_pot_index, "pot index parsed");
    expect_eq_u32(1536, ctx.last_pot_value, "pot value parsed");
    expect_true(ctx.output_used == 0, "override pot set emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("override switch set 1 1", &services, &io),
                  "override switch set succeeds");
    expect_eq_u8(1, ctx.last_switch_index, "switch index parsed");
    expect_true(ctx.last_switch_enabled, "switch enabled parsed");
    expect_true(ctx.output_used == 0, "override switch set emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("config set overdrive.level 77", &services, &io),
                  "config set succeeds");
    expect_true(strcmp("overdrive.level", ctx.last_config_key) == 0, "config key parsed");
    expect_eq_u32(77, (uint32_t)ctx.last_config_value, "config value parsed");
    expect_true(ctx.output_used == 0, "config set emits no success output");
}

static void test_rom_and_log_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);
    ctx.rom_read_bytes[0] = 0xAA;
    ctx.rom_read_bytes[1] = 0xBB;
    ctx.rom_read_size = 2;

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("rom read 0x40 2", &services, &io),
                  "rom read succeeds");
    expect_true(strstr(ctx.output, "AABB") != NULL, "rom bytes formatted in output");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream", &services, &io),
                  "log stream succeeds");
    expect_true(ctx.log_stream_enabled, "log stream parsed");
    expect_true(ctx.log_enabled, "log stream enables logging");
    expect_true(strstr(ctx.output, "log stream active") != NULL,
                "log stream emits active status");

    reset_output(&ctx);
    ctx.log_level = 3;
    ctx.frame_count = 11;
    ctx.failure_count = 2;
    ctx.log_stream_enabled = false;
    ctx.step_failure_count = 0;
    ctx.step_failure_streak = 0;
    ctx.stream_batch_size = 1;
    ctx.stream_queue_count = 0;
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stats", &services, &io),
                  "log stats succeeds");
    expect_true(strstr(ctx.output, "frames=11") != NULL, "log stats includes frame count");
    expect_true(strstr(ctx.output, "stream=0") != NULL, "log stats includes stream state");

    reset_output(&ctx);
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
    expect_true(ctx.test_input_mode_enabled, "test input mode enabled state stored");
    expect_true(ctx.output_used == 0, "test input mode emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input vector lut", &services, &io),
                  "test input vector command succeeds");
    expect_eq_u8(1, ctx.test_input_vector, "test input vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input vector sweep", &services, &io),
                  "test input sweep vector command succeeds");
    expect_eq_u8(2, ctx.test_input_vector, "test input sweep vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input freq 1234", &services, &io),
                  "test input freq command succeeds");
    expect_eq_u16(1234, ctx.test_input_frequency_hz, "test input frequency parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test input amp 4000", &services, &io),
                  "test input amp command succeeds");
    expect_eq_u16(4000, ctx.test_input_amplitude, "test input amplitude parsed");

    reset_output(&ctx);
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
    expect_true(ctx.test_output_mode_enabled, "test output mode enabled state stored");
    expect_true(ctx.output_used == 0, "test output mode emits no success output");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output vector lut", &services, &io),
                  "test output vector command succeeds");
    expect_eq_u8(1, ctx.test_output_vector, "test output vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output vector sweep", &services, &io),
                  "test output sweep vector command succeeds");
    expect_eq_u8(2, ctx.test_output_vector, "test output sweep vector parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output freq 5678", &services, &io),
                  "test output freq command succeeds");
    expect_eq_u16(5678, ctx.test_output_frequency_hz, "test output frequency parsed");

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("test output amp 8000", &services, &io),
                  "test output amp command succeeds");
    expect_eq_u16(8000, ctx.test_output_amplitude, "test output amplitude parsed");

    reset_output(&ctx);
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
                  cli_core_process_line("log stream batch 5", &services, &io),
                  "log stream batch 5 succeeds");
    expect_true(ctx.log_stream_enabled, "log stream enabled with batch");
    expect_eq_u8(5, ctx.stream_batch_size, "batch size 5 parsed");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stream batch 1", &services, &io),
                  "log stream batch 1 succeeds");
    expect_eq_u8(1, ctx.stream_batch_size, "batch size 1 parsed");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_PARSE_ERROR,
                  cli_core_process_line("log stream batch 0", &services, &io),
                  "log stream batch 0 rejected as invalid");
}

static void test_log_stats_reset(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);
    ctx.frame_count = 42;
    ctx.failure_count = 5;

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("log stats reset", &services, &io),
                  "log stats reset succeeds");
    expect_eq_u32(0, ctx.frame_count, "frame count reset");
    expect_eq_u32(0, ctx.failure_count, "failure count reset");
}

static void test_reboot_command(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("reboot", &services, &io),
                  "reboot returns ok");
    expect_true(ctx.reboot_called, "reboot service called");
    expect_true(strstr(ctx.output, "reboot ok") != NULL, "reboot writes confirmation");
}

static void test_i2c_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("i2c ping 0x50", &services, &io),
                  "i2c ping succeeds");
    expect_eq_u8(0x50, ctx.i2c_last_address, "i2c ping address captured");
    expect_true(strstr(ctx.output, "i2c ping ok") != NULL, "i2c ping writes ok");

    reset_output(&ctx);
    ctx.fail_service = true;
    expect_eq_u32(CLI_STATUS_SERVICE_ERROR,
                  cli_core_process_line("i2c ping 0x50", &services, &io),
                  "i2c ping fails on service error");
    ctx.fail_service = false;

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_INVALID_ARGUMENTS,
                  cli_core_process_line("i2c ping", &services, &io),
                  "i2c ping with no addr returns invalid args");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_PARSE_ERROR,
                  cli_core_process_line("i2c ping xx", &services, &io),
                  "i2c ping with non-numeric addr returns parse error");

    reset_output(&ctx);
    ctx.i2c_transfer_rx_buf[0] = 0xDE;
    ctx.i2c_transfer_rx_buf[1] = 0xAD;
    ctx.i2c_transfer_rx_len = 2;
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("i2c read 0x50 0x00 2", &services, &io),
                  "i2c read succeeds");
    expect_true(strstr(ctx.output, "DE") != NULL, "i2c read data DE");
    expect_true(strstr(ctx.output, "AD") != NULL, "i2c read data AD");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("i2c write 0x50 0x00 DEAD", &services, &io),
                  "i2c write succeeds");
    expect_true(ctx.i2c_transfer_tx_len >= 3, "i2c write includes reg + data");
    expect_eq_u8(0x00, ctx.i2c_transfer_tx_buf[0], "i2c write first byte is reg");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("i2c send 0x50 DEADBEEF", &services, &io),
                  "i2c send succeeds");
    expect_eq_u8(0x50, ctx.i2c_last_address, "i2c send address");

    reset_output(&ctx);
    ctx.i2c_transfer_rx_buf[0] = 0xCA;
    ctx.i2c_transfer_rx_buf[1] = 0xFE;
    ctx.i2c_transfer_rx_len = 2;
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("i2c recv 0x50 2", &services, &io),
                  "i2c recv succeeds");
    expect_true(strstr(ctx.output, "CA") != NULL, "i2c recv data CA");
    expect_true(strstr(ctx.output, "FE") != NULL, "i2c recv data FE");

    reset_output(&ctx);
    ctx.i2c_scanned_addrs[0] = 0x3C;
    ctx.i2c_scanned_addrs[1] = 0x50;
    ctx.i2c_scanned_count = 2;
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("i2c scan", &services, &io),
                  "i2c scan succeeds");
    expect_true(strstr(ctx.output, "0x3C") != NULL, "i2c scan found 0x3C");
    expect_true(strstr(ctx.output, "0x50") != NULL, "i2c scan found 0x50");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK,
                  cli_core_process_line("i2c scan 0x10 0x20", &services, &io),
                  "i2c scan with range succeeds");
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

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_PARSE_ERROR,
                  cli_core_process_line("rom read nope 2", &services, &io),
                  "parse error status");

    reset_output(&ctx);
    ctx.fail_service = true;
    expect_eq_u32(CLI_STATUS_SERVICE_ERROR,
                  cli_core_process_line("rom save-state", &services, &io),
                  "service error status");
}

static void test_slot_commands(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    // overdrive(0) echo(1) compression(2) empty, only overdrive enabled.
    ctx.slots[0] = 0;
    ctx.slots[1] = 1;
    ctx.slots[2] = 2;
    ctx.slots[3] = 0xFFu;
    ctx.slot_enabled[0] = 1;
    ctx.slot_count = 4;

    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("slot", &services, &io), "slot get ok");
    expect_true(strstr(ctx.output, "0=overdrive(on)") != NULL, "slot list shows overdrive on");
    expect_true(strstr(ctx.output, "3=empty") != NULL, "slot list shows empty slot");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("slot set 3 echo", &services, &io),
                  "slot set ok");
    expect_eq_u8(1, ctx.slots[3], "echo assigned to slot3");
    expect_eq_u8(0xFFu, ctx.slots[1], "echo vacated from slot1");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("slot enable 3 1", &services, &io),
                  "slot enable ok");
    expect_true(ctx.slot_enabled[3], "slot3 enabled");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("slot clear 0", &services, &io),
                  "slot clear ok");
    expect_eq_u8(0xFFu, ctx.slots[0], "slot0 cleared");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("slot swap 2 3", &services, &io),
                  "slot swap ok");
    expect_eq_u8(1, ctx.slots[2], "slot2 now echo after swap");
    expect_eq_u8(2, ctx.slots[3], "slot3 now compression after swap");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_PARSE_ERROR,
                  cli_core_process_line("slot set 0 foo", &services, &io),
                  "slot set unknown effect parse error");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_INVALID_ARGUMENTS,
                  cli_core_process_line("slot set 0", &services, &io),
                  "slot set missing arg invalid");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_SERVICE_ERROR,
                  cli_core_process_line("slot clear 9", &services, &io),
                  "slot clear out-of-range rejected by service");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_UNKNOWN_COMMAND,
                  cli_core_process_line("slot frobnicate", &services, &io),
                  "unknown slot subcommand");
}

static void test_effects_command(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    ctx.slots[0] = 0;     // overdrive
    ctx.slots[1] = 0xFFu; // empty
    ctx.slots[2] = 2;     // compression
    ctx.slots[3] = 0xFFu; // empty
    ctx.slot_count = 4;

    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("effects", &services, &io), "effects ok");
    expect_true(strstr(ctx.output, "overdrive@0") != NULL, "overdrive assigned to slot0");
    expect_true(strstr(ctx.output, "echo(unassigned)") != NULL, "echo unassigned");
    expect_true(strstr(ctx.output, "compression@2") != NULL, "compression at slot2");
}

static void test_slot_active_command(void)
{
    CliCoreTestContext ctx = {0};
    CliServices services = make_services(&ctx);
    CliIo io = make_io(&ctx);

    ctx.slots[0] = 0;
    ctx.slots[1] = 1;
    ctx.slots[2] = 2;
    ctx.slots[3] = 0xFFu;
    ctx.slot_count = 4;
    ctx.active_slot = 1;

    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("slot active", &services, &io),
                  "slot active get ok");
    expect_true(strstr(ctx.output, "active slot 1 echo") != NULL, "slot active shows echo");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_OK, cli_core_process_line("slot active 2", &services, &io),
                  "slot active set ok");
    expect_eq_u8(2, ctx.active_slot, "active slot updated");

    reset_output(&ctx);
    expect_eq_u32(CLI_STATUS_SERVICE_ERROR, cli_core_process_line("slot active 9", &services, &io),
                  "slot active out-of-range rejected by service");
}

int main(void)
{
    test_ping_and_help();
    test_override_and_config_commands();
    test_slot_commands();
    test_slot_active_command();
    test_effects_command();
    test_rom_and_log_commands();
    test_log_stream_batch_command();
    test_log_stats_reset();
    test_test_input_mode_commands();
    test_test_output_mode_commands();
    test_reboot_command();
    test_i2c_commands();
    test_error_paths();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli core tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli core tests passed");
    return 0;
}