#include "mapdlg.h"

#include <stddef.h>

#include "../ui/button.h"
#include "../base/cstr.h"
#include "../ui/cursormgr.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "glossarydlg.h"
#include "../base/memory.h"
#include "../draw/objbase.h"
#include "../app/otiiapp.h"
#include "../app/app.h"
#include "../ui/picview.h"
#include "../draw/pictserver.h"
#include "../draw/server.h"
#include "../data/strrsrc.h"
#include "../ui/view.h"

bool g_glossary_open = false;

const struct map_dlg_ops MAP_DLG_OPS_TABLE = {
	{
		{
			{map_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			map_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		map_dlg_begin,
		modal_dialog_accept,
	},
};

static void set_rect(struct rect *r, int32_t top, int32_t left, int32_t bottom, int32_t right)
{
	r->top = top;
	r->left = left;
	r->right = right;
	r->bottom = bottom;
}

static struct clip_button *new_button(struct map_dlg *self, int32_t id, const struct rect *rect, int32_t p1)
{
	return ClipButtonCreate(id, &self->base.base.eh_event, rect, CMD_CONTROL_ACTION, p1, 0);
}

static void add_child(struct view *parent, void *child)
{
	VIEW_OPS(parent)->AddChild(parent, (struct view *) child);
}

static void set_cursor(void *v, int32_t cursor_id)
{
	VIEW_OPS(v)->SetCursorId((struct view *) v, cursor_id);
}

static void get_size(struct drawable *d, struct point *out)
{
	OBJ_BASE_OPS(d)->GetSize((struct obj_base *) d, out);
}

void MapDlgInit(struct map_dlg *self, struct assets *owner, int32_t page, int32_t year)
{
	ModalDialogInit(&self->base, -2, true, true);
	self->base.base.base.ops = &MAP_DLG_OPS_TABLE.base.base.base;

	struct point left_size = {0, 0};
	struct point right_size = {0, 0};
	struct rect rect = {0, 0, 0, 0};
	int32_t handlers[2];
	struct drawable *right_art;
	struct drawable *left_art;
	struct server *pict_server = (struct server *) g_pict_server;

	handlers[0] = CMD_CLOSE_WINDOW;
	handlers[1] = CMD_CONTROL_ACTION;
	self->owner = owner;
	self->unused_18A = 0;

	if (page % 2)
		self->cur_page = page - 1;
	else
		self->cur_page = page;

	if (year >= 1840) {
		self->page_count = (year - 1840) / 2;
		self->mode = MAP_DLG_GUIDEBOOK;

	} else {
		self->page_count = 20;
		self->mode = MAP_DLG_GLOSSARY;
	}

	switch (self->mode) {
	case MAP_DLG_GUIDEBOOK:
		left_art = ServerCreateVirtual(pict_server, 951, true);
		get_size(left_art, &left_size);
		right_art = ServerCreateVirtual(pict_server, 952, true);
		get_size(right_art, &right_size);
		self->base.title = CStrCreateFrom("Guidebook");
		break;

	case MAP_DLG_GLOSSARY:
		left_art = ServerCreateVirtual(pict_server, 961, true);
		get_size(left_art, &left_size);
		right_art = ServerCreateVirtual(pict_server, 962, true);
		get_size(right_art, &right_size);
		self->base.title = CStrCreateFrom("Glossary");
		g_glossary_open = true;
		break;
	}

	set_rect(&rect, 0, 0, left_size.v, right_size.h + left_size.h);
	self->base.content = ViewCreateId(-2, &rect);
	self->content_rect = rect;
	struct view *content = self->base.content;
	struct rect *cr = &self->content_rect;

	switch (self->mode) {
	case MAP_DLG_GUIDEBOOK:
		set_rect(&rect, cr->bottom - 35, cr->right - 50, cr->bottom, cr->right);
		self->next_page_btn = (struct button *) new_button(self, GUIDE_NEXT_PAGE, &rect, 0);
		set_rect(&rect, cr->bottom - 35, cr->left, cr->bottom, cr->left + 56);
		self->prev_page_btn = (struct button *) new_button(self, GUIDE_PREV_PAGE, &rect, 0);
		set_rect(&rect, cr->top + 34, cr->left, cr->top + 100, cr->left + 30);
		self->left_tab_btn = (struct button *) new_button(self, GUIDE_TAB_CONTENTS, &rect, 0);
		set_rect(&rect, cr->top + 39, cr->right - 20, cr->top + 97, cr->right);
		self->right_tab_btn = (struct button *) new_button(self, GUIDE_TAB_ADVICE, &rect, 0);
		set_rect(&rect, cr->top + 119, cr->right - 20, cr->top + 190, cr->right);
		self->right_lower_tab_btn = (struct button *) new_button(self, GUIDE_TAB_ITINERARY, &rect, 0);
		add_child(content, self->right_lower_tab_btn);
		set_cursor(self->right_lower_tab_btn, CURSOR_PAGE);
		add_child(content, self->right_tab_btn);
		set_cursor(self->right_tab_btn, CURSOR_PAGE);
		break;

	case MAP_DLG_GLOSSARY:
		set_rect(&rect, cr->bottom - 42, cr->right - 46, cr->bottom, cr->right);
		self->next_page_btn = (struct button *) new_button(self, MAP_NEXT_PAGE, &rect, 0);
		set_rect(&rect, cr->bottom - 42, cr->left, cr->bottom, cr->left + 46);
		self->prev_page_btn = (struct button *) new_button(self, MAP_PREV_PAGE, &rect, 0);
		set_rect(&rect, cr->top + 18, cr->left, cr->top + 100, cr->left + 15);
		self->left_tab_btn = (struct button *) new_button(self, MAP_START, &rect, 0);
		set_rect(&rect, cr->top + 18, cr->right - 15, cr->top + 100, cr->right);
		self->right_tab_btn = (struct button *) new_button(self, MAP_GLOSSARY, &rect, 0);
		add_child(content, self->right_tab_btn);
		set_cursor(self->right_tab_btn, CURSOR_PAGE);
		break;
	}

	add_child(content, self->prev_page_btn);
	set_cursor(self->prev_page_btn, CURSOR_LEFT);
	add_child(content, self->next_page_btn);
	set_cursor(self->next_page_btn, CURSOR_RIGHT);
	add_child(content, self->left_tab_btn);
	set_cursor(self->left_tab_btn, CURSOR_PAGE);
	EvtHandlerBindHandler(&self->base.base, 2, handlers);
}

struct map_dlg *MapDlgCreate(struct assets *owner, int32_t page, int32_t year)
{
	struct map_dlg *self = MemAlloc(1, sizeof(struct map_dlg));
	MapDlgInit(self, owner, page, year);

	return self;
}

void MapDlgDestroy(struct map_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void map_dlg_destroy(struct object *obj)
{
	g_glossary_open = false;

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void map_dlg_on_event(struct evt_handler *handler, struct cmd_event *ev)
{
	struct map_dlg *self = (struct map_dlg *) handler;

	switch (ev->command) {
	case CMD_CLOSE_WINDOW:
		self->base.result = DLG_OK;
		ev->handled = true;
		break;

	case CMD_CONTROL_ACTION: {
		ev->handled = true;
		int32_t button_id = ev->spec_b.id;

		if (button_id == MAP_GLOSSARY) {
			ev->handled = true;
			int32_t glossary_pos = 0;
			struct glossary_dlg glossary;
			GlossaryDlgInit(&glossary, self->owner);

			if (ModalDialogRun(&glossary.base, (intptr_t) &glossary_pos, self->base.content->owner_window)
				!= DLG_CANCEL)
				MapDlgSetPosition(self, glossary_pos);

			OBJECT_OPS(&glossary)->destroy((struct object *) &glossary);

		} else {
			int32_t current_position = (int16_t) ev->param1;
			MapDlgNavCommand(self, button_id, current_position);
		}

		break;
	}
	}
}

void MapDlgReleaseHalves(struct map_dlg *self)
{
	VIEW_OPS(self->picview_a)->RemoveChild((struct view *) self->picview_a, &self->overlay_a->base.eh_event);
	VIEW_OPS(self->picview_b)->RemoveChild((struct view *) self->picview_b, &self->overlay_b->base.eh_event);
	ObjectDelete(self->overlay_b);
	ObjectDelete(self->overlay_a);
}

void MapDlgNavCommand(struct map_dlg *self, int32_t command, int32_t current_position)
{
	if (command >= GUIDE_FIRST_LINK && self->mode == MAP_DLG_GUIDEBOOK)
		command = GUIDE_FIRST_LINK;

	if (command >= MAP_FIRST_LINK && self->mode == MAP_DLG_GLOSSARY)
		command = MAP_FIRST_LINK;

	int32_t pos = self->cur_page;

	switch (command) {
	case GUIDE_PREV_PAGE:
		if (self->zones[MAP_DLG_ZONE_COVER].end_page < self->cur_page) {
			if (self->zone == MAP_DLG_ZONE_CONTENTS)
				MapDlgReleaseHalves(self);

			pos = self->cur_page - 2;
			MapDlgScrollTo(self, pos);
		}
		break;

	case GUIDE_NEXT_PAGE:
		if (self->zones[self->cur_zone].end_page - 1 > self->cur_page) {
			if (self->zone == MAP_DLG_ZONE_CONTENTS)
				MapDlgReleaseHalves(self);

			pos = self->cur_page + 2;
			MapDlgScrollTo(self, pos);
		}
		break;

	case GUIDE_TAB_ADVICE:
		if (self->zone == MAP_DLG_ZONE_CONTENTS)
			MapDlgReleaseHalves(self);

		if (self->zones[MAP_DLG_ZONE_ADVICE].start_page != self->cur_page) {
			pos = self->zones[MAP_DLG_ZONE_ADVICE].start_page;
			MapDlgScrollTo(self, pos);
		}
		break;

	case GUIDE_TAB_ITINERARY:
		if (self->zone == MAP_DLG_ZONE_CONTENTS)
			MapDlgReleaseHalves(self);

		if (self->zones[MAP_DLG_ZONE_ITINERARY].start_page - 1 != self->cur_page) {
			pos = self->zones[MAP_DLG_ZONE_ITINERARY].start_page - 1;
			MapDlgScrollTo(self, pos);
		}
		break;

	case GUIDE_TAB_CONTENTS:
		if (self->mode == MAP_DLG_GUIDEBOOK && self->zones[MAP_DLG_ZONE_CONTENTS].start_page != self->cur_page) {
			if (self->zone == MAP_DLG_ZONE_CONTENTS)
				MapDlgReleaseHalves(self);

			pos = self->zones[MAP_DLG_ZONE_CONTENTS].start_page;
			MapDlgScrollTo(self, pos);
		}
		break;

	case GUIDE_FIRST_LINK:
		if (self->zone == MAP_DLG_ZONE_CONTENTS) {
			MapDlgReleaseHalves(self);

			if (current_position % 2)
				pos = current_position - 1;
			else
				pos = current_position;

			MapDlgScrollTo(self, pos);
		}
		break;

	case MAP_PREV_PAGE:
		if (self->zones[MAP_DLG_ZONE_COVER].end_page < self->cur_page) {
			if (self->zone == MAP_DLG_ZONE_CONTENTS)
				MapDlgReleaseHalves(self);

			pos = self->cur_page - 2;
			MapDlgScrollTo(self, pos);
		}
		break;

	case MAP_NEXT_PAGE:
		if (self->zones[self->cur_zone].end_page - 1 > self->cur_page) {
			if (self->zone == MAP_DLG_ZONE_CONTENTS)
				MapDlgReleaseHalves(self);

			pos = self->cur_page + 2;
			MapDlgScrollTo(self, pos);
		}
		break;

	case MAP_START:
		if (self->mode == MAP_DLG_GLOSSARY && self->zones[MAP_DLG_ZONE_CONTENTS].start_page - 1 != self->cur_page) {
			pos = self->zones[MAP_DLG_ZONE_CONTENTS].start_page - 1;
			MapDlgScrollTo(self, pos);
		}
		break;

	case MAP_FIRST_LINK:
		if (self->zone == MAP_DLG_ZONE_CONTENTS) {
			MapDlgReleaseHalves(self);

			if (current_position % 2)
				pos = current_position - 1;
			else
				pos = current_position;

			MapDlgScrollTo(self, pos);
		}
		break;
	}

	self->cur_page = pos;
}

void MapDlgScrollTo(struct map_dlg *self, int32_t pos)
{
	struct server *pict_server = (struct server *) g_pict_server;

	OtiiAppPlaySfx((struct otii_app *) g_app, 0x6987, false);
	self->zone = MapDlgFindZone(self, pos);
	int32_t saved_zone = self->zone;
	self->terrain = MapDlgTerrainTile(self, pos);
	self->pending_image = ServerCreateVirtualPacked(pict_server, self->terrain,
		pos - self->zones[self->zone].start_page + self->zones[self->zone].base_image, true);
	PicViewSetImage(self->picview_a, self->pending_image);
	self->zone = MapDlgFindZone(self, pos + 1);
	self->terrain = MapDlgTerrainTile(self, pos + 1);

	if (self->zone == MAP_DLG_ZONE_NONE ||self->zones[self->zone].end_page < pos + 1) {
		self->pending_image = ServerCreateVirtual(pict_server, self->terrain, true);
		self->zone = saved_zone;

	} else {
		self->pending_image = ServerCreateVirtualPacked(pict_server, self->terrain,
			(pos + 1) - self->zones[self->zone].start_page + self->zones[self->zone].base_image, true);
	}

	PicViewSetImage(self->picview_b, self->pending_image);

	if (self->zone == MAP_DLG_ZONE_CONTENTS)
		MapDlgPlaceLandmarks(self, pos);

	MapDlgUpdateNavArrows(self, pos);
}

void MapDlgPlaceLandmarks(struct map_dlg *self, int32_t segment)
{
	struct rect button_rect = {0, 0, 0, 0};

	self->overlay_b = ViewCreateId(-2, &self->right_page_rect);
	self->overlay_a = ViewCreateId(-2, &self->left_page_rect);
	add_child((struct view *) self->picview_a, self->overlay_a);
	add_child((struct view *) self->picview_b, self->overlay_b);
	self->left_landmark_count = MapDlgLoadSegmentObjects(self, segment);

	if (self->left_landmark_count < 100) {
		for (int32_t i = 0; i < self->left_landmark_count; i++) {
			set_rect(&button_rect, self->landmarks[5 * i + 1], self->landmarks[5 * i],
				self->landmarks[5 * i + 3], self->landmarks[5 * i + 2]);
			self->landmark_btn = new_button(self, i + GUIDE_FIRST_LINK, &button_rect, self->landmarks[5 * i + 4]);
			add_child(self->overlay_a, self->landmark_btn);
			set_cursor(self->landmark_btn, CURSOR_PAGE);
		}
	}

	self->right_landmark_count = MapDlgLoadSegmentObjects(self, segment + 1);

	if (self->right_landmark_count < 100) {
		for (int32_t i = 0; i < self->right_landmark_count; i++) {
			int32_t half = (self->content_rect.right - self->content_rect.left) / 2;
			set_rect(&button_rect, self->landmarks[5 * i + 1], self->landmarks[5 * i] + half,
				self->landmarks[5 * i + 3], self->landmarks[5 * i + 2] + half);
			self->landmark_btn = new_button(self, i + self->left_landmark_count + 7, &button_rect,
				self->landmarks[5 * i + 4]);
			add_child(self->overlay_b, self->landmark_btn);
			set_cursor(self->landmark_btn, CURSOR_PAGE);
		}
	}
}

void map_dlg_begin(struct modal_dialog *dlg, intptr_t param)
{
	(void) param;
	struct map_dlg *self = (struct map_dlg *) dlg;
	struct server *pict_server = (struct server *) g_pict_server;
	struct point image_size = {0, 0};
	struct rect rect = {0, 0, 0, 0};
	struct cstr *word = CStrCreate();
	int32_t val = 0;

	switch (self->mode) {
	case MAP_DLG_GUIDEBOOK:
		self->num_zones = MAP_DLG_ZONE_COUNT;
		self->cur_zone = MAP_DLG_ZONE_ITINERARY;
		self->zone_rsrc = 30000;
		self->base_rsrc = 30005;
		break;

	case MAP_DLG_GLOSSARY:
		self->num_zones = MAP_DLG_ZONE_COUNT;
		self->cur_zone = MAP_DLG_ZONE_ITINERARY;
		self->zone_rsrc = 30100;
		self->base_rsrc = 30101;
		break;
	}

	struct strrsrc *zonebook = StrRsrcLoad(self->owner, self->zone_rsrc, true);
	struct cstr *line = StrRsrcGetCStr(zonebook, self->page_count - 20 * (self->page_count == 20));

	for (int32_t i = 0; i < self->num_zones; i++) {
		CStrGetWord2(line, 3 * i + 1, word, 0);
		CStrToInt(word, &val);
		self->zones[i].base_image = val;
		CStrGetWord2(line, 3 * i + 2, word, 0);
		CStrToInt(word, &val);
		self->zones[i].start_page = (int16_t) val;
		CStrGetWord2(line, 3 * i + 3, word, 0);
		CStrToInt(word, &val);
		self->zones[i].end_page = val;
	}

	CStrDestroy(&line);
	self->zone = MapDlgFindZone(self, self->cur_page);
	self->terrain = MapDlgTerrainTile(self, self->cur_page);

	if (self->zones[self->zone].end_page < self->cur_page + 1) {
		self->pending_image = ServerCreateVirtual(pict_server, self->terrain, true);

	} else {
		self->pending_image = ServerCreateVirtualPacked(pict_server, self->terrain,
			self->cur_page - self->zones[self->zone].start_page + self->zones[self->zone].base_image, true);
	}

	get_size(self->pending_image, &image_size);
	set_rect(&rect, rect.top, rect.left, rect.top + image_size.v, rect.left + image_size.h);
	self->left_page_rect = rect;
	self->picview_a = PicViewCreateImage(-2, &rect, self->pending_image);
	self->zone = MapDlgFindZone(self, self->cur_page + 1);
	self->terrain = MapDlgTerrainTile(self, self->cur_page + 1);
	self->pending_image = ServerCreateVirtualPacked(pict_server, self->terrain,
		self->cur_page + 1 - self->zones[self->zone].start_page + self->zones[self->zone].base_image, true);
	get_size(self->pending_image, &image_size);
	set_rect(&rect, rect.top, rect.left, rect.top + image_size.v, rect.left + image_size.h);

	int32_t half_width = self->content_rect.right - self->content_rect.left;

	if (half_width % 2)
		half_width = half_width / 2 + 1;
	else
		half_width = half_width / 2;

	int32_t width = rect.right - rect.left;
	int32_t height = rect.bottom - rect.top;
	rect.top = 0;
	rect.left = half_width;
	rect.right = width + half_width;
	rect.bottom = height;
	self->right_page_rect = rect;
	self->picview_b = PicViewCreateImage(-2, &rect, self->pending_image);
	add_child(self->base.content, self->picview_a);
	add_child(self->base.content, self->picview_b);

	if (self->zone == MAP_DLG_ZONE_CONTENTS)
		MapDlgPlaceLandmarks(self, self->cur_page);

	MapDlgUpdateNavArrows(self, self->cur_page);
	StrRsrcDestroy(&zonebook);
	CStrDestroy(&word);
}

void MapDlgUpdateNavArrows(struct map_dlg *self, int32_t page)
{
	struct view *prev = (struct view *) self->prev_page_btn;
	struct view *next = (struct view *) self->next_page_btn;

	if (page < 2) {
		VIEW_OPS(prev)->Hide(prev, false, false);
		VIEW_OPS(next)->Show(next);

	} else if (self->zones[self->cur_zone].end_page == page || self->zones[self->cur_zone].end_page - 1 == page) {
		VIEW_OPS(prev)->Show(prev);
		VIEW_OPS(next)->Hide(next, false, false);

	} else if (page > 1 && self->zones[self->cur_zone].end_page > page) {
		VIEW_OPS(prev)->Show(prev);
		VIEW_OPS(next)->Show(next);
	}
}

int32_t MapDlgLoadSegmentObjects(struct map_dlg *self, int32_t segment)
{
	struct strrsrc *rsrc = NULL;
	int32_t value = 0;

	if (self->mode == MAP_DLG_GUIDEBOOK) {
		if (self->page_count <= 3 && segment > 4)
			return 100;

		if (segment == 2)
			rsrc = StrRsrcLoad(self->owner, 30001, true);
		else
			rsrc = StrRsrcLoad(self->owner, segment + self->base_rsrc + 5 * self->page_count - 3, true);

	} else {
		if (self->mode == MAP_DLG_GLOSSARY) {
			if (segment == 0)
				return 100;
			else
				rsrc = StrRsrcLoad(self->owner, self->base_rsrc, true);
		}
	}

	if (!rsrc)
		return 100;

	struct cstr *word = CStrCreate();
	struct cstr *line = StrRsrcGetCStr(rsrc, 0);
	CStrGetWord2(line, 1, word, 0);
	CStrToInt(word, &value);
	int32_t count = (int16_t) value;
	CStrDestroy(&line);

	for (int32_t i = 0; count > i; i++) {
		line = StrRsrcGetCStr(rsrc, i + 1);
		CStrGetWord2(line, 1, word, 0);
		CStrToInt(word, &value);
		self->landmarks[5 * i + 1] = (int16_t) value;
		CStrGetWord2(line, 2, word, 0);
		CStrToInt(word, &value);
		self->landmarks[5 * i] = (int16_t) value;
		CStrGetWord2(line, 3, word, 0);
		CStrToInt(word, &value);
		self->landmarks[5 * i + 3] = (int16_t) value;
		CStrGetWord2(line, 4, word, 0);
		CStrToInt(word, &value);
		self->landmarks[5 * i + 2] = (int16_t) value;
		CStrGetWord2(line, 5, word, 0);
		CStrToInt(word, &value);
		self->landmarks[5 * i + 4] = (int16_t) value;
		CStrDestroy(&line);
	}

	StrRsrcDestroy(&rsrc);
	CStrDestroy(&word);

	return count;
}

int32_t MapDlgFindZone(struct map_dlg *self, int32_t pos)
{
	for (int32_t i = 0; i < MAP_DLG_ZONE_COUNT; i++) {
		if (self->zones[i].start_page <= pos && self->zones[i].end_page >= pos)
			return i;
	}

	return MAP_DLG_ZONE_NONE;
}

int32_t MapDlgTerrainTile(struct map_dlg *self, int32_t pos)
{
	int32_t tile;

	if (self->mode == MAP_DLG_GUIDEBOOK)
		tile = 950;
	else
		tile = 960;

	if (pos == 0) {
		tile += 4;

	} else if (self->zones[self->cur_zone].end_page == pos) {
		if (pos % 2)
			tile += 5;
		else
			++tile;

	} else if (self->zones[self->cur_zone].end_page < pos) {
		tile += 5;

	} else {
		if (pos >= 1 && self->zones[self->cur_zone].end_page > pos) {
			if (pos % 2 == 0)
				++tile;
			else
				tile += 2;
		}
	}

	return tile;
}

void MapDlgSetPosition(struct map_dlg *self, int32_t pos)
{
	int32_t page;

	if (self->zone == MAP_DLG_ZONE_CONTENTS)
		MapDlgReleaseHalves(self);

	if (pos % 2)
		page = pos - 1;
	else
		page = pos;

	if (self->cur_page != page)
		MapDlgScrollTo(self, page);

	self->cur_page = page;
}
