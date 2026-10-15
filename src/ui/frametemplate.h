#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/object.h"

struct frame_template;
struct ctrl_desc;
struct assets;
struct rsrc;
struct strrsrc;
struct event;
struct view;

struct frame_template_ops {
	struct object_ops base;
	struct view *(*CreateItem)(struct frame_template *self, struct ctrl_desc *item, struct event *event,
		struct strrsrc *caller_strs);
};

struct frame_template {
	struct object base;
	struct ctrl_desc *items;
	int32_t count;
	int32_t id_count;
	int32_t id;
};

#define FRAME_TEMPLATE_OPS(p) ((const struct frame_template_ops *) ((const struct object *) (p))->ops)

extern const struct frame_template_ops FRAME_TEMPLATE_OPS_TABLE;

void FrameTemplateInit(struct frame_template *self, struct assets *lib, int32_t id);
struct frame_template *FrameTemplateCreate(struct assets *lib, int32_t id);
void FrameTemplateDestroy(struct frame_template **ctx_mod);
void frame_template_destroy(struct object *self);

struct view *frame_template_create_item(struct frame_template *self, struct ctrl_desc *item, struct event *event,
	struct strrsrc *caller_strs);

struct view *FrameTemplateBuild(struct frame_template *self, struct view *parent, struct event *event,
	struct strrsrc *caller_strs, int32_t **ids);
bool FrameTemplateGetBlock(struct frame_template *self, int32_t id, int32_t ndw, const int32_t *dw_idx,
	int32_t *dw_out, int32_t nw, const int32_t *w_idx, int32_t *w_out);
void FrameTemplateParseItems(struct frame_template *self, struct rsrc *rsrc);
struct ctrl_desc *FrameTemplateFindItem(struct frame_template *self, struct ctrl_desc *items, int32_t search_id);
void FrameTemplateSetItemValue(struct frame_template *self, int32_t id, int32_t value);
void FrameTemplateSetBlock(struct frame_template *self, int32_t id, int32_t ndw, const int32_t *dw_idx,
	const int32_t *dw_out, int32_t nw, const int32_t *w_idx, const int32_t *w_out);
void FrameTemplateSetWord(struct frame_template *self, int32_t id, int32_t index, int32_t value);
void FrameTemplateSetDword(struct frame_template *self, int32_t id, int32_t index, int32_t value);
int32_t FrameTemplateGetWord(struct frame_template *self, int32_t id, int32_t index);
int32_t FrameTemplateGetDword(struct frame_template *self, int32_t id, int32_t index);
void FrameTemplateSetItemAttrs(struct frame_template *self, int32_t id, int32_t v14, int32_t v2a, int32_t v2e);
int32_t FrameTemplateItemIdAt(struct frame_template *self, int32_t index);
struct strrsrc *FrameTemplateGetStringData(struct frame_template *self, struct ctrl_desc *item,
	struct strrsrc *caller_strs);
void FrameTemplateApplyDefaults(struct frame_template *self, struct ctrl_desc *item);
