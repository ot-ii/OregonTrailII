#pragma once

#include <stdbool.h>
#include <stddef.h>

struct assets;

extern struct assets *g_text_lib;
extern struct assets *g_data_lib;

bool GfxDevFindDataFolder(const char *data_name, char *folder, size_t size);
bool GfxDevOpenLibraries(const char *data_name, const char *data_folder);
void GfxDevCloseLibraries(void);
