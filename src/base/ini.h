#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

const char *IniPath(void);
const char *IniDataPath(void);

void IniGetString(const char *section, const char *key, const char *def, char *out, size_t size);
int32_t IniGetInt(const char *section, const char *key, int32_t def);
bool IniSetString(const char *section, const char *key, const char *value);
bool IniSetInt(const char *section, const char *key, int32_t value);

bool IniReadLayered(const char *base, const char *top, const char *section, const char *key, char *out, size_t size);
