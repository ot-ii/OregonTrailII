#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include <windows.h>

#ifdef TWITCH_PARTY

#include "../../src/ui/combobox.h"
#include "../../src/base/cstr.h"
#include "../../src/ui/edit.h"
#include "../../src/dialogs/modaldialog.h"
#include "../../src/dialogs/newgamedlg.h"
#include "../../src/features/twitchirc.h"
#include "../../src/features/twitchparty.h"
#include "../../src/ui/view.h"
#include "../testsupport.h"

static const char REWARD[] = "test-party-reward";

static bool wait_for(bool (*done)(void), int32_t ms)
{
	DWORD until = GetTickCount() + (DWORD) ms;

	while (!done() && (LONG) (GetTickCount() - until) < 0)
		TestPump(100);
	return done();
}

static bool connected(void)
{
	char s[256];

	TwitchStatus(s, sizeof(s));
	return !strncmp(s, "connected", 9);
}

static int32_t g_expect;

static bool have_expected(void)
{
	return TwitchPartyPending() >= g_expect;
}

static void ask(const char *login, const char *display, const char *text)
{
	char line[300];

	snprintf(line, sizeof(line), "TESTREDEEM %s %s %s :%s", login, display, REWARD, text);
	CHECK_MSG(TwitchSendRaw(line), "sending %s", line);
}

static void edit_text(struct new_game_dlg *dlg, int32_t id, char *out, size_t size)
{
	struct edit *e = (struct edit *) ViewFindById(dlg->base.content, id);
	struct cstr *t = e ? EditGetText(e) : NULL;

	out[0] = 0;
	if (t) {
		snprintf(out, size, "%s", CStrGet(t));
		CStrDestroy(&t);
	}
}

static HWND window(void)
{
	return (HWND) TwitchPartyWindow();
}

static int list_count(void)
{
	return (int) SendMessageW(GetDlgItem(window(), TWITCH_PARTY_LIST), LB_GETCOUNT, 0, 0);
}

static void list_line(int i, char *out, int size)
{
	wchar_t w[400];

	w[0] = 0;
	SendMessageW(GetDlgItem(window(), TWITCH_PARTY_LIST), LB_GETTEXT, (WPARAM) i, (LPARAM) w);
	WideCharToMultiByte(CP_UTF8, 0, w, -1, out, size, NULL, NULL);
}

static void pick_lines(const char *const *parts, int n)
{
	HWND list = GetDlgItem(window(), TWITCH_PARTY_LIST);
	int i, k, count = list_count();

	SendMessageW(list, LB_SETSEL, FALSE, -1);
	for (k = 0; k < n; ++k) {
		for (i = 0; i < count; ++i) {
			char line[400];
			list_line(i, line, sizeof(line));
			if (strstr(line, parts[k]) && SendMessageW(list, LB_GETSEL, (WPARAM) i, 0) <= 0)
				break;
		}
		CHECK_MSG(i < count, "no line with \"%s\"", parts[k]);
		if (i < count)
			SendMessageW(list, LB_SETSEL, TRUE, i);
	}
	SendMessageW(window(), WM_COMMAND, MAKEWPARAM(TWITCH_PARTY_PICK, BN_CLICKED), 0);
	TestPump(250 * (n + 2));
}

TEST_GAME(scn_twitch, without_the_reward_nothing_starts)
{
	TwitchTestReset();
	CHECK(!TwitchPartyConnect("testchan", "", "irc://127.0.0.1:6668"));
	CHECK(TwitchPartyWindow() == NULL);
	CHECK(!TwitchPartyConnect("", REWARD, "irc://127.0.0.1:6668"));
	CHECK(TwitchPartyWindow() == NULL);
	CHECK(!TwitchPartyConnect("testchan", REWARD, "irc://example.com:6668"));
	CHECK(TwitchPartyWindow() == NULL);
	TwitchPartyDisconnect();
}

