#pragma once

#include <stdint.h>

struct cstr;
struct alert;

struct alert *AlertCreate(const char *s);
void AlertDestroy(struct alert **ctx_mod);

int32_t AlertShow(struct alert *ctx, int32_t icon, const struct cstr *msg, int32_t buttons, int32_t def);
int32_t AlertShowRsrc(struct alert *ctx, int32_t icon, int32_t rsrc_id, int32_t str_index, int32_t buttons,
	int32_t def);

extern struct alert *g_alert_handler;
