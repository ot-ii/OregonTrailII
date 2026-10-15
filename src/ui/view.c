#include "view.h"

#include <stddef.h>

#include "cursormgr.h"
#include "../draw/dc.h"
#include "../data/docfile.h"
#include "../base/list.h"
#include "../base/memory.h"
#include "../base/object.h"
#include "../draw/region.h"
#include "../draw/screendc.h"
#include "../draw/windc.h"
#include "window.h"

struct view *g_focus_object = NULL;

const struct view_ops VIEW_OPS_TABLE = {
	{
		{view_destroy},
		evt_handler_is_handled,
		evt_handler_set_handled_for,
		evt_handler_set_handled,
		evt_handler_clear_handled_for,
		evt_handler_clear_handled,
		view_on_event,
		view_on_idle,
		view_on_idle_spare,
	},
	view_add_child,
	view_get_bounds,
	view_get_clip_region,
	view_hide,
	view_remove_child,
	view_remove_from_parent,
	view_set_cursor_id,
	view_set_device,
	view_show,
	view_draw_children,
	view_move_children,
};

static struct obarray *child_list_create(void)
{
	return ObarrayCreate(ObjectDelete);
}

static bool child_matches(const void *item, const void *key)
{
	const struct evt_handler *h = item;

	return EventMatches(key, &h->eh_event);
}

void ViewInit(struct view *self, int32_t classid, int32_t id, const struct rect *rect)
{
	EvtHandlerInit(&self->base, classid, id);
	self->base.base.ops = &VIEW_OPS_TABLE.base.base;
	self->rect_a = *rect;
	self->rect_b = *rect;

	self->owner_window = NULL;
	self->cursor_id = 0;
	self->parent = NULL;
	self->has_child_list = false;
	self->child_list = NULL;
	self->data_file = NULL;
	self->enabled = true;
	self->region_clip = RegionCreate();
	struct rect r = *rect;
	self->region_bounds = RegionCreateRect(&r, 1, 0, 0);
	evt_handler_clear_handled(&self->base);
}

void ViewInitId(struct view *self, int32_t id, const struct rect *rect)
{
	ViewInit(self, EVCLS_VIEW, id, rect);
}

void ViewInitPoint(struct view *self, int32_t classid, int32_t id, const struct point *pos)
{
	EvtHandlerInit(&self->base, classid, id);
	self->base.base.ops = &VIEW_OPS_TABLE.base.base;
	self->rect_a = RectMake(pos->v, pos->h, 0, 0);
	self->rect_b = RectMake(pos->v, pos->h, 0, 0);

	self->owner_window = NULL;
	self->cursor_id = 0;
	self->parent = NULL;
	self->data_file = NULL;
	self->has_child_list = false;
	self->child_list = NULL;
	self->enabled = true;
	self->region_clip = RegionCreate();
	self->region_bounds = RegionCreate();
	evt_handler_clear_handled(&self->base);
}

struct view *ViewCreate(int32_t classid, int32_t id, const struct rect *rect)
{
	struct view *self = MemAlloc(1, sizeof(struct view));
	ViewInit(self, classid, id, rect);

	return self;
}

struct view *ViewCreateId(int32_t id, const struct rect *rect)
{
	struct view *self = MemAlloc(1, sizeof(struct view));
	ViewInitId(self, id, rect);

	return self;
}

struct view *ViewCreatePoint(int32_t classid, int32_t id, const struct point *pos)
{
	struct view *self = MemAlloc(1, sizeof(struct view));
	ViewInitPoint(self, classid, id, pos);

	return self;
}

