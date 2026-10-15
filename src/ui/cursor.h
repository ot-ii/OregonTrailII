#pragma once

#include <stdint.h>

#include "../base/object.h"

struct cursor;
struct assets;

struct cursor_ops {
	struct object_ops base;
	void (*Set)(struct cursor *self);
	void (*ResetArrow)(struct cursor *self);
};

struct cursor {
	struct object base;
	int32_t cursor_id;
	void *hcursor;
	void *hwnd;
};

#define CURSOR_OPS(p) ((const struct cursor_ops *) ((const struct object *) (p))->ops)

extern const struct cursor_ops CURSOR_OPS_TABLE;

void CursorInit(struct cursor *self);
void CursorInitKind(struct cursor *self, int32_t kind);
void CursorInitLib(struct cursor *self, struct assets *lib, int32_t id);
struct cursor *CursorCreate(void);
struct cursor *CursorCreateKind(int32_t kind);
struct cursor *CursorCreateLib(struct assets *lib, int32_t id);
void CursorDestroy(struct cursor **ctx_mod);
void cursor_destroy(struct object *self);

void cursor_set(struct cursor *self);
void cursor_reset_arrow(struct cursor *self);

void CursorHide(void);
void CursorShow(void);
