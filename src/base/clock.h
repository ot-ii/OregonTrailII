#pragma once

#include <stdint.h>

uint32_t TimeTicks(void);
uint32_t TimeUnix(void);
void TimeSleep(uint32_t ms);
