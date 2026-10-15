#include "../controlstate.h"

#ifdef CONTROL

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../ui/button.h"
#include "../../base/byteorder.h"
#include "../../ui/cliplabelview.h"
#include "../../ui/clipview.h"
#include "../../ui/combobox.h"
#include "../../ui/control.h"
#include "../../dialogs/convdlg.h"
#include "../../base/cstr.h"
#include "../../base/date.h"
#include "../debugscreen.h"
#include "../../data/decode.h"
#include "../../draw/dispobj.h"
#include "../../ui/edit.h"
#include "../../app/event.h"
#include "../../app/evthandler.h"
#include "../../ui/framewin.h"
#include "../../game/game.h"
#include "../../game/gamecmd.h"
#include "../../data/gfxdev.h"
#include "../../data/assets.h"
#include "../../hunt/hunting.h"
#include "../../dialogs/huntweapondlg.h"
#include "../../hunt/animalbase.h"
#include "../../data/image.h"
#include "../../screens/invgrid.h"
#include "../../game/invitem.h"
#include "../../game/invlist.h"
#include "../../dialogs/journaldlg.h"
#include "../../draw/label.h"
#include "../../base/list.h"
#include "../../screens/mainui.h"
#include "../../trail/maparea.h"
#include "../../dialogs/mapdlg.h"
#include "../../trail/mapview.h"
#include "../../game/member.h"
#include "../../base/memory.h"
#include "../../trail/meter.h"
#include "../../game/msg.h"
#include "../../trail/node.h"
#include "../../app/otiiapp.h"
#include "../../draw/palette.h"
#include "../../game/party.h"
#include "../../ui/pictctrl.h"
#include "../../dialogs/quickstartdlg.h"
#include "../../app/replay.h"
#include "../../data/rsrc.h"
#include "../../ui/scaler.h"
#include "../../screens/sceneframe.h"
#include "../../draw/sprobj.h"
#include "../../draw/canvas.h"
#include "../../draw/statefuldispobj.h"
#include "../../base/str.h"
#include "../../data/strrsrc.h"
#include "../../draw/text.h"
#include "../../draw/textlabel.h"
#include "../../ui/textview.h"
#include "../../ui/uicontainer.h"
#include "../../ui/view.h"
#include "../../draw/viewsurface.h"
#include "../../game/weather.h"
#include "../../ui/window.h"

static const void *const BUTTON_KINDS[] = {
	&BUTTON_OPS_TABLE, &CHECK_BOX_OPS_TABLE, &CLIP_BUTTON_OPS_TABLE, &DEF_BUTTON_OPS_TABLE, &DIRTY_BUTTON_OPS_TABLE,
	&TEXT_BTN_OPS_TABLE, &PICT_CTRL_OPS_TABLE, &PICT_CHECK_OPS_TABLE, &PICT_RADIO_OPS_TABLE, &SCROLL_CTRL_OPS_TABLE,
	&UI_CONTAINER_OPS_TABLE, NULL
};
static const void *const EDIT_KINDS[] = {&EDIT_OPS_TABLE, NULL};
static const void *const COMBO_BOX_KINDS[] = {&COMBO_BOX_OPS_TABLE, NULL};
static const void *const CLIP_VIEW_KINDS[] = {&CLIP_VIEW_OPS_TABLE, &CLIP_LABEL_VIEW_OPS_TABLE, &TEXT_VIEW_OPS_TABLE,
	NULL};
static const void *const SCENE_FRAME_KINDS[] = {&SCENE_FRAME_OPS_TABLE, NULL};
static const void *const MAP_VIEW_KINDS[] = {&MAP_VIEW_OPS_TABLE, NULL};
static const void *const HUNTING_KINDS[] = {&HUNTING_OPS_TABLE, NULL};
static const void *const METER_KINDS[] = {&METER_OPS_TABLE, NULL};
static const void *const MAP_AREA_KINDS[] = {&MAP_AREA_OPS_TABLE, NULL};
static const void *const INV_GRID_KINDS[] = {&INV_GRID_OPS_TABLE, &STORE_GRID_OPS_TABLE, &DROP_GRID_OPS_TABLE, NULL};
static const void *const TEXT_LABEL_KINDS[] = {&TEXT_LABEL_OPS_TABLE, &STATEFUL_DISP_OBJ_OPS_TABLE,
	&DISP_OBJ_OPS_TABLE, NULL};
static const void *const TEXT_KINDS[] = {&TEXT_OPS_TABLE, NULL};
static const void *const LABEL_KINDS[] = {&LABEL_OPS_TABLE, &TEXT_OPS_TABLE, NULL};

static void *as_kind(const void *obj, const void *const *kinds)
{
	if (!obj)
		return NULL;

	const void *ops = ((const struct object *) obj)->ops;

	for (int32_t i = 0; kinds[i]; ++i)
		if (ops == kinds[i])
			return (void *) obj;

	return NULL;
}

static struct button *as_button(const void *v)
{
	return as_kind(v, BUTTON_KINDS);
}

static struct inv_grid *as_inv_grid(const void *v)
{
	return as_kind(v, INV_GRID_KINDS);
}

static const char *class_name(const void *obj)
{
	static const struct {
		const void *ops;
		const char *name;
	} NAMES[] = {
		{&BUTTON_OPS_TABLE, "class CButton"}, {&CHECK_BOX_OPS_TABLE, "class CCheckBox"},
		{&CLIP_BUTTON_OPS_TABLE, "class CClipButton"}, {&DEF_BUTTON_OPS_TABLE, "class CDefButton"},
		{&DIRTY_BUTTON_OPS_TABLE, "class CDirtyButton"}, {&TEXT_BTN_OPS_TABLE, "class CTextBtn"},
		{&PICT_CTRL_OPS_TABLE, "class CPictCtrl"}, {&PICT_CHECK_OPS_TABLE, "class CPictCheck"},
		{&PICT_RADIO_OPS_TABLE, "class CPictRadio"}, {&SCROLL_CTRL_OPS_TABLE, "class CScrollCtrl"},
		{&UI_CONTAINER_OPS_TABLE, "class CUIContainer"}, {&TEXT_LABEL_OPS_TABLE, "class CTextLabel"},
		{&STATEFUL_DISP_OBJ_OPS_TABLE, "class CStatefulDispObj"}, {&DISP_OBJ_OPS_TABLE, "class CDispObj"},
		{NULL, NULL}
	};

	for (int32_t i = 0; NAMES[i].ops; ++i)
		if (((const struct object *) obj)->ops == NAMES[i].ops)
			return NAMES[i].name;

	return "class CObject";
}

static void append_f(char *dst, int32_t size, int32_t *len, const char *fmt, ...)
{
	va_list a;
	int32_t n;

	if (*len < 0 || *len >= size - 1)
		return;

	va_start(a, fmt);
	n = vsnprintf(dst + *len, (size_t) (size - *len), fmt, a);
	va_end(a);

	if (n > 0)
		*len = *len + n < size - 1 ? *len + n : size - 1;
}

static void copy(char *dst, int32_t size, const struct cstr *s)
{
	dst[0] = 0;
	if (!s)
		return;

	StrCopyN(dst, (size_t) size, CStrGet(s));

	for (char *c = dst; *c; ++c)
		if (*c == '\r' || *c == '\n' || *c == '\t')
			*c = ' ';
}

static void copy_first_line(char *dst, int32_t size, const struct cstr *s)
{
	const char *p;
	int32_t i;

	dst[0] = 0;
	if (!s)
		return;

	p = CStrGet(s);
	for (i = 0; p && p[i] && p[i] != '\r' && p[i] != '\n' && i < size - 1; ++i)
		dst[i] = p[i];
	dst[i] = 0;
}