void ViewDestroy(struct view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void view_destroy(struct object *obj)
{
	struct view *self = (struct view *) obj;

	if (self->parent)
		VIEW_OPS(self->parent)->RemoveChild(self->parent, &self->base.eh_event);

	ObjectDelete(self->region_clip);
	ObjectDelete(self->region_bounds);

	if (self == g_focus_object) {
		if (g_mouse_captured)
			g_mouse_captured = false;

		if (self->cursor_id)
			CursorSetId(CURSOR_ARROW);

		g_focus_object = NULL;
	}

	if (self->has_child_list) {
		struct obarray *list = self->child_list;
		ObarrayDestroy(&list);
	}

	self->base.base.ops = &EVT_HANDLER_OPS_TABLE.base;
	evt_handler_destroy(obj);
}

struct view *CreateGroupFromTemplate(struct ctrl_desc *desc)
{
	struct rect bounds = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);

	struct view *view = ViewCreate(desc->_00, desc->id, &bounds);

	if (desc->cursor_id > 0)
		VIEW_OPS(view)->SetCursorId(view, desc->cursor_id);

	return view;
}

void view_add_child(struct view *self, struct view *node)
{
	if (!self->has_child_list) {
		self->has_child_list = true;
		self->child_list = child_list_create();
	}

	node->parent = self;
	node->base.eh_event.event_code = self->base.eh_event.event_code;
	ObarrayAddUnique(self->child_list, node);

	if (self->owner_window != node->owner_window)
		VIEW_OPS(node)->SetDevice(node, self->owner_window);

	ViewSetInheritFlag(node, self->data_file);
	ViewNotifyChanged(node);
}

void ViewSetInheritFlag(struct view *self, struct doc_file *flag)
{
	if (self->data_file != flag) {
		self->data_file = flag;

		if (self->has_child_list) {
			int32_t count = ObarrayCount(self->child_list);

			for (int32_t i = 0; i < count; ++i) {
				struct view *child = ObarrayGetAt(self->child_list, i);

				if (child)
					ViewSetInheritFlag(child, flag);
			}
		}
	}
}

struct rect view_get_bounds(struct view *self)
{
	return self->rect_b;
}

void ViewSetDataFlag(struct view *self)
{
	if (self->data_file)
		DocFileSetDirtyV(self->data_file, true);
}

void ViewSubtractRegion(struct view *self, struct region *rgn)
{
	int32_t i = 0;
	struct view *child;

	if (self->enabled) {
		RegionSubtract(rgn, VIEW_OPS(self)->GetClipRegion(self));

		if (self->has_child_list) {
			do {
				child = ObarrayGetAt(self->child_list, i++);

				if (child)
					ViewSubtractRegion(child, rgn);

			} while (child);
		}
	}
}

void ViewComputeClip(struct view *self, struct screen_dc *dc)
{
	if (!self->enabled)
		return;

	int32_t child_index = 0;
	struct region *clip = ScreenDcGetClip(dc);
	RegionIntersect(clip, self->region_bounds);

	if (self->has_child_list) {
		struct view *child;

		do {
			child = ObarrayGetAt(self->child_list, child_index++);

			if (child)
				ViewSubtractRegion(child, clip);

		} while (child);
	}

	SCREEN_DC_OPS(dc)->SetClipRegion(dc, clip);
	ObjectDelete(clip);
}

void view_draw_children(struct view *self, struct screen_dc *dc, const struct rect *bounds)
{
	(void) bounds;
	int32_t idx = 0;

	if (!self->enabled)
		return;

	if (!self->has_child_list)
		return;

	struct dc scratch_dc;
	DcInit(&scratch_dc, dc);

	struct region *clip = ScreenDcGetClip(dc);
	RegionIntersect(clip, self->region_bounds);
	SCREEN_DC_OPS(dc)->SetClipRegion(dc, clip);

	struct view *child;

	do {
		child = ObarrayGetAt(self->child_list, idx++);

		if (child && child->enabled) {
			struct rect own = VIEW_OPS(self)->GetBounds(self);
			VIEW_OPS(child)->DrawChildren(child, dc, &own);
		}
	} while (child);

	ObjectDelete(clip);
	dc_destroy(&scratch_dc.base);
}

struct region *view_get_clip_region(struct view *self)
{
	return self->region_clip;
}

