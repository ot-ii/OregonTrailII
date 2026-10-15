//

#pragma once

#include <stdint.h>

#include "../base/geom.h"

struct event;
struct strrsrc;

#define CTRL_DESC_WORDS 21
#define CTRL_DESC_DWORDS 5

struct ctrl_desc {
	union {
		struct {
			int32_t _00;
			int32_t id;
			int32_t parent_id;
			struct rect rect;
			int32_t cursor_id;
			int32_t flags;
			int32_t obj_type;
			int32_t cmd_id;
			int32_t event_id;
			int32_t value;
			int32_t focus_order;
			int32_t rsrc_id;
			int32_t item_index;
			int32_t font_id;
			int32_t margin_x;
			int32_t margin_y;
			int32_t style;
			int32_t arg7;
			int32_t cmd_param1;
			int32_t cmd_param2;
			int32_t brush_color;
			int32_t alt_brush_color;
			int32_t pen_color;
		};
		struct {
			int32_t w[21];
			int32_t dw[5];
		};
	};
	struct event *event;
	struct strrsrc *src;
};
