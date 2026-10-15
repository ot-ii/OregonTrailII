#pragma once

#include <stdint.h>

void DeltaDecode(uint8_t state[4], int32_t index, int32_t count, uint8_t *buf);
