#include "invgrid.h"

#include <stddef.h>
#include <stdlib.h>

#include "../app/app.h"
#include "../ui/cliplabelview.h"
#include "../ui/combobox.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../ui/edit.h"
#include "../game/game.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../ui/pictctrl.h"
#include "../base/str.h"
#include "../data/strrsrc.h"

struct grid_btn_def {
	const char *text;
	int32_t v, h;
	int32_t id;
	int32_t img_id;
	int32_t cmd;
	int32_t param;
};

struct grid_label_def {
	const char *text;
	int32_t top, left, bottom, right;
	int32_t font_id;
	int32_t align;
	int32_t id;
};

static const struct grid_btn_def g_grid_buttons[3] = {
	{"", 316, 44, GRID_PREV_PAGE, 2103, 0, 0},
	{"", 316, 336, GRID_NEXT_PAGE, 2104, 0, 0},
	{0},
};

static const struct grid_label_def g_grid_headers[11] = {
	{"You", 50, 5, 70, 30, 4, 2, GRID_HAVE_HEADER},
	{"Amt", 50, 35, 70, 60, 4, 2, GRID_AMOUNT_HEADER},
	{"", 50, 65, 70, 100, 4, 0, GRID_QTY_HEADER},
	{"Item", 50, 105, 70, 245, 4, 0, GRID_ITEM_HEADER},
	{"", 50, 250, 70, 320, 4, 2, GRID_UNIT_HEADER},
	{"", 50, 330, 70, 370, 4, 2, GRID_LINE_HEADER},
	{"Weight (lbs)", 290, 230, 305, 320, 4, 2, GRID_TOTAL_LABEL},
	{"", 290, 325, 305, 370, 4, 2, GRID_TOTAL},
	{"Cash on hand $", 290, 35, 305, 125, 4, 2, GRID_LIMIT_LABEL},
	{"", 290, 135, 305, 200, 4, 0, GRID_LIMIT},
	{0},
};

static const struct grid_label_def g_grid_cellcols[6] = {
	{"", 70, 5, 93, 30, 4, 2, GRID_ROW_HAVE},
	{"", 70, 35, 93, 60, 4, 2, GRID_ROW_AMOUNT},
	{"", 70, 100, 93, 290, 5, 0, GRID_ROW_NAME},
	{"", 70, 270, 93, 320, 4, 2, GRID_ROW_UNIT},
	{"", 70, 325, 93, 370, 4, 2, GRID_ROW_LINE},
	{0},
};

#define INV_GRID_VIEW_OPS(destroy) \
	{ \
		{ \
			{ \
				{destroy}, \
				evt_handler_is_handled, \
				evt_handler_set_handled_for, \
				evt_handler_set_handled, \
				evt_handler_clear_handled_for, \
				evt_handler_clear_handled, \
				inv_grid_on_event, \
				view_on_idle, \
				view_on_idle_spare, \
			}, \
			view_add_child, \
			view_get_bounds, \
			view_get_clip_region, \
			view_hide, \
			view_remove_child, \
			view_remove_from_parent, \
			view_set_cursor_id, \
			view_set_device, \
			view_show, \
			pic_view_draw_children, \
			view_move_children, \
		}, \
	}

const struct inv_grid_ops INV_GRID_OPS_TABLE = {
	INV_GRID_VIEW_OPS(inv_grid_destroy),
	inv_grid_get_unit_text,
	inv_grid_get_line_text,
	inv_grid_pause,
};

const struct store_grid_ops STORE_GRID_OPS_TABLE = {
	{
		INV_GRID_VIEW_OPS(store_grid_destroy),
		store_grid_get_unit_text,
		store_grid_get_line_text,
		store_grid_pause,
	},
};

const struct drop_grid_ops DROP_GRID_OPS_TABLE = {
	{
		INV_GRID_VIEW_OPS(drop_grid_destroy),
		drop_grid_get_unit_text,
		drop_grid_get_line_text,
		drop_grid_pause,
	},
};

