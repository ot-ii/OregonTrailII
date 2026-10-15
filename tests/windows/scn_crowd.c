#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include <winsock2.h>
#include <windows.h>

#ifdef CROWD_CONTROL

#include "../../src/game/ailment.h"
#include "../../src/app/app.h"
#include "../../src/features/crowdcontrol.h"
#include "../../src/features/crowdnet.h"
#include "../../src/base/cstr.h"
#include "../../src/ui/framewin.h"
#include "../../src/game/game.h"
#include "../../src/game/invitem.h"
#include "../../src/game/invlist.h"
#include "../../src/base/list.h"
#include "../../src/game/member.h"
#include "../../src/game/msg.h"
#include "../../src/game/msgcontroller.h"
#include "../../src/game/msgqueue.h"
#include "../../src/app/otiiapp.h"
#include "../../src/game/party.h"
#include "../../src/third_party/mty_json.h"
#include "../../src/game/weather.h"
#include "../../src/features/memtrack.h"
#include "../testsupport.h"

static int32_t g_request;

static const char *request(const char *effect, const char *params_json, int32_t quantity, int32_t duration)
{
	static char payload[1024];
	char extra[256] = "";

	if (duration)
		snprintf(extra, sizeof(extra), ",\"duration\":%d", duration);
	snprintf(payload, sizeof(payload),
		"{\"requestID\":\"test-%d\",\"effect\":{\"effectID\":\"%s\"%s},\"quantity\":%d,\"parameters\":{%s},"
		"\"requester\":{\"name\":\"TestViewer\"}}", ++g_request, effect, extra, quantity, params_json ? params_json : "");
	return payload;
}

static const char *run(const char *effect, const char *params_json, int32_t quantity, int32_t duration, char *message,
	int32_t size)
{
	const char *status;

	for (int32_t tries = 0;; ++tries) {
		status = CrowdControlHandle(request(effect, params_json, quantity, duration), message, size);
		if (tries >= 5 || strcmp(status, "failTemporary") || !strstr(message, "window is open"))
			break;

		TestPump(400);
	}

	TestPump(300);
	return status;
}

static bool ailments(struct member *m, int32_t id)
{
	return m && MemberHasAilment(m, id);
}

static struct member *leader(void)
{
	return g_game->party->members[0];
}

static int32_t count(const char *key)
{
	return TestItemCount(key);
}

static int32_t stacked_count(const char *key)
{
	int32_t n = 0;

	for (struct list_node *node = g_game->party->inventory.base.head; node; node = node->next) {
		struct inv_item *it = InvItemFromNode(node);
		if (CStrEqualBuf(it->name, key))
			n += InvItemGetCount(it, true);
	}
	return n;
}

static int32_t alive_count(void)
{
	int32_t n = 0;

	for (int32_t i = 0; i < g_game->party->member_count; ++i)
		if (g_game->party->members[i] && g_game->party->members[i]->state == MEMBER_ALIVE)
			++n;
	return n;
}

static HWND main_hwnd(void)
{
	return g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;
}

