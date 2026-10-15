#include "../timer.h"

#include <stddef.h>

#include <windows.h>

#include "../list.h"
#include "../memory.h"
#include "../../app/replay.h"

struct timer {
	int32_t interval;
	int32_t next_fire;
	TimerCallback callback;
	void *callback_arg;
};

static struct obarray *g_timer_list;
static UINT_PTR g_timer_id;
static uint32_t g_timer_delay;
static bool g_timer_pending;

static void reschedule_timer(void);

static int32_t get_tick_count(void)
{
	return (int32_t) RpGetTickCount();
}

static int32_t timer_count(void)
{
	return g_timer_list ? ObarrayCount(g_timer_list) : 0;
}

struct timer *TimerCreate(int32_t ms, TimerCallback cb, void *cb_arg)
{
	struct timer *ctx = MemAlloc(1, sizeof(struct timer));

	if (!g_timer_list)
		g_timer_list = ObarrayCreate(NULL);

	ObarrayAppend(g_timer_list, ctx);
	ctx->interval = ms;
	ctx->next_fire = get_tick_count() + ctx->interval;
	ctx->callback = cb;
	ctx->callback_arg = cb_arg;
	reschedule_timer();

	return ctx;
}

void TimerDestroy(struct timer **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct timer *ctx = *ctx_mod;
	*ctx_mod = NULL;

	ObarrayRemoveAt(g_timer_list, ObarrayIndexOf(g_timer_list, ctx));
	reschedule_timer();

	MemFree(ctx);
}

static int32_t find_next_timer(void)
{
	int32_t next = 0;

	for (int32_t i = 1; timer_count() > i; ++i) {
		struct timer *a = ObarrayGetAt(g_timer_list, next);
		struct timer *b = ObarrayGetAt(g_timer_list, i);

		if (a->next_fire > b->next_fire)
			next = i;
	}

	return next;
}

static void kill_global_timer(void)
{
	KillTimer(NULL, g_timer_id);
	g_timer_id = 0;
}

static VOID CALLBACK timer_func(HWND hwnd, UINT msg, UINT_PTR id, DWORD time);

static void set_global_timer(uint32_t delay)
{
	if (g_timer_id) {
		if (delay == g_timer_delay)
			return;

		kill_global_timer();
	}

	g_timer_id = RpSetTimer(NULL, 0, delay, (void *) timer_func);
	g_timer_delay = delay;
}

static void reschedule_timer(void)
{
	if (timer_count() == 0) {
		if (g_timer_id)
			kill_global_timer();

		g_timer_pending = false;
		return;
	}

	struct timer *next = ObarrayGetAt(g_timer_list, find_next_timer());
	uint32_t delay = (uint32_t) (next->next_fire - get_tick_count());
	set_global_timer(delay);
}

static void process_due_timers(void)
{
	int32_t tick_count = get_tick_count();

	for (int32_t i = 0; timer_count() > i; ++i) {
		struct timer *t = ObarrayGetAt(g_timer_list, i);

		if (t->next_fire <= tick_count)
			TimerFire(ObarrayGetAt(g_timer_list, i));
	}

	reschedule_timer();
}

static VOID CALLBACK timer_func(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	g_timer_pending = true;
	process_due_timers();
}

void TimerFire(struct timer *ctx)
{
	ctx->callback(ctx->callback_arg);
	ctx->next_fire = get_tick_count() + ctx->interval;
}