static void text_line(char *dst, int32_t size, int32_t id, int32_t line)
{
	struct strrsrc *list = g_text_lib ? StrRsrcLoad(g_text_lib, id, true) : NULL;

	dst[0] = 0;
	if (list && line >= 0 && line < StrRsrcCount(list)) {
		struct cstr *s = StrRsrcGetCStr(list, line);
		CStrMacRomanToAnsi(s, 0);
		copy(dst, size, s);
		CStrDestroy(&s);
	}

	StrRsrcDestroy(&list);
}

static void join(char *dst, int32_t size, const struct strrsrc *lines)
{
	int32_t i, len = 0;

	dst[0] = 0;
	for (i = 0; lines && i < StrRsrcCount(lines) && len < size - 2; ++i) {
		struct cstr *s = StrRsrcGetCStr(lines, i);
		char part[1024];

		copy(part, sizeof(part), s);
		CStrDestroy(&s);
		if (!part[0])
			continue;

		append_f(dst, size, &len, "%s%s", len ? " " : "", part);
	}

	dst[size - 1] = 0;
}

static int32_t caption(struct button *b, char *out, int32_t size)
{
	out[0] = 0;
	if (b->label_rsrc && CStrLength(b->label_rsrc) > 0) {
		copy(out, size, b->label_rsrc);

	} else if (b->data_src) {
		struct text_label *tl = as_kind(b->data_src, TEXT_LABEL_KINDS);
		if (tl && tl->text && tl->text->content)
			join(out, size, tl->text->content);
	}

	return out[0] != 0;
}

static struct scene_frame *scene_of(struct view *v)
{
	for (; v; v = v->parent) {
		struct scene_frame *f = as_kind(v, SCENE_FRAME_KINDS);
		if (f)
			return f;
	}

	return NULL;
}

static void store_name(struct scene_frame *f, int32_t store, char *out, int32_t size)
{
	static const char *const drop[] = {"Jumping off town ", "Town ", NULL};
	struct rsrc *r = f && f->data_lib ? AssetsGetElement(f->data_lib, "STORE", (int16_t) store, true) : NULL;
	char line[96], *p, *s;
	int32_t i;

	StrPrintfN(out, (size_t) (size - 1), "store %d", store);
	out[size - 1] = 0;
	if (!r)
		return;

	p = (char *) RsrcData(r);
	for (i = 0; p && i < (int32_t) sizeof(line) - 1 && i < RsrcSize(r) && p[i] && p[i] != '\r' && p[i] != '\n'; ++i)
		line[i] = p[i];
	line[i] = 0;
	RsrcDestroy(&r);

	while (i > 0 && (line[i - 1] == '\t' || line[i - 1] == ':' || line[i - 1] == ' '))
		line[--i] = 0;

	s = line;
	for (i = 0; drop[i]; ++i)
		if (!strncmp(s, drop[i], strlen(drop[i])))
			s += strlen(drop[i]);

	if ((p = strstr(s, " in ")) != NULL)
		*p = 0;

	if (*s)
		StrPrintfN(out, (size_t) (size - 1), "%s", s);
	out[size - 1] = 0;
}

static void scene_name(struct scene_frame *f, int32_t id, char *out, int32_t size)
{
	struct rsrc *r = f && f->data_lib ? AssetsGetElement(f->data_lib, "SCENE", (int16_t) id, true) : NULL;
	int32_t store = 0, hotspots = 0, people = 0, k;

	if (r) {
		RsrcLoadAndUnpack(r);
		const uint8_t *s = RsrcData(r);
		if (s) {
			struct scene_record rec;
			SceneRecordRead(s, &rec);
			store = rec.store_list;
			hotspots = rec.hotspot_count;
			for (k = 0; k < hotspots; ++k) {
				struct scene_hotspot hot;
				SceneHotspotRead(s, k, &hot);
				if (hot.kind == GCMD_TALK)
					++people;
			}
		}
		RsrcDestroy(&r);
	}

	if (store > 0) {
		char name[64];
		store_name(f, store, name, sizeof(name));
		StrPrintfN(out, (size_t) (size - 1), "Go to the %s", name);

	} else if (hotspots == SCENE_NEWSPAPER) {
		StrPrintfN(out, (size_t) (size - 1), "Go to the newspaper office");

	} else if (people) {
		StrPrintfN(out, (size_t) (size - 1), "Go over to %d %s (scene %d)", people, people == 1 ? "person" : "people",
			id);

	} else {
		StrPrintfN(out, (size_t) (size - 1), "Walk on (scene %d)", id);
	}

	out[size - 1] = 0;
}

static void shop_name(struct button *b, int32_t category, char *out, int32_t size)
{
	struct scene_frame *f = scene_of((struct view *) b);
	struct strrsrc *cats = GetItemCategoryNames();
	char cat[48], item[48];
	int32_t len, n = 0, i = 0;
	struct inv_item **list;

	if (cats && category >= 0 && category < StrRsrcCount(cats)) {
		struct cstr *c = StrRsrcGetCStr(cats, category);
		copy(cat, sizeof(cat), c);
		CStrDestroy(&c);

	} else {
		StrPrintfN(cat, sizeof(cat) - 1, "goods %d", category);
	}

	len = 0;
	out[0] = 0;
	append_f(out, size, &len, "Buy: %s", category ? cat : "everything");
	if (f && f->store_items) {
		list = InvListCollectCategory(f->store_items, (int16_t) category, &n);
		if (!category)
			n = 0;

		for (i = 0; i < n && i < 4 && len < size - 8; ++i) {
			copy(item, sizeof(item), list[i]->name);
			append_f(out, size, &len, "%s%s", i ? ", " : " (", item);
		}

		if (n > 4 && len < size - 8)
			append_f(out, size, &len, ", ...");

		if (i && len < size - 2)
			append_f(out, size, &len, ")");

		MemFree(list);
	}

	out[size - 1] = 0;
}

static void guidebook_link(int32_t pos, char *out, int32_t size)
{
	static const struct {
		int32_t page;
		const char *title;
	} toc[] = {
		{1, "Introduction"}, {3, "The Wagon"}, {6, "Draft Animals"}, {15, "The Pace of Travel"},
		{19, "Food and Medicine"}, {23, "Wild Game"}, {28, "Clothing and Supplies"}, {37, "Starting the Journey"},
		{42, "Diseases"}, {59, "More About Medicines"}, {66, "River Crossings"}, {73, "Steep Hills and Canyons"},
		{78, "Rafting Down the Columbia"}, {81, "Deserts"}, {82, "Indian Peoples of the West"},
		{86, "Arriving at Your Destination"}, {89, "From Independence to Fort John"},
		{95, "From Fort John to Fort Bridger"}, {100, "From Fort Bridger to the Junction of the California Trail"},
		{102, "Route to Oregon from the Junction of the California Trail to Fort Boise"},
		{105, "Route to Oregon from Fort Boise to The Dalles"}, {110, "Rafting Down the Columbia from The Dalles"},
		{111, "The \"South Alternate Route\" from Three Islands to East Cow Hollow"},
		{113, "Shortcut from Doe Canyon to the Umatilla River"}, {0, NULL}
	};
	int32_t page = pos - 6, i;

	for (i = 0; toc[i].title; ++i)
		if (toc[i].page == page) {
			StrPrintfN(out, (size_t) (size - 1), "Go to page %d: %s", page, toc[i].title);
			out[size - 1] = 0;
			return;
		}

	StrPrintfN(out, (size_t) (size - 1), "Go to page %d", page);
	out[size - 1] = 0;
}

