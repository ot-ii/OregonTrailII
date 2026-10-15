#include "hiscoredlg.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "../ui/cliplabelview.h"
#include "../base/cstr.h"
#include "../app/event.h"
#include "../base/ini.h"
#include "../base/memory.h"
#include "../features/onlinescores.h"
#include "../ui/pictctrl.h"
#include "../ui/picview.h"
#include "../base/str.h"
#include "../ui/view.h"

struct hi_score_rec {
	char name[32];
	int32_t score;
};

static struct hi_score_rec g_hiscore_names[10] = {
	{"Elisha Stephens", 15835},
	{"Stephen Meek", 13492},
	{"Celinda Hines", 11673},
	{"George Washington Bush", 10340},
	{"Thomas Knight", 8771},
	{"Ezra Meeker", 6239},
	{"Jesse Applegate", 5507},
	{"Mary Bartlett", 5198},
	{"William Wiggins", 4615},
	{"Eliza Hart Spalding", 2734},
};

#ifdef ONLINE_SCORES
static int32_t g_online_count;
static char g_online_names[10][32];
static int32_t g_online_scores[10];
#endif

static void hiscore_load(struct hi_score_rec *table);
static void hiscore_save(const struct hi_score_rec *table);

const struct hi_score_dlg_ops HI_SCORE_DLG_OPS_TABLE = {
	{
		{
			{hi_score_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			hi_score_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		hi_score_dlg_begin,
		hi_score_dlg_accept,
	},
};

void HiScoreDlgInit(struct hi_score_dlg *self)
{
	ModalDialogInit(&self->base, -2, false, true);
	self->base.base.base.ops = &HI_SCORE_DLG_OPS_TABLE.base.base.base;

	struct rect bounds;
	struct point origin;
	struct view *child;
	int32_t handled_cmds[2];
	const struct event *ev = &self->base.base.eh_event;

#ifdef ONLINE_SCORES
	g_online_count = OnlineScoresTop10(g_online_names, g_online_scores);
	self->base.title = CStrCreateFrom(g_online_count ? "List of Legends (online)" : "List of Legends");
#else
	self->base.title = CStrCreateFrom("List of Legends");
#endif

	origin.h = 0;
	origin.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &origin, 22032);
	struct view *content = self->base.content;

	for (int32_t row = 0; row < 10; row++) {
		bounds.top = 20 * row + 70;
		bounds.left = 170;
		bounds.right = 395;
		bounds.bottom = 20 * row + 90;
		child = (struct view *) ClipLabelViewCreate(row + 3, &bounds, NULL, 3, 0, 1, 0, 0, 255, 0, 1);
		VIEW_OPS(content)->AddChild(content, child);

		bounds.top = 20 * row + 70;
		bounds.left = 395;
		bounds.right = 445;
		bounds.bottom = 20 * row + 90;
		child = (struct view *) ClipLabelViewCreate(row + 20, &bounds, NULL, 3, 2, 1, 0, 0, 255, 0, 1);
		VIEW_OPS(content)->AddChild(content, child);
	}

	origin.h = 140;
	origin.v = 300;
	child = (struct view *) PictCtrlCreate(1, ev, &origin, 2102, 0, 0, CStrCreateFrom("OK"), 0, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	origin.h = 380;
	origin.v = 300;
	child = (struct view *) PictCtrlCreate(2, ev, &origin, 2102, 0, 0, CStrCreateFrom("Reset"), 0, 0, 0);
	VIEW_OPS(content)->AddChild(content, child);

	handled_cmds[0] = CMD_CONTROL_ACTION;
	handled_cmds[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handled_cmds);
}

struct hi_score_dlg *HiScoreDlgCreate(void)
{
	struct hi_score_dlg *self = MemAlloc(1, sizeof(struct hi_score_dlg));
	HiScoreDlgInit(self);

	return self;
}

void HiScoreDlgDestroy(struct hi_score_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void hi_score_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

static void fill_line(char *line, const char *name)
{
	StrCopyN(line, 256, name);

	for (int32_t fill_pos = StrLen(line); fill_pos < 254; fill_pos++) {
		line[fill_pos] = '.';
		fill_pos++;
		line[fill_pos] = ' ';
	}

	line[254] = 0;
}

void hi_score_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) param;
	struct hi_score_rec table[10];
	char line[256];

	hiscore_load(table);
#ifdef ONLINE_SCORES
	if (g_online_count) {
		for (int32_t index = 0; index < 10; index++) {
			StrCopy(table[index].name, index < g_online_count ? g_online_names[index] : "");
			table[index].score = index < g_online_count ? g_online_scores[index] : 0;
		}
	}
#endif

	for (int32_t index = 0; index < 10; index++) {
		struct clip_label_view *label = (struct clip_label_view *) ViewFindById(self->content, index + 3);
#ifdef ONLINE_SCORES
		if (g_online_count && index >= g_online_count) {
			ClipLabelViewSetLabelAttr(label, CStrCreate());
			label = (struct clip_label_view *) ViewFindById(self->content, index + 20);
			ClipLabelViewSetLabelAttr(label, CStrCreate());
			continue;
		}
#endif
		fill_line(line, table[index].name);
		ClipLabelViewSetLabelAttr(label, CStrCreateFrom(line));
		label = (struct clip_label_view *) ViewFindById(self->content, index + 20);
		ClipLabelViewSetLabelAttr(label, CStrCreateInt(table[index].score, true));
	}
}

void hi_score_dlg_accept(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void hi_score_dlg_on_event(struct evt_handler *self, struct cmd_event *evt)
{
	struct modal_dialog *dlg = (struct modal_dialog *) self;
	char line[256];
	int32_t control_id = evt->spec_b.id;

	if (evt->command == CMD_CONTROL_ACTION) {
		switch (control_id) {
			case 1:
				dlg->result = DLG_OK;
				break;
			case 2:
				hiscore_save(g_hiscore_names);
#ifdef ONLINE_SCORES
				if (g_online_count)
					break;
#endif

				for (int32_t index = 0; index < 10; index++) {
					struct clip_label_view *label = (struct clip_label_view *) ViewFindById(dlg->content,
						index + 3);
					fill_line(line, g_hiscore_names[index].name);
					ClipLabelViewSetLabelAttr(label, CStrCreateFrom(line));
					label = (struct clip_label_view *) ViewFindById(dlg->content, index + 20);
					ClipLabelViewSetLabelAttr(label, CStrCreateInt(g_hiscore_names[index].score, true));
				}
				break;
		}

		evt->handled = true;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW)
			dlg->result = DLG_OK;
	}
}

static void hiscore_load(struct hi_score_rec *table)
{
	char profile_value[64];
	char key_name[32];
	char default_value[64];

	for (int32_t index = 0; index < 10; index++) {
		StrPrintf(default_value, "%s;%d", g_hiscore_names[index].name, g_hiscore_names[index].score);
		StrPrintf(key_name, "hiscore%d", index + 1);
		IniGetString("scores", key_name, default_value, profile_value, sizeof(profile_value));
		char *separator = strchr(profile_value, ';');

		if (separator) {
			*separator = 0;
			separator++;
		}

		StrCopy(table[index].name, profile_value);
		table[index].score = separator ? atoi(separator) : 0;
	}
}

static void hiscore_save(const struct hi_score_rec *table)
{
	char key_name[32];
	char value[64];

	for (int32_t index = 0; index < 10; index++) {
		StrPrintf(value, "%s;%d", table[index].name, table[index].score);
		StrPrintf(key_name, "hiscore%d", index + 1);
		IniSetString("scores", key_name, value);
	}
}

void HiscoreInsert(struct cstr *name, int32_t score)
{
	struct hi_score_rec table[10];
	bool inserted = false;

	hiscore_load(table);

	for (int32_t rank = 0; rank < 10; rank++) {
		if (table[rank].score < score) {
			int32_t shift_rank;

			for (shift_rank = 9; shift_rank > rank; shift_rank--) {
				StrCopy(table[shift_rank].name, table[shift_rank - 1].name);
				table[shift_rank].score = table[shift_rank - 1].score;
			}

			StrCopy(table[shift_rank].name, CStrGet(name));
			table[shift_rank].score = score;
			inserted = true;
			break;
		}
	}

	if (inserted)
		hiscore_save(table);
}
