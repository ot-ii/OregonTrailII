#pragma once

#include <stdint.h>

typedef void (*TimerCallback)(void *arg);

struct timer;

struct timer *TimerCreate(int32_t ms, TimerCallback cb, void *cb_arg);
void TimerDestroy(struct timer **ctx_mod);

void TimerFire(struct timer *ctx);
