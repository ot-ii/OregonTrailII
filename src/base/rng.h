#pragma once

#include <stdint.h>

typedef uint32_t (*RngTimeFunc)(void);
typedef void (*RngTraceFunc)(int32_t lo, int32_t hi, int32_t count, int32_t result);

void RngSetTimeSource(RngTimeFunc func);
void RngSetTrace(RngTraceFunc func);

void RngSetSeed(uint32_t seed);
void RngRestore(uint32_t seed, uint64_t calls);
uint32_t RngSeed(void);
uint64_t RngCalls(void);
int32_t RandRangeAvg(int32_t lo, int32_t hi, int32_t count);
void RandSelect(int32_t lo, int32_t hi, int32_t k, int32_t *out);
