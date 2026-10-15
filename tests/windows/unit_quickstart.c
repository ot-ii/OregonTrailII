#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "../../src/app/app.h"
#include "../../src/data/assets.h"
#include "../../src/draw/canvas.h"
#include "../../src/ui/clipview.h"
#include "../../src/base/cstr.h"
#include "../../src/data/dataversion.h"
#include "../../src/base/date.h"
#include "../../src/data/docfile.h"
#include "../../src/app/docmanager.h"
#include "../../src/app/event.h"
#include "../../src/app/evthandler.h"
#include "../../src/ui/framewin.h"
#include "../../src/game/game.h"
#include "../../src/data/gfxdev.h"
#include "../../src/data/image.h"
#include "../../src/game/member.h"
#include "../../src/base/object.h"
#include "../../src/app/otiiapp.h"
#include "../../src/game/party.h"
#include "../../src/dialogs/quickstartdlg.h"
#include "../../src/data/rsrc.h"
#include "../../src/data/strrsrc.h"
#include "../../src/draw/text.h"
#include "../../src/screens/titleanim.h"
#include "../../src/ui/view.h"
#include "../testsupport.h"

static bool has(struct assets *lib, const char *type, int32_t id)
{
	struct rsrc *r = AssetsGetElement(lib, type, id, false);

	if (!r)
		return false;
	RsrcDestroy(&r);
	return true;
}

static void story_text(struct quick_start_dlg *dlg, char *out, int32_t size)
{
	struct view *view = ViewFindById(dlg->base.content, 1);
	struct strrsrc *lines = view ? ((struct text *) ((struct clip_view *) view)->shape)->content : NULL;
	int32_t used = 0;

	out[0] = 0;
	for (int32_t i = 0; lines && i < StrRsrcCount(lines) && used < size - 1; ++i) {
		char line[512];
		StrRsrcCopyTo(lines, i, line, sizeof(line));
		used += _snprintf_s(out + used, (size_t) (size - used), _TRUNCATE, "%s ", line) > 0 ?
			(int32_t) strlen(out + used) : 0;
	}
}

static void run_game(int32_t ms)
{
	DWORD until = GetTickCount() + (DWORD) ms;
	MSG msg;

	do {
		if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);

		} else {
			RunIdleProcessing(3);
		}
	} while ((LONG) (GetTickCount() - until) < 0);
}

static uint32_t title_sum(void)
{
	uint32_t sum = 0;

	if (!g_title_canvas)
		return 0;
	for (int32_t y = 0; y < g_title_canvas->bits.rect.bottom - g_title_canvas->bits.rect.top; y += 1)
		for (int32_t x = 0; x < g_title_canvas->bits.rect.right - g_title_canvas->bits.rect.left; x += 1)
			sum = sum * 31 + CanvasGetPixel(g_title_canvas, x, y);
	return sum;
}

static void media_done(void)
{
	struct cmd_event *ev = CmdEventCreate(CMD_MEDIA_DONE);
	EVT_HANDLER_OPS(g_app)->OnEvent((struct evt_handler *) g_app, ev);
	CmdEventDestroy(&ev);
}

TEST(unit_quickstart, detects_the_data)
{
	bool h = has(g_data_lib, "CBMP", 612) && has(g_data_lib, "CBMP", 22201) && has(g_data_lib, "CBMP", 22202) &&
		has(g_data_lib, "CBMP", 22203) && has(g_text_lib, "STRLST", 2208) && has(g_text_lib, "STRLST", 2214);

	TNote("the data is the %s data", DataIs13() ? "1.3" : "1.2");
	CHECK_EQ(DataIs13(), h);
	if (!DataIs13())
		CHECK(!has(g_data_lib, "CBMP", 612) && !has(g_data_lib, "CBMP", 22201));
}

TEST_GAME(unit_quickstart, random_party_in_range)
{
	int32_t sizes[7] = {0};

	if (!DataIs13()) {
		TNote("1.2 data: skipped");
		return;
	}
	for (int32_t run = 0; run < 200; ++run) {
		struct party *party = PartyCreateQuickStart(CMD_NEW_GAME_OPTIONS);
		TCase("party %d", run);
		CHECK_RANGE(party->member_count, 4, 6);
		if (party->member_count >= 4 && party->member_count <= 6)
			++sizes[party->member_count];
		for (int32_t i = 0; i < 6; ++i) {
			struct member *m = party->members[i];
			if (i >= party->member_count) {
				CHECK(m == NULL);
				continue;
			}
			CHECK(m != NULL);
			if (!m)
				continue;
			CHECK(m->name && CStrLength(m->name) > 0);
			if (i < 2)
				CHECK_RANGE(m->age, 15, 20);
			else if (i < 4)
				CHECK_RANGE(m->age, 4, 17);
			else
				CHECK_RANGE(m->age, 18, 24);
		}
		for (int32_t i = 1; i < party->member_count; ++i)
			for (int32_t j = 1; j < i; ++j)
				CHECK_MSG(!CStrEqual(party->members[i]->name, party->members[j]->name), "two members named alike");
		CHECK_RANGE(party->occupation, 0, 8);
		CHECK(party->wagons.base.head != NULL);
		CHECK(party->cash > 0);
		PartyDestroy(&party);
	}
	TNote("parties of 4/5/6: %d/%d/%d", sizes[4], sizes[5], sizes[6]);
	CHECK(sizes[4] && sizes[5] && sizes[6]);
}