static void command_name(struct button *b, const char *title, char *out, int32_t size)
{
	static const char *const walk[4] = {"Turn left", "Turn right", "Walk forward", "Turn around"};
	struct control *ctl = &b->base;
	int32_t command = ctl->command_id, code = ctl->command_p1, arg = ctl->command_p2;
	int32_t id = ctl->base.base.eh_event.id;
	const char *name = NULL;
	struct otii_app *app = (struct otii_app *) g_app;

	switch (command) {
	case GCMD_SCENE_WALK:
		if (!arg && code >= 0 && code < 4) {
			name = walk[code];

		} else if (arg == -2) {
			name = "Go back";

		} else if (arg < 0) {
			name = "Leave (back to the trail)";

		} else {
			scene_name(scene_of((struct view *) b), arg, out, size);
			return;
		}
		break;
	case GCMD_PLAY_SFX:
		name = "Poke at something (makes a sound)";
		break;
	case GCMD_BUY_ITEM:
		shop_name(b, code, out, size);
		return;
	case GCMD_TALK: {
		char said[512], *end;
		text_line(said, sizeof(said), code + CONV_TEXT_BASE, 0);
		if ((end = strpbrk(said, ".?!")) != NULL && end - said < 90)
			end[1] = 0;
		else if (strlen(said) > 90)
			StrCopyN(said + 86, 5, " ...");

		if (said[0])
			StrPrintfN(out, (size_t) (size - 1), "Talk to someone: \"%s\"", said);
		else
			StrPrintfN(out, (size_t) (size - 1), "Talk to someone (conversation %d)", code);
		out[size - 1] = 0;
		return;
	}
	case CMD_NEW_GAME:
		name = "New Game";
		break;
	case CMD_LOAD_GAME:
		name = "Load Game";
		break;
	case CMD_NEW_GAME_OPTIONS:
		name = "Quick Start";
		break;
	case CMD_QUICK_START_CANCEL:
		name = "Cancel";
		break;
	case CMD_QUICK_START_REROLL:
		name = "Change";
		break;
	case CMD_QUICK_START_ACCEPT:
		name = "Hit the Trail!";
		break;
	case CMD_END_GAME:
		name = "End Game";
		break;
	case CMD_SAVE_GAME:
		name = "Save Game";
		break;
	case CMD_SAVE_GAME_AS:
		name = "Save Game As";
		break;
	case CMD_CONTROL_ACTION:
		if (!strcmp(title, "Character Creation")) {
			switch (id) {
			case 19:
				name = "Skills...";
				break;
			case 20:
				name = "Levels of play (help)";
				break;
			case 21:
				name = "Occupations (help)";
				break;
			case 12:
				name = "Map of the trails";
				break;
			}

		} else if (!strcmp(title, "Prologue") && id == 2) {
			name = "Continue";

		} else if (!strcmp(title, "Guidebook")) {
			switch (id) {
			case GUIDE_PREV_PAGE:
			case MAP_PREV_PAGE:
				name = "Previous page";
				break;
			case GUIDE_NEXT_PAGE:
			case MAP_NEXT_PAGE:
				name = "Next page";
				break;
			case GUIDE_TAB_CONTENTS:
				name = "Section: Contents";
				break;
			case GUIDE_TAB_ADVICE:
				name = "Section: Advice";
				break;
			case GUIDE_TAB_ITINERARY:
				name = "Section: Itinerary";
				break;
			case MAP_GLOSSARY:
				name = "Glossary (look up a word)";
				break;
			case MAP_START:
				name = "Section: Contents";
				break;
			default:
				if (id >= MAP_FIRST_LINK) {
					StrPrintfN(out, (size_t) (size - 1), "Go to linked entry (position %d)", ctl->command_p1);
					out[size - 1] = 0;
					return;
				}
				guidebook_link(ctl->command_p1, out, size);
				return;
			}

		} else if (!strcmp(title, "Diary")) {
			switch (id) {
			case DIARY_PREV:
				name = "Previous entry";
				break;
			case DIARY_NEXT:
				name = "Next entry";
				break;
			case DIARY_DONE:
				name = "Done";
				break;
			}

		} else if (as_inv_grid(ctl->base.parent)) {
			switch (id) {
			case GRID_NEXT_PAGE:
				name = "Next page of items";
				break;
			case GRID_PREV_PAGE:
				name = "Previous page of items";
				break;
			}

		} else if (!strcmp(title, "Health Status") && g_game && g_game->party) {
			struct member *m = PartyGetMember(g_game->party, (int16_t) (id - 1));
			char who[64];
			if (m && m->name) {
				copy(who, sizeof(who), m->name);
				StrPrintfN(out, (size_t) (size - 1), "Health of %s", who);
				out[size - 1] = 0;
				return;
			}

		} else {
			switch (id) {
			case TOOL_MAP:
				name = "Map";
				break;
			case TOOL_JOURNAL:
				name = app && app->app_state == APP_STATE_HUNTING ? "Reload" : "Journal";
				break;
			case TOOL_HEALTH:
				name = app && app->app_state == APP_STATE_HUNTING ? "Rotate (look another way)" : "Party health";
				break;
			case TOOL_SUPPLIES:
				name = !app ? "Supplies" :
					app->app_state == APP_STATE_HUNTING ? "Back to the party (end the hunt)" :
					app->app_state == APP_STATE_SUPPLIES ? "Back to travel" : "Supplies";
				break;
			case TOOL_HUNT:
				name = "Hunt";
				break;
			case TOOL_TRADE:
				name = "Trade";
				break;
			case TOOL_DUMP:
				name = "Dump (lighten the load)";
				break;
			case TOOL_TURN:
				name = "Turn";
				break;
			}
		}
		break;
	}

	if (name)
		StrPrintfN(out, (size_t) (size - 1), "%s", name);
	else
		StrPrintfN(out, (size_t) (size - 1), "(image button, id %d, command %d/%d)", id, command, code);
	out[size - 1] = 0;
}

struct walk_finds {
	int32_t scene;
	struct meter *meter;
	struct map_area *overview;
	int32_t travel_map;
	struct hunting *hunt;
};

static void view_text(struct view *v, char *dst, int32_t size)
{
	struct clip_view *cv = as_kind(v, CLIP_VIEW_KINDS);
	struct text *t;
	struct label *l;

	dst[0] = 0;
	if (!cv || !cv->shape)
		return;

	t = as_kind(cv->shape, TEXT_KINDS);
	l = as_kind(cv->shape, LABEL_KINDS);
	if (t && t->content)
		join(dst, size, t->content);
	else if (l && l->text)
		join(dst, size, l->text);
}

static void grid_text(struct inv_grid *g, int32_t id, char *dst, int32_t size)
{
	struct view *v = ViewFindById((struct view *) g, (int16_t) id);
	char *s, *e;

	dst[0] = 0;
	if (!v)
		return;

	view_text(v, dst, size);
	for (s = dst; *s == ' '; ++s)
		;
	memmove(dst, s, strlen(s) + 1);
	for (e = dst + strlen(dst); e > dst && e[-1] == ' '; --e)
		e[-1] = 0;
}