TEST_GAME(scn_twitch, pick_and_remove)
{
	struct modal_dialog *was = g_running_dialog;
	struct new_game_dlg *dlg;
	char text[400], rolled_age[16];
	int i;

	TwitchTestReset();
	CHECK(TwitchPartyConnect("testchan", REWARD, "irc://127.0.0.1:6668"));
	CHECK(TwitchPartyWindow() != NULL);
	if (!TwitchPartyWindow())
		return;
	CHECK_MSG(wait_for(connected, 15000), "no connection to the test server (is tools/twitch_testserver.py --test "
		"--port 6668 running?)");
	if (!connected()) {
		TwitchPartyDisconnect();
		return;
	}
	ask("ann", "Ann", "25 Hey chat, for no particular reason");
	ask("bob", "Bob", "31");
	ask("bob", "Bob", "hello");
	ask("cid", "Cid", "7");
	ask("dee", "Dee", "44");
	ask("bob", "Bob", "50");
	g_expect = 6;
	CHECK(wait_for(have_expected, 10000));
	TestPump(600);

	TCase("the window lists them, nobody picked yet");
	CHECK_EQ(list_count(), 6);
	for (i = 0; i < list_count(); ++i) {
		list_line(i, text, sizeof(text));
		if (strstr(text, "Bob"))
			CHECK_MSG(strstr(text, "Bob [0]") != NULL, "line %d: %s", i, text);
		if (strstr(text, "Ann"))
			CHECK_MSG(!strcmp(text, "Ann [0] 25 Hey chat, for no particular reason"), "line %d: %s", i, text);
	}

	TCase("Pick on the New Game screen");
	dlg = NewGameDlgCreate();
	g_running_dialog = &dlg->base;
	edit_text(dlg, 9009 + 2, rolled_age, sizeof(rolled_age));
	{
		static const char *const FIRST[] = {"Ann", "Bob [0] 31", "Bob [0] hello", "Cid"};
		pick_lines(FIRST, 4);
	}
	CHECK_EQ(NewGamePickedSlots(), 4);
	edit_text(dlg, 30, text, sizeof(text));
	CHECK(!strcmp(text, "Ann"));
	edit_text(dlg, 31, text, sizeof(text));
	CHECK(!strcmp(text, "Bob"));
	edit_text(dlg, 32, text, sizeof(text));
	CHECK(!strcmp(text, "Bob"));
	edit_text(dlg, 33, text, sizeof(text));
	CHECK(!strcmp(text, "Cid"));
	edit_text(dlg, 9009, text, sizeof(text));
	CHECK(!strcmp(text, "25"));
	edit_text(dlg, 9010, text, sizeof(text));
	CHECK(!strcmp(text, "31"));
	edit_text(dlg, 9011, text, sizeof(text));
	CHECK_MSG(!strcmp(text, rolled_age), "\"hello\" is no age: the rolled %s stays (shows %s)", rolled_age, text);
	edit_text(dlg, 9012, text, sizeof(text));
	CHECK(!strcmp(text, "7"));
	{
		struct combo_box *size = (struct combo_box *) ViewFindById(dlg->base.content, 9007);
		CHECK(size && size->base.value + 2 >= 4);
	}
	TCase("the counts after the picks: Bob picked twice, Ann once");
	for (i = 0; i < list_count(); ++i) {
		list_line(i, text, sizeof(text));
		if (strstr(text, "Bob"))
			CHECK_MSG(strstr(text, "Bob [2]") != NULL, "line %d: %s", i, text);
		if (strstr(text, "Ann"))
			CHECK_MSG(strstr(text, "Ann [1]") != NULL, "line %d: %s", i, text);
		if (strstr(text, "Dee"))
			CHECK_MSG(strstr(text, "Dee [0]") != NULL, "line %d: %s", i, text);
	}
	for (i = 0; i < list_count(); ++i) {
		list_line(i, text, sizeof(text));
		if (strstr(text, "Ann") || strstr(text, "Cid") || strstr(text, "31") || strstr(text, "hello"))
			CHECK_MSG(!strncmp(text, "\xE2\x9C\x93 ", 4), "line %d is not marked picked: %s", i, text);
		else
			CHECK_MSG(strncmp(text, "\xE2\x9C\x93 ", 4) != 0, "line %d is marked picked: %s", i, text);
	}

	TCase("Random keeps the picked");
	NewGameDlgRandomize(dlg);
	edit_text(dlg, 30, text, sizeof(text));
	CHECK(!strcmp(text, "Ann"));
	edit_text(dlg, 9009, text, sizeof(text));
	CHECK(!strcmp(text, "25"));
	edit_text(dlg, 9010, text, sizeof(text));
	CHECK(!strcmp(text, "31"));
	edit_text(dlg, 9012, text, sizeof(text));
	CHECK(!strcmp(text, "7"));

	TCase("the fifth fills the party, a sixth finds it full");
	{
		static const char *const DEE[] = {"Dee"};
		static const char *const BOB50[] = {"Bob [2] 50"};
		pick_lines(DEE, 1);
		CHECK_EQ(NewGamePickedSlots(), 5);
		{
			struct combo_box *size = (struct combo_box *) ViewFindById(dlg->base.content, 9007);
			CHECK(size && size->base.value + 2 == 5);
		}
		pick_lines(BOB50, 1);
		CHECK_EQ(NewGamePickedSlots(), 5);
		GetWindowTextA(GetDlgItem(window(), TWITCH_PARTY_STATUS), text, sizeof(text));
		CHECK_MSG(strstr(text, "full") != NULL, "status: %s", text);
	}

	TCase("Remove played");
	SendMessageW(window(), WM_COMMAND, MAKEWPARAM(TWITCH_PARTY_REMOVE, BN_CLICKED), 0);
	TestPump(300);
	CHECK_EQ(TwitchPartyPending(), 1);
	CHECK_EQ(list_count(), 1);
	list_line(0, text, sizeof(text));
	CHECK_MSG(!strcmp(text, "Bob [2] 50"), "left: %s", text);

	TCase("closed, then Help > Twitch Redemptions shows it again");
	SendMessageW(window(), WM_CLOSE, 0, 0);
	CHECK(window() != NULL && !IsWindowVisible(window()));
	{
		HMENU menu = CreatePopupMenu();
		AppendMenuA(menu, MF_STRING, 6099, "Debug State...");
		TwitchPartyMenuUpdate(menu);
		CHECK(GetMenuState(menu, TWITCH_PARTY_MENU_ID, MF_BYCOMMAND) != (UINT) -1);
		CHECK_EQ((int) GetMenuItemID(menu, 0), TWITCH_PARTY_MENU_ID);
		TwitchPartyMenuUpdate(menu);
		CHECK_EQ(GetMenuItemCount(menu), 2);
		DestroyMenu(menu);
	}
	TwitchPartyShowWindow();
	CHECK(IsWindowVisible(window()));
	CHECK_EQ(list_count(), 1);

	TCase("");
	g_running_dialog = was;
	NewGameDlgDestroy(&dlg);
	TwitchPartyDisconnect();
	TwitchTestReset();
}

const struct test_reg TESTS_scn_twitch[] = {
	TEST_GAME_ENTRY(scn_twitch, without_the_reward_nothing_starts),
	TEST_GAME_ENTRY(scn_twitch, pick_and_remove),
	TEST_END
};

#else

const struct test_reg TESTS_scn_twitch[] = {
	TEST_END
};

#endif

#endif
