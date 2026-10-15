#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "view.h"

struct cstr;
struct label;
struct screen_dc;

struct labeled_control_ops {
	struct view_ops base;
};

struct labeled_control {
	struct view base;
	struct label *label;
	int32_t fill_color;
	int32_t line_color;
	int32_t style;
	int32_t pen_width;
	int32_t label_x;
	int32_t label_y;
	bool has_label;
};

#define LABELED_CONTROL_OPS(p) ((const struct labeled_control_ops *) ((const struct object *) (p))->ops)

extern const struct labeled_control_ops LABELED_CONTROL_OPS_TABLE;

void LabeledControlInit(struct labeled_control *self, int32_t id, const struct rect *src, struct cstr *content,
	int32_t font, int32_t h_align, int32_t fill, int32_t line, int32_t style);
struct labeled_control *LabeledControlCreate(int32_t id, const struct rect *src, struct cstr *content, int32_t font,
	int32_t h_align, int32_t fill, int32_t line, int32_t style);
void LabeledControlDestroy(struct labeled_control **ctx_mod);
void labeled_control_destroy(struct object *self);

void labeled_control_draw_children(struct view *self, struct screen_dc *dc, const struct rect *invalid_rect);

void LabeledControlSetLineColor(struct labeled_control *self, int32_t line_color);
void LabeledControlSetFillColor(struct labeled_control *self, int32_t fill_color);
void LabeledControlSetTitle(struct labeled_control *self, struct cstr *title);

struct view *CreateLabeledControlFromTemplate(struct ctrl_desc *desc);