TEST_GAME(unit_quickstart, roll_fills_the_story)
{
	struct quick_start_choices choices;
	struct quick_start_dlg *dlg;
	static char text[4096], before[4096];
	int32_t changed = 0;

	if (!DataIs13()) {
		TNote("1.2 data: skipped");
		return;
	}
	memset(&choices, 0, sizeof(choices));
	dlg = QuickStartDlgCreate();
	MODAL_DIALOG_OPS(dlg)->Begin(&dlg->base, (intptr_t) &choices);
	story_text(dlg, before, sizeof(before));
	TNote("story: %.300s", before);
	for (int32_t run = 0; run < 100; ++run) {
		TCase("roll %d", run);
		if (run)
			QuickStartDlgRoll(dlg);
		story_text(dlg, text, sizeof(text));
		CHECK(strlen(text) > 100);
		CHECK_MSG(!strchr(text, '['), "a tag is left: %.200s", strchr(text, '[') ? strchr(text, '[') : "");
		CHECK_RANGE(choices.year, 1840, 1860);
		CHECK_RANGE(choices.month, 3, 5);
		CHECK_MSG(choices.oxen == 6 || choices.oxen == 8, "%d oxen", choices.oxen);
		if (choices.year < 1844)
			CHECK_RANGE(choices.wagons, 5, 15);
		else
			CHECK_RANGE(choices.wagons, 5, 50);
		CHECK_RANGE(choices.people, 5 * choices.wagons, 5 * choices.wagons + 9);
		CHECK(choices.start_node == 1 || choices.start_node == 267 || choices.start_node == 289 ||
			choices.start_node == 312 || choices.start_node == 313 || choices.start_node == 315);
		CHECK_RANGE(choices.destination, 0, 6);
		CHECK(choices.party != NULL);
		if (choices.party) {
			CHECK(choices.party->animals.base.head != NULL);
			CHECK(choices.party->inventory.base.head != NULL);
		}
		if (run && strcmp(text, before))
			++changed;
		strcpy_s(before, sizeof(before), text);
	}
	CHECK_MSG(changed >= 95, "only %d of 99 rolls told another story", changed);
	PartyDestroy(&choices.party);
	QuickStartDlgDestroy(&dlg);
}

TEST_GAME(unit_quickstart, animation_frames_draw)
{
	static const int32_t ids[3] = {22201, 22202, 22203};

	if (!DataIs13()) {
		TNote("1.2 data: skipped");
		return;
	}
	for (int32_t l = 0; l < 3; ++l) {
		struct rsrc *rsrc = AssetsGetElement(g_data_lib, "CBMP", ids[l], false);
		struct cbmp *image = rsrc ? CbmpCreate(rsrc) : NULL;
		struct rect rect = RectMake(0, 0, 480, 640);
		struct canvas canvas;
		int32_t frames = image ? CbmpFrameCount(image) : 0, with_change = 0;
		int32_t size_h = 0, size_v = 0;

		CanvasInitRect(&canvas, &rect, 8, 255);
		TCase("CBMP %d", ids[l]);
		CHECK(frames > 2);
		if (image)
			CbmpFrameSize(image, 0, &size_h, &size_v);
		for (int32_t f = 1; f < frames; ++f) {
			struct rect dirty;
			int32_t status = CanvasDrawFrameAt(&canvas, image, f, &dirty, 0, 0);
			CHECK_EQ(status, 0);
			if (dirty.bottom > dirty.top && dirty.right > dirty.left) {
				++with_change;
				CHECK(dirty.top >= 0 && dirty.left >= 0 && dirty.right <= size_h + 8 && dirty.bottom <= size_v + 8);
			}
		}
		{
			int32_t painted = 0;
			for (int32_t y = 0; y < size_v; y += 2)
				for (int32_t x = 0; x < size_h; x += 2)
					painted += CanvasGetPixel(&canvas, x, y) != 255;
			TNote("CBMP %d: %d frames of %dx%d, %d change something, %d of the sampled pixels drawn", ids[l], frames,
				size_h, size_v, with_change, painted);
			CHECK(painted > 0);
		}
		CHECK(with_change * 4 >= frames - 1);
		canvas_destroy((struct object *) &canvas);
		CbmpDestroy(&image);
		RsrcDestroy(&rsrc);
	}
}