static void add_grid(struct inv_grid *g, struct control_state *st)
{
	static const int32_t head_id[6] = {GRID_HAVE_HEADER, GRID_AMOUNT_HEADER, GRID_QTY_HEADER, GRID_ITEM_HEADER,
		GRID_UNIT_HEADER, GRID_LINE_HEADER};
	static const int32_t cell_id[6] = {GRID_ROW_HAVE, GRID_ROW_AMOUNT, GRID_ROW_QTY, GRID_ROW_NAME, GRID_ROW_UNIT,
		GRID_ROW_LINE};
	static const int32_t width[6] = {5, 6, 6, 30, 10, 10};
	char cells[10][6][64], line[200];
	int32_t used[6] = {0}, rows = 0, r, c, len;

	for (c = 0; c < 6; ++c) {
		grid_text(g, head_id[c], cells[0][c], 64);
		if (cells[0][c][0])
			used[c] = 1;
	}

	for (r = 0; r < 9; ++r) {
		char name[64];
		grid_text(g, GRID_ROW_NAME + r, name, sizeof(name));
		if (!name[0])
			continue;

		++rows;
		for (c = 0; c < 6; ++c) {
			if (cell_id[c] == GRID_ROW_QTY) {
				struct edit *e = as_kind(ViewFindById((struct view *) g, (int16_t) (GRID_ROW_QTY + r)), EDIT_KINDS);
				cells[rows][c][0] = 0;
				if (e && e->base.hwnd)
					GetWindowTextA((HWND) e->base.hwnd, cells[rows][c], 64);
				if (e && e->base.hwnd)
					used[c] = 1;

			} else {
				grid_text(g, cell_id[c] + r, cells[rows][c], 64);
			}

			if (cells[rows][c][0])
				used[c] = 1;
		}
	}

	if (used[2] && !cells[0][2][0])
		StrCopyN(cells[0][2], 64, "Qty");

	for (r = 0; r <= rows && st->table_count < 32; ++r) {
		len = 0;
		line[0] = 0;
		for (c = 0; c < 6; ++c)
			if (used[c]) {
				if (c == 3)
					append_f(line, sizeof(line), &len, "%-*s ", width[c], cells[r][c]);
				else
					append_f(line, sizeof(line), &len, "%*s  ", width[c], cells[r][c]);
			}

		while (len > 0 && line[len - 1] == ' ')
			line[--len] = 0;

		StrCopy(st->table[st->table_count++], line);
		if (!r && rows == 0 && st->table_count < 32)
			StrCopy(st->table[st->table_count++], "(no items)");
	}

	if (st->table_count < 32) {
		char a[64], b[64], c2[64], d[64];
		grid_text(g, GRID_LIMIT_LABEL, a, sizeof(a));
		grid_text(g, GRID_LIMIT, b, sizeof(b));
		grid_text(g, GRID_TOTAL_LABEL, c2, sizeof(c2));
		grid_text(g, GRID_TOTAL, d, sizeof(d));
		len = 0;
		line[0] = 0;
		if (a[0])
			append_f(line, sizeof(line), &len, "%s %s   ", a, b);
		if (c2[0])
			append_f(line, sizeof(line), &len, "%s: %s   ", c2, d);
		if (g->last_page > 0)
			append_f(line, sizeof(line), &len, "page %d of %d", g->page + 1, g->last_page + 1);
		if (len)
			StrCopy(st->table[st->table_count++], line);
	}
}

static void known_label(const char *title, struct control_field *f, struct view *v)
{
	static const struct {
		const char *title;
		int32_t id;
		const char *label;
	} known[] = {
		{"Character Creation", 1, "Leader's name"},
		{"Character Creation", 9001, "Level (experience)"},
		{"Character Creation", 9002, "Occupation"},
		{"Character Creation", 9003, "Month"},
		{"Character Creation", 9004, "Year"},
		{"Character Creation", 9005, "Setting out from"},
		{"Character Creation", 9006, "Destination"},
		{"Character Creation", 9007, "Number of companions"},
		{"Character Creation", 9008, "Wagon"},
		{"Character Creation", 9020, "Difficulty for Random"},
		{"Character Creation", 9021, "Seed"},
		{"Diary", 102, "Your own notes for this day"},
		{NULL, 9013, "Travel pace"},
		{NULL, 9014, "Rations"},
		{NULL, 0, NULL}
	};
	struct inv_grid *grid;
	int32_t i;

	for (i = 0; known[i].label; ++i)
		if (known[i].id == f->id && (!known[i].title || !strcmp(known[i].title, title))) {
			StrCopy(f->label, known[i].label);
			return;
		}

	if (!strcmp(title, "Character Creation") && f->id >= 30 && f->id < 35) {
		StrPrintfN(f->label, sizeof(f->label) - 1, "Companion %d name", f->id - 29);
		return;
	}

	if (!strcmp(title, "Character Creation") && f->id >= 9009 && f->id < 9014) {
		StrPrintfN(f->label, sizeof(f->label) - 1, "Companion %d age", f->id - 9008);
		return;
	}

	if ((grid = as_inv_grid(v->parent)) != NULL) {
		if (f->is_choice && f->id == grid->ctl_id) {
			StrCopy(f->label, "Show items");

		} else if (!f->is_choice && f->id >= GRID_ROW_QTY && f->id < GRID_ROW_QTY + 9) {
			char name[48];
			grid_text(grid, GRID_ROW_NAME + f->id - GRID_ROW_QTY, name, sizeof(name));
			StrPrintfN(f->label, sizeof(f->label) - 1, "Quantity: %s", name);
		}
	}
}

static void nearby_label(struct control_state *st, struct control_field *f, const struct rect *r, void *h)
{
	int32_t i, best = -1, best_key = -30000;
	char *s;

	for (i = 0; i < st->text_count; ++i) {
		const struct rect *b = &st->text_box[i];
		if (st->text_hwnd[i] != h)
			continue;
		if (b->right <= r->left + 4 && b->top < r->bottom && b->bottom > r->top && b->right > best_key) {
			best_key = b->right;
			best = i;
		}
	}

	if (best < 0) {
		for (i = 0, best_key = -30000; i < st->text_count; ++i) {
			const struct rect *b = &st->text_box[i];
			if (st->text_hwnd[i] != h)
				continue;
			if (b->bottom <= r->top + 4 && b->left < r->right && b->right > r->left && r->top - b->bottom < 40 &&
				b->bottom > best_key) {
				best_key = b->bottom;
				best = i;
			}
		}
	}

	if (best < 0)
		return;

	StrCopy(f->label, st->text[best]);
	for (s = f->label + strlen(f->label); s > f->label && (s[-1] == ' ' || s[-1] == ':' || s[-1] == ','); --s)
		s[-1] = 0;
	for (s = f->label; *s == ' ' || *s == ','; ++s)
		;
	memmove(f->label, s, strlen(s) + 1);
}

static void walk(struct view *v, struct control_state *st, const char *title, void *h, int32_t in_grid,
	struct walk_finds *finds)
{
	int32_t i;
	struct button *b;
	struct edit *e;
	struct combo_box *cb;
	struct clip_view *cv;
	struct inv_grid *grid;

	if (!v || !v->enabled)
		return;

	if (as_kind(v, SCENE_FRAME_KINDS))
		finds->scene = 1;
	if (as_kind(v, MAP_VIEW_KINDS))
		finds->travel_map = 1;
	if (!finds->hunt)
		finds->hunt = as_kind(v, HUNTING_KINDS);
	if (!finds->meter)
		finds->meter = as_kind(v, METER_KINDS);
	if (!finds->overview)
		finds->overview = as_kind(v, MAP_AREA_KINDS);

	if ((grid = as_inv_grid(v)) != NULL) {
		add_grid(grid, st);
		in_grid = 1;
	}

	if ((b = as_button(v)) != NULL) {
		struct evt_handler *eh = (struct evt_handler *) b;
		if (EVT_HANDLER_OPS(eh)->IsHandled(eh) && st->option_count < 64) {
			struct control_option *o = &st->options[st->option_count];
			o->labelled = caption(b, o->label, sizeof(o->label));
			if (!o->labelled && b->base.command_id)
				command_name(b, title, o->label, sizeof(o->label));
			if (o->label[0]) {
				o->index = st->option_count++;
				o->command = b->base.command_id;
				o->code = b->base.command_p1;
				o->button = b;
				o->hwnd = v->owner_window ? v->owner_window->hwnd : st->hwnd;
			}
		}

	} else if ((e = as_kind(v, EDIT_KINDS)) != NULL) {
		if (e->base.hwnd && st->field_count < 24) {
			struct control_field *f = &st->fields[st->field_count];
			memset(f, 0, sizeof(*f));
			f->index = st->field_count++;
			f->hwnd = e->base.hwnd;
			f->edit = e;
			f->id = e->base.base.base.eh_event.id;
			GetWindowTextA((HWND) e->base.hwnd, f->text, sizeof(f->text));
			known_label(title, f, v);
			if (!f->label[0]) {
				struct rect r = VIEW_OPS(v)->GetBounds(v);
				nearby_label(st, f, &r, h);
			}
		}

	} else if ((cb = as_kind(v, COMBO_BOX_KINDS)) != NULL) {
		if (cb->base.hwnd && cb->items && st->field_count < 24) {
			struct control_field *f = &st->fields[st->field_count];
			memset(f, 0, sizeof(*f));
			f->index = st->field_count++;
			f->is_choice = 1;
			f->combo = cb;
			f->hwnd = cb->base.hwnd;
			f->id = cb->base.base.base.eh_event.id;
			f->choice_count = StrRsrcCount(cb->items);
			if (cb->base.value >= 0 && cb->base.value < StrRsrcCount(cb->items)) {
				struct cstr *s = StrRsrcGetCStr(cb->items, cb->base.value);
				copy(f->text, sizeof(f->text), s);
				CStrDestroy(&s);
			}
			known_label(title, f, v);
			if (!f->label[0]) {
				struct rect r = VIEW_OPS(v)->GetBounds(v);
				nearby_label(st, f, &r, h);
			}
		}

	} else if (!in_grid && (cv = as_kind(v, CLIP_VIEW_KINDS)) != NULL && cv->shape && st->text_count < 24) {
		char *dst = st->text[st->text_count];
		view_text(v, dst, sizeof(st->text[0]));
		if (dst[0]) {
			struct rect r = VIEW_OPS(v)->GetBounds(v);
			st->text_box[st->text_count] = r;
			st->text_hwnd[st->text_count] = h;
			++st->text_count;
		}
	}