TEST_GAME(scn_crowd, every_effect)
{
	char msg[256];
	const char *st;
	double cash;
	int32_t n;
	int32_t i;

	CrowdControlSetQuiet(true);
	CHECK(TestSave("crowd_base"));

	TCase("cholera for the second person");
	st = run("cholera", "\"member\":{\"value\":\"second\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	TestLoad("crowd_base");

	TCase("a broken arm, random person");
	st = run("broken_arm", "\"member\":{\"value\":\"random\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	TestLoad("crowd_base");

	TCase("heal");
	MemberAddAilment(leader(), AILMENT_DYSENTERY, STAGE_INCUBATION);
	leader()->health = 10;
	st = run("heal_member", "\"member\":{\"value\":\"leader\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	CHECK(!ailments(leader(), AILMENT_DYSENTERY));
	CHECK_EQ(leader()->health, leader()->max_health);
	st = run("heal_all", NULL, 1, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "success"));

	TestLoad("crowd_base");

	TCase("money");
	cash = g_game->party->cash;
	st = run("give_money", NULL, 250, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "success"));
	CHECK_NEAR(g_game->party->cash, cash + 250, 0.001);
	st = run("take_money", NULL, 100, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "success"));
	CHECK_NEAR(g_game->party->cash, cash + 150, 0.001);
	g_game->party->cash = 0;
	st = run("take_money", NULL, 10, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "failTemporary"), "%s", st);
	TestLoad("crowd_base");

	TCase("food");
	{
		static const char *const foods[] = {"salt pork", "flour", "dried beans", "coffee beans"};
		for (i = 0; i < 4; ++i) {
			char params[64];
			int32_t before = PartyGetFoodPounds(g_game->party);

			TCase("add food: %s", foods[i]);
			snprintf(params, sizeof(params), "\"food\":{\"value\":\"%s\"}", foods[i]);
			st = run("add_food", params, 20, 0, msg, sizeof(msg));
			CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
			CHECK_MSG(PartyGetFoodPounds(g_game->party) >= before + 19, "%d lb before, %d after", before,
				PartyGetFoodPounds(g_game->party));
		}
	}
	TCase("remove food");
	{
		int32_t before = PartyGetFoodPounds(g_game->party);
		st = run("remove_food", NULL, 50, 0, msg, sizeof(msg));
		CHECK(!strcmp(st, "success"));
		CHECK_MSG(PartyGetFoodPounds(g_game->party) <= before - 40, "%d lb before, %d after", before,
			PartyGetFoodPounds(g_game->party));
	}
	TestLoad("crowd_base");

	TCase("supplies");
	n = count("bullets");
	st = run("give_ammo", "\"item\":{\"value\":\"bullets\"}", 3, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "success"));
	CHECK_EQ(count("bullets"), n + 3);
	st = run("take_ammo", "\"item\":{\"value\":\"bullets\"}", 2, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "success"));
	CHECK_EQ(count("bullets"), n + 1);
	n = count("boots");
	st = run("give_clothing", "\"clothing\":{\"value\":\"boots\"}", 2, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "success"));
	CHECK_EQ(count("boots"), n + 2);
	n = count("spare wagon axle");
	st = run("give_spare_part", "\"item\":{\"value\":\"spare wagon axle\"}", 1, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "success"));
	CHECK_EQ(count("spare wagon axle"), n + 1);
	TestLoad("crowd_base");

	TCase("animals");
	{
		st = run("give_ox", NULL, 1, 0, msg, sizeof(msg));
		CHECK(!strcmp(st, "success"));
		TNote("give_ox: %s", msg);
		st = run("ox_missing", NULL, 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	}
	TestLoad("crowd_base");

	TCase("timed: forced pace");
	{
		int32_t pace = g_game->travel_pace;
		int32_t target = pace == PACE_EXHAUSTING ? PACE_8_HOURS : PACE_EXHAUSTING;
		const char *key = target == PACE_EXHAUSTING ? "grueling" : "steady";
		char params[64];

		snprintf(params, sizeof(params), "\"pace\":{\"value\":\"%s\"}", key);
		st = run("force_pace", params, 1, 30, msg, sizeof(msg));
		CHECK(!strcmp(st, "success"));
		CHECK_EQ(CrowdControlTimedCount(), 1);
		CHECK_EQ(g_game->travel_pace, target);
		g_game->travel_pace = pace;
		CrowdControlAdvanceTimed(10000);
		CHECK_EQ(g_game->travel_pace, target);
		st = run("force_pace", params, 1, 30, msg, sizeof(msg));
		CHECK(!strcmp(st, "failTemporary"));
		CrowdControlAdvanceTimed(25000);
		CHECK_EQ(CrowdControlTimedCount(), 0);
		CHECK_EQ(g_game->travel_pace, pace);
	}
	TCase("timed: forced rations");
	{
		int32_t rations = g_game->party->ration_level;
		st = run("force_rations", "\"rations\":{\"value\":\"bare_bones\"}", 1, 5, msg, sizeof(msg));
		CHECK(!strcmp(st, "success"));
		CHECK_EQ(g_game->party->ration_level, RATIONS_BARE_BONES);
		CrowdControlAdvanceTimed(6000);
		CHECK_EQ(g_game->party->ration_level, rations);
	}
	TestLoad("crowd_base");

	TCase("game events");
	{
		static const char *const events[] = {"find_wild_fruit", "find_wild_vegetables", "find_abandoned_wagon",
			"blizzard", "prairie_fire", "buffalo_stampede", "wagon_fire", "locusts", "random_good_event",
			"random_bad_event"};
		for (i = 0; i < 10; ++i) {
			TCase("trail event: %s", events[i]);
			TestLoad("crowd_base");
			st = run(events[i], NULL, 1, 0, msg, sizeof(msg));
			CHECK_MSG(!strcmp(st, "success") || !strcmp(st, "failTemporary"), "%s: %s", st, msg);
			TNote("%s: %s %s", events[i], st, msg);
			TestPump(500);
		}
		TestLoad("crowd_base");
		TCase("break part");
		st = run("break_part", "\"part\":{\"value\":\"wheel\"}", 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		TestPump(500);
		TestLoad("crowd_base");
		TCase("theft");
		st = run("theft", NULL, 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success") || !strcmp(st, "failTemporary"), "%s: %s", st, msg);
		TestPump(500);
	}
	TestLoad("crowd_base");

	TCase("rest");
	{
		int32_t days = g_game->days_traveled;
		st = run("rest", NULL, 2, 0, msg, sizeof(msg));
		CHECK(!strcmp(st, "success"));
		TNote("rest 2 days: days traveled %d -> %d", days, g_game->days_traveled);
	}
	TestLoad("crowd_base");

	TCase("kill");
	{
		int32_t alive = alive_count();
		st = run("kill", "\"member\":{\"value\":\"second\"}", 1, 0, msg, sizeof(msg));
		if (g_game->party->member_count > 1) {
			CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
			CHECK_EQ(alive_count(), alive - 1);
		}
	}
	TestLoad("crowd_base");
}

TEST_GAME(scn_crowd, more_events_and_items)
{
	char msg[256], params[96];
	const char *st, *key, *give;
	int32_t n;
	int32_t i;

	CrowdControlSetQuiet(true);
	CHECK(TestSave("crowd_base"));

	{
		for (i = 0; i < 12; ++i) {
			const char *which = i & 1 ? "random_bad_event" : "random_good_event";
			TCase("%s %d", which, i);
			TestLoad("crowd_base");
			g_game->weather->snow_depth = i < 6 ? 0 : 200;
			st = run(which, NULL, 1, 0, msg, sizeof(msg));
			CHECK_MSG(!strcmp(st, "success") || !strcmp(st, "failTemporary"), "%s: %s", st, msg);
			CHECK_MSG(!(i < 6 && strstr(msg, "Snowbound")), "%s", msg);
			TNote("%s: %s %s", which, st, msg);
			TestPump(500);
		}
	}

	TCase("random item: give one, take one");
	TestLoad("crowd_base");
	{
		st = run("give_random_item", NULL, 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		TNote("%s", msg);
		st = run("take_random_item", NULL, 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		TNote("%s", msg);
	}

	TCase("tipped wagon");
	TestLoad("crowd_base");
	st = run("tipped_wagon", NULL, 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "success") || !strcmp(st, "failTemporary"), "%s: %s", st, msg);
	TNote("tipped_wagon: %s %s", st, msg);
	TestPump(500);

	TCase("crew member missing");
	TestLoad("crowd_base");
	st = run("member_missing", "\"member\":{\"value\":\"leader\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "failPermanent"), "%s: %s", st, msg);
	if (g_game->party->member_count > 1) {
		struct member *second = g_game->party->members[1];
		st = CrowdControlHandle(request("member_missing", "\"member\":{\"value\":\"second\"}", 1, 0), msg,
			sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		CHECK_EQ(second->state, MEMBER_MISSING);
		CHECK(g_game->event_node != NULL);
		TestPump(500);
	}

	TCase("give and take every item");
	for (i = 0; (key = CrowdControlItemKey(i, &give)) != NULL; ++i) {
		char take[64];
		snprintf(take, sizeof(take), "take_%s", give + 5);
		TCase("item: %s", key);
		TestLoad("crowd_base");
		n = stacked_count(key);
		snprintf(params, sizeof(params), "\"%s\":{\"value\":\"%s\"}", !strcmp(give, "give_clothing") ? "clothing" :
			"item", key);
		st = run(give, params, 2, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		CHECK_EQ(stacked_count(key), n + 2);
		st = run(take, params, 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		CHECK_EQ(stacked_count(key), n + 1);
		st = run(take, params, 10, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		CHECK_EQ(stacked_count(key), n + 1 > 10 ? n + 1 - 10 : 0);
		if (!stacked_count(key)) {
			st = run(take, params, 1, 0, msg, sizeof(msg));
			CHECK_MSG(!strcmp(st, "failTemporary"), "%s: %s", st, msg);
		}
	}
	TCase("");
	CHECK(i >= 20);
	st = run("give_medicine", "\"item\":{\"value\":\"caviar\"}", 1, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));
	st = run("give_medicine", "\"item\":{\"value\":\"rifle\"}", 1, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));
	TestLoad("crowd_base");
}

TEST_GAME(scn_crowd, illness_effects)
{
	static const struct {
		const char *effect;
		int32_t ailment;
	} ills[] = {
		{"bad_cold", AILMENT_BAD_COLD}, {"dysentery", AILMENT_DYSENTERY}, {"measles", AILMENT_MEASLES},
		{"typhoid_fever", AILMENT_TYPHOID_FEVER}, {"cholera", AILMENT_CHOLERA}, {"smallpox", AILMENT_SMALLPOX},
		{"rabies", AILMENT_RABIES}, {"frostbite", AILMENT_FROSTBITE}, {"broken_arm", AILMENT_BROKEN_ARM},
		{"snakebite", AILMENT_SNAKEBITE}, {"broken_leg", AILMENT_BROKEN_LEG}, {"drowning", AILMENT_DROWNING},
	};
	char msg[256];
	const char *st;
	int32_t i;

	CrowdControlSetQuiet(true);
	st = run("heal_all", NULL, 1, 0, msg, sizeof(msg));
	CHECK(TestSave("crowd_base"));

	for (i = 0; i < (int32_t) (sizeof(ills) / sizeof(ills[0])); ++i) {
		TCase("%s for the second person", ills[i].effect);
		TestLoad("crowd_base");
		st = run(ills[i].effect, "\"member\":{\"value\":\"leader\"}", 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "failPermanent"), "%s: %s", st, msg);
		st = run(ills[i].effect, "\"member\":{\"value\":\"second\"}", 1, 0, msg, sizeof(msg));
		if (!strcmp(st, "failTemporary") && strstr(msg, "could not take hold")) {
			TNote("%s: %s", ills[i].effect, msg);
			continue;
		}
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		CHECK(ailments(g_game->party->members[1], ills[i].ailment) || g_game->party->members[1]->state != MEMBER_ALIVE);
		CHECK(!ailments(leader(), ills[i].ailment));
	}

	TCase("random disease and injury: the crew only");
	TestLoad("crowd_base");
	st = run("random_disease", "\"member\":{\"value\":\"leader\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "failPermanent"), "%s: %s", st, msg);
	st = run("random_disease", "\"member\":{\"value\":\"second\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	TNote("%s", msg);
	st = run("random_injury", "\"member\":{\"value\":\"random\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	TNote("%s", msg);

	TCase("the crew's effects never take the leader");
	TestLoad("crowd_base");
	{
		int32_t hit = 0;
		for (i = 0; i < 15; ++i) {
			TestLoad("crowd_base");
			run("random_injury", "\"member\":{\"value\":\"random\"}", 1, 0, msg, sizeof(msg));
			run("kill", "\"member\":{\"value\":\"random\"}", 1, 0, msg, sizeof(msg));
			hit += leader()->state != MEMBER_ALIVE;
		}
		CHECK_EQ(hit, 0);
		TestLoad("crowd_base");
		st = run("kill", "\"member\":{\"value\":\"leader\"}", 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "failPermanent"), "%s: %s", st, msg);
	}
	st = run("member_missing", "\"member\":{\"value\":\"leader\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "failPermanent"), "%s: %s", st, msg);
	st = run("rabies", "\"member\":{\"value\":\"nobody\"}", 1, 0, msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "failPermanent"), "%s: %s", st, msg);

	TCase("removed effects are refused");
	st = run("disease", "\"disease\":{\"value\":\"cholera\"}", 1, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));
	st = run("double_money", NULL, 1, 0, msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));

	TCase("an ox dies: the event's heading names the animal (not the raw \"@ Died\")");
	TestLoad("crowd_base");
	{
		struct msg *death;
		MsgControllerKillDraftAnimals(GetAuxMsgqueue(), 1, MSG_ANIMAL_DIED_ACCIDENT);
		death = MsgQueueFindByCmd(GetMessageQueue(), MSG_ANIMAL_DIED_ACCIDENT);
		CHECK(death && death->title);
		if (death && death->title) {
			TNote("heading: %s", CStrGet(death->title));
			CHECK(strstr(CStrGet(death->title), " died.") != NULL);
		}
		TestPump(500);
	}
	TestLoad("crowd_base");
}

TEST_GAME(scn_crowd, pack_variables)
{
	static const char *const vars[6] = {"leader_name", "member2_name", "member3_name", "member4_name", "member5_name",
		"member6_name"};
	char json[1024], msg[256], name[64];
	const char *st;
	MTY_JSON *v;
	const MTY_JSON *disabled;
	struct party *p = g_game->party;
	int32_t i;

	CrowdControlSetQuiet(true);
	run("heal_all", NULL, 1, 0, msg, sizeof(msg));
	CHECK(TestSave("crowd_base"));
	CHECK(CrowdControlPackVariables(json, sizeof(json)));
	TNote("%s", json);
	v = MTY_JSONParse(json);
	CHECK(v != NULL);
	disabled = MTY_JSONObjGetItem(v, "__disabledVariables");
	CHECK(MTY_JSONGetType(disabled) == MTY_JSON_ARRAY);
	for (i = 0; v && i < 6; ++i) {
		const MTY_JSON *value = MTY_JSONObjGetItem(v, vars[i]);
		bool seat = i < p->member_count && p->members[i] && p->members[i]->state == MEMBER_ALIVE;
		bool off = false;
		TCase("seat %d", i + 1);
		for (uint32_t k = 0; k < MTY_JSONArrayGetLength(disabled); ++k) {
			const char *d = MTY_JSONStringPtr(MTY_JSONArrayGetItem(disabled, k));

			if (d && !strcmp(d, vars[i]))
				off = true;
		}
		if (seat) {
			snprintf(name, sizeof(name), "%s", CStrGet(p->members[i]->name));
			CHECK(MTY_JSONStringPtr(value) && !strcmp(MTY_JSONStringPtr(value), name));
			CHECK(!off);

		} else {
			CHECK(!value);
			CHECK(off);
		}
	}
	MTY_JSONDestroy(&v);

	if (p->member_count > 1) {
		TCase("a member dies: their seat is hidden");
		st = run("kill", "\"member\":{\"value\":\"second\"}", 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		CHECK(CrowdControlPackVariables(json, sizeof(json)));
		CHECK(strstr(json, "\"member2_name\"]") || strstr(json, "\"member2_name\","));
		CHECK(!strstr(json, "\"member2_name\":"));

		TCase("a request for that seat goes to someone at random");
		st = run("bad_cold", "\"member\":{\"value\":\"second\"}", 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	}
	if (p->member_count < 6) {
		TCase("a seat nobody has: someone at random");
		TestLoad("crowd_base");
		st = run("frostbite", "\"member\":{\"value\":\"sixth\"}", 1, 0, msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
	}
	TestLoad("crowd_base");
}

TEST_GAME(scn_crowd, menu_on_the_bar)
{
	HMENU bar = main_hwnd() ? GetMenu(main_hwnd()) : NULL;
	int i, j, on_bar = 0, in_popup = 0;

	CHECK(bar != NULL);
	for (i = 0; bar && i < GetMenuItemCount(bar); ++i) {
		HMENU sub = GetSubMenu(bar, i);
		if (!sub && GetMenuItemID(bar, i) == CROWD_CONTROL_MENU_ID)
			++on_bar;
		for (j = 0; sub && j < GetMenuItemCount(sub); ++j)
			if (GetMenuItemID(sub, j) == CROWD_CONTROL_MENU_ID)
				++in_popup;
	}
	CHECK_EQ(on_bar, 1);
	CHECK_EQ(in_popup, 0);
}

static void save_window_shot(HWND hwnd, const char *name)
{
	char path[MAX_PATH], *dot;
	RECT r;
	HDC screen, dc;
	HBITMAP bmp;
	BITMAPINFOHEADER bi;
	BITMAPFILEHEADER bf;
	void *bits;
	FILE *f;
	int w, h;

	GetWindowRect(hwnd, &r);
	w = r.right - r.left;
	h = r.bottom - r.top;
	screen = GetDC(NULL);
	dc = CreateCompatibleDC(screen);
	memset(&bi, 0, sizeof(bi));
	bi.biSize = sizeof(bi);
	bi.biWidth = w;
	bi.biHeight = h;
	bi.biPlanes = 1;
	bi.biBitCount = 32;
	bmp = CreateDIBSection(screen, (BITMAPINFO *) &bi, DIB_RGB_COLORS, &bits, NULL, 0);
	SelectObject(dc, bmp);
	PrintWindow(hwnd, dc, 2);
	GetModuleFileNameA(NULL, path, sizeof(path));
	dot = strrchr(path, '.');
	if (dot)
		*dot = 0;
	strncat_s(path, sizeof(path), "_", _TRUNCATE);
	strncat_s(path, sizeof(path), name, _TRUNCATE);
	strncat_s(path, sizeof(path), ".bmp", _TRUNCATE);
	if (bmp && fopen_s(&f, path, "wb") == 0 && f) {
		memset(&bf, 0, sizeof(bf));
		bf.bfType = 0x4D42;
		bf.bfOffBits = sizeof(bf) + sizeof(bi);
		bf.bfSize = bf.bfOffBits + 4 * (DWORD) w * (DWORD) h;
		fwrite(&bf, sizeof(bf), 1, f);
		fwrite(&bi, sizeof(bi), 1, f);
		fwrite(bits, 4, (size_t) w * (size_t) h, f);
		fclose(f);
		TNote("window shot: %s", path);
	}
	DeleteObject(bmp);
	DeleteDC(dc);
	ReleaseDC(NULL, screen);
}

TEST_GAME(scn_crowd, window_opens)
{
	bool fonts = false, logo = false;
	int32_t buttons = 0;
	HWND w;

	CrowdControlSetQuiet(true);
	if (CrowdControlWindowInfo(&fonts, &logo, &buttons))
		CrowdControlToggleWindow();
	CrowdControlToggleWindow();
	w = (HWND) CrowdControlWindowInfo(&fonts, &logo, &buttons);
	CHECK(w != NULL);
	CHECK(fonts);
	CHECK(logo);
	CHECK(buttons >= 2);
	if (w) {
		char msg[256];
		CrowdControlHandle(request("give_money", NULL, 5, 0), msg, sizeof(msg));
		TestPump(1500);
		save_window_shot(w, "crowd_window");
	}
	CrowdControlToggleWindow();
	CHECK(CrowdControlWindowInfo(&fonts, &logo, &buttons) == NULL);
}

TEST_GAME(scn_crowd, window_closes_through_the_menu)
{
	bool fonts = false, logo = false;
	int32_t buttons = 0;
	HWND w;
	MSG msg;
	bool posted;

	CrowdControlSetQuiet(true);
	if (!CrowdControlWindowInfo(&fonts, &logo, &buttons))
		CrowdControlToggleWindow();
	w = (HWND) CrowdControlWindowInfo(&fonts, &logo, &buttons);
	CHECK(w != NULL);
	if (!w)
		return;

	SendMessageA(w, WM_CLOSE, 0, 0);
	CHECK_MSG(IsWindow(w), "WM_CLOSE destroyed the window itself");
	posted = PeekMessageA(&msg, main_hwnd(), WM_COMMAND, WM_COMMAND, PM_NOREMOVE) &&
		LOWORD(msg.wParam) == CROWD_CONTROL_MENU_ID;
	CHECK_MSG(posted, "no Crowd Control menu command in the game window's queue");
	TestPump(500);
	CHECK(CrowdControlWindowInfo(&fonts, &logo, &buttons) == NULL);
	CHECK(!IsWindow(w));
	if (IsWindow(w))
		DestroyWindow(w);
}

TEST_GAME(scn_crowd, retry_and_refuse)
{
	char msg[256];
	const char *st;
	double cash;

	CrowdControlSetQuiet(true);
	CHECK(TestSave("crowd_base"));
	cash = g_game->party->cash;

	TCase("an event on screen");
	MsgQueuePost(GetMessageQueue(), MSG_THUNDERSTORM, NULL, 0, 0, NULL, -1);
	CHECK(g_game->event_node != NULL);
	st = CrowdControlHandle(request("give_money", NULL, 100, 0), msg, sizeof(msg));
	CHECK_MSG(!strcmp(st, "failTemporary"), "%s", st);
	CHECK_NEAR(g_game->party->cash, cash, 0.001);
	TestLoad("crowd_base");

	TCase("the game in the background: effects still arrive");
	{
		HWND other = CreateWindowExA(0, "STATIC", "another program", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 100,
			NULL, NULL, GetModuleHandleA(NULL), NULL);
		SetForegroundWindow(other);
		SetActiveWindow(other);
		TestPump(200);
		st = CrowdControlHandle(request("give_money", NULL, 3, 0), msg, sizeof(msg));
		CHECK_MSG(!strcmp(st, "success"), "%s: %s", st, msg);
		CHECK_NEAR(g_game->party->cash, cash + 3, 0.001);
		DestroyWindow(other);
	}
	TestLoad("crowd_base");

	TCase("a game dialog open (the main window disabled): not now");
	EnableWindow(main_hwnd(), FALSE);
	st = CrowdControlHandle(request("give_money", NULL, 100, 0), msg, sizeof(msg));
	EnableWindow(main_hwnd(), TRUE);
	CHECK_MSG(!strcmp(st, "failTemporary"), "%s", st);
	CHECK_NEAR(g_game->party->cash, cash, 0.001);

	TCase("hunting");
	{
		struct otii_app *app = (struct otii_app *) g_app;
		int32_t was = app->app_state;
		app->app_state = APP_STATE_HUNTING;
		st = CrowdControlHandle(request("give_money", NULL, 100, 0), msg, sizeof(msg));
		app->app_state = was;
		CHECK(!strcmp(st, "failTemporary"));
		CHECK_NEAR(g_game->party->cash, cash, 0.001);
	}

	TCase("no such effect / option");
	st = CrowdControlHandle(request("make_it_rain_gold", NULL, 1, 0), msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));
	st = CrowdControlHandle(request("disease", "\"disease\":{\"value\":\"space_flu\"}", 1, 0), msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));
	st = CrowdControlHandle(request("add_food", "\"food\":{\"value\":\"caviar\"}", 10, 0), msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));
	st = CrowdControlHandle("{not json", msg, sizeof(msg));
	CHECK(!strcmp(st, "failPermanent"));
	TestLoad("crowd_base");
}

TEST_GAME(scn_crowd, through_the_network)
{
	char state[256];
	double cash;
	int32_t i, s = 0;
	DWORD until;

	state[0] = 0;
	CrowdControlSetQuiet(true);
	CHECK(TestSave("crowd_base"));
	CHECK(CrowdControlConnect("ws://127.0.0.1:8766/", "http://127.0.0.1:8766/", "http://127.0.0.1:8766/auth",
		"test-pack"));
	for (until = GetTickCount() + 15000; GetTickCount() < until; ) {
		TestPump(200);
		s = CrowdControlState(state, sizeof(state));
		if (s == CROWD_SESSION)
			break;
	}
	TNote("state %d: %s", s, state);
	CHECK_MSG(s == CROWD_SESSION, "no game session with the test server (is tools/cc_testserver.py --test --port 8766 "
		"running?): %s", state);
	if (s == CROWD_SESSION) {
		cash = g_game->party->cash;
		CrowdNetSendRaw(CrowdControlNet(), "{\"action\":\"test-trigger\",\"data\":{\"effect\":\"give_money\","
			"\"quantity\":7,\"viewer\":\"NetViewer\"}}");
		for (i = 0; i < 40 && g_game->party->cash < cash + 6.5; ++i)
			TestPump(250);
		CHECK_NEAR(g_game->party->cash, cash + 7, 0.001);
		{
			int32_t alive = 0;
			for (i = 0; i < g_game->party->member_count; ++i)
				if (g_game->party->members[i] && g_game->party->members[i]->state == MEMBER_ALIVE)
					++alive;
			CrowdNetSendRaw(CrowdControlNet(), "{\"action\":\"test-metadata-echo\"}");
			TestPump(500);
			cash = g_game->party->cash;
			CrowdControlResendVariables();
			for (i = 0; i < 40 && g_game->party->cash < cash + alive - 0.5; ++i)
				TestPump(250);
			CHECK_NEAR(g_game->party->cash, cash + alive, 0.001);
		}
		{
			bool fonts, logo;
			int32_t buttons = 0;
			HWND w;
			if (!CrowdControlWindowInfo(&fonts, &logo, &buttons))
				CrowdControlToggleWindow();
			TestPump(1500);
			w = (HWND) CrowdControlWindowInfo(&fonts, &logo, &buttons);
			CHECK_EQ(buttons, 3);
			if (w)
				save_window_shot(w, "crowd_window_session");
			CrowdControlToggleWindow();
		}
	}
	CrowdControlDisconnect();
	TestLoad("crowd_base");
}

static void live_memory(int32_t *blocks, int64_t *bytes)
{
	*blocks = 0;
	*bytes = 0;
#ifdef DIAG_BUILD
	MemTrackLive(blocks, bytes);
#endif
}

static DWORD connect_and_close(const char *server, const char *api, int32_t want, DWORD wait_ms, int32_t *blocks,
	int64_t *bytes)
{
	char state[256];
	int32_t b0, b1;
	int64_t y0, y1;
	DWORD until, start, took;
	int32_t s = 0;

	live_memory(&b0, &y0);
	CHECK(CrowdControlConnect(server, api, "http://127.0.0.1:8766/auth", "test-pack"));
	for (until = GetTickCount() + wait_ms; GetTickCount() < until; ) {
		Sleep(100);
		s = CrowdControlState(state, sizeof(state));
		if (want >= 0 && s == want)
			break;
	}
	TNote("state %d: %s", s, state);
	if (want >= 0)
		CHECK_MSG(s == want, "state %d, not %d: %s", s, want, state);
	if (s == CROWD_SESSION) {
		CrowdNetSendRaw(CrowdControlNet(), "{\"action\":\"test-trigger\",\"data\":{\"effect\":\"give_money\","
			"\"quantity\":1,\"viewer\":\"QueuedViewer\"}}");
		Sleep(500);
	}
	start = GetTickCount();
	CrowdControlDisconnect();
	took = GetTickCount() - start;
	live_memory(&b1, &y1);
	*blocks = b1 - b0;
	*bytes = y1 - y0;
	TNote("disconnect took %lu ms, left %d blocks, %lld bytes", took, *blocks, *bytes);

	return took;
}

TEST_GAME(scn_crowd, shutdown_is_prompt)
{
	WSADATA wsa;
	SOCKET stall;
	struct sockaddr_in a;
	int len = sizeof(a);
	char server[64], api[64];
	int32_t blocks;
	int64_t bytes;
	DWORD took;

	CrowdControlSetQuiet(true);
	if (CrowdControlNet())
		CrowdControlDisconnect();

	TCase("a server that never answers");
	CHECK(WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
	stall = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	memset(&a, 0, sizeof(a));
	a.sin_family = AF_INET;
	a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	CHECK(stall != INVALID_SOCKET && bind(stall, (struct sockaddr *) &a, sizeof(a)) == 0 && listen(stall, 4) == 0 &&
		getsockname(stall, (struct sockaddr *) &a, &len) == 0);
	snprintf(server, sizeof(server), "ws://127.0.0.1:%d/", ntohs(a.sin_port));
	snprintf(api, sizeof(api), "http://127.0.0.1:%d/", ntohs(a.sin_port));
	took = connect_and_close(server, api, -1, 1500, &blocks, &bytes);
	CHECK_MSG(took < 4000, "the disconnect took %lu ms", took);
	CHECK_MSG(blocks == 0 && bytes == 0, "%d blocks, %lld bytes left", blocks, bytes);
	closesocket(stall);
	WSACleanup();

	TCase("the test server, a session running");
	took = connect_and_close("ws://127.0.0.1:8766/", "http://127.0.0.1:8766/", CROWD_SESSION, 15000, &blocks, &bytes);
	CHECK_MSG(took < 2000, "the disconnect took %lu ms", took);
	CHECK_MSG(blocks == 0 && bytes == 0, "%d blocks, %lld bytes left", blocks, bytes);
}

const struct test_reg TESTS_scn_crowd[] = {
	TEST_GAME_ENTRY(scn_crowd, every_effect),
	TEST_GAME_ENTRY(scn_crowd, more_events_and_items),
	TEST_GAME_ENTRY(scn_crowd, illness_effects),
	TEST_GAME_ENTRY(scn_crowd, pack_variables),
	TEST_GAME_ENTRY(scn_crowd, menu_on_the_bar),
	TEST_GAME_ENTRY(scn_crowd, window_opens),
	TEST_GAME_ENTRY(scn_crowd, window_closes_through_the_menu),
	TEST_GAME_ENTRY(scn_crowd, retry_and_refuse),
	TEST_GAME_ENTRY(scn_crowd, through_the_network),
	TEST_GAME_ENTRY(scn_crowd, shutdown_is_prompt),
	TEST_END
};

#else

const struct test_reg TESTS_scn_crowd[] = {
	TEST_END
};

#endif

#endif