static void *find_child(struct inv_grid *self, const struct event *spec)
{
	struct view *view = &self->base.base;

	if (!view->has_child_list)
		return NULL;

	int32_t count = ObarrayCount(view->child_list);

	for (int32_t index = 0; index < count; ++index) {
		struct evt_handler *obj = ObarrayGetAt(view->child_list, index);

		if (EventMatches(spec, &obj->eh_event))
			return obj;
	}

	return NULL;
}

void InvGridInit(struct inv_grid *self, int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t subtype_filter, double capacity, bool has_edits, bool owns_list, int32_t ctl_id,
	bool build_party_list)
{
	PicViewInitPoint(&self->base, id, pos, img_id);
	self->base.base.base.base.ops = &INV_GRID_OPS_TABLE.base.base.base.base;

	char capacity_text[32];
	int32_t handled_cmds[2];

	self->ctl_id = ctl_id;
	self->list = list;
	self->category_filter = subtype_filter;
	self->capacity = capacity;
	self->has_edits = has_edits;
	self->owns_list = owns_list;
	self->table = NULL;
	self->total = 0;

	if (build_party_list) {
		self->ref_list = PartyBuildInventoryList(GetGameObj()->party);

	} else {
		self->ref_list = NULL;
	}

	struct list_node *node = self->list->base.head;

	while (node) {
		InvItemFromNode(node)->selected_qty = 0;
		node = node->next;
	}

	InvGridRebuildPages(self);

	int32_t base_h = pos->h;
	int32_t base_v = pos->v;

	struct rect rect;
	rect.top = base_v + 20;
	rect.left = base_h + 100;
	rect.right = base_h + 300;
	rect.bottom = base_v + 40;

	struct strrsrc *items = StrRsrcCreate();
	struct strrsrc *category_names = GetItemCategoryNames();
	int32_t icon_count = StrRsrcCount(category_names);

	if (icon_count) {
		for (int32_t icon_index = 0; icon_count > icon_index; icon_index++)
			StrRsrcAppend(items, StrRsrcGetAt(category_names, icon_index));
	}

	struct combo_box *combo = ComboBoxCreate(self->ctl_id, &self->base.base.base.eh_event, &rect, 205, items,
		self->category_filter);
	combo->font_id = 3;

	struct view *child = (struct view *) combo;
	ViewNotifyChanged(child);
	VIEW_OPS(self)->AddChild(&self->base.base, child);

	for (int32_t index = 0; g_grid_buttons[index].text; index++) {
		struct point origin;
		origin.h = g_grid_buttons[index].h + base_h;
		origin.v = g_grid_buttons[index].v + base_v;

		struct cstr *label = CStrCreateFrom(g_grid_buttons[index].text);
		child = (struct view *) PictCtrlCreate(g_grid_buttons[index].id, &self->base.base.base.eh_event, &origin,
			g_grid_buttons[index].img_id, g_grid_buttons[index].cmd, g_grid_buttons[index].param, label, 0, 0, 0);
		VIEW_OPS(self)->AddChild(&self->base.base, child);
	}

	int32_t first_header = build_party_list ? 0 : 1;

	for (int32_t index = first_header; g_grid_headers[index].text; index++) {
		rect.top = g_grid_headers[index].top + base_v;
		rect.left = g_grid_headers[index].left + base_h;
		rect.right = g_grid_headers[index].right + base_h;
		rect.bottom = g_grid_headers[index].bottom + base_v;

		struct cstr *label = CStrCreateFrom(g_grid_headers[index].text);
		child = (struct view *) ClipLabelViewCreate(g_grid_headers[index].id, &rect, label,
			g_grid_headers[index].font_id, g_grid_headers[index].align, 1, 0, 0, 255, 0, 1);
		VIEW_OPS(self)->AddChild(&self->base.base, child);
	}

	for (int32_t index = 0; index < 9; index++) {
		for (int32_t column = 0; g_grid_cellcols[column].text; column++) {
			rect.top = g_grid_cellcols[column].top + 23 * index + base_v;
			rect.left = g_grid_cellcols[column].left + base_h;
			rect.right = g_grid_cellcols[column].right + base_h;
			rect.bottom = g_grid_cellcols[column].bottom + 23 * index + base_v;

			struct cstr *label = CStrCreateFrom(g_grid_cellcols[column].text);
			child = (struct view *) ClipLabelViewCreate(g_grid_cellcols[column].id + index, &rect, label,
				g_grid_cellcols[column].font_id, g_grid_cellcols[column].align, 1, 0, 0, 255, 0, 1);
			VIEW_OPS(self)->AddChild(&self->base.base, child);
		}

		if (self->has_edits) {
			rect.top = g_grid_cellcols[0].top + 23 * index + base_v + 2;
			rect.left = base_h + 65;
			rect.right = base_h + 95;
			rect.bottom = g_grid_cellcols[0].bottom + 23 * index + base_v - 2;

			struct cstr *label = CStrCreate();
			child = (struct view *) EditCreate(index + GRID_ROW_QTY, &self->base.base.base.eh_event, &rect, label,
				false, true, 0, 4, 4, false, false, 253, true);
			VIEW_OPS(self)->AddChild(&self->base.base, child);
		}
	}

	StrPrintf(capacity_text, "%7.2f", self->capacity);
	InvGridSetLabel(self, GRID_LIMIT, CStrCreateFrom(capacity_text));

	handled_cmds[0] = CMD_CONTROL_ACTION;
	handled_cmds[1] = CMD_EDIT_COMMITTED;
	EvtHandlerBindHandler(&self->base.base.base, 2, handled_cmds);
}