	if (v->has_child_list && v->child_list)
		for (i = 0; i < ObarrayCount(v->child_list); ++i)
			walk(ObarrayGetAt(v->child_list, i), st, title, h, in_grid, finds);
}

struct win_list {
	HWND h[32];
	int32_t n;
};

static BOOL CALLBACK enum_ours(HWND h, LPARAM lp)
{
	struct win_list *w = (struct win_list *) lp;

	if (IsWindowVisible(h) && w->n < 32)
		w->h[w->n++] = h;

	return TRUE;
}

static void info(struct control_state *st, const char *fmt, ...)
{
	va_list a;

	if (st->info_count >= 12)
		return;

	va_start(a, fmt);
	vsnprintf(st->info[st->info_count], sizeof(st->info[0]), fmt, a);
	va_end(a);
	st->info[st->info_count][sizeof(st->info[0]) - 1] = 0;
	++st->info_count;
}

static int32_t merge_rows(struct control_state *st, int32_t from, int32_t to)
{
	int32_t i, j, removed = 0;

	for (i = from; i + 1 < to - removed; ) {
		struct rect *a = &st->text_box[i], *b = &st->text_box[i + 1];
		int32_t overlap = (a->bottom < b->bottom ? a->bottom : b->bottom) - (a->top > b->top ? a->top : b->top);
		int32_t shorter = (a->bottom - a->top < b->bottom - b->top ? a->bottom - a->top : b->bottom - b->top);
		if (st->text_hwnd[i] == st->text_hwnd[i + 1] && shorter > 0 && overlap * 2 >= shorter &&
			b->left >= a->right - 4 &&
			b->left - a->right <= 80 &&
			strlen(st->text[i]) + strlen(st->text[i + 1]) + 3 < sizeof(st->text[0])) {
			StrCatN(st->text[i], sizeof(st->text[0]), "  ");
			StrCatN(st->text[i], sizeof(st->text[0]), st->text[i + 1]);
			a->right = b->right;
			for (j = i + 1; j + 1 < st->text_count; ++j) {
				memcpy(st->text[j], st->text[j + 1], sizeof(st->text[0]));
				st->text_box[j] = st->text_box[j + 1];
				st->text_hwnd[j] = st->text_hwnd[j + 1];
			}
			--st->text_count;
			++removed;

		} else {
			++i;
		}
	}

	return removed;
}

static int32_t shot_result(struct hunting *h, int32_t x, int32_t y, int32_t *heavy)
{
	struct canvas *overlay = h->base.surface->cover_mask;
	uint8_t pixel;
	struct point pt;
	int32_t i;

	*heavy = 0;
	if (x < h->blit_rect.left || x >= h->blit_rect.right || y < h->blit_rect.top || y >= h->blit_rect.bottom)
		return -1;

	pixel = overlay ? CanvasGetPixel(overlay, (int16_t) x, (int16_t) y) : 0;
	if (pixel && pixel != 255)
		return -1;

	pt.h = (int16_t) x;
	pt.v = (int16_t) y;
	for (i = 0; i < h->animal_count; ++i) {
		struct spr_obj *a = h->animals[i];
		struct rect *r = &a->region1.rect;
		bool in_shape;
		if (!a->visible || !((struct animal_base *) a)->alive)
			continue;
		if (x < r->left || x > r->right || y < r->top || y > r->bottom)
			continue;

		in_shape = SPR_OBJ_OPS(a)->ObjectHitTest(a, &pt);
		if (h->aim[HUNT_WEAPON] == WEAPON_SHOTGUN) {
			*heavy = in_shape != 0;
			return i;
		}

		if (!in_shape)
			return -1;

		*heavy = (g_game && g_game->party && g_game->party->skills[SKILL_SHARPSHOOTING]) ||
			(r->right - r->left) / 2 + r->left > x;
		return i;
	}

	return -1;
}

static const char *animal_name(struct animal_base *a)
{
	static char unknown[32];
	int32_t s = a->species_id, female = a->looks_variant;

	if (s >= SPECIES_BUFFALO && s <= SPECIES_BUFFALO_FAR)
		return "buffalo";
	if (s >= SPECIES_DEER && s <= SPECIES_DEER_FAR)
		return female ? "deer (doe)" : "deer (buck)";
	if (s >= SPECIES_ANTELOPE && s <= SPECIES_ANTELOPE_FAR)
		return "antelope";
	if (s >= SPECIES_MOOSE && s <= SPECIES_MOOSE_FAR)
		return female ? "moose (cow)" : "moose (bull)";
	if (s >= SPECIES_ELK && s <= SPECIES_ELK_FAR)
		return female ? "elk (cow)" : "elk (bull)";
	if (s >= SPECIES_MOUNTAIN_GOAT && s <= SPECIES_MOUNTAIN_GOAT_FAR)
		return "mountain goat";
	if (s >= SPECIES_GRIZZLY && s <= SPECIES_GRIZZLY_FAR)
		return "grizzly bear";
	if (s >= SPECIES_BLACK_BEAR && s <= SPECIES_BLACK_BEAR_FAR)
		return "black bear";
	if (s >= SPECIES_MALLARD && s <= SPECIES_MALLARD_FAR)
		return female ? "mallard (hen)" : "mallard (drake)";
	if (s >= SPECIES_GOOSE && s <= SPECIES_GOOSE_FAR)
		return "goose";
	if (s >= SPECIES_SQUIRREL && s <= SPECIES_SQUIRREL_FAR)
		return "squirrel";
	if (s >= SPECIES_RABBIT && s <= SPECIES_RABBIT_FAR)
		return "rabbit";
	if (s >= SPECIES_PRAIRIE_DOG && s <= SPECIES_PRAIRIE_DOG_FAR)
		return "prairie dog";

	StrPrintfN(unknown, sizeof(unknown) - 1, "animal kind %d", s / 10);
	unknown[sizeof(unknown) - 1] = 0;
	return unknown;
}