void view_move_children(struct view *self, int32_t delta, const struct rect *reserved)
{
	(void) reserved;
	int32_t counter = 0;

	if (!self->enabled)
		return;

	if (!self->has_child_list)
		return;

	struct view *child;

	do {
		child = ObarrayGetAt(self->child_list, counter++);

		if (child) {
			if (child->enabled) {
				struct rect own = VIEW_OPS(self)->GetBounds(self);
				VIEW_OPS(child)->MoveChildren(child, delta, &own);
			}
		}
	} while (child);
}

struct view *ViewFindByName(struct view *self, struct event spec)
{
	struct view *found = NULL;
	int32_t child_index = 0;

	if (EventMatches(&self->base.eh_event, &spec)) {
		found = self;

	} else {
		struct obarray *children = ViewGetChildList(self);

		if (children) {
			struct view *child;

			do {
				child = ObarrayGetAt(children, child_index++);

				if (child)
					found = ViewFindByName(child, spec);

			} while (child && !found);
		}
	}

	return found;
}

struct view *ViewFindByTypeId(struct view *self, int32_t code, int32_t id)
{
	struct event spec = EventMake(code, id, -1);

	return ViewFindByName(self, spec);
}

struct view *ViewFindById(struct view *self, int32_t id)
{
	struct event spec = EventMake(EVCLS_ANY, id, -1);

	return ViewFindByName(self, spec);
}

struct view *ViewFindChildAt(struct view *self, struct point pt)
{
	struct view *found = self;
	int32_t i = 0;

	if (self->has_child_list) {
		struct view *child;

		do {
			child = ObarrayGetAt(self->child_list, i++);

			if (child && ViewPointInBounds(child, &pt) && ViewIsVisible(child)) {
				found = ViewFindChildAt(child, pt);
				child = NULL;
			}
		} while (child);
	}

	return found;
}

struct screen_dc *ViewGetWinDc(struct view *self)
{
	if (self->owner_window)
		return &self->owner_window->windc->base;

	return NULL;
}

bool ViewIsVisible(struct view *self)
{
	return self->enabled;
}

void view_hide(struct view *self, bool fill, bool invalidate)
{
	int32_t counter = 0;

	if (!self->enabled)
		return;

	self->enabled = false;
	EVT_HANDLER_OPS(self)->ClearHandled(&self->base);

	if (self->owner_window) {
		if (fill)
			ScreenDcFillRect(&self->owner_window->windc->base, &self->rect_b, 0);

		if (invalidate)
			WindowInvalidate(self->owner_window, &self->rect_b);
	}

	if (self->has_child_list) {
		struct view *child;

		do {
			child = ObarrayGetAt(self->child_list, counter++);

			if (child)
				VIEW_OPS(child)->Hide(child, fill, invalidate);

		} while (child);
	}
}

bool ViewPointInBounds(struct view *self, const struct point *pt)
{
	if (self->rect_b.left > pt->h)
		return false;

	if (self->rect_b.right < pt->h)
		return false;

	if (self->rect_b.top > pt->v)
		return false;

	if (self->rect_b.bottom < pt->v)
		return false;

	return true;
}

struct view *view_remove_child(struct view *self, struct event *ev)
{
	if (!self->has_child_list)
		return NULL;

	struct view *child = ObarrayFind(self->child_list, child_matches, ev);

	if (child) {
		ViewNotifyChanged(child);
		VIEW_OPS(child)->SetDevice(child, NULL);
		ViewSetInheritFlag(child, NULL);
		ObarrayRemoveMatch(self->child_list, child_matches, ev);

		if (!ObarrayCount(self->child_list)) {
			self->has_child_list = false;
			ObarrayDestroy(&self->child_list);
		}

		child->parent = NULL;
	}

	return child;
}

struct view *view_remove_from_parent(struct view *self)
{
	struct view *parent = self->parent;

	if (!self->parent)
		return NULL;

	VIEW_OPS(self->parent)->RemoveChild(self->parent, &self->base.eh_event);

	return parent;
}

void view_on_event(struct evt_handler *self, struct cmd_event *event)
{
	(void) self;
	(void) event;
}

void view_on_idle(struct evt_handler *self, void *task)
{
	(void) self;
	(void) task;
}

void view_on_idle_spare(struct evt_handler *self, void *task)
{
	(void) self;
	(void) task;
}

