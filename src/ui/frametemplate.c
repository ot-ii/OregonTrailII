#include "frametemplate.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "ctrldesc.h"
#include "../app/event.h"
#include "../data/gfxdev.h"
#include "../base/log.h"
#include "../base/memory.h"
#include "../app/registrar.h"
#include "../data/rsrc.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "view.h"

const struct frame_template_ops FRAME_TEMPLATE_OPS_TABLE = {
	{frame_template_destroy},
	frame_template_create_item,
};

void FrameTemplateInit(struct frame_template *self, struct assets *lib, int32_t id)
{
	ObjectInit(&self->base);
	self->base.ops = &FRAME_TEMPLATE_OPS_TABLE.base;

	bool error = false;
	self->items = NULL;
	self->count = 0;
	self->id_count = 0;
	self->id = 0;

	struct rsrc *rsrc = AssetsGetElement(lib, "FRMTP", id, true);

	if (!rsrc)
		return;

	if (RsrcSize(rsrc)) {
		self->id = id;

		switch (RsrcStorage(rsrc)) {
			case RSRC_PACKED:
			case RSRC_NATIVE:
				RsrcLoadAndUnpack(rsrc);
				break;
			case RSRC_TEXT: {
				char msg[100];
				StrPrintf(msg, "Window Template %d not packed", (int32_t) (int16_t) id);
				LogError(LOG_ERROR, msg);
				break;
			}
			default: {
				char msg[100];
				StrPrintf(msg, "Window Template %d, invalid format", (int32_t) (int16_t) id);
				LogError(LOG_ERROR, msg);
				error = true;
				break;
			}
		}

	} else {
		error = true;
	}

	if (!error)
		FrameTemplateParseItems(self, rsrc);

	RsrcDestroy(&rsrc);
}

struct frame_template *FrameTemplateCreate(struct assets *lib, int32_t id)
{
	struct frame_template *self = MemAlloc(1, sizeof(struct frame_template));
	FrameTemplateInit(self, lib, id);

	return self;
}

void FrameTemplateDestroy(struct frame_template **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void frame_template_destroy(struct object *obj)
{
	struct frame_template *self = (struct frame_template *) obj;

	MemFree(self->items);
	self->items = NULL;

	object_destroy(obj);
}

void FrameTemplateSetItemValue(struct frame_template *self, int32_t id, int32_t value)
{
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item)
		item->id = (int16_t) value;
}

struct view *FrameTemplateBuild(struct frame_template *self, struct view *parent, struct event *event,
	struct strrsrc *caller_strs, int32_t **ids)
{
	struct view *first = NULL;
	int32_t *id_list = NULL;
	struct event search = EventMake(EVCLS_ANY, -1, -1);

	if (self->items) {
		struct view *parent_view = parent;

		if (ids) {
			*ids = MemRealloc(*ids, (size_t) self->count + 1, sizeof(int32_t));
			id_list = *ids;
		}

		struct ctrl_desc *item = self->items;

		for (int32_t i = 0; i < self->count; ++i) {
			if (item->id > 0) {
				struct view *created = FRAME_TEMPLATE_OPS(self)->CreateItem(self, item, event, caller_strs);

				if (created) {
					if (!first)
						first = created;

					if (!parent_view) {
						parent_view = created;

					} else if (item->parent_id != 0) {
						EventSetId(&search, item->parent_id);
						struct view *found = ViewFindByName(parent_view, search);

						if (!found)
							found = parent_view;

						VIEW_OPS(found)->AddChild(found, created);

					} else {
						VIEW_OPS(parent_view)->AddChild(parent_view, created);
					}

					if (id_list) {
						int32_t cmd_id = item->cmd_id;

						if (cmd_id) {
							int32_t found_index = -1;

							for (int32_t j = 0; self->id_count > j; ++j) {
								if (id_list[j] == cmd_id) {
									found_index = j;
									break;
								}
							}

							if (found_index == -1) {
								id_list[self->id_count] = cmd_id;
								++self->id_count;
							}
						}
					}
				}
			}

			item++;
		}
	}

	return first;
}

int32_t FrameTemplateGetWord(struct frame_template *self, int32_t id, int32_t index)
{
	int32_t word = 0;
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item)
		word = item->w[index];

	return word;
}

bool FrameTemplateGetBlock(struct frame_template *self, int32_t id, int32_t ndw, const int32_t *dw_idx,
	int32_t *dw_out, int32_t nw, const int32_t *w_idx, int32_t *w_out)
{
	bool ok = false;
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item) {
		ok = true;

		if (ndw != 0) {
			for (int32_t i = 0; i < ndw; ++i) {
				if (dw_idx[i] < CTRL_DESC_DWORDS)
					dw_out[i] = item->dw[dw_idx[i]];
			}
		}

		if (nw != 0) {
			for (int32_t i = 0; i < nw; ++i) {
				if (w_idx[i] < CTRL_DESC_WORDS)
					w_out[i] = item->w[w_idx[i]];
			}
		}
	}

	return ok;
}

