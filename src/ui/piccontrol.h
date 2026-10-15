#pragma once

#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "picview.h"

struct drawable;

struct pic_control_ops {
	struct pic_view_ops base;
};

struct pic_control {
	struct pic_view base;
	struct event pic_event;
	int32_t cmd_code;
	int32_t cmd_param1;
	int32_t cmd_param2;
};

#define PIC_CONTROL_OPS(p) ((const struct pic_control_ops *) ((const struct object *) (p))->ops)

extern const struct pic_control_ops PIC_CONTROL_OPS_TABLE;

void PicControlInitImage(struct pic_control *self, int32_t id, const struct rect *rect, struct drawable *image,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
void PicControlInit(struct pic_control *self, int32_t id, const struct rect *rect, int32_t img_id,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
void PicControlInitPoint(struct pic_control *self, int32_t id, const struct point *pos, int32_t img_id,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
void PicControlInitPointEmpty(struct pic_control *self, int32_t id, const struct point *pos,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
struct pic_control *PicControlCreateImage(int32_t id, const struct rect *rect, struct drawable *image,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
struct pic_control *PicControlCreate(int32_t id, const struct rect *rect, int32_t img_id, const struct event *cmd,
	int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
struct pic_control *PicControlCreatePoint(int32_t id, const struct point *pos, int32_t img_id,
	const struct event *cmd, int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
struct pic_control *PicControlCreatePointEmpty(int32_t id, const struct point *pos, const struct event *cmd,
	int32_t cmd_code, int32_t cmd_param1, int32_t cmd_param2);
void PicControlDestroy(struct pic_control **ctx_mod);
void pic_control_destroy(struct object *self);

void pic_control_on_event(struct evt_handler *self, struct cmd_event *event);

void PicControlSetCommand(struct pic_control *self, const struct event *cmd, int32_t cmd_code, int32_t cmd_param1,
	int32_t cmd_param2);

struct view *PicControlCreateFromDesc(struct ctrl_desc *desc);
