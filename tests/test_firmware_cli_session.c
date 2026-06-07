#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cli_session.h"
#include "harness/expect.h"

int failures = 0;

typedef struct
{
    char output[2048];
    size_t output_used;
} CliSessionTestContext;

typedef struct
{
    bool log_stream_enabled;
    char pending_log_line[64];
    bool has_pending_log_line;
} SessionServicesContext;

static bool test_log_set_stream(bool enabled, void *context)
{
    SessionServicesContext *service_context = (SessionServicesContext *)context;
    service_context->log_stream_enabled = enabled;
    return true;
}

static bool test_log_read_line(char *line_out,
                               size_t line_capacity,
                               bool *has_line_out,
                               void *context)
{
    SessionServicesContext *service_context = (SessionServicesContext *)context;
    if (line_out == NULL || line_capacity == 0 || has_line_out == NULL)
    {
        return false;
    }

    if (!service_context->has_pending_log_line)
    {
        *has_line_out = false;
        line_out[0] = '\0';
        return true;
    }

    strncpy(line_out, service_context->pending_log_line, line_capacity - 1U);
    line_out[line_capacity - 1U] = '\0';
    service_context->has_pending_log_line = false;
    *has_line_out = true;
    return true;
}

static bool test_write(const char *text, void *context)
{
    CliSessionTestContext *ctx = (CliSessionTestContext *)context;
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

static size_t count_occurrences(const char *text, const char *needle)
{
    size_t count = 0;
    const char *cursor = text;
    size_t needle_length = strlen(needle);

    if (needle_length == 0)
    {
        return 0;
    }

    while ((cursor = strstr(cursor, needle)) != NULL)
    {
        count++;
        cursor += needle_length;
    }

    return count;
}

static CliSession make_session(CliSessionTestContext *ctx, CliServices *services)
{
    CliSessionTransport transport = {
        .write = test_write,
        .context = ctx,
    };

    CliSession session;
    cli_session_init(&session, services, &transport);
    return session;
}

static void test_start_writes_banner_once(void)
{
    CliSessionTestContext ctx = {0};
    CliServices services = {0};
    CliSession session = make_session(&ctx, &services);

    expect_true(cli_session_start(&session), "start succeeds first call");
    expect_true(cli_session_start(&session), "start succeeds second call");

    expect_eq_size(1, count_occurrences(ctx.output, "OU Audio Effects CLI\r\n"),
                   "banner title emitted once");
    expect_eq_size(1, count_occurrences(ctx.output, "Type help for commands\r\n"),
                   "banner hint emitted once");
    expect_eq_size(1, count_occurrences(ctx.output, "> "), "initial prompt emitted once");
}

static void test_ping_echo_and_prompt(void)
{
    CliSessionTestContext ctx = {0};
    CliServices services = {0};
    CliSession session = make_session(&ctx, &services);
    (void)cli_session_start(&session);

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t input[] = {'p', 'i', 'n', 'g', '\r'};
    cli_session_push_bytes(&session, input, sizeof(input));

    expect_true(strstr(ctx.output, "ping\r\n") != NULL, "typed input echoed with newline");
    expect_true(strstr(ctx.output, "pong\r\n") != NULL, "ping result uses CRLF");
    expect_true(strstr(ctx.output, "> ") != NULL, "prompt emitted after command");
}

static void test_backspace_and_crlf_pair(void)
{
    CliSessionTestContext ctx = {0};
    CliServices services = {0};
    CliSession session = make_session(&ctx, &services);
    (void)cli_session_start(&session);

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t corrected_ping[] = {'p', 'i', 'n', 'g', 'x', '\b', '\r'};
    cli_session_push_bytes(&session, corrected_ping, sizeof(corrected_ping));

    expect_true(strstr(ctx.output, "pingx\b \b\r\n") != NULL,
                "backspace erases one character in echo");
    expect_true(strstr(ctx.output, "pong\r\n") != NULL, "corrected line executes");

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t help_crlf[] = {'h', 'e', 'l', 'p', '\r', '\n'};
    cli_session_push_bytes(&session, help_crlf, sizeof(help_crlf));

    expect_eq_size(1, count_occurrences(ctx.output, "commands:"),
                   "CRLF executes command once");
    expect_eq_size(1, count_occurrences(ctx.output, "> "),
                   "CRLF emits one prompt after command");
}

static void test_arrow_key_escape_sequences_are_ignored(void)
{
    CliSessionTestContext ctx = {0};
    CliServices services = {0};
    CliSession session = make_session(&ctx, &services);
    (void)cli_session_start(&session);

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t input[] = {'p', 'i', 'n', 'g', 0x1BU, '[', 'A', '\r'};
    cli_session_push_bytes(&session, input, sizeof(input));

    expect_true(strstr(ctx.output, "ping\r\n") != NULL,
                "arrow-key escape sequence does not alter command echo");
    expect_true(strstr(ctx.output, "pong\r\n") != NULL,
                "arrow-key escape sequence does not break command execution");
}

static void test_arrow_key_escape_sequence_split_across_reads(void)
{
    CliSessionTestContext ctx = {0};
    CliServices services = {0};
    CliSession session = make_session(&ctx, &services);
    (void)cli_session_start(&session);

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t prefix[] = {'p', 'i', 'n', 'g', 0x1BU};
    const uint8_t csi_and_end[] = {'[', 'A', '\r'};

    cli_session_push_bytes(&session, prefix, sizeof(prefix));
    cli_session_push_bytes(&session, csi_and_end, sizeof(csi_and_end));

    expect_true(strstr(ctx.output, "ping\r\n") != NULL,
                "split arrow-key sequence does not alter command echo");
    expect_true(strstr(ctx.output, "pong\r\n") != NULL,
                "split arrow-key sequence does not break command execution");
}

static void test_log_stream_command_hides_prompt_until_quit(void)
{
    CliSessionTestContext ctx = {0};
    SessionServicesContext services_context = {.log_stream_enabled = false};
    CliServices services = {0};
    services.log_set_stream = test_log_set_stream;
    services.log_read_line = test_log_read_line;
    services.context = &services_context;

    CliSession session = make_session(&ctx, &services);
    (void)cli_session_start(&session);

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t command[] = {'l', 'o', 'g', ' ', 's', 't', 'r', 'e', 'a', 'm', '\r'};
    cli_session_push_bytes(&session, command, sizeof(command));

    expect_true(services_context.log_stream_enabled, "log stream command enables stream mode");
    expect_true(strstr(ctx.output, "log stream active") != NULL,
                "log stream command emits active status");
    expect_eq_size(0, count_occurrences(ctx.output, "> "),
                   "stream mode does not emit prompt after command");
}

static void test_log_stream_poll_writes_stream_lines(void)
{
    CliSessionTestContext ctx = {0};
    SessionServicesContext services_context = {.log_stream_enabled = false};
    CliServices services = {0};
    services.log_set_stream = test_log_set_stream;
    services.log_read_line = test_log_read_line;
    services.context = &services_context;

    CliSession session = make_session(&ctx, &services);
    (void)cli_session_start(&session);

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t command[] = {'l', 'o', 'g', ' ', 's', 't', 'r', 'e', 'a', 'm', '\r'};
    cli_session_push_bytes(&session, command, sizeof(command));

    strcpy(services_context.pending_log_line, "event: frame dropped");
    services_context.has_pending_log_line = true;
    cli_session_poll(&session);

    expect_true(strstr(ctx.output, "event: frame dropped\r\n") != NULL,
                "poll emits queued stream line");
}

static void test_log_stream_mode_exits_on_q(void)
{
    CliSessionTestContext ctx = {0};
    SessionServicesContext services_context = {.log_stream_enabled = true};
    CliServices services = {0};
    services.log_set_stream = test_log_set_stream;
    services.log_read_line = test_log_read_line;
    services.context = &services_context;

    CliSession session = make_session(&ctx, &services);
    (void)cli_session_start(&session);
    session.command_stream_mode = true;

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t input[] = {'q'};
    cli_session_push_bytes(&session, input, sizeof(input));

    expect_false(services_context.log_stream_enabled, "q disables log stream mode");
    expect_true(strstr(ctx.output, "\r\n> ") != NULL, "q returns the CLI prompt");
}

int main(void)
{
    test_start_writes_banner_once();
    test_ping_echo_and_prompt();
    test_backspace_and_crlf_pair();
    test_arrow_key_escape_sequences_are_ignored();
    test_arrow_key_escape_sequence_split_across_reads();
    test_log_stream_command_hides_prompt_until_quit();
    test_log_stream_poll_writes_stream_lines();
    test_log_stream_mode_exits_on_q();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli session tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli session tests passed");
    return 0;
}