static void add_hunt_targets(struct control_state *st, struct hunting *h, void *hwnd)
{
	static const int32_t side[5] = {-4, -2, 0, 2, 4};
	struct point origin = ViewSurfaceGetOrigin(h->base.surface);
	int32_t i, blocked = 0;

	for (i = 0; i < h->animal_count && st->option_count < 64; ++i) {
		struct spr_obj *a = h->animals[i];
		struct animal_base *animal = (struct animal_base *) a;
		struct rect r;
		int32_t x, y, best_x = 0, best_y = 0, best = -1;

		if (!a || !a->visible || !animal->alive)
			continue;

		r = a->region1.rect;
		if (r.right < h->blit_rect.left || r.left >= h->blit_rect.right || r.bottom < h->blit_rect.top ||
			r.top >= h->blit_rect.bottom)
			continue;

		for (y = r.top - 12; y <= r.bottom; y += 2)
			for (x = r.left; x <= r.right; x += 2) {
				int32_t distance = (h->blit_rect.bottom - y) / 2, lo = 0, hi = 0, dv, s, score = 0, n = 0, heavy;
				if (x < h->blit_rect.left || x >= h->blit_rect.right || y < h->blit_rect.top ||
					y >= h->blit_rect.bottom)
					continue;

				if (distance >= 30 && distance <= 80) {
					hi = 8;

				} else if (distance > 80) {
					lo = 5;
					hi = 12;
				}

				for (dv = lo; dv <= hi; dv += 1)
					for (s = 0; s < 5; ++s) {
						++n;
						if (shot_result(h, x + side[s], y + dv, &heavy) == i)
							score += 2 + heavy;
					}

				if (score * 100 / n > best) {
					best = score * 100 / n;
					best_x = x;
					best_y = y;
				}
			}

		if (best <= 0) {
			++blocked;
			continue;
		}

		struct control_option *o = &st->options[st->option_count];
		int32_t hits = 0, n = 0, dv, s, heavy;
		int32_t distance = (h->blit_rect.bottom - best_y) / 2, lo = 0, hi = 0;
		if (distance >= 30 && distance <= 80) {
			hi = 8;

		} else if (distance > 80) {
			lo = 5;
			hi = 12;
		}

		for (dv = lo; dv <= hi; ++dv)
			for (s = 0; s < 5; ++s, ++n)
				if (shot_result(h, best_x + side[s], best_y + dv, &heavy) == i)
					++hits;

		memset(o, 0, sizeof(*o));
		StrPrintfN(o->label, sizeof(o->label) - 1, "Shoot: %s (%d lbs of meat) at %d,%d -- about %d%% to hit",
			animal_name(animal), animal->meat_yield, origin.h + best_x, origin.v + best_y, n ? 100 * hits / n : 0);
		o->index = st->option_count++;
		o->command = CMD_MOUSE;
		o->x = origin.h + best_x;
		o->y = origin.v + best_y;
		o->hwnd = hwnd;
	}

	if (blocked)
		info(st, "%d animal%s behind scenery -- cannot be hit from here", blocked, blocked == 1 ? " is" : "s are");
}

static void add_info(struct control_state *st, const struct walk_finds *finds)
{
	struct otii_app *app = (struct otii_app *) g_app;
	struct game *g = g_game;
	char name[96];
	int32_t i, len;

	if (finds->meter && finds->meter->step_count > 0)
		info(st, "River progress: %d%% of the way down", 100 * finds->meter->step_cur / finds->meter->step_count);

	if (finds->overview)
		info(st, st->has_game ? "Overview map of the trails (a picture); the highlighted box is where you are: %s" :
			"Overview map of the trails (a picture)%s", st->has_game ? st->location : "");

	if (!g || !g->party || !g->weather)
		return;

	if (g->weather) {
		struct weather *w = g->weather;
		const char *falling = MainUiGetCatFlag(CAT_SNOWING) ? "snowing" :
			MainUiGetCatFlag(CAT_RAINY) ? "raining" : "no rain or snow";
		char line[256];
		len = 0;
		line[0] = 0;
		append_f(line, sizeof(line), &len, "Weather: %s, high %d, low %d, average %d degrees", falling,
			WeatherGetHighTemp(w, 0), WeatherGetLowTemp(w, 0), WeatherAvgTemp(w, 0));
		if (w->snow_depth)
			append_f(line, sizeof(line), &len, ", snow on the ground (%d)", w->snow_depth);
		info(st, "%s", line);
	}

	if (g->cur_node && NodeBaseGetId(g->cur_node) == NODE_KIND_TRAIL) {
		static const char *const terrain[8] = {"open trail", "sand", "uphill", "downhill", "rough trail", "terrain 5",
			"river (raft)", "desert"};
		int32_t t = NodeGetTerrain((struct node *) g->cur_node);
		info(st, "Terrain: %s", t >= 0 && t < 8 ? terrain[t] : "?");
	}

	if (g->node_names && g->next_node_id > 0 && g->cur_node && NodeBaseGetId(g->cur_node) == NODE_KIND_TRAIL) {
		struct cstr *s = StrRsrcGetCStr(g->node_names, g->next_node_id);
		copy_first_line(name, sizeof(name), s);
		CStrDestroy(&s);
		if (strstr(name, "phantom") || !strncmp(name, "Start of Segment", 16) || !strncmp(name, "End of Segment", 14))
			name[0] = 0;

		if (!name[0] && g->miles_to_go > 0)
			info(st, "Next trail point: %d miles ahead", g->miles_to_go);
		else if (name[0] && g->miles_to_go > 0)
			info(st, "Next: %s, %d miles ahead", name, g->miles_to_go);
		else if (name[0])
			info(st, "Next: %s", name);
	}

	if (finds->travel_map)
		info(st, "Trail map on screen: the wagon marker shows the party%s%s", st->location[0] ? " near " : "",
			st->location);

	if (app && app->app_state == APP_STATE_HUNTING && g_hunt_state) {
		struct hunting *h = g_hunt_state;
		info(st, "Hunting with the %s: %d shots left, %d fired, %d hits, %d lbs of meat so far%s",
			h->aim[HUNT_WEAPON] == WEAPON_SHOTGUN ? "shotgun" : "rifle", h->aim[HUNT_SHOTS_LEFT],
			h->aim[HUNT_SHOTS_FIRED], h->aim[HUNT_HITS], h->aim[HUNT_MEAT],
			h->aim[HUNT_SHOTS_LEFT] && !h->ready_to_fire ? "  -- not loaded: use the Reload option" : "");
	}

	if (app && app->ui) {
		char line[256];
		len = 0;
		line[0] = 0;
		for (i = 0; i < CAT_COUNT; ++i)
			if (g_main_ui_cat_flags[i] && app->ui->strings && i < StrRsrcCount(app->ui->strings)) {
				struct cstr *s = StrRsrcGetCStr(app->ui->strings, (int16_t) i);
				copy(name, sizeof(name), s);
				CStrDestroy(&s);
				if (name[0])
					append_f(line, sizeof(line), &len, "%s%s", len ? " " : "", name);
			}

		if (len)
			info(st, "All conditions: %s", line);
	}
}

