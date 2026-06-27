#include "sysinfo.h"

#ifndef SYSINFO_BOARD
#define SYSINFO_BOARD "unknown"
#endif

#ifndef SYSINFO_MCU
#define SYSINFO_MCU "unknown"
#endif

#ifndef SYSINFO_VERSION
#define SYSINFO_VERSION "0.0.0"
#endif

static const SysinfoEntry entries[] = {
    {"audio_in", "1"},
    {"audio_out", "1"},
    {"audio_routing", "dual-cdc"},
    {"board", SYSINFO_BOARD},
    {"mcu", SYSINFO_MCU},
    {"version", SYSINFO_VERSION},
};

const SysinfoEntry *sysinfo_get_entries(void)
{
    return entries;
}

size_t sysinfo_get_entry_count(void)
{
    return sizeof(entries) / sizeof(entries[0]);
}
