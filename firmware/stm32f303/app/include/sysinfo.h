#ifndef SYSINFO_H
#define SYSINFO_H

#include <stddef.h>

typedef struct
{
    const char *key;
    const char *value;
} SysinfoEntry;

const SysinfoEntry *sysinfo_get_entries(void);

size_t sysinfo_get_entry_count(void);

#endif