int32_t ControlGetState(struct control_state *st)
{
	struct win_list wins;
	struct walk_finds finds;
	int32_t w, primary = -1, side = -1;
	char title[128];

	memset(st, 0, sizeof(*st));
	memset(&finds, 0, sizeof(finds));
	st->event_id = -1;
	st->has_game = g_game != NULL;
	if (g_game) {
		if (g_game->date) {
			st->year = g_game->date->year;
			st->month = g_game->date->month;
			st->day = g_game->date->day;
		}

		if (g_game->node_names) {
			struct cstr *name = StrRsrcGetCStr(g_game->node_names, g_game->cur_node_id);
			copy_first_line(st->location, sizeof(st->location), name);
			CStrDestroy(&name);
		}

		if (g_game->event_node)
			st->event_id = g_game->event_node->number_index;
	}

	wins.n = 0;
	EnumThreadWindows(GetCurrentThreadId(), enum_ours, (LPARAM) &wins);
	st->screen = CONTROL_NONE;

	for (w = 0; w < wins.n; ++w) {
		struct window *win = WindowFromHwnd(wins.h[w]);
		if (!win || !win->root_view || !IsWindowEnabled(wins.h[w]))
			continue;

		GetWindowTextA(wins.h[w], title, sizeof(title));
		if (!strcmp(title, "Health Status")) {
			if (side < 0)
				side = w;

		} else if (primary < 0) {
			primary = w;
		}
	}

	if (primary < 0 && side >= 0) {
		primary = side;
		side = -1;
	}

	for (w = 0; w < wins.n; ++w) {
		struct window *win;
		if (w == side)
			continue;

		win = WindowFromHwnd(wins.h[w]);
		if (!win || !win->root_view || !IsWindowEnabled(wins.h[w]))
			continue;

		GetWindowTextA(wins.h[w], title, sizeof(title));
		if (w == primary) {
			st->hwnd = wins.h[w];
			StrCopy(st->title, title);
			st->screen = win->win_type != 1 ? CONTROL_DIALOG : CONTROL_TRAVEL;
		}

		struct walk_finds found;
		memset(&found, 0, sizeof(found));
		walk(win->root_view, st, title, wins.h[w], 0, &found);
		if (w == primary)
			finds = found;
		else if (!finds.meter)
			finds.meter = found.meter;
	}

	if (finds.hunt && st->hwnd)
		add_hunt_targets(st, finds.hunt, st->hwnd);

	st->side_text_first = st->text_count;
	st->side_option_first = st->option_count;
	if (side >= 0) {
		struct window *win = WindowFromHwnd(wins.h[side]);
		struct walk_finds ignored;
		memset(&ignored, 0, sizeof(ignored));
		walk(win->root_view, st, "Health Status", wins.h[side], 0, &ignored);
	}

	st->side_text_first -= merge_rows(st, 0, st->side_text_first);
	merge_rows(st, st->side_text_first, st->text_count);
	if (st->screen == CONTROL_TRAVEL && finds.scene)
		st->screen = CONTROL_TOWN;
	else if (st->screen == CONTROL_TRAVEL && st->side_option_first)
		st->screen = g_game && g_game->event_node ? CONTROL_EVENT : CONTROL_LANDMARK;

	if (st->screen == CONTROL_TRAVEL || st->screen == CONTROL_TOWN || st->screen == CONTROL_LANDMARK ||
		st->screen == CONTROL_EVENT || finds.meter || finds.overview)
		add_info(st, &finds);

	return st->hwnd != NULL;
}

static int32_t write_bmp8(const char *path, const uint8_t *bits, int32_t width, int32_t height, int32_t stride)
{
	PALETTEENTRY pal[256];
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER ih;
	RGBQUAD quad[256];
	FILE *f;
	int32_t i, y;

	memset(pal, 0, sizeof(pal));
	if (g_palette && g_palette->hpal)
		GetPaletteEntries((HPALETTE) g_palette->hpal, 0, 256, pal);

	for (i = 0; i < 256; ++i) {
		quad[i].rgbRed = pal[i].peRed;
		quad[i].rgbGreen = pal[i].peGreen;
		quad[i].rgbBlue = pal[i].peBlue;
		quad[i].rgbReserved = 0;
	}

	memset(&fh, 0, sizeof(fh));
	memset(&ih, 0, sizeof(ih));
	fh.bfType = 0x4D42;
	fh.bfOffBits = sizeof(fh) + sizeof(ih) + sizeof(quad);
	fh.bfSize = fh.bfOffBits + (DWORD) (stride * height);
	ih.biSize = sizeof(ih);
	ih.biWidth = width;
	ih.biHeight = height;
	ih.biPlanes = 1;
	ih.biBitCount = 8;
	ih.biClrUsed = 256;
	if (fopen_s(&f, path, "wb") || !f)
		return 0;

	fwrite(&fh, sizeof(fh), 1, f);
	fwrite(&ih, sizeof(ih), 1, f);
	fwrite(quad, sizeof(quad), 1, f);
	for (y = height - 1; y >= 0; --y)
		fwrite(bits + y * stride, (size_t) stride, 1, f);
	fclose(f);

	return 1;
}

int32_t ControlExportAnimals(const char *dir, char *report, int32_t size)
{
	static const int32_t ids[] = {
		16010, 16020, 16030, 16040, 16041, 16050, 16051, 16060, 16061, 16070, 16071, 16080, 16081, 16090, 16091,
		16100, 16101, 16110, 16111, 16120, 16121, 16160, 16161, 16170, 16171, 16180, 16181, 16190, 16200, 16210,
		16220, 16230, 16240, 16250, 16260, 16270, 16280, 16281, 16290, 16291, 16300, 16301, 16310, 16320, 16330,
		16350, 16360, 16380, 16390, 16410, 16420
	};
	int32_t n, written = 0, len = 0;

	report[0] = 0;
	CreateDirectoryA(dir, NULL);
	for (n = 0; n < (int32_t) (sizeof(ids) / sizeof(ids[0])); ++n) {
		int32_t id = ids[n], kind = (id - 16000) / 10, variant = id % 10, count, i, sheet_w = 0, sheet_h = 0,
			row_w = 0, row_h = 0, stride;
		struct rsrc *r = g_data_lib ? AssetsGetElement(g_data_lib, "CBMP", (int16_t) id, true) : NULL;
		uint8_t *sheet;
		struct cbmp *cbmp;
		char path[MAX_PATH];
		const int32_t PER_ROW = 8, GAP = 4;

		if (!r)
			continue;

		cbmp = CbmpCreate(r);
		RsrcDestroy(&r);
		count = CbmpFrameCount(cbmp);
		if (count <= 0 || count > 200) {
			CbmpDestroy(&cbmp);
			continue;
		}

		for (i = 0; i < count; ++i) {
			int32_t w, h;
			CbmpFrameSize(cbmp, i, &w, &h);
			if (i % PER_ROW == 0) {
				sheet_h += row_h + (i ? GAP : 0);
				if (row_w > sheet_w)
					sheet_w = row_w;
				row_w = row_h = 0;
			}
			row_w += w + GAP;
			if (h > row_h)
				row_h = h;
		}

		sheet_h += row_h;
		if (row_w > sheet_w)
			sheet_w = row_w;
		stride = (sheet_w + 3) & ~3;
		sheet = calloc((size_t) stride, (size_t) sheet_h + 1);
		if (sheet) {
			int32_t x = 0, y = 0;
			row_h = 0;
			for (i = 0; i < count; ++i) {
				int32_t w, h;
				CbmpFrameSize(cbmp, i, &w, &h);
				if (i && i % PER_ROW == 0) {
					y += row_h + GAP;
					x = 0;
					row_h = 0;
				}
				CbmpDecode(cbmp, i, sheet + y * stride + x, stride, DECODE_OPAQUE, false);
				x += w + GAP;
				if (h > row_h)
					row_h = h;
			}

			StrPrintfN(path, sizeof(path) - 1, "%s\\kind_%02d%s_cbmp_%d.bmp", dir, kind, variant ? "_variant" : "", id);
			path[sizeof(path) - 1] = 0;
			if (write_bmp8(path, sheet, sheet_w, sheet_h, stride)) {
				++written;
				append_f(report, size, &len, "%s  (%d frames)\n", path, count);
			}
			free(sheet);
		}

		CbmpDestroy(&cbmp);
	}

	return written;
}

int32_t ControlDebugText(char *buf, int32_t size)
{
	const char *text = DebugScreenText();
	int32_t len = 0;

	buf[0] = 0;
	for (; *text && len < size - 1; ++text)
		if (*text != '\r')
			buf[len++] = *text;
	buf[len] = 0;

	return len;
}

static int32_t click_in(void *hwnd, int32_t x, int32_t y)
{
	int32_t k = ScalerScale();
	LPARAM lp = MAKELPARAM(x * k + k / 2, y * k + k / 2);
	HWND h = (HWND) hwnd;

	if (!h || !IsWindow(h))
		return 0;

	PostMessageA(h, WM_MOUSEMOVE, 0, lp);
	PostMessageA(h, WM_LBUTTONDOWN, MK_LBUTTON, lp);
	PostMessageA(h, WM_LBUTTONUP, 0, lp);

	return 1;
}

int32_t ControlClickAt(const struct control_state *st, int32_t x, int32_t y)
{
	return click_in(st->hwnd, x, y);
}

