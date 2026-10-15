#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include <windows.h>

#ifdef TWITCH_PARTY

#include "../../src/dialogs/newgamedlg.h"
#include "../../src/features/twitchirc.h"
#include "../../src/features/twitchparty.h"

static const char REWARD[] = "0b7c3f0e-1111-2222-3333-444455556666";

static void redeem(const char *login, const char *display, const char *id, const char *reward, const char *text)
{
	char line[600];

	snprintf(line, sizeof(line), "@badge-info=;display-name=%s;%s%s%scustom-reward-id=%s;user-id=7 "
		":%s!%s@%s.tmi.twitch.tv PRIVMSG #testchan :%s", display, id ? "id=" : "", id ? id : "", id ? ";" : "", reward,
		login, login, login, text);
	TwitchTestReceive(line, REWARD);
}

TEST(unit_twitch, redemptions)
{
	struct twitch_message m;

	TwitchTestReset();
	redeem("ann", "Ann", "id-1", REWARD, "25");
	redeem("bob", "Bob", "id-2", "some-other-reward", "hi");
	TwitchTestReceive("@display-name=Cid;id=id-3 :cid!cid@cid.tmi.twitch.tv PRIVMSG #testchan :just chatting", REWARD);
	TCase("only the party reward joins the list");
	CHECK_EQ(TwitchPartyPending(), 1);
	CHECK(TwitchPartyPeek(0, &m));
	CHECK(!strcmp(m.login, "ann"));
	CHECK(!strcmp(m.display_name, "Ann"));
	CHECK(!strcmp(m.text, "25"));
	CHECK(!strcmp(m.reward_id, REWARD));
	CHECK(!strcmp(m.id, "id-1"));

	TCase("no id tag: a key of its own");
	redeem("dee", "Dee", NULL, REWARD, "30");
	redeem("dee", "Dee", NULL, REWARD, "31");
	{
		struct twitch_message a, b;
		CHECK(TwitchPartyPeek(1, &a) && TwitchPartyPeek(2, &b));
		CHECK(a.id[0] && b.id[0] && strcmp(a.id, b.id));
	}
	TCase("");
	TwitchTestReset();
}

TEST(unit_twitch, play_counts_and_remove_played)
{
	struct twitch_message m;
	int32_t i, bobs = 0;

	TwitchTestReset();
	redeem("bob", "Bob", "b-1", REWARD, "31");
	redeem("ann", "Ann", "a-1", REWARD, "25");
	redeem("bob", "Bob", "b-2", REWARD, "hello");
	redeem("bob", "Bob", "b-3", REWARD, "50");
	TCase("redeemed, not picked yet");
	CHECK_EQ(TwitchPartyPlayCount("bob"), 0);
	CHECK_EQ(TwitchPartyPlayCount("ann"), 0);
	CHECK_EQ(TwitchPartyPlayCount("nobody"), 0);

	TCase("two of bob's three picked, then Remove played");
	CHECK(TwitchPartyMarkPicked("b-1"));
	CHECK_EQ(TwitchPartyPlayCount("bob"), 1);
	CHECK(TwitchPartyMarkPicked("b-2"));
	CHECK_EQ(TwitchPartyPlayCount("bob"), 2);
	CHECK_EQ(TwitchPartyPlayCount("BOB"), 2);
	CHECK(TwitchPartyMarkPicked("b-2"));
	CHECK_EQ(TwitchPartyPlayCount("bob"), 2);
	CHECK_EQ(TwitchPartyPlayCount("ann"), 0);
	CHECK(!TwitchPartyMarkPicked("no-such-id"));
	CHECK(TwitchPartyPicked(0));
	CHECK(!TwitchPartyPicked(1));
	CHECK_EQ(TwitchPartyRemovePicked(), 2);
	CHECK_EQ(TwitchPartyPending(), 2);
	for (i = 0; TwitchPartyPeek(i, &m); ++i) {
		CHECK(!TwitchPartyPicked(i));
		if (!strcmp(m.login, "bob")) {
			++bobs;
			CHECK(!strcmp(m.id, "b-3"));
		}
	}
	CHECK_EQ(bobs, 1);
	TCase("the count stays after Remove played, and grows with the next pick");
	CHECK_EQ(TwitchPartyPlayCount("bob"), 2);
	CHECK(TwitchPartyMarkPicked("b-3"));
	CHECK_EQ(TwitchPartyPlayCount("bob"), 3);
	TCase("");
	TwitchTestReset();
}

