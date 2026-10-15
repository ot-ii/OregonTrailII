#include "maparea.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../draw/dc.h"
#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "../ui/polyctrl.h"
#include "../draw/shape.h"
#include "../data/strrsrc.h"

const struct map_area_ops MAP_AREA_OPS_TABLE = {
	{
		{
			{
				{
					{map_area_destroy},
					evt_handler_is_handled,
					evt_handler_set_handled_for,
					evt_handler_set_handled,
					evt_handler_clear_handled_for,
					evt_handler_clear_handled,
					pic_control_on_event,
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
				map_area_draw_children,
				view_move_children,
			},
		},
	},
};

void MapAreaInit(struct map_area *self, int32_t id, const struct point *pos, int32_t img_id, const struct event *cmd,
	int32_t rsrc_id, int32_t str_index)
{
	PicControlInitPoint(&self->base, id, pos, img_id, cmd, CMD_CONTROL_ACTION, 0, 0);
	self->base.base.base.base.base.ops = &MAP_AREA_OPS_TABLE.base.base.base.base.base;

	struct strrsrc *lines = NULL;
	struct cstr *desc = NULL;

	if (str_index != 0) {
		lines = StrRsrcLoad(g_data_lib, rsrc_id, true);

		if (lines)
			desc = StrRsrcGetCStr(lines, str_index);
	}

	if (str_index == 0 || !desc) {
		self->outline = PolyCtrlCreate(0, NULL, true, 0, 255, 255, 1);

	} else {
		struct cstr *word = CStrCreate();
		struct point pts[4];
		int32_t x = 0;
		int32_t y = 0;

		for (int32_t point_index = 0; point_index < 4; point_index++) {
			CStrGetWord2(desc, point_index * 2 + 2, word, 0);
			CStrToInt(word, &x);
			CStrGetWord2(desc, point_index * 2 + 3, word, 0);
			CStrToInt(word, &y);
			pts[point_index].h = x;
			pts[point_index].v = y - 5;
		}

		struct point swap = pts[3];
		pts[3] = pts[2];
		pts[2] = swap;

		self->outline = PolyCtrlCreate(4, pts, true, 160, 255, 255, 2);
		CStrDestroy(&word);
	}

	CStrDestroy(&desc);
	StrRsrcDestroy(&lines);
}

struct map_area *MapAreaCreate(int32_t id, const struct point *pos, int32_t img_id, const struct event *cmd,
	int32_t rsrc_id, int32_t str_index)
{
	struct map_area *self = MemAlloc(1, sizeof(struct map_area));
	MapAreaInit(self, id, pos, img_id, cmd, rsrc_id, str_index);

	return self;
}

void MapAreaDestroy(struct map_area **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void map_area_destroy(struct object *obj)
{
	struct map_area *self = (struct map_area *) obj;

	PolyCtrlDestroy(&self->outline);

	obj->ops = &PIC_CONTROL_OPS_TABLE.base.base.base.base;
	pic_control_destroy(obj);
}

void map_area_draw_children(struct view *view, struct screen_dc *dc, const struct rect *bounds)
{
	struct map_area *self = (struct map_area *) view;

	pic_view_draw_children(view, dc, bounds);

	struct dc d;
	DcInit(&d, dc);
	ViewComputeClip(view, dc);
	SHAPE_OPS(self->outline)->DrawOutline(&self->outline->base, dc);
	dc_destroy(&d.base);
}
