#include "meter.h"

#include <stddef.h>

#include "../data/assets.h"
#include "../base/byteorder.h"
#include "../ui/framewin.h"
#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "../data/rsrc.h"
#include "tilemap.h"
#include "../draw/windc.h"

static const int32_t g_meter_dx[8] = {-1, -1, 0, 1, 1, 1, 0, -1};
static const int32_t g_meter_dy[8] = {0, -1, -1, -1, 0, 1, 1, 1};

const struct meter_ops METER_OPS_TABLE = {
	{
		{
			{
				{meter_destroy},
				evt_handler_is_handled,
				evt_handler_set_handled_for,
				evt_handler_set_handled,
				evt_handler_clear_handled_for,
				evt_handler_clear_handled,
				pic_view_on_event,
				view_on_idle,
				meter_on_idle_spare,
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
			meter_draw_children,
			view_move_children,
		},
	},
};

int32_t MeterMpvctStartPoint(const uint8_t *mpvct)
{
	return ReadBe16(mpvct);
}

int32_t MeterMpvctStepCount(const uint8_t *mpvct)
{
	return ReadBe16(mpvct + 2);
}

void MeterInit(struct meter *self, int32_t id, const struct point *pos, int32_t img_id, int32_t river_id,
	struct raft_frame *raft)
{
	PicViewInitPoint(&self->base, id, pos, img_id);
	self->base.base.base.base.ops = &METER_OPS_TABLE.base.base.base.base;
	ShapeInit(&self->shape, 0, 255, 255, 1, 0);

	int32_t path_id = 0;

	self->shape.brushColor = 139;
	self->needle_rect.top = 0;
	self->needle_rect.left = 0;
	self->needle_rect.right = 6;
	self->needle_rect.bottom = 6;
	self->river_id = river_id;
	self->raft = raft;
	self->value_min = 30;
	self->step_cur = self->value_min;
	self->step_target = self->step_cur;

	switch (self->river_id) {
		case 1000:
			self->range = 5800;
			self->origin_x = 30;
			self->origin_y = 331;
			path_id = 17556;
			self->reverse_path = 0;
			break;

		case 1001:
			self->range = 3700;
			self->origin_x = 79;
			self->origin_y = 228;
			path_id = 17695;
			self->reverse_path = 1;
			break;

		case 1002:
			self->range = 5800;
			self->origin_x = 73;
			self->origin_y = 216;
			path_id = 17560;
			self->reverse_path = 0;
			break;

		case 1003:
			self->range = 4700;
			self->origin_x = 119;
			self->origin_y = 76;
			path_id = 17562;
			self->reverse_path = 1;
			break;
	}

	self->range = self->range - self->value_min;
	self->origin_x += 455;
	self->origin_y += 19;
	self->needle_x = self->origin_x;
	self->needle_y = self->origin_y;
	self->mpvct_rsrc = AssetsGetElement(g_data_lib, "MPVCT", path_id, true);

	if (self->mpvct_rsrc) {
		self->mpvct_ptr = RsrcData(self->mpvct_rsrc);
		self->step_count = MeterMpvctStepCount(self->mpvct_ptr);
	}
}

struct meter *MeterCreate(int32_t id, const struct point *pos, int32_t img_id, int32_t river_id,
	struct raft_frame *raft)
{
	struct meter *self = MemAlloc(1, sizeof(struct meter));
	MeterInit(self, id, pos, img_id, river_id, raft);

	return self;
}

void MeterDestroy(struct meter **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void meter_destroy(struct object *obj)
{
	struct meter *self = (struct meter *) obj;

	if (self->mpvct_rsrc) {
		RsrcDestroy(&self->mpvct_rsrc);
		self->mpvct_ptr = NULL;
	}

	shape_destroy(&self->shape.base);

	obj->ops = &PIC_VIEW_OPS_TABLE.base.base.base;
	pic_view_destroy(obj);
}

void meter_on_idle_spare(struct evt_handler *handler, void *task)
{
	(void) task;
	struct meter *self = (struct meter *) handler;

	int32_t value = RaftFrameGetTileMap(self->raft)->scroll_pos.v;
	struct screen_dc *dc = &g_parent_win->base.windc->base;

	if (self->value_min > value)
		value = self->value_min;

	value = value - self->value_min;

	if (self->range < value)
		value = self->range;

	double frames = (double) value;
	double range = (double) self->range;
	double ratio = frames / range;
	frames = (double) self->step_count * ratio;
	self->step_target = (int32_t) ((double) self->step_count - frames);

	if (self->step_target < self->step_cur) {
		self->needle_x = self->origin_x;
		self->needle_y = self->origin_y;
		self->step_cur = 0;
	}

	if (self->step_target > self->step_cur) {
		if (self->reverse_path) {
			for (int32_t step = self->step_cur; self->step_target > step; ++step) {
				uint8_t dir_code = self->mpvct_ptr[MPVCT_STEPS + step];
				self->needle_x += g_meter_dx[dir_code];
				self->needle_y += g_meter_dy[dir_code];
			}

		} else {
			for (int32_t step = self->step_cur; self->step_target > step; ++step) {
				uint8_t dir_code = self->mpvct_ptr[MPVCT_STEPS + self->step_count - 1 - step];
				self->needle_x = self->needle_x - g_meter_dx[dir_code];
				self->needle_y = self->needle_y - g_meter_dy[dir_code];
			}
		}

		int32_t new_y = self->needle_y;
		int32_t new_x = self->needle_x;
		int32_t width = self->needle_rect.right - self->needle_rect.left;
		int32_t height = self->needle_rect.bottom - self->needle_rect.top;
		self->needle_rect.top = new_y;
		self->needle_rect.left = new_x;
		self->needle_rect.right = new_x + width;
		self->needle_rect.bottom = new_y + height;

		SHAPE_OPS(&self->shape)->SetRect(&self->shape, &self->needle_rect);
		VIEW_OPS(self)->DrawChildren(&self->base.base, dc, &self->base.base.rect_b);
	}

	self->step_cur = self->step_target;
}

void meter_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct meter *self = (struct meter *) view;

	pic_view_draw_children(view, dc, bounds);

	if (self->needle_rect.left)
		SHAPE_OPS(&self->shape)->Draw(&self->shape, dc);
}
