#include "picview.h"

#include <stddef.h>

#include "../draw/dc.h"
#include "../draw/drawable.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../draw/objbase.h"
#include "../draw/pictserver.h"
#include "../draw/region.h"
#include "../draw/server.h"
#include "../draw/windc.h"
#include "window.h"

const struct pic_view_ops PIC_VIEW_OPS_TABLE = {
	{
		{
			{pic_view_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			pic_view_on_event,
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
		pic_view_draw_children,
		view_move_children,
	},
};

static struct drawable *create_picture(int32_t img_id)
{
	return ServerCreateVirtual(&g_pict_server->base.base, img_id, true);
}

static void picture_size(struct drawable *image, struct point *out)
{
	OBJ_BASE_OPS(image)->GetSize(&image->base, out);
}

static void set_clip_to_bounds(struct pic_view *self)
{
	struct view *view = &self->base;

	ObjectDelete(view->region_clip);

	struct rect bounds = VIEW_OPS(view)->GetBounds(view);
	view->region_clip = RegionCreateRect(&bounds, 1, 0, 0);
}

static void region_bounds_from_frame(struct pic_view *self)
{
	ObjectDelete(self->base.region_bounds);
	self->base.region_bounds = RegionCreateRect(&self->base.rect_b, 1, 0, 0);
}

static void fit_views(struct pic_view *self, const struct point *size)
{
	struct rect *a = &self->base.rect_a;
	int32_t right_a = a->left + size->h;
	int32_t bottom_a = a->top + size->v;
	a->right = right_a;
	a->bottom = bottom_a;

	struct rect *b = &self->base.rect_b;
	int32_t right_b = b->left + size->h;
	int32_t bottom_b = b->top + size->v;
	b->right = right_b;
	b->bottom = bottom_b;
}

void PicViewInitImage(struct pic_view *self, int32_t id, const struct rect *rect, struct drawable *image)
{
	ViewInit(&self->base, EVCLS_PICVIEW, id, rect);
	self->base.base.base.ops = &PIC_VIEW_OPS_TABLE.base.base.base;

	self->image = image;
	self->cur_frame = 0;
	set_clip_to_bounds(self);
}

void PicViewInit(struct pic_view *self, int32_t id, const struct rect *rect, int32_t img_id)
{
	ViewInit(&self->base, EVCLS_PICVIEW, id, rect);
	self->base.base.base.ops = &PIC_VIEW_OPS_TABLE.base.base.base;

	self->image = create_picture(img_id);
	self->cur_frame = 0;
	set_clip_to_bounds(self);
}

void PicViewInitPoint(struct pic_view *self, int32_t id, const struct point *pos, int32_t img_id)
{
	ViewInitPoint(&self->base, EVCLS_PICVIEW, id, pos);
	self->base.base.base.ops = &PIC_VIEW_OPS_TABLE.base.base.base;

	struct point image_size = PointMake(0, 0);

	self->image = create_picture(img_id);
	self->cur_frame = 0;

	if (self->image)
		picture_size(self->image, &image_size);

	fit_views(self, &image_size);
	region_bounds_from_frame(self);
	set_clip_to_bounds(self);
}

void PicViewInitPointImage(struct pic_view *self, int32_t id, const struct point *pos, struct drawable *image)
{
	ViewInitPoint(&self->base, EVCLS_PICVIEW, id, pos);
	self->base.base.base.ops = &PIC_VIEW_OPS_TABLE.base.base.base;

	struct point image_size = PointMake(0, 0);

	self->image = image;
	self->cur_frame = 0;

	if (self->image)
		picture_size(self->image, &image_size);

	fit_views(self, &image_size);
	region_bounds_from_frame(self);
	set_clip_to_bounds(self);
}

void PicViewInitPointEmpty(struct pic_view *self, int32_t id, const struct point *pos)
{
	ViewInitPoint(&self->base, EVCLS_PICVIEW, id, pos);
	self->base.base.base.ops = &PIC_VIEW_OPS_TABLE.base.base.base;

	self->image = NULL;
	self->cur_frame = 0;
}

struct pic_view *PicViewCreateImage(int32_t id, const struct rect *rect, struct drawable *image)
{
	struct pic_view *self = MemAlloc(1, sizeof(struct pic_view));
	PicViewInitImage(self, id, rect, image);

	return self;
}

struct pic_view *PicViewCreate(int32_t id, const struct rect *rect, int32_t img_id)
{
	struct pic_view *self = MemAlloc(1, sizeof(struct pic_view));
	PicViewInit(self, id, rect, img_id);

	return self;
}

struct pic_view *PicViewCreatePoint(int32_t id, const struct point *pos, int32_t img_id)
{
	struct pic_view *self = MemAlloc(1, sizeof(struct pic_view));
	PicViewInitPoint(self, id, pos, img_id);

	return self;
}

struct pic_view *PicViewCreatePointImage(int32_t id, const struct point *pos, struct drawable *image)
{
	struct pic_view *self = MemAlloc(1, sizeof(struct pic_view));
	PicViewInitPointImage(self, id, pos, image);

	return self;
}

struct pic_view *PicViewCreatePointEmpty(int32_t id, const struct point *pos)
{
	struct pic_view *self = MemAlloc(1, sizeof(struct pic_view));
	PicViewInitPointEmpty(self, id, pos);

	return self;
}

void PicViewDestroy(struct pic_view **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void pic_view_destroy(struct object *obj)
{
	struct pic_view *self = (struct pic_view *) obj;

	if (self->image)
		ReleaseDrawable(&self->image->base.base, false);

	obj->ops = &VIEW_OPS_TABLE.base.base;
	view_destroy(obj);
}

struct view *CreatePicView(struct ctrl_desc *desc)
{
	struct pic_view *result;
	struct rect bounds = RectMake(desc->rect.top, desc->rect.left, desc->rect.bottom, desc->rect.right);

	if (bounds.bottom != -1) {
		result = PicViewCreate(desc->id, &bounds, desc->obj_type);

	} else {
		struct point top_left = RectTopLeft(&bounds);
		result = PicViewCreatePoint(desc->id, &top_left, desc->obj_type);
	}

	if (result->image) {
		int32_t code = CMD_PICVIEW_FRAME;
		EvtHandlerBindHandler(&result->base.base, 1, &code);

		if (desc->cursor_id > 0)
			VIEW_OPS(result)->SetCursorId(&result->base, desc->cursor_id);

	} else {
		ObjectDelete(result);
		result = NULL;
	}

	return result ? &result->base : NULL;
}

void pic_view_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct pic_view *self = (struct pic_view *) view;

	struct rect b1 = VIEW_OPS(view)->GetBounds(view);
	struct rect b2 = VIEW_OPS(view)->GetBounds(view);
	struct point pt = PointMake(b1.left, b2.top);

	if (self->image) {
		struct dc dc2;
		DcInit(&dc2, dc);
		ViewComputeClip(view, dc);
		DRAWABLE_OPS(self->image)->Draw(self->image, dc, &pt, self->cur_frame, 0);
		dc_destroy(&dc2.base);
	}

	view_draw_children(view, dc, bounds);
}

static void fit_to_image(struct pic_view *self)
{
	struct point image_size = PointMake(0, 0);

	if (self->image)
		picture_size(self->image, &image_size);

	fit_views(self, &image_size);
	region_bounds_from_frame(self);
	set_clip_to_bounds(self);

	ObjectDelete(self->base.region_bounds);

	struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
	self->base.region_bounds = RegionCreateRect(&bounds, 1, 0, 0);
	ViewNotifyChanged(&self->base);
}

void PicViewSetImageByType(struct pic_view *self, int32_t img_id)
{
	if (self->image)
		ReleaseDrawable(&self->image->base.base, false);

	self->image = create_picture(img_id);
	fit_to_image(self);
}

void PicViewSetImage(struct pic_view *self, struct drawable *image)
{
	if (self->image)
		ReleaseDrawable(&self->image->base.base, false);

	self->image = image;
	fit_to_image(self);
}

void pic_view_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct pic_view *self = (struct pic_view *) handler;

	if (evt->command == CMD_PICVIEW_FRAME) {
		if (self->image) {
			switch (evt->param1) {
				case 0:
					++self->cur_frame;

					if (self->image->item_count <= self->cur_frame)
						self->cur_frame = 0;
					break;

				case 1:
					--self->cur_frame;

					if (self->cur_frame < 0)
						self->cur_frame = self->image->item_count - 1;
					break;

				case 2: {
					int32_t want = (int16_t) evt->param2;

					if (self->image->item_count > want)
						self->cur_frame = want;
					break;
				}
			}
		}

		if (self->base.owner_window) {
			struct rect bounds = VIEW_OPS(self)->GetBounds(&self->base);
			VIEW_OPS(self)->DrawChildren(&self->base, &self->base.owner_window->windc->base, &bounds);
		}

		evt->handled = true;
	}
}