static struct button *g_fire_button;
static UINT_PTR g_fire_timer;

static void CALLBACK fire_timer(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	struct button *b = g_fire_button;
	struct control_state *st;
	int32_t i;

	KillTimer(NULL, g_fire_timer);
	g_fire_timer = 0;
	g_fire_button = NULL;
	if (!b)
		return;

	st = MemAlloc(1, sizeof(struct control_state));
	if (ControlGetState(st)) {
		for (i = 0; i < st->option_count; ++i)
			if (st->options[i].button == b) {
				ControlFireCommand(&b->base);
				break;
			}
	}
	MemFree(st);
}

static int32_t fire_later(struct button *b)
{
	g_fire_button = b;
	if (!g_fire_timer)
		g_fire_timer = RpSetTimer(NULL, 0, USER_TIMER_MINIMUM, (void *) fire_timer);

	return g_fire_timer != 0;
}

int32_t ControlClick(const struct control_state *st, int32_t index)
{
	struct rect r, other;
	int32_t gx, gy, i, x, y;
	const int32_t GRID = 12;

	if (index < 0 || index >= st->option_count)
		return 0;

	if (!st->options[index].button)
		return click_in(st->options[index].hwnd, st->options[index].x, st->options[index].y);

	if (st->options[index].command == GCMD_SCENE_WALK)
		return fire_later(st->options[index].button);

	struct view *bv = (struct view *) st->options[index].button;
	r = VIEW_OPS(bv)->GetBounds(bv);

	for (gy = 0; gy <= GRID; ++gy)
		for (gx = 0; gx <= GRID; ++gx) {
			int32_t dy = (gy + 1) / 2 * (gy & 1 ? -1 : 1), dx = (gx + 1) / 2 * (gx & 1 ? -1 : 1);
			x = (r.left + r.right) / 2 + dx * (r.right - r.left) / (GRID + 2);
			y = (r.top + r.bottom) / 2 + dy * (r.bottom - r.top) / (GRID + 2);
			for (i = 0; i < st->option_count; ++i) {
				if (i == index || st->options[i].hwnd != st->options[index].hwnd || !st->options[i].button)
					continue;

				struct view *ov = (struct view *) st->options[i].button;
				other = VIEW_OPS(ov)->GetBounds(ov);
				if (x >= other.left && x < other.right && y >= other.top && y < other.bottom)
					break;
			}

			if (i == st->option_count)
				return click_in(st->options[index].hwnd, x, y);
		}

	return fire_later(st->options[index].button);
}

int32_t ControlSetField(const struct control_state *st, int32_t index, const char *text)
{
	const struct control_field *f;
	int32_t i, pick = -1, n = (int32_t) strlen(text);

	if (index < 0 || index >= st->field_count)
		return 0;

	f = &st->fields[index];
	if (!f->is_choice) {
		struct cmd_event *done;
		if (!SetWindowTextA((HWND) f->hwnd, text))
			return 0;

		f->edit->change_pending = false;
		done = CmdEventCreateSpecs(CMD_EDIT_COMMITTED, &f->edit->base.base.base.eh_event, &f->edit->base.c_event);
		done->param1 = 0;
		DispatchGlobalEvent(done);
		return 1;
	}

	if (text[0] == '#')
		pick = atoi(text + 1);

	for (i = 0; pick < 0 && n && i < StrRsrcCount(f->combo->items); ++i) {
		char name[128];
		struct cstr *s = StrRsrcGetCStr(f->combo->items, (int16_t) i);
		copy(name, sizeof(name), s);
		CStrDestroy(&s);
		if (!_strnicmp(name, text, (size_t) n))
			pick = i;
	}

	if (pick < 0 || pick >= StrRsrcCount(f->combo->items))
		return 0;

	SendMessageA((HWND) f->hwnd, CB_SETCURSEL, (WPARAM) pick, 0);
	struct control *combo = &f->combo->base;
	CONTROL_OPS(combo)->OnCommand(combo, GetParent((HWND) f->hwnd), GetDlgCtrlID((HWND) f->hwnd), f->hwnd,
		CBN_SELCHANGE);

	return 1;
}

int32_t ControlFieldChoices(const struct control_state *st, int32_t index, char (*names)[64], int32_t max)
{
	const struct control_field *f;
	int32_t i;

	if (index < 0 || index >= st->field_count || !st->fields[index].is_choice)
		return 0;

	f = &st->fields[index];
	for (i = 0; i < StrRsrcCount(f->combo->items) && i < max; ++i) {
		struct cstr *s = StrRsrcGetCStr(f->combo->items, (int16_t) i);
		copy(names[i], 64, s);
		CStrDestroy(&s);
	}

	return i;
}

int32_t ControlClose(const struct control_state *st)
{
	if (!st->hwnd || !IsWindow((HWND) st->hwnd))
		return 0;

	PostMessageA((HWND) st->hwnd, WM_CLOSE, 0, 0);
	return 1;
}

int32_t ControlMenuItems(int32_t *ids, char (*names)[64], int32_t max)
{
	HMENU bar = g_parent_win ? GetMenu((HWND) g_parent_win->base.hwnd) : NULL;
	int32_t i, j, n = 0;
	char top[32], item[48];

	if (!bar)
		return 0;

	for (i = 0; i < GetMenuItemCount(bar); ++i) {
		HMENU sub = GetSubMenu(bar, i);
		GetMenuStringA(bar, (UINT) i, top, sizeof(top), MF_BYPOSITION);
		for (j = 0; sub && j < GetMenuItemCount(sub) && n < max; ++j) {
			int32_t id = (int32_t) GetMenuItemID(sub, j);
			char *s, *d;
			if (id <= 0 || (GetMenuState(sub, (UINT) j, MF_BYPOSITION) & (MF_GRAYED | MF_DISABLED)))
				continue;

			GetMenuStringA(sub, (UINT) j, item, sizeof(item), MF_BYPOSITION);
			ids[n] = id;
			StrPrintfN(names[n], 63, "%s > %s", top, item);
			names[n][63] = 0;
			for (s = d = names[n]; *s; ++s)
				if (*s != '&')
					*d++ = *s;
			*d = 0;
			++n;
		}
	}

	return n;
}

int32_t ControlMenu(int32_t id)
{
	if (!g_parent_win)
		return 0;

	PostMessageA((HWND) g_parent_win->base.hwnd, WM_COMMAND, (WPARAM) id, 0);
	return 1;
}

static void dump_walk(struct view *v, char *buf, int32_t size, int32_t *len)
{
	int32_t i;
	struct button *b;

	if (!v || *len >= size - 200)
		return;

	if ((b = as_button(v)) != NULL) {
		char cap[96];
		const char *src = "none";
		struct evt_handler *eh = (struct evt_handler *) b;
		if (b->data_src)
			src = class_name(b->data_src);
		caption(b, cap, sizeof(cap));
		append_f(buf, size, len, "%s shown=%d live=%d cmd=%d code=%d label_rsrc=%p data_src=%s caption='%s'\n",
			class_name(b), v->enabled, EVT_HANDLER_OPS(eh)->IsHandled(eh), b->base.command_id, b->base.command_p1,
			(void *) b->label_rsrc, src, cap);
	}

	if (v->has_child_list && v->child_list)
		for (i = 0; i < ObarrayCount(v->child_list); ++i)
			dump_walk(ObarrayGetAt(v->child_list, i), buf, size, len);
}

int32_t ControlDump(char *buf, int32_t size)
{
	struct win_list wins;
	int32_t w, len = 0;

	buf[0] = 0;
	wins.n = 0;
	EnumThreadWindows(GetCurrentThreadId(), enum_ours, (LPARAM) &wins);
	for (w = 0; w < wins.n; ++w) {
		struct window *win = WindowFromHwnd(wins.h[w]);
		if (win && win->root_view) {
			dump_walk(win->root_view, buf, size, &len);
			break;
		}
	}

	return len;
}

#endif
