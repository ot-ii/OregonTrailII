#include "newgamedlg.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../ui/button.h"
#include "clickdlg.h"
#include "../ui/cliplabelview.h"
#include "../ui/combobox.h"
#include "../ui/control.h"
#include "../base/cstr.h"
#include "../ui/cursormgr.h"
#include "datelistbuilder.h"
#include "../ui/dlgwin.h"
#include "../draw/drawable.h"
#include "../ui/edit.h"
#include "../app/event.h"
#include "../data/gfxdev.h"
#include "../game/invitem.h"
#include "../game/invlist.h"
#include "mapdlg.h"
#include "../base/memory.h"
#include "msgbox.h"
#include "../game/newgamescenario.h"
#include "../game/occupmgr.h"
#include "../app/otiiapp.h"
#include "overviewmapdlg.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../app/replay.h"
#include "../base/rng.h"
#include "../app/settings.h"
#include "skilldlg.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../features/twitchcontrol.h"
#include "../features/twitchirc.h"
#include "../ui/view.h"
#include "../game/wagonitem.h"

struct new_game_label_def {
	const char *text;
	int32_t top;
	int32_t left;
	int32_t bottom;
	int32_t right;
	int32_t font_id;
	int32_t align;
	int32_t id;
};

struct new_game_button_def {
	const char *text;
	int32_t v;
	int32_t h;
	int32_t id;
	int32_t img_id;
	int32_t cmd;
	int32_t param;
};

static const struct new_game_label_def g_newgame_labels[20] = {
	{"The Trail Journal of", 20, 60, 40, 200, 3, 0, 0},
	{", formerly a", 90, 170, 110, 245, 3, 0, 0},
	{"who, in", 120, 185, 140, 245, 3, 0, 0},
	{",", 150, 150, 170, 155, 3, 0, 0},
	{"set out from", 180, 30, 200, 110, 3, 0, 0},
	{"to start a new life by settling in", 210, 30, 230, 235, 3, 0, 0},
	{"", 270, 30, 290, 235, 3, 0, 13},
	{"", 300, 30, 320, 235, 3, 0, 14},
	{"", 360, 30, 380, 235, 3, 0, 15},
	{"accompanied by", 20, 310, 40, 410, 3, 0, 0},
	{"others:", 20, 470, 40, 525, 3, 0, 0},
	{", age", 50, 400, 70, 440, 3, 0, 40},
	{", age", 75, 400, 95, 440, 3, 0, 41},
	{", age", 100, 400, 120, 440, 3, 0, 42},
	{", age", 125, 400, 145, 440, 3, 0, 43},
	{", age", 150, 400, 170, 440, 3, 0, 44},
	{"", 195, 310, 215, 510, 3, 1, 16},
	{"", 335, 310, 355, 510, 3, 1, 17},
	{"Seed:", 368, 382, 388, 420, 3, 0, 0},
	{NULL},
};

static const struct new_game_button_def g_newgame_buttons[4] = {
	{"Travel the Trail!", 385, 30, 10, 2201, 0, 0},
	{"Guidebook", 365, 300, 2, 2102, 0, 0},
	{"Random", 395, 300, 3, 2102, 0, 0},
	{NULL},
};

const struct new_game_dlg_ops NEW_GAME_DLG_OPS_TABLE = {
	{
		{
			{new_game_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			new_game_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		new_game_dlg_begin,
		new_game_dlg_accept,
	},
};

static void rect_set(struct rect *r, int32_t top, int32_t left, int32_t bottom, int32_t right)
{
	r->top = top;
	r->left = left;
	r->right = right;
	r->bottom = bottom;
}

static void add_child(struct new_game_dlg *self, void *child)
{
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) child);
}

static struct view *find_by_id(struct new_game_dlg *self, int32_t id)
{
	return ViewFindById(self->base.content, id);
}

static struct window *parent_window(struct new_game_dlg *self)
{
	return self->base.dlg_win ? &self->base.dlg_win->base : NULL;
}


static int32_t age_from_index(int32_t index)
{
	if (index < 16)
		return index + 5;

	return 5 * (index - 15) + 20;
}

static struct cstr *age_text(int32_t age)
{
	char text[8];

	StrPrintf(text, "%d", age);
	return CStrCreateFrom(text);
}

static int32_t read_age(struct edit *edit)
{
	if (!edit)
		return 20;

	struct cstr *text = EditGetText(edit);
	int32_t age = atoi(CStrGet(text));
	CStrDestroy(&text);

	if (age < 1)
		return 20;

	if (age > 99)
		return 99;

	return age;
}

static int32_t g_ng_difficulty = 5;

#if defined(OTII_TWITCH) || defined(TWITCH_PARTY)
static int32_t g_ng_twitch_used;
static int32_t g_ng_twitch_age[5];

