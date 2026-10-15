#pragma once

#include <stdbool.h>

void AutosaveStart(void);
void AutosaveSkipCurrent(void);
bool AutosaveEnabled(void);
void AutosaveSetEnabled(bool enabled);
const char *AutosaveStatus(void);