int32_t FrameTemplateGetDword(struct frame_template *self, int32_t id, int32_t index)
{
	int32_t value = 0;
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item)
		value = item->dw[index];

	return value;
}

int32_t FrameTemplateItemIdAt(struct frame_template *self, int32_t index)
{
	if (index < 0 || self->count <= index)
		return -1;

	return self->items[index].id;
}

void FrameTemplateSetWord(struct frame_template *self, int32_t id, int32_t index, int32_t value)
{
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item)
		item->w[index] = (int16_t) value;
}

void FrameTemplateSetBlock(struct frame_template *self, int32_t id, int32_t ndw, const int32_t *dw_idx,
	const int32_t *dw_out, int32_t nw, const int32_t *w_idx, const int32_t *w_out)
{
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item) {
		if (ndw) {
			for (int32_t i = 0; i < ndw; ++i) {
				if (dw_idx[i] < CTRL_DESC_DWORDS)
					item->dw[dw_idx[i]] = dw_out[i];
			}
		}

		if (nw) {
			for (int32_t i = 0; i < nw; ++i) {
				if (w_idx[i] < CTRL_DESC_WORDS)
					item->w[w_idx[i]] = (int16_t) w_out[i];
			}
		}
	}
}

void FrameTemplateSetDword(struct frame_template *self, int32_t id, int32_t index, int32_t value)
{
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item)
		item->dw[index] = value;
}

void FrameTemplateSetItemAttrs(struct frame_template *self, int32_t id, int32_t v14, int32_t v2a, int32_t v2e)
{
	struct ctrl_desc *item = FrameTemplateFindItem(self, self->items, id);

	if (item) {
		item->cmd_id = (int16_t) v14;
		item->cmd_param1 = (int16_t) v2a;
		item->cmd_param2 = (int16_t) v2e;
	}
}

struct ctrl_desc *FrameTemplateFindItem(struct frame_template *self, struct ctrl_desc *items, int32_t search_id)
{
	int32_t i;

	for (i = 0; i < self->count; ++i) {
		if (items->id != search_id) {
			items++;

		} else {
			i = self->count + 1;
		}
	}

	if (self->count < i)
		return items;

	return NULL;
}

struct view *frame_template_create_item(struct frame_template *self, struct ctrl_desc *item, struct event *event,
	struct strrsrc *caller_strs)
{
	struct view *view = NULL;

	if (item->_00 & 0xFF00) {
		int32_t event_id = item->event_id;
		struct event *new_event = NULL;

		if (event_id > 0) {
			new_event = MemAlloc(1, sizeof(struct event));
			*new_event = EventMake(EVCLS_ANY, event_id, -1);
			item->event = new_event;

		} else {
			item->event = event;
		}

		item->src = FrameTemplateGetStringData(self, item, caller_strs);
		FrameTemplateApplyDefaults(self, item);
		view = RegistrarCreateObject(item->_00, item);

		if (item->src && item->src != caller_strs && item->item_index != -1)
			StrRsrcDestroy(&item->src);

		if (event_id > 0)
			MemFree(new_event);

	} else {
		item->event = event;
		item->src = caller_strs;
		view = RegistrarCreateObject(item->_00, item);
	}

	return view;
}

void FrameTemplateParseItems(struct frame_template *self, struct rsrc *rsrc)
{
	const uint8_t *source = RsrcData(rsrc);
	self->count = ReadLe16(source);
	source += 2;

	if (self->count < 0)
		self->count = 0;

	self->items = MemAlloc((size_t) self->count + 1, sizeof(struct ctrl_desc));
	struct ctrl_desc *item = self->items;

	for (int32_t i = 0; self->count > i; ++i) {
		for (int32_t j = 0; j < CTRL_DESC_WORDS; ++j) {
			item->w[j] = ReadLe16(source);
			source += 2;
		}

		for (int32_t j = 0; j < CTRL_DESC_DWORDS; ++j) {
			item->dw[j] = ReadLe32(source);
			source += 4;
		}

		item++;
	}
}

void FrameTemplateApplyDefaults(struct frame_template *self, struct ctrl_desc *item)
{
	(void) self;

	if (item->pen_color == -1)
		item->pen_color = 0;

	if (item->brush_color == -1)
		item->brush_color = 0;

	if (item->alt_brush_color == -1)
		item->alt_brush_color = 0xFF;
}

struct strrsrc *FrameTemplateGetStringData(struct frame_template *self, struct ctrl_desc *item,
	struct strrsrc *caller_strs)
{
	(void) self;
	struct strrsrc *strs = NULL;

	if (item->rsrc_id) {
		if (item->rsrc_id > 0) {
			if (item->item_index > -1) {
				strs = StrRsrcLoad(g_text_lib, item->rsrc_id, false);

				if (!strs) {
					char errbuf[52];
					StrPrintf(errbuf, "CStrLst id %d not found", item->rsrc_id);
					LogError(LOG_ERROR, errbuf);
				}
			}

		} else {
			strs = caller_strs;
		}
	}

	return strs;
}