struct inv_grid *InvGridCreate(int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t subtype_filter, double capacity, bool has_edits, bool owns_list, int32_t ctl_id,
	bool build_party_list)
{
	struct inv_grid *self = MemAlloc(1, sizeof(struct inv_grid));
	InvGridInit(self, id, pos, img_id, list, subtype_filter, capacity, has_edits, owns_list, ctl_id,
		build_party_list);

	return self;
}

void InvGridDestroy(struct inv_grid **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void inv_grid_destroy(struct object *obj)
{
	struct inv_grid *self = (struct inv_grid *) obj;

	if (self->table)
		MemFree(self->table);

	if (self->owns_list && self->list)
		InvListDestroy(&self->list);

	if (self->ref_list)
		InvListDestroy(&self->ref_list);

	obj->ops = &PIC_VIEW_OPS_TABLE.base.base.base;
	pic_view_destroy(obj);
}

void inv_grid_on_event(struct evt_handler *handler, struct cmd_event *event)
{
	struct inv_grid *self = (struct inv_grid *) handler;

	char qty_text[32];
	int32_t control_id = event->spec_b.id;

	if (event->command == CMD_CONTROL_ACTION) {
		if (self->ctl_id == control_id) {
			struct control *combo = find_child(self, &event->spec_b);
			self->category_filter = combo->value;
			InvGridRebuildPages(self);
			InvGridRefresh(self);

		} else {
			switch (control_id) {
				case GRID_NEXT_PAGE:
					self->page++;
					OtiiAppPlaySfx((struct otii_app *) g_app, 27015, false);
					InvGridRefresh(self);
					break;

				case GRID_PREV_PAGE:
					self->page--;
					OtiiAppPlaySfx((struct otii_app *) g_app, 27015, false);
					InvGridRefresh(self);
					break;
			}
		}

		event->handled = true;
	}

	if (self->has_edits) {
		if (event->command == CMD_EDIT_COMMITTED) {
			int32_t qty = 0;
			struct edit *edit = find_child(self, &event->spec_b);
			struct cstr *text = EditGetText(edit);
			const char *text_chars = CStrGet(text);

			if (text_chars) {
				int32_t typed = atoi(text_chars);
				qty = typed < 0 ? 0 : typed > 32767 ? 32767 : typed;
			}

			CStrDestroy(&text);

			struct inv_item *item = InvGridGetItemAt(self, control_id - GRID_ROW_QTY);

			if (!item) {
				event->handled = true;
				return;

			} else {
				struct cstr *line_str = INV_GRID_OPS(self)->GetLineText(self, item);
				self->total = self->total - atof(CStrGet(line_str));
				CStrDestroy(&line_str);
			}

			int32_t available = InvItemGetCount(item, false);
			struct cstr *unit_str = INV_GRID_OPS(self)->GetUnitText(self, item);
			double unit_weight = atof(CStrGet(unit_str));

			if (unit_weight == 0.0)
				unit_weight = 1.0;

			int32_t room = (int32_t) ((self->capacity - self->total) / unit_weight);
			CStrDestroy(&unit_str);

			if (room < available)
				available = room;

			if (qty > available) {
				qty = (int16_t) available;
				StrPrintf(qty_text, "%d", qty);

				struct cstr *new_text = CStrCreateFrom(qty_text);
				EditSetText(edit, new_text);
			}

			item->selected_qty = qty;
			InvGridSetCell(self, item, control_id - GRID_ROW_QTY);
			InvGridReadTotal(self);
			event->handled = true;
		}
	}
}

void InvGridSetLabel(struct inv_grid *self, int32_t id, struct cstr *text)
{
	struct event query = EventMake(EVCLS_ANY, -1, -1);
	EventSetId(&query, id);

	struct clip_label_view *label = find_child(self, &query);
	ClipLabelViewSetLabelAttr(label, text);
}

struct cstr *inv_grid_get_unit_text(struct inv_grid *self, struct inv_item *item)
{
	return CStrCreate();
}

struct cstr *inv_grid_get_line_text(struct inv_grid *self, struct inv_item *item)
{
	return CStrCreate();
}

struct cstr *inv_grid_pause(struct inv_grid *self)
{
	char text[32];

	int32_t weight = InvListTotalWeight(self->list) / 16;
	StrPrintf(text, "%d", weight);

	return CStrCreateFrom(text);
}

void InvGridRebuildPages(struct inv_grid *self)
{
	if (self->table)
		MemFree(self->table);

	if (!self->category_filter) {
		self->table = NULL;
		self->count = self->list->base.count;

	} else {
		self->table = InvListCollectCategory(self->list, self->category_filter, &self->count);
	}

	self->page = 0;
	self->last_page = (self->count - 1) / 9;
}

void InvGridRefresh(struct inv_grid *self)
{
	char text[128];
	struct event query = EventMake(EVCLS_ANY, -1, -1);

	for (int32_t cell = 0; cell < 9; cell++) {
		struct cstr *text_str;
		struct cstr *ref_str;

		struct inv_item *item = InvGridGetItemAt(self, cell);

		if (item == NULL) {
			text_str = CStrCreate();

		} else {
			StrPrintf(text, "%d", InvItemGetCount(item, false));
			text_str = CStrCreateFrom(text);
		}

		InvGridSetLabel(self, cell + GRID_ROW_AMOUNT, text_str);

		if (self->ref_list) {
			if (item == NULL) {
				ref_str = CStrCreate();

			} else {
				struct inv_item *ref_item = InvListFindByType(self->ref_list, item->table_index);

				if (ref_item) {
					StrPrintf(text, "%d", InvItemGetCount(ref_item, false));
					ref_str = CStrCreateFrom(text);

				} else {
					ref_str = CStrCreate();
				}
			}

			InvGridSetLabel(self, cell + GRID_ROW_HAVE, ref_str);
		}

		if (self->has_edits) {
			EventSetId(&query, cell + GRID_ROW_QTY);
			struct edit *edit = find_child(self, &query);
			struct view *edit_view = (struct view *) edit;

			if (item == NULL) {
				VIEW_OPS(edit_view)->Hide(edit_view, false, true);
				text_str = CStrCreate();

			} else {
				VIEW_OPS(edit_view)->Show(edit_view);

				if (item->selected_qty == 0) {
					text[0] = 0;

				} else {
					StrPrintf(text, "%d", item->selected_qty);
				}

				text_str = CStrCreateFrom(text);
			}

			EditSetText(edit, text_str);
			ViewNotifyChanged(edit_view);
		}

		if (item == NULL)
			text_str = CStrCreate();
		else
			text_str = InvItemGetName(item);

		InvGridSetLabel(self, cell + GRID_ROW_NAME, text_str);

		if (item == NULL)
			text_str = CStrCreate();
		else
			text_str = INV_GRID_OPS(self)->GetUnitText(self, item);

		InvGridSetLabel(self, cell + GRID_ROW_UNIT, text_str);
		InvGridSetCell(self, item, cell);
	}

	InvGridReadTotal(self);

	EventSetId(&query, GRID_PREV_PAGE);
	struct evt_handler *page_button = find_child(self, &query);

	if (self->page == 0)
		EVT_HANDLER_OPS(page_button)->ClearHandled(page_button);
	else
		EVT_HANDLER_OPS(page_button)->SetHandled(page_button);

	EventSetId(&query, GRID_NEXT_PAGE);
	page_button = find_child(self, &query);

	if (self->page == self->last_page)
		EVT_HANDLER_OPS(page_button)->ClearHandled(page_button);
	else
		EVT_HANDLER_OPS(page_button)->SetHandled(page_button);
}

void InvGridSetCell(struct inv_grid *self, struct inv_item *item, int32_t cell)
{
	InvGridSetLabel(self, cell + GRID_ROW_LINE, INV_GRID_OPS(self)->GetLineText(self, item));
}

void InvGridReadTotal(struct inv_grid *self)
{
	struct cstr *total_str = INV_GRID_OPS(self)->Pause(self);
	self->total = atof(CStrGet(total_str));
	InvGridSetLabel(self, GRID_TOTAL, total_str);
}

struct inv_item *InvGridGetItemAt(struct inv_grid *self, int32_t cell)
{
	int32_t index = cell + 9 * self->page;

	if (self->count == 0 || self->count <= index)
		return NULL;

	if (!self->category_filter)
		return InvItemFromNode(ListNodeAt(&self->list->base, index));

	return self->table[index];
}

void InvGridSetList(struct inv_grid *self, struct inv_list *list)
{
	if (self->owns_list && self->list)
		InvListDestroy(&self->list);

	self->list = list;
	InvGridRebuildPages(self);
	InvGridRefresh(self);
}


void StoreGridInit(struct store_grid *self, int32_t id, const struct point *pos, int32_t img_id,
	struct inv_list *list, int32_t direct, double capacity)
{
	InvGridInit(&self->base, id, pos, img_id, list, direct, capacity, true, false, 9015, true);
	self->base.base.base.base.base.ops = &STORE_GRID_OPS_TABLE.base.base.base.base.base;

	InvGridSetLabel(&self->base, GRID_HAVE_HEADER, CStrCreateFrom("You"));
	InvGridSetLabel(&self->base, GRID_AMOUNT_HEADER, CStrCreateFrom("Max"));
	InvGridSetLabel(&self->base, GRID_QTY_HEADER, CStrCreateFrom("Buy"));
	InvGridSetLabel(&self->base, GRID_UNIT_HEADER, CStrCreateFrom("Unit Cost"));
	InvGridSetLabel(&self->base, GRID_LINE_HEADER, CStrCreateFrom("Cost"));
	InvGridSetLabel(&self->base, GRID_TOTAL_LABEL, CStrCreateFrom("Total Cost $"));
	InvGridSetLabel(&self->base, GRID_LIMIT_LABEL, CStrCreateFrom("You have $"));
}

struct store_grid *StoreGridCreate(int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t direct, double capacity)
{
	struct store_grid *self = MemAlloc(1, sizeof(struct store_grid));
	StoreGridInit(self, id, pos, img_id, list, direct, capacity);

	return self;
}

void StoreGridDestroy(struct store_grid **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void store_grid_destroy(struct object *obj)
{
	obj->ops = &INV_GRID_OPS_TABLE.base.base.base.base;
	inv_grid_destroy(obj);
}

struct cstr *store_grid_get_unit_text(struct inv_grid *self, struct inv_item *item)
{
	char buf[32];

	if (item == NULL)
		return CStrCreate();

	StrPrintf(buf, "%5.2lf", (double) InvItemGetUnitPrice(item) / 100.0);

	return CStrCreateFrom(buf);
}

struct cstr *store_grid_get_line_text(struct inv_grid *self, struct inv_item *item)
{
	char buf[32];

	if (item == NULL)
		return CStrCreate();

	StrPrintf(buf, "%5.2lf", (double) (InvItemGetUnitPrice(item) * item->selected_qty) / 100.0);

	return CStrCreateFrom(buf);
}

struct cstr *store_grid_pause(struct inv_grid *self)
{
	char buf[32];

	double total = 0.0;
	struct inv_item *item = InvItemFromNode(self->list->base.head);

	while (item) {
		total = (double) (InvItemGetUnitPrice(item) * item->selected_qty) + total;
		item = InvItemFromNode(item->node.next);
	}

	total = total / 100.0;
	StrPrintf(buf, "%5.2lf", total);

	return CStrCreateFrom(buf);
}


void DropGridInit(struct drop_grid *self, int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t direct)
{
	InvGridInit(&self->base, id, pos, img_id, list, direct, (double) InvListTotalWeight(list) / 16.0, true, false,
		9012, false);
	self->base.base.base.base.base.ops = &DROP_GRID_OPS_TABLE.base.base.base.base.base;

	InvGridSetLabel(&self->base, GRID_QTY_HEADER, CStrCreateFrom("Dump"));
	InvGridSetLabel(&self->base, GRID_UNIT_HEADER, CStrCreateFrom("Unit Weight"));
	InvGridSetLabel(&self->base, GRID_LINE_HEADER, CStrCreateFrom("Weight"));
	InvGridSetLabel(&self->base, GRID_TOTAL_LABEL, CStrCreateFrom("Weight to Dump"));
	InvGridSetLabel(&self->base, GRID_LIMIT_LABEL, CStrCreateFrom("Wagon Weight"));
}

struct drop_grid *DropGridCreate(int32_t id, const struct point *pos, int32_t img_id, struct inv_list *list,
	int32_t direct)
{
	struct drop_grid *self = MemAlloc(1, sizeof(struct drop_grid));
	DropGridInit(self, id, pos, img_id, list, direct);

	return self;
}

void DropGridDestroy(struct drop_grid **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void drop_grid_destroy(struct object *obj)
{
	obj->ops = &INV_GRID_OPS_TABLE.base.base.base.base;
	inv_grid_destroy(obj);
}

struct cstr *drop_grid_get_unit_text(struct inv_grid *self, struct inv_item *item)
{
	char buf[32];

	if (item == NULL)
		return CStrCreate();

	StrPrintf(buf, "%5.2lf", (double) INV_ITEM_OPS(item)->GetUnitWeightX16(item) / 16.0);

	return CStrCreateFrom(buf);
}

struct cstr *drop_grid_get_line_text(struct inv_grid *self, struct inv_item *item)
{
	char buf[32];

	if (item == NULL)
		return CStrCreate();

	StrPrintf(buf, "%5.2lf", (double) (INV_ITEM_OPS(item)->GetUnitWeightX16(item) * item->selected_qty) / 16.0);

	return CStrCreateFrom(buf);
}

struct cstr *drop_grid_pause(struct inv_grid *self)
{
	char buf[32];

	double total = 0.0;
	struct inv_item *item = InvItemFromNode(self->list->base.head);

	while (item) {
		total = (double) (INV_ITEM_OPS(item)->GetUnitWeightX16(item) * item->selected_qty) + total;
		item = InvItemFromNode(item->node.next);
	}

	total = total / 16.0;
	StrPrintf(buf, "%5.2lf", total);

	return CStrCreateFrom(buf);
}