TEST_GAME(unit_quickstart, title_screen_animates)
{
	struct view *root;
	uint32_t sum0, sum1, sum2;
	int32_t drawn;

	media_done();
	TestPump(300);
	root = g_parent_win->base.root_view;
	CHECK(ViewFindById(root, 97) != NULL);
	CHECK(ViewFindById(root, 96) != NULL);
	if (!DataIs13()) {
		CHECK(ViewFindById(root, 95) == NULL);
		CHECK(!TitleAnimRunning());
		TestScreenshot("qs_title_12");
		return;
	}
	CHECK(ViewFindById(root, 95) != NULL);
	CHECK(TitleAnimRunning());
	sum0 = title_sum();
	TestScreenshot("qs_title_a");
	run_game(1500);
	sum1 = title_sum();
	drawn = TitleAnimFramesDrawn();
	TestScreenshot("qs_title_b");
	TNote("%d animation frames changed the picture in 1.5 s", drawn);
	CHECK(drawn > 5);
	run_game(6000);
	sum2 = title_sum();
	TNote("%d frames in 7.5 s; the picture %s after 1.5 s, %s after 7.5 s", TitleAnimFramesDrawn(),
		sum1 != sum0 ? "changed" : "had not changed", sum2 != sum0 ? "changed" : "had not changed");
	CHECK_MSG(sum0 != sum2, "the picture did not change");
	TestScreenshot("qs_title_c");
}

static void quick_start(const char *button)
{
	struct otii_app *app = (struct otii_app *) g_app;
	struct doc_file *doc;

	media_done();
	TestPump(300);
	doc = DocManagerGetSingleDoc(APP_OPS(g_app)->GetDocManager(g_app));
	if (doc)
		DocFileSetDirtyV(doc, false);
	TestScreensClear();
	TestPrefer(button);
	TestPump(10);
	OtiiAppHandleDocCommand(app, CMD_NEW_GAME_OPTIONS, g_parent_win->base.hwnd);
	TestPrefer(NULL);
}

TEST_GAME(unit_quickstart, quick_start_starts_the_game)
{
	struct otii_app *app = (struct otii_app *) g_app;

	if (!DataIs13()) {
		TNote("1.2 data: skipped");
		return;
	}
	quick_start("Hit the Trail!");
	TNote("screens: %s", TestScreensSeen());
	CHECK_MSG(strstr(TestScreensSeen(), "Prologue") != NULL, "no Quick Start dialog");
	CHECK(app->new_game_choices == NULL);
	CHECK(!TitleAnimRunning());
	CHECK(g_game != NULL);
	if (!g_game)
		return;
	TestPump(2000);
	TestScreenshot("qs_started");
	TNote("started at node %d, %d/%d, to node %d (destination %d), %d wagons, %d people", g_game->cur_node_id,
		g_game->date->month, g_game->date->year, g_game->next_node_id, g_game->destination, g_game->train_wagons,
		g_game->train_people);
	CHECK(g_game->cur_node_id == 1 || g_game->cur_node_id == 267 || g_game->cur_node_id == 289 ||
		g_game->cur_node_id == 312 || g_game->cur_node_id == 313 || g_game->cur_node_id == 315);
	CHECK_RANGE(g_game->date->month, 3, 5);
	CHECK_RANGE(g_game->date->year, 1840, 1860);
	CHECK(g_game->next_node_id == 265 || g_game->next_node_id == 266 || g_game->next_node_id == 506 ||
		g_game->next_node_id == 507 || g_game->next_node_id == 415 || g_game->next_node_id == 619 ||
		g_game->next_node_id == 620);
	CHECK_RANGE(g_game->train_wagons, 5, 50);
	CHECK(g_game->party && g_game->party->member_count >= 4);
}

TEST_GAME(unit_quickstart, cancel_returns_to_the_title)
{
	struct otii_app *app = (struct otii_app *) g_app;

	if (!DataIs13()) {
		TNote("1.2 data: skipped");
		return;
	}
	quick_start("Cancel");
	CHECK_MSG(strstr(TestScreensSeen(), "Prologue") != NULL, "no Quick Start dialog");
	CHECK(app->new_game_choices == NULL);
	CHECK(TitleAnimRunning());
	CHECK(ViewFindById(g_parent_win->base.root_view, 95) != NULL);
}

const struct test_reg TESTS_unit_quickstart[] = {
	TEST_ENTRY(unit_quickstart, detects_the_data),
	TEST_GAME_ENTRY(unit_quickstart, random_party_in_range),
	TEST_GAME_ENTRY(unit_quickstart, roll_fills_the_story),
	TEST_GAME_ENTRY(unit_quickstart, animation_frames_draw),
	TEST_GAME_ENTRY(unit_quickstart, title_screen_animates),
	TEST_GAME_ENTRY(unit_quickstart, quick_start_starts_the_game),
	TEST_GAME_ENTRY(unit_quickstart, cancel_returns_to_the_title),
	TEST_END
};

#endif