void ViewNotifyChanged(struct view *self)
{
	if (self->owner_window)
		WindowInvalidate(self->owner_window, &self->rect_b);
}

void view_set_cursor_id(struct view *self, int32_t cursor_id)
{
	if (self->cursor_id != cursor_id) {
		CursorFindOrCreate(cursor_id);

		if (g_focus_object == self)
			CursorSetId(cursor_id);
	}

	self->cursor_id = cursor_id;
}

static void adjust_rect_to_point(struct rect *rect, const struct point *point)
{
	int32_t width = rect->right - rect->left;
	int32_t height = rect->bottom - rect->top;
	int32_t right = point->h + width;
	int32_t bottom = point->v + height;
	int32_t left = point->h;
	int32_t top = point->v;
	rect->top = top;
	rect->left = left;
	rect->right = right;
	rect->bottom = bottom;
}

void ViewMoveTo(struct view *self, const struct point *pos)
{
	int32_t child_index = 0;

	adjust_rect_to_point(&self->rect_a, pos);

	if (self->parent) {
		struct point origin = PointMake(0, 0);
		int32_t origin_h = self->parent->rect_b.left + pos->h;
		int32_t origin_v = self->parent->rect_b.top + pos->v;
		origin.h = origin_h;
		origin.v = origin_v;

		adjust_rect_to_point(&self->rect_b, &origin);

	} else {
		adjust_rect_to_point(&self->rect_b, pos);
	}

	ObjectDelete(self->region_bounds);
	self->region_bounds = RegionCreateRect(&self->rect_b, 1, 0, 0);

	if (self->has_child_list) {
		struct view *child;

		do {
			child = ObarrayGetAt(self->child_list, child_index++);

			if (child) {
				struct point origin = ViewGetOrigin(child);
				ViewMoveTo(child, &origin);
			}
		} while (child);
	}
}

void view_set_device(struct view *self, struct window *device)
{
	int32_t counter = 0;

	self->owner_window = device;

	if (self->owner_window) {
		self->base.eh_event.event_code = self->owner_window->window_id;
		EVT_HANDLER_OPS(self)->SetHandled(&self->base);

	} else {
		self->base.eh_event.event_code = -1;
		EVT_HANDLER_OPS(self)->ClearHandled(&self->base);
	}

	if (!self->has_child_list)
		return;

	struct view *child;

	do {
		child = ObarrayGetAt(self->child_list, counter++);

		if (child) {
			if (child->owner_window != device)
				VIEW_OPS(child)->SetDevice(child, device);
		}
	} while (child);
}

void view_show(struct view *self)
{
	int32_t counter = 0;

	if (self->enabled)
		return;

	self->enabled = true;
	EVT_HANDLER_OPS(self)->SetHandled(&self->base);

	if (self->owner_window)
		WindowInvalidate(self->owner_window, &self->rect_b);

	if (self->has_child_list) {
		struct view *child;

		do {
			child = ObarrayGetAt(self->child_list, counter++);

			if (child)
				VIEW_OPS(child)->Show(child);

		} while (child);
	}
}

void ViewSetVisible(struct view *self)
{
	if (self->enabled)
		return;

	self->enabled = true;
	EVT_HANDLER_OPS(self)->SetHandled(&self->base);

	if (self->owner_window)
		WindowInvalidate(self->owner_window, &self->rect_b);
}

void ViewToLocal(struct view *self, struct point *pt)
{
	int32_t local_h = pt->h - self->rect_b.left;
	int32_t local_v = pt->v - self->rect_b.top;
	pt->h = local_h;
	pt->v = local_v;
}

struct point ViewGetOrigin(struct view *self)
{
	return RectTopLeft(&self->rect_a);
}

struct obarray *ViewGetChildList(struct view *self)
{
	if (self->has_child_list)
		return self->child_list;

	return NULL;
}

bool ViewHitTest(struct view *view, const struct rect *ir)
{
	struct rect t = VIEW_OPS(view)->GetBounds(view);
	struct rect rc;

	return RectIntersect(ir, &t, &rc);
}