static int32_t twitch_age(const char *text)
{
	int32_t age = 0;
	int32_t digits = 0;

	while (*text == ' ')
		++text;

	for (; *text >= '0' && *text <= '9'; ++text, ++digits)
		if (digits < 3)
			age = age * 10 + (*text - '0');

	uint8_t next = (uint8_t) *text;
	if ((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z') || next >= 0x80)
		return 0;

	return !digits || digits > 2 || age < 1 || age > 99 ? 0 : age;
}

static const char *twitch_name(const struct twitch_message *m)
{
	const char *c;

	for (c = m->display_name; *c; ++c)
		if ((uint8_t) *c >= 0x80)
			break;

	return !m->display_name[0] || *c ? m->login : m->display_name;
}

static void twitch_put(struct view *content, int32_t slot, const struct twitch_message *m)
{
	struct edit *name_edit = (struct edit *) ViewFindById(content, slot + 30);
	struct edit *age_edit = (struct edit *) ViewFindById(content, slot + 9009);

	if (name_edit)
		EditSetText(name_edit, CStrCreateFrom(twitch_name(m)));

	g_ng_twitch_age[slot] = twitch_age(m->text);
	if (g_ng_twitch_age[slot] && age_edit)
		EditSetText(age_edit, age_text(g_ng_twitch_age[slot]));
}
#endif

#ifdef TWITCH_PARTY
int32_t NewGamePartyPick(struct new_game_dlg *dlg, const struct twitch_message *m)
{
	if (!dlg || !m || g_ng_twitch_used >= 5)
		return -1;

	int32_t slot = g_ng_twitch_used++;
	twitch_put(dlg->base.content, slot, m);

	struct combo_box *size_combo = (struct combo_box *) ViewFindById(dlg->base.content, 9007);
	if (size_combo && size_combo->base.value + 2 < slot + 1) {
		CONTROL_OPS(size_combo)->SetValue(&size_combo->base, slot + 1 - 2, true);
		NewGameDlgUpdateMemberFields(dlg);
	}

	return slot;
}

int32_t NewGameTestAge(const char *text)
{
	return twitch_age(text);
}

int32_t NewGamePickedSlots(void)
{
	return g_ng_twitch_used;
}

struct new_game_dlg *NewGameDlgRunning(void)
{
	if (!g_running_dialog || ((const struct object *) g_running_dialog)->ops !=
		(const void *) &NEW_GAME_DLG_OPS_TABLE)
		return NULL;

	return (struct new_game_dlg *) g_running_dialog;
}
#endif

#ifdef OTII_TWITCH
static void twitch_fill(struct view *content)
{
	struct twitch_message m;
	char taken[5][64];
	int32_t i;

	g_ng_twitch_used = 0;

	for (i = 0; i < 5 && RpQueryBytes(RPQ_PARTY_ENTRY, &m, sizeof(m), TwitchPartyPeek(i, &m)); ++i) {
		twitch_put(content, i, &m);
		StrCopy(taken[i], m.login);
		++g_ng_twitch_used;
	}

	for (int32_t k = 0; i < 5 && RpQueryBytes(RPQ_PARTY_ENTRY, &m, sizeof(m), TwitchChatterPeek(k, &m)); ++k) {
		struct edit *name_edit = (struct edit *) ViewFindById(content, i + 30);
		int32_t j;

		for (j = 0; j < i && !StrEqualNoCase(taken[j], m.login); ++j)
			;

		if (j < i)
			continue;

		if (name_edit)
			EditSetText(name_edit, CStrCreateFrom(twitch_name(&m)));

		StrCopy(taken[i], m.login);
		++i;
	}

	struct combo_box *size_combo = (struct combo_box *) ViewFindById(content, 9007);
	if (size_combo)
		CONTROL_OPS(size_combo)->SetValue(&size_combo->base, 5 - 2, true);
}
#endif

void NewGameDlgInit(struct new_game_dlg *self)
{
	ModalDialogInit(&self->base, 46, false, true);
	self->base.base.base.ops = &NEW_GAME_DLG_OPS_TABLE.base.base.base;

	char count_text[32];
	int32_t handled_cmds[3];
	int32_t picked_names[6];
	struct point origin;
	struct rect rect = {0, 0, 0, 0};
	struct view *child;
	struct strrsrc *items;
	struct cstr *label;

	self->occmgr = OccupMgrCreate();
	self->date_lists = DateListBuilderCreate();

	for (int32_t index = 0; index < 15; index++) {
		self->skill_flags.checked[index] = false;
		self->skill_flags.enabled[index] = false;
	}

	self->base.title = CStrCreateFrom("Character Creation");

	origin.h = 0;
	origin.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, 2200);

	for (int32_t index = 0; g_newgame_labels[index].text; index++) {
		const struct new_game_label_def *label_def = &g_newgame_labels[index];
		rect_set(&rect, label_def->top, label_def->left, label_def->bottom, label_def->right);
		label = CStrCreateFrom(label_def->text);
		child = (struct view *) ClipLabelViewCreate(label_def->id, &rect, label, label_def->font_id,
			label_def->align, 1, 0, 0, 255, 0, 1);
		add_child(self, child);
	}

	struct event *ev = &self->base.base.eh_event;

	rect.top = 50;
	rect.left = 65;
	rect.right = 180;
	rect.bottom = 75;
	label = CStrCreate();
	child = (struct view *) EditCreate(1, ev, &rect, label, false, true, 0, 3, 20, false, false, 0, false);
	add_child(self, child);

	items = StrRsrcCreate();
	StrRsrcAppend(items, "A Greenhorn");
	StrRsrcAppend(items, "An adventurer");
	StrRsrcAppend(items, "A trail guide");
	rect.top = 90;
	rect.left = 30;
	rect.right = 165;
	rect.bottom = 110;
	child = (struct view *) ComboBoxCreate(9001, ev, &rect, 60, items, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	add_child(self, child);

	items = StrRsrcLoad(g_text_lib, 2200, true);
	rect.top = 120;
	rect.left = 30;
	rect.right = 180;
	rect.bottom = 140;
	child = (struct view *) ComboBoxCreate(9002, ev, &rect, 290, items, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	add_child(self, child);

	rect.top = 150;
	rect.left = 30;
	rect.right = 145;
	rect.bottom = 170;
	items = StrRsrcLoad(g_text_lib, 2206, true);
	child = (struct view *) ComboBoxCreate(9003, ev, &rect, 140, items, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	add_child(self, child);

	rect.top = 150;
	rect.left = 160;
	rect.right = 230;
	rect.bottom = 170;
	items = StrRsrcLoad(g_text_lib, 2202, true);
	child = (struct view *) ComboBoxCreate(9004, ev, &rect, 225, items, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	add_child(self, child);

	rect.top = 180;
	rect.left = 109;
	rect.right = 240;
	rect.bottom = 200;
	child = (struct view *) ComboBoxCreate(9005, ev, &rect, 80, NULL, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	add_child(self, child);

	rect.top = 240;
	rect.left = 30;
	rect.right = 240;
	rect.bottom = 260;
	child = (struct view *) ComboBoxCreate(9006, ev, &rect, 80, NULL, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	add_child(self, child);

	items = StrRsrcLoad(g_text_lib, 2207, true);
	rect.top = 330;
	rect.left = 30;
	rect.right = 240;
	rect.bottom = 350;
	child = (struct view *) ComboBoxCreate(9008, ev, &rect, 60, items, 1);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	add_child(self, child);

	items = StrRsrcCreate();

	for (int32_t index = 2; index <= 5; index++) {
		StrPrintf(count_text, "%d", index);
		StrRsrcAppend(items, count_text);
	}

	rect.top = 20;
	rect.left = 415;
	rect.right = 465;
	rect.bottom = 40;
	child = (struct view *) ComboBoxCreate(9007, ev, &rect, 80, items, 0);
	((struct combo_box *) child)->font_id = 3;
	ViewNotifyChanged(child);
	CONTROL_OPS(child)->SetValue((struct control *) child, 2, true);
	add_child(self, child);

	for (int32_t index = 0; index < 5; index++)
		picked_names[index] = -1;

	items = StrRsrcLoad(g_text_lib, 2203, true);

	for (int32_t index = 0; index < 5; index++) {
		bool is_unique = false;
		rect.top = 25 * index + 50;
		rect.left = 310;
		rect.right = 395;
		rect.bottom = 25 * index + 75;

		while (!is_unique) {
			picked_names[index] = RandRangeAvg(0, StrRsrcCount(items) - 1, 1);
			is_unique = true;

			for (int32_t prev_index = 0; index > prev_index; prev_index++) {
				if (picked_names[prev_index] == picked_names[index])
					is_unique = false;
			}
		}

		label = StrRsrcGetCStr(items, picked_names[index]);
		child = (struct view *) EditCreate(index + 30, ev, &rect, label, false, true, 0, 3, 0, false, false, 0,
			false);
		add_child(self, child);

		rect.top = 25 * index + 50;
		rect.left = 440;
		rect.right = 500;
		rect.bottom = 25 * index + 70;

		int32_t age;

		if (index < 2) {
			age = RandRangeAvg(15, 20, 1);

		} else if (index < 4) {
			age = RandRangeAvg(4, 17, 1);

		} else {
			age = RandRangeAvg(18, 24, 1);
		}

		child = (struct view *) EditCreate(index + 9009, ev, &rect, age_text(age_from_index(age)), false, true, 0, 3,
			2, true, false, 0xFD, false);
		add_child(self, child);
	}

	StrRsrcDestroy(&items);

	origin.h = 335;
	origin.v = 220;
	child = (struct view *) PicViewCreatePoint(18, &origin, 2203);
	add_child(self, child);

	static const int32_t page_buttons[4][5] = {
		{305, 1, 365, 25, 19},
		{65, 1, 125, 25, 20},
		{126, 1, 198, 25, 21},
		{235, 1, 295, 25, 12},
	};

	for (int32_t n = 0; n < 4; n++) {
		rect.top = page_buttons[n][0];
		rect.left = page_buttons[n][1];
		rect.right = page_buttons[n][3];
		rect.bottom = page_buttons[n][2];
		int32_t id = page_buttons[n][4];
		child = (struct view *) ClipButtonCreate(id, ev, &rect, CMD_CONTROL_ACTION, id, 0);
		((struct control *) child)->flags = 0;
		add_child(self, child);
		VIEW_OPS(child)->SetCursorId(child, CURSOR_PAGE);
	}

	for (int32_t index = 0; g_newgame_buttons[index].text; index++) {
		const struct new_game_button_def *button_def = &g_newgame_buttons[index];
		origin.h = button_def->h;
		origin.v = button_def->v;
		label = CStrCreateFrom(button_def->text);
		child = (struct view *) PictCtrlCreate(button_def->id, ev, &origin, button_def->img_id, button_def->cmd,
			button_def->param, label, 3, 1, 1);
		((struct control *) child)->flags = 0;
		add_child(self, child);
	}

	{
		struct strrsrc *levels = StrRsrcCreate();
		char level_text[32];

		for (int32_t index = 1; index <= 10; index++) {
			if (index == 1) {
				StrCopy(level_text, "Difficulty 1 (easy)");

			} else if (index == 10) {
				StrCopy(level_text, "Difficulty 10 (hard)");

			} else {
				StrPrintf(level_text, "Difficulty %d", index);
			}

			StrRsrcAppend(levels, level_text);
		}

#ifdef OTII_TWITCH
		g_ng_difficulty = TwitchControlNewGameDifficulty();
#endif
		rect_set(&rect, 398, 382, 418, 520);
		child = (struct view *) ComboBoxCreate(9020, ev, &rect, 170, levels, 0);
		((struct combo_box *) child)->font_id = 3;
		ViewNotifyChanged(child);
		add_child(self, child);
		CONTROL_OPS(child)->SetValue((struct control *) child, g_ng_difficulty - 1, true);
	}

	{
		char seed_text[16];

		StrPrintf(seed_text, "%u", RpTime());
		rect_set(&rect, 366, 420, 386, 520);
		child = (struct view *) EditCreate(9021, ev, &rect, CStrCreateFrom(seed_text), false, true, 0, 3, 10, true,
			false, 0xFD, false);
		add_child(self, child);
	}

#ifdef OTII_TWITCH
	twitch_fill(self->base.content);
#endif
#ifdef TWITCH_PARTY
	g_ng_twitch_used = 0;
#endif

	handled_cmds[0] = CMD_CONTROL_ACTION;
	handled_cmds[1] = CMD_EDIT_COMMITTED;
	handled_cmds[2] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 3, handled_cmds);
}

struct new_game_dlg *NewGameDlgCreate(void)
{
	struct new_game_dlg *self = MemAlloc(1, sizeof(struct new_game_dlg));
	NewGameDlgInit(self);

	return self;
}

void NewGameDlgDestroy(struct new_game_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void new_game_dlg_destroy(struct object *obj)
{
	struct new_game_dlg *self = (struct new_game_dlg *) obj;

	OccupMgrDestroy(&self->occmgr);
	DateListBuilderDestroy(&self->date_lists);

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void new_game_dlg_begin(struct modal_dialog *dlg, intptr_t param)
{
	(void) param;
	struct new_game_dlg *self = (struct new_game_dlg *) dlg;

	struct settings *settings = ((struct otii_app *) g_app)->settings;
	struct control *ctrl = (struct control *) find_by_id(self, 9001);
	CONTROL_OPS(ctrl)->SetValue(ctrl, SettingsGetField10(settings), false);
	ctrl = (struct control *) find_by_id(self, 9002);
	CONTROL_OPS(ctrl)->SetValue(ctrl, SettingsGetField4(settings), false);
	NewGameDlgApplyOccupationSkills(self);
	ctrl = (struct control *) find_by_id(self, 9003);
	CONTROL_OPS(ctrl)->SetValue(ctrl, SettingsGetField6(settings) - 2, false);
	ctrl = (struct control *) find_by_id(self, 9004);
	CONTROL_OPS(ctrl)->SetValue(ctrl, SettingsGetFieldA(settings) - 1840, false);
	NewGameDlgUpdateDateCombos(self);

	ctrl = (struct control *) find_by_id(self, 9005);
	CONTROL_OPS(ctrl)->SetValue(ctrl, SettingsGetFieldC(settings), false);
	ctrl = (struct control *) find_by_id(self, 9006);
	CONTROL_OPS(ctrl)->SetValue(ctrl, SettingsGetFieldE(settings), false);
	NewGameDlgUpdateMemberFields(self);
	NewGameDlgUpdateWagonLabels(self);
}

void new_game_dlg_accept(struct modal_dialog *dlg, intptr_t out_arg)
{
	struct new_game_dlg *self = (struct new_game_dlg *) dlg;
	struct new_game_scenario *out = (struct new_game_scenario *) out_arg;
	struct combo_box *combo;
	struct edit *edit;
	struct cstr *item;
	struct cstr *text;

	{
		struct edit *seed_edit = (struct edit *) find_by_id(self, 9021);
		uint32_t seed = RpTime();

		if (seed_edit) {
			struct cstr *seed_text = EditGetText(seed_edit);
			const char *digits = CStrGet(seed_text);

			if (digits && *digits)
				seed = (uint32_t) strtoul(digits, NULL, 10);

			CStrDestroy(&seed_text);
		}

		RngSetSeed(seed);
	}

	combo = (struct combo_box *) find_by_id(self, 9001);
	out->player_level = combo->base.value;
	combo = (struct combo_box *) find_by_id(self, 9002);
	out->occupation = combo->base.value;
	out->cash = OccupMgrGetStartingCash(self->occmgr, out->occupation) - self->spent;
	out->score_multiplier = OccupMgrGetScoreMultX10(self->occmgr, out->occupation);
	combo = (struct combo_box *) find_by_id(self, 9003);
	out->start_month = combo->base.value + 2;

	combo = (struct combo_box *) find_by_id(self, 9004);
	item = StrRsrcGetCStr(combo->items, combo->base.value);
	out->start_year = atoi(CStrGet(item));
	CStrDestroy(&item);

	combo = (struct combo_box *) find_by_id(self, 9005);
	item = StrRsrcGetCStr(combo->items, combo->base.value);
	out->start_node = DateListBuilderLookupStartTown(self->date_lists, item);
	CStrDestroy(&item);

	combo = (struct combo_box *) find_by_id(self, 9006);
	item = StrRsrcGetCStr(combo->items, combo->base.value);
	out->destination = DateListBuilderLookupDestination(self->date_lists, item);
	CStrDestroy(&item);

	combo = (struct combo_box *) find_by_id(self, 9008);
	out->wagon_type = combo->base.value;

	if (out->names)
		StrRsrcDestroy(&out->names);

	out->names = StrRsrcCreate();

	edit = (struct edit *) find_by_id(self, 1);
	text = EditGetText(edit);
	StrRsrcAppend(out->names, CStrGet(text));
	CStrDestroy(&text);

	out->ages[0] = RandRangeAvg(15, 45, 1);

	combo = (struct combo_box *) find_by_id(self, 9007);
	int32_t member_count = combo->base.value + 2;

	for (int32_t index = 0; index < member_count; index++) {
		edit = (struct edit *) find_by_id(self, index + 30);
		text = EditGetText(edit);
		StrRsrcAppend(out->names, CStrGet(text));
		CStrDestroy(&text);
		out->ages[index + 1] = read_age((struct edit *) find_by_id(self, index + 9009));
	}

#ifdef OTII_TWITCH
	TwitchPartyTake(g_ng_twitch_used < member_count ? g_ng_twitch_used : member_count);
	g_ng_twitch_used = 0;
#endif

	int32_t skill_cash = 120;

	for (int32_t index = 0; index < 15; index++) {
		out->skills[index] = self->skill_flags.checked[index];

		if (self->skill_flags.checked[index] && !self->skill_flags.enabled[index])
			skill_cash -= OccupMgrGetSkillCost(self->occmgr, index);
	}

	out->unused_skill_points = skill_cash > 0 ? skill_cash : 0;
}

void new_game_dlg_on_event(struct evt_handler *handler, struct cmd_event *event)
{
	struct new_game_dlg *self = (struct new_game_dlg *) handler;

	int32_t control_id = (int16_t) event->spec_b.id;

	if (event->command == CMD_CONTROL_ACTION) {
		switch (control_id) {
		case 9002:
			NewGameDlgApplyOccupationSkills(self);
			break;

		case 9003:
			break;

		case 9004:
			NewGameDlgUpdateDateCombos(self);
			break;

		case 9005:
			break;

		case 9006:
			break;

		case 9008:
			NewGameDlgUpdateWagonLabels(self);
			break;

		case 9007:
			NewGameDlgUpdateMemberFields(self);
			break;

		case 9001:
			break;

		case 19: {
			struct skill_dlg *skill_dlg = SkillDlgCreate(self->occmgr);
			ModalDialogRun(&skill_dlg->base, (intptr_t) &self->skill_flags, parent_window(self));
			SkillDlgDestroy(&skill_dlg);
			NewGameDlgUpdateSkillsLabel(self);
			break;
		}

		case 12: {
			struct overview_map_dlg *map_dlg = OverviewMapDlgCreate(0);
			ModalDialogRun(&map_dlg->base, 0, parent_window(self));
			OverviewMapDlgDestroy(&map_dlg);
			break;
		}

		case 20: {
			struct click_dlg *levels_dlg = ClickDlgCreate(CStrCreateFrom("Levels Of Play"), 191);
			ModalDialogRun(&levels_dlg->base, 0, parent_window(self));
			ClickDlgDestroy(&levels_dlg);
			break;
		}

		case 21: {
			struct click_dlg *occupations_dlg = ClickDlgCreate(CStrCreateFrom("Occupations"), 192);
			ModalDialogRun(&occupations_dlg->base, 0, parent_window(self));
			ClickDlgDestroy(&occupations_dlg);
			break;
		}

		case 10:
			if (NewGameDlgValidateName(self))
				self->base.result = DLG_OK;

			break;

		case 3:
		case 9020:
			NewGameDlgRandomize(self);
			break;

		case 2: {
			struct map_dlg *trail_map_dlg = MapDlgCreate(g_text_lib, 1, self->year);
			ModalDialogRun(&trail_map_dlg->base, 0, parent_window(self));
			MapDlgDestroy(&trail_map_dlg);
			break;
		}
		}

		event->handled = true;

	} else if (event->command == CMD_EDIT_COMMITTED) {
		event->handled = true;

	} else if (event->command == CMD_CLOSE_WINDOW) {
		if (NewGameDlgValidateName(self))
			self->base.result = DLG_OK;

		event->handled = true;
	}
}

void NewGameDlgUpdateMemberFields(struct new_game_dlg *self)
{
	struct control *size_combo = (struct control *) find_by_id(self, 9007);
	int32_t member_count = size_combo->value + 2;

	for (int32_t index = 0; index < 5; index++) {
		struct view *name_edit = find_by_id(self, index + 30);

		if (index < member_count) {
			VIEW_OPS(name_edit)->Show(name_edit);

		} else {
			VIEW_OPS(name_edit)->Hide(name_edit, true, true);
		}

		struct view *age_label = find_by_id(self, index + 40);

		if (index < member_count) {
			VIEW_OPS(age_label)->Show(age_label);

		} else {
			VIEW_OPS(age_label)->Hide(age_label, true, true);
		}

		struct view *age_edit = find_by_id(self, index + 9009);

		if (index < member_count) {
			VIEW_OPS(age_edit)->Show(age_edit);

		} else {
			VIEW_OPS(age_edit)->Hide(age_edit, true, true);
		}
	}
}

void NewGameDlgApplyOccupationSkills(struct new_game_dlg *self)
{
	struct control *occupation_combo = (struct control *) find_by_id(self, 9002);
	int32_t occupation = occupation_combo->value;

	for (int32_t skill_index = 0; skill_index < 15; skill_index++) {
		if (self->skill_flags.enabled[skill_index])
			self->skill_flags.checked[skill_index] = false;
	}

	for (int32_t skill_index = 0; StrRsrcCount(self->occmgr->skill_names) > skill_index; skill_index++) {
		if (OccupMgrHasSkill(self->occmgr, occupation, skill_index)) {
			self->skill_flags.checked[skill_index] = true;
			self->skill_flags.enabled[skill_index] = true;
		}
	}

	NewGameDlgUpdateSkillsLabel(self);
	NewGameDlgUpdateCashLabel(self);
}

void NewGameDlgUpdateDateCombos(struct new_game_dlg *self)
{
	int32_t count = 0;

	struct combo_box *combo = (struct combo_box *) find_by_id(self, 9004);
	struct cstr *item = StrRsrcGetCStr(combo->items, combo->base.value);
	self->year = atoi(CStrGet(item));
	CStrDestroy(&item);

	combo = (struct combo_box *) find_by_id(self, 9005);
	item = StrRsrcGetCStr(combo->items, combo->base.value);
	struct strrsrc *list = DateListBuilderBuildStartTownList(self->date_lists, self->year, item, 0, &count);
	ComboBoxSetList(combo, list, count);
	CStrDestroy(&item);

	combo = (struct combo_box *) find_by_id(self, 9006);
	item = StrRsrcGetCStr(combo->items, combo->base.value);
	list = DateListBuilderBuildDestinationList(self->date_lists, self->year, item, 0, &count);
	ComboBoxSetList(combo, list, count);
	CStrDestroy(&item);
}

void NewGameDlgUpdateWagonLabels(struct new_game_dlg *self)
{
	char text[64];

	struct inv_list unused_inventory;
	InvListInit(&unused_inventory);

	struct combo_box *wagon_combo = (struct combo_box *) find_by_id(self, 9008);
	struct cstr *wagon_name = StrRsrcGetCStr(wagon_combo->items, wagon_combo->base.value);
	int32_t selection = wagon_combo->base.value;
	struct wagon_item *wagon = WagonItemCreate(wagon_name, 1);
	self->spent = InvItemGetUnitPrice(&wagon->base) / 100;
	int32_t capacity = WagonItemGetCapacity(wagon);
	WagonItemDestroy(&wagon);

	StrPrintf(text, "worth $%d dollars.", self->spent);
	struct clip_label_view *label = (struct clip_label_view *) find_by_id(self, 15);
	ClipLabelViewSetLabelAttr(label, CStrCreateFrom(text));
	label = (struct clip_label_view *) find_by_id(self, 16);
	ClipLabelViewSetLabelAttr(label, wagon_name);
	StrPrintf(text, "Capacity %d lbs", capacity);
	label = (struct clip_label_view *) find_by_id(self, 17);
	ClipLabelViewSetLabelAttr(label, CStrCreateFrom(text));

	struct pic_view *wagon_pic = (struct pic_view *) find_by_id(self, 18);

	if (wagon_pic->image) {
		if (wagon_pic->image->item_count <= selection) {
			wagon_pic->cur_frame = wagon_pic->image->item_count - 1;

		} else if (selection < 0) {
			wagon_pic->cur_frame = 0;

		} else {
			wagon_pic->cur_frame = selection;
		}
	}

	ViewNotifyChanged(&wagon_pic->base);
	NewGameDlgUpdateCashLabel(self);
	inv_list_destroy(&unused_inventory);
}

bool NewGameDlgValidateName(struct new_game_dlg *self)
{
	int32_t length = 0;
	struct view *edit = find_by_id(self, 1);

	if (edit) {
		struct cstr *text = EditGetText((struct edit *) edit);
		length = CStrLength(text);
		CStrDestroy(&text);
	}

	if (!length) {
		struct msg_box *box = MsgBoxCreate(CStrCreateFrom("You must enter a name to continue."), false);
		ModalDialogRun(&box->base, 0, parent_window(self));
		MsgBoxDestroy(&box);
		return false;
	}

	return true;
}

void NewGameDlgUpdateSkillsLabel(struct new_game_dlg *self)
{
	struct clip_label_view *label = (struct clip_label_view *) find_by_id(self, 13);
	int32_t skill_count = 0;
	int32_t skill_index = -1;
	struct cstr *text;

	for (int32_t index = 0; index < 15; index++) {
		if (self->skill_flags.checked[index]) {
			skill_count++;

			if (skill_count == 1)
				skill_index = index;
		}
	}

	if (skill_count == 0) {
		text = CStrCreateFrom("with no special skills,");

	} else if (skill_count != 1) {
		text = CStrCreateFrom("with various skills,");

	} else {
		text = CStrCreateFrom("with ");
		CStrAppendBuf(text, StrRsrcGetAt(self->occmgr->skill_names, skill_index));
		CStrAppendBuf(text, " skills,");
	}

	ClipLabelViewSetLabelAttr(label, text);
}

void NewGameDlgUpdateCashLabel(struct new_game_dlg *self)
{
	char buf[64];

	struct control *occupation_combo = (struct control *) find_by_id(self, 9002);
	int32_t occ = occupation_combo->value;
	StrPrintf(buf, "$%d dollars cash, and a", OccupMgrGetStartingCash(self->occmgr, occ) - self->spent);
	struct clip_label_view *label = (struct clip_label_view *) find_by_id(self, 14);
	ClipLabelViewSetLabelAttr(label, CStrCreateFrom(buf));
}

static double abs_d(double v)
{
	return v < 0 ? -v : v;
}

static double unit_roll(void)
{
	return RandRangeAvg(0, 1000, 1) / 1000.0;
}

static int32_t g_ng_year;

static double text_hardness(int32_t id, const char *text, int32_t index)
{
	static const double skill_ease[25] = {
		0.0, 0.05, 0.0, 0.07, 0.08, 0.04, 0.0, 0.08, 0.08, 0.08, 0.0, 0.0, 0.0,
		0.10, 0.0, 0.03, 0.0, 0.0, 0.12, 0.12, 0.03, 0.08, 0.12, 0.0, 0.0,
	};
	double h;

	switch (id) {
	case 9001:
		return index / 2.0;

	case 9002:
		h = index / 24.0 - (index >= 0 && index < 25 ? skill_ease[index] : 0.0);
		return h < 0.0 ? 0.0 : h;

	case 9003: {
		static const double month_h[7] = {0.7, 0.35, 0.0, 0.15, 0.5, 0.85, 1.0};
		return (index >= 0 && index < 7) ? month_h[index] : 0.5;
	}

	case 9004:
		switch (atoi(text)) {
		case 1850:
			return 1.0;
		case 1849:
			return 0.9;
		case 1852:
			return 0.85;
		case 1846:
			return 0.8;
		case 1856:
			return 0.7;
		case 1844:
			return 0.6;
		}

		return 0.25;

	case 9005:
		if (strstr(text, "St. Joseph"))
			return 0.15;

		if (strstr(text, "Independence"))
			return 0.2;

		if (strstr(text, "Nauvoo"))
			return g_ng_year == 1846 ? 1.0 : 0.3;

		return g_ng_year == 1846 ? 0.8 : 0.0;

	case 9006:
		if (strstr(text, "Salt Lake"))
			return 0.0;

		if (strstr(text, "Willamette") || strstr(text, "Oregon City"))
			return 0.4;

		if (strstr(text, "Rogue") || strstr(text, "Jacksonville"))
			return 0.7;

		return 1.0;

	case 9008: {
		static const double wagon_h[3] = {0.25, 0.0, 1.0};
		return (index >= 0 && index < 3) ? wagon_h[index] : 0.5;
	}
	}

	return 0.5;
}

static void pick_for(struct view *content, int32_t id, double t)
{
	struct combo_box *combo = (struct combo_box *) ViewFindById(content, id);
	double best = 2.0;
	double h[64];
	int32_t choices = 0;

	if (!combo || !combo->items || StrRsrcCount(combo->items) <= 0)
		return;

	int32_t count = StrRsrcCount(combo->items) > 64 ? 64 : StrRsrcCount(combo->items);
	double target = t + (unit_roll() - 0.5) * 0.3;

	for (int32_t i = 0; i < count; i++) {
		struct cstr *text = StrRsrcGetCStr(combo->items, i);
		h[i] = text_hardness(id, text ? CStrGet(text) : "", i);
		CStrDestroy(&text);

		if (abs_d(h[i] - target) < best)
			best = abs_d(h[i] - target);
	}

	for (int32_t i = 0; i < count; i++)
		if (abs_d(h[i] - target) <= best + 0.05)
			choices++;

	int32_t pick = RandRangeAvg(0, choices - 1, 1);

	for (int32_t i = 0; i < count; i++) {
		if (abs_d(h[i] - target) <= best + 0.05 && pick-- == 0) {
			CONTROL_OPS(combo)->SetValue(&combo->base, i, true);
			return;
		}
	}
}

static int32_t age_for(double t)
{
	double r = unit_roll();

	if (r < t * 0.7)
		return RandRangeAvg(0, 1, 1) ? RandRangeAvg(2, 10, 1) : RandRangeAvg(56, 70, 1);

	if (r < t)
		return RandRangeAvg(0, 1, 1) ? RandRangeAvg(12, 20, 1) : RandRangeAvg(41, 50, 1);

	return RandRangeAvg(21, 40, 1);
}

void NewGameDlgRandomize(struct new_game_dlg *self)
{
	struct view *content = self->base.content;
	struct combo_box *difficulty = (struct combo_box *) ViewFindById(content, 9020);

	if (difficulty)
		g_ng_difficulty = difficulty->base.value + 1;

	double t = (g_ng_difficulty - 1) / 9.0;

	pick_for(content, 9001, t);
	pick_for(content, 9002, t);
	NewGameDlgApplyOccupationSkills(self);
	pick_for(content, 9003, t);
	pick_for(content, 9004, t);

	{
		struct combo_box *year_combo = (struct combo_box *) ViewFindById(content, 9004);
		struct cstr *year_text = year_combo && year_combo->items && year_combo->base.value >= 0 &&
			year_combo->base.value < StrRsrcCount(year_combo->items) ?
			StrRsrcGetCStr(year_combo->items, year_combo->base.value) : NULL;
		g_ng_year = 0;

		if (year_text) {
			const char *s = CStrGet(year_text);
			g_ng_year = s ? atoi(s) : 0;
			CStrDestroy(&year_text);
		}
	}

	NewGameDlgUpdateDateCombos(self);
	pick_for(content, 9005, t);
	pick_for(content, 9006, t);
	pick_for(content, 9008, t);
	NewGameDlgUpdateWagonLabels(self);

	for (int32_t index = 0; index < 5; index++) {
		struct edit *age_edit = (struct edit *) ViewFindById(content, index + 9009);

		if (age_edit)
			EditSetText(age_edit, age_text(age_for(t)));

#if defined(OTII_TWITCH) || defined(TWITCH_PARTY)
		if (age_edit && index < g_ng_twitch_used && g_ng_twitch_age[index])
			EditSetText(age_edit, age_text(g_ng_twitch_age[index]));
#endif
	}
}
