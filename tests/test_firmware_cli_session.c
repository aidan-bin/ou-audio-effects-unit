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
    expect_true(strstr(ctx.output, "ok pong\r\n") != NULL, "ping result uses CRLF");
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
    expect_true(strstr(ctx.output, "ok pong\r\n") != NULL, "corrected line executes");

    ctx.output_used = 0;
    ctx.output[0] = '\0';

    const uint8_t help_crlf[] = {'h', 'e', 'l', 'p', '\r', '\n'};
    cli_session_push_bytes(&session, help_crlf, sizeof(help_crlf));

    expect_eq_size(1, count_occurrences(ctx.output, "ok commands:"),
                   "CRLF executes command once");
    expect_eq_size(1, count_occurrences(ctx.output, "> "),
                   "CRLF emits one prompt after command");
}

int main(void)
{
    test_start_writes_banner_once();
    test_ping_echo_and_prompt();
    test_backspace_and_crlf_pair();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli session tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli session tests passed");
    return 0;
}