TEST(unit_twitch, age_text)
{
	static const struct {
		const char *text;
		int32_t age;
	} CASES[] = {
		{"25", 25}, {" 7 ", 7}, {"99", 99}, {"1", 1}, {"0", 0}, {"100", 0}, {"abc", 0}, {"25 years", 25}, {"", 0},
		{"-5", 0}, {"07", 7}, {"34 Hey chat, for no particular reason", 34}, {"34, hi", 34}, {"34!", 34},
		{"34abc", 0}, {"1234", 0}, {"100 years", 0}, {"Hey chat 34", 0}, {"age 34", 0},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(CASES) / sizeof(CASES[0])); ++i) {
		TCase("\"%s\"", CASES[i].text);
		CHECK_EQ(NewGameTestAge(CASES[i].text), CASES[i].age);
	}
	TCase("");
}

TEST(unit_twitch, settings)
{
	TCase("server");
	CHECK(TwitchSetServer(""));
	CHECK(TwitchSetServer(NULL));
	CHECK(TwitchSetServer("irc://127.0.0.1:6668"));
	CHECK(TwitchSetServer("irc://localhost"));
	CHECK(TwitchSetServer("irc://[::1]:6667"));
	CHECK(!TwitchSetServer("irc://example.com:6667"));
	CHECK(!TwitchSetServer("irc://192.168.1.5:6667"));
	CHECK(TwitchSetServer("irc.example.com:6697"));
	CHECK(TwitchSetServer("ircs://irc.example.com"));
	CHECK(!TwitchSetServer("irc.example.com:abc"));
	CHECK(!TwitchSetServer("irc.example.com:99999"));
	CHECK(!TwitchSetServer("bad host:6697"));
	CHECK(TwitchSetServer(""));
	TCase("both a channel and the reward's id");
	CHECK(!TwitchPartySettings("", "", ""));
	CHECK(!TwitchPartySettings("testchan", "", ""));
	CHECK(!TwitchPartySettings("", REWARD, ""));
	CHECK(TwitchPartySettings("testchan", REWARD, ""));
	CHECK(TwitchPartySettings("testchan", REWARD, "irc://127.0.0.1:6667"));
	TCase("not on: no Help > Twitch Redemptions, and nothing to show");
	{
		HMENU menu = CreatePopupMenu();
		AppendMenuA(menu, MF_STRING, 6099, "Debug State...");
		TwitchPartyMenuUpdate(menu);
		CHECK(GetMenuState(menu, TWITCH_PARTY_MENU_ID, MF_BYCOMMAND) == (UINT) -1);
		AppendMenuA(menu, MF_STRING, TWITCH_PARTY_MENU_ID, "Twitch Redemptions");
		TwitchPartyMenuUpdate(menu);
		CHECK(GetMenuState(menu, TWITCH_PARTY_MENU_ID, MF_BYCOMMAND) == (UINT) -1);
		DestroyMenu(menu);
		TwitchPartyShowWindow();
		CHECK(TwitchPartyWindow() == NULL);
	}
	TCase("");
}

const struct test_reg TESTS_unit_twitch[] = {
	TEST_ENTRY(unit_twitch, redemptions),
	TEST_ENTRY(unit_twitch, play_counts_and_remove_played),
	TEST_ENTRY(unit_twitch, age_text),
	TEST_ENTRY(unit_twitch, settings),
	TEST_END
};

#else

const struct test_reg TESTS_unit_twitch[] = {
	TEST_END
};

#endif

#endif
