#pragma once

#include <stdint.h>

#include "../app/event.h"
#include "../base/geom.h"
#include "view.h"

struct drawable;
struct screen_dc;

struct pic_view_ops {
	struct view_ops base;
};

struct pic_view {
	struct view base;
	struct drawable *image;
	int32_t cur_frame;
};

#define PIC_VIEW_OPS(p) ((const struct pic_view_ops *) ((const struct object *) (p))->ops)

extern const struct pic_view_ops PIC_VIEW_OPS_TABLE;

void PicViewInitImage(struct pic_view *self, int32_t id, const struct rect *rect, struct drawable *image);
void PicViewInit(struct pic_view *self, int32_t id, const struct rect *rect, int32_t img_id);
void PicViewInitPoint(struct pic_view *self, int32_t id, const struct point *pos, int32_t img_id);
void PicViewInitPointImage(struct pic_view *self, int32_t id, const struct point *pos, struct drawable *image);
void PicViewInitPointEmpty(struct pic_view *self, int32_t id, const struct point *pos);
struct pic_view *PicViewCreateImage(int32_t id, const struct rect *rect, struct drawable *image);
struct pic_view *PicViewCreate(int32_t id, const struct rect *rect, int32_t img_id);
struct pic_view *PicViewCreatePoint(int32_t id, const struct point *pos, int32_t img_id);
struct pic_view *PicViewCreatePointImage(int32_t id, const struct point *pos, struct drawable *image);
struct pic_view *PicViewCreatePointEmpty(int32_t id, const struct point *pos);
void PicViewDestroy(struct pic_view **ctx_mod);
void pic_view_destroy(struct object *self);

void pic_view_on_event(struct evt_handler *self, struct cmd_event *event);
void pic_view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds);

void PicViewSetImageByType(struct pic_view *self, int32_t img_id);
void PicViewSetImage(struct pic_view *self, struct drawable *image);

struct view *CreatePicView(struct ctrl_desc *desc);
