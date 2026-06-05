#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cli_line_buffer.h"
#include "harness/expect.h"

int failures = 0;

static void test_push_pop_line(void)
{
    CliLineBuffer buffer;
    cli_line_buffer_init(&buffer);

    const uint8_t input[] = "ping\n";
    cli_line_buffer_push_bytes(&buffer, input, sizeof(input) - 1);

    char line[CLI_MAX_LINE_LENGTH + 1] = {0};
    expect_true(cli_line_buffer_pop_line(&buffer, line, sizeof(line)), "line available");
    expect_true(strcmp(line, "ping") == 0, "line contents");
}

static void test_crlf_handling(void)
{
    CliLineBuffer buffer;
    cli_line_buffer_init(&buffer);

    const uint8_t input[] = "help\r\n";
    cli_line_buffer_push_bytes(&buffer, input, sizeof(input) - 1);

    char line[CLI_MAX_LINE_LENGTH + 1] = {0};
    expect_true(cli_line_buffer_pop_line(&buffer, line, sizeof(line)), "crlf line available");
    expect_true(strcmp(line, "help") == 0, "crlf parsed line");
}

static void test_overflow_drops_long_line(void)
{
    CliLineBuffer buffer;
    cli_line_buffer_init(&buffer);

    uint8_t too_long[CLI_MAX_LINE_LENGTH + 4];
    memset(too_long, 'a', sizeof(too_long));
    too_long[sizeof(too_long) - 1] = '\n';

    cli_line_buffer_push_bytes(&buffer, too_long, sizeof(too_long));

    char line[CLI_MAX_LINE_LENGTH + 1] = {0};
    expect_false(cli_line_buffer_pop_line(&buffer, line, sizeof(line)), "long line dropped");
}

int main(void)
{
    test_push_pop_line();
    test_crlf_handling();
    test_overflow_drops_long_line();

    if (failures != 0)
    {
        fprintf(stderr, "Firmware cli line buffer tests failed: %d\n", failures);
        return 1;
    }

    puts("Firmware cli line buffer tests passed");
    return 0;
}
