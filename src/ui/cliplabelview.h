#pragma once

#include <stdint.h>

#include "clipview.h"
#include "../base/geom.h"

struct cstr;
struct label;
struct strrsrc;

struct clip_label_view_ops {
	struct clip_view_ops base;
};

struct clip_label_view {
	struct clip_view base;
};

#define CLIP_LABEL_VIEW_OPS(p) ((const struct clip_label_view_ops *) ((const struct object *) (p))->ops)

extern const struct clip_label_view_ops CLIP_LABEL_VIEW_OPS_TABLE;

void ClipLabelViewInit(struct clip_label_view *self, int32_t id, const struct rect *rect, struct cstr *src,
	int32_t font, int32_t h_align, int32_t v_align, int32_t pen, int32_t brush, int32_t alt, int32_t pen_width,
	int32_t bg_mode);
void ClipLabelViewInitStrs(struct clip_label_view *self, int32_t id, const struct rect *rect,
	struct strrsrc *rsrc_src, int32_t font, int32_t h_align, int32_t v_align, int32_t pen, int32_t brush,
	int32_t alt, int32_t pen_width, int32_t bg_mode);
void ClipLabelViewInitLabel(struct clip_label_view *self, int32_t id, const struct rect *rect, struct label *label);
struct clip_label_view *ClipLabelViewCreate(int32_t id, const struct rect *rect, struct cstr *src, int32_t font,
	int32_t h_align, int32_t v_align, int32_t pen, int32_t brush, int32_t alt, int32_t pen_width, int32_t bg_mode);
struct clip_label_view *ClipLabelViewCreateStrs(int32_t id, const struct rect *rect, struct strrsrc *rsrc_src,
	int32_t font, int32_t h_align, int32_t v_align, int32_t pen, int32_t brush, int32_t alt, int32_t pen_width,
	int32_t bg_mode);
struct clip_label_view *ClipLabelViewCreateLabel(int32_t id, const struct rect *rect, struct label *label);
void ClipLabelViewDestroy(struct clip_label_view **ctx_mod);
void clip_label_view_destroy(struct object *self);

void ClipLabelViewSetLabelObj(struct clip_label_view *self, struct label *label);
void ClipLabelViewSetLabelAttr(struct clip_label_view *self, struct cstr *attr);
void ClipLabelViewApplyLabel(struct clip_label_view *self, struct strrsrc *label);

struct view *CreateClipLabelView(struct ctrl_desc *desc);
