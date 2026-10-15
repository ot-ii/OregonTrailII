#pragma once

#include <stdint.h>

#include "button.h"
#include "../app/event.h"
#include "../base/geom.h"

struct cstr;

struct ui_container_ops {
	struct button_ops base;
};

struct ui_container {
	struct button base;
};

#define UI_CONTAINER_OPS(p) ((const struct ui_container_ops *) ((const struct object *) (p))->ops)

extern const struct ui_container_ops UI_CONTAINER_OPS_TABLE;

void UiContainerInit(struct ui_container *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t type_id, int32_t ctrl_type, int32_t init_value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y);
struct ui_container *UiContainerCreate(int32_t id, const struct event *event, const struct rect *rect,
	int32_t type_id, int32_t ctrl_type, int32_t init_value, struct cstr *source, int32_t font_id, int32_t margin_x,
	int32_t margin_y);
void UiContainerDestroy(struct ui_container **ctx_mod);
void ui_container_destroy(struct object *self);

struct view *CreateUiContainer(struct ctrl_desc *desc);
