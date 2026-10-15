#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../app/evthandler.h"
#include "../ui/floatwin.h"
#include "../base/geom.h"

struct cmd_event;

struct health_win_ops {
	struct window_ops base;
};

struct health_win_member {
	int32_t health;
	int32_t state;
};

struct health_win {
	struct float_win base;
	struct evt_handler handler;
	int32_t selected_member;
	struct health_win_member member_health[6];
};

#define HEALTH_WIN_OPS(p) ((const struct health_win_ops *) ((const struct object *) (p))->ops)

extern const struct health_win_ops HEALTH_WIN_OPS_TABLE;
extern const struct evt_handler_ops HEALTH_WIN_HANDLER_OPS_TABLE;

extern struct health_win *g_health_window;
extern struct rect g_healthwin_rect;

void HealthWinInit(struct health_win *self, struct window *parent);
struct health_win *HealthWinCreate(struct window *parent);
void HealthWinDestroy(struct health_win **ctx_mod);
void health_win_destroy(struct object *self);

struct health_win *HealthWinFromHandler(struct evt_handler *handler);

void health_win_set_handled(struct evt_handler *self);
void health_win_on_event(struct evt_handler *self, struct cmd_event *evt);
void HealthWinUpdate(struct health_win *self, bool flag);
