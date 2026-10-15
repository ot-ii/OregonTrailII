#include "../autopilot.h"

#ifdef OTII_AUTO_MODE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../base/cstr.h"
#include "../../base/date.h"
#include "../../game/endgame.h"
#include "../../game/game.h"
#include "../../game/gamecmd.h"
#include "../../data/gfxdev.h"
#include "../../data/assets.h"
#include "../../game/member.h"
#include "../../game/party.h"
#include "../../app/replay.h"
#include "../../base/rng.h"
#include "../../data/rsrc.h"
#include "../../base/str.h"
#include "../../data/strrsrc.h"

static int32_t g_active;
static int32_t g_difficulty = 5;
static uint32_t g_seed;
static int32_t g_minutes = 180;
static uint32_t g_started;
static UINT_PTR g_watch;
static int32_t g_had_game;
static void *g_new_dlg;
static int32_t g_new_step;
static char g_state[512];

static void append(const char *line)
{
	char path[MAX_PATH];
	char *dot;
	FILE *f;

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return;

	dot = strrchr(path, '.');
	if (dot)
		*dot = 0;
	StrCat(path, "_batch.txt");
	if ((f = fopen(path, "a")) != NULL) {
		fputs(line, f);
		fputc('\n', f);
		fclose(f);
	}
}

static int32_t alive(void)
{
	int32_t i, n = 0;

	for (i = 0; i < g_game->party->member_count && i < 6; ++i)
		if (g_game->party->members[i] && g_game->party->members[i]->state == MEMBER_ALIVE)
			++n;

	return n;
}

static void snapshot(void)
{
	struct party *p = g_game->party;

	StrPrintfN(g_state, sizeof(g_state) - 1,
		"dest=%d level=%d occupation=%d date=%d-%02d-%02d days=%d alive=%d/%d morale=%d cash=%.2f food=%d node=%d",
		g_game->destination, g_game->player_level, p->occupation, g_game->date->year, g_game->date->month,
		g_game->date->day, g_game->days_traveled, alive(), p->member_count, p->reputation, p->cash,
		PartyGetFoodPounds(p), g_game->cur_node_id);
}

static void finish(const char *outcome, int32_t score)
{
	char line[768];

	StrPrintfN(line, sizeof(line) - 1, "difficulty=%d seed=%u outcome=%s score=%d minutes=%lu %s", g_difficulty,
		g_seed, outcome, score, (unsigned long) ((RpClockNow() - g_started) / 60000), g_state);
	append(line);
	ExitProcess(0);
}

struct ailment_text {
	char name[48];
	char died[96];
};

static struct ailment_text g_ailments[100];
static int32_t g_ailment_count = -1;

static void load_ailment_texts(void)
{
	int32_t id;

	g_ailment_count = 0;
	for (id = 0; id < 100; ++id) {
		struct rsrc *probe = g_text_lib ? AssetsGetElement(g_text_lib, "STRLST", (int16_t) (15000 + id), false) : NULL;
		struct strrsrc *s;
		struct ailment_text *a = &g_ailments[g_ailment_count];
		if (!probe)
			continue;

		RsrcDestroy(&probe);
		s = StrRsrcLoad(g_text_lib, 15000 + id, true);
		memset(a, 0, sizeof(*a));
		if (StrRsrcCount(s) > 0) {
			StrRsrcCopyTo(s, 0, a->name, sizeof(a->name));
			if (StrRsrcCount(s) > 3) {
				char line[128];
				StrRsrcCopyTo(s, 3, line, sizeof(line));
				StrCopy(a->died, strncmp(line, "@1 ", 3) ? line : line + 3);
			}
			if (a->name[0])
				++g_ailment_count;
		}
		StrRsrcDestroy(&s);
	}
}

static void record(const char *kind, const char *what, const char *who, const char *text)
{
	char path[MAX_PATH], line[512];
	char *dot;
	FILE *f;

	if (!GetModuleFileNameA(NULL, path, MAX_PATH))
		return;

	dot = strrchr(path, '.');
	if (dot)
		*dot = 0;
	StrCat(path, "_diseases.txt");
	StrPrintfN(line, sizeof(line) - 1, "%u\t%d-%02d-%02d\t%s\t%s\t%s\t%.200s", g_seed, g_game->date->year,
		g_game->date->month, g_game->date->day, kind, what, who, text);
	if ((f = fopen(path, "a")) != NULL) {
		fputs(line, f);
		fputc('\n', f);
		fclose(f);
	}
}

static void who_in(const char *text, char *who, int32_t size)
{
	int32_t i;

	who[0] = 0;
	for (i = 0; i < g_game->party->member_count && i < 6; ++i) {
		struct member *m = g_game->party->members[i];
		const char *name;
		if (!m || !m->name)
			continue;

		name = CStrGet(m->name);
		if (name && name[0] && strstr(text, name))
			StrCopyN(who, (size_t) size, name);
		if (who[0])
			return;
	}
}

static void track(const struct control_state *st)
{
	static char last[4096];
	char key[4096], who[64];
	int32_t i, k, len = 0;

	if (g_ailment_count < 0)
		load_ailment_texts();

	key[0] = 0;
	for (i = 0; i < st->side_text_first && len < 3900; ++i)
		len += StrPrintfN(key + len, sizeof(key) - (size_t) len, "%d|%.120s|", st->screen, st->text[i]);

	if (!strcmp(key, last))
		return;

	StrCopy(last, key);
	for (i = 0; i < st->side_text_first; ++i) {
		const char *t = st->text[i];
		if (strstr(t, "has died") || strstr(t, " died.")) {
			const char *cause = "cause not shown";
			for (k = 0; k < g_ailment_count; ++k)
				if (g_ailments[k].died[0] && strstr(g_ailments[k].died, " of ") && strstr(t, g_ailments[k].died)) {
					cause = g_ailments[k].name;
					break;
				}
			who_in(t, who, sizeof(who));
			record("died", cause, who, t);
		}

		if (st->screen == CONTROL_EVENT)
			for (k = 0; k < g_ailment_count; ++k)
				if (!_stricmp(t, g_ailments[k].name)) {
					char all[1024];
					int32_t j, n = 0;
					all[0] = 0;
					for (j = 0; j < st->side_text_first && n < 900; ++j)
						n += StrPrintfN(all + n, sizeof(all) - (size_t) n, "%.200s ", st->text[j]);
					who_in(all, who, sizeof(who));
					record("shown", g_ailments[k].name, who, all);
				}
	}
}

static void unstick(struct control_state *st)
{
	static int32_t last_key = -1;
	static uint32_t since;
	uint32_t key;
	int32_t i, other = 0;

	for (i = 0; i < st->side_option_first; ++i)
		if (strcmp(st->options[i].label, "Map") && strcmp(st->options[i].label, "Journal") &&
			strcmp(st->options[i].label, "Party health") && strcmp(st->options[i].label, "Supplies"))
			other = 1;

	key = ((uint32_t) g_game->date->year * 400u + (uint32_t) g_game->date->month * 32u + (uint32_t) g_game->date->day) *
		4096u + (uint32_t) g_game->total_miles * 16u + (uint32_t) g_game->miles_to_go;
	key = key * 2u + (uint32_t) other;
	if ((int32_t) key != last_key) {
		last_key = (int32_t) key;
		since = RpClockNow();
		return;
	}

	if (RpClockNow() - since < (uint32_t) (other ? 20000 : 40000))
		return;

	since = RpClockNow();
	if (!other) {
		ControlClickAt(st, 300, 200);
		return;
	}

	for (i = 0; i < st->side_option_first; ++i)
		if (!strcmp(st->options[i].label, "Back to travel") || !strcmp(st->options[i].label, "OK") ||
			!strcmp(st->options[i].label, "Continue")) {
			ControlClick(st, st->options[i].index);
			return;
		}
}

static void CALLBACK watch(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;

	static struct control_state st;
	int32_t i;

	if (g_game && g_game->party && g_game->date) {
		g_had_game = 1;
		snapshot();
		if (g_game->end_score && g_game->end_score->final_total)
			finish("arrived", g_game->end_score->final_total);
		if (g_game->party->member_count > 0 && !alive())
			finish("all_died", 0);
		if (ControlGetState(&st)) {
			for (i = 0; i < st.side_option_first; ++i)
				if (st.options[i].command == GCMD_CHOICE && st.options[i].code == NODE_CMD_EVENT_END_GAME)
					finish("game_over_event", 0);
			track(&st);
			unstick(&st);
		}

	} else if (g_had_game) {
		finish("closed", 0);

	} else if (!AutoPilotRunning() && RpClockNow() - g_started > 8000 && ControlGetState(&st)) {
		for (i = 0; i < st.option_count; ++i)
			if (!strcmp(st.options[i].label, "New Game")) {
				AutoPilotStart();
				break;
			}
	}

	if (RpClockNow() - g_started > (uint32_t) g_minutes * 60000u)
		finish("timeout", 0);
}

void AutoBatchStart(void)
{
	const char *cmd = GetCommandLineA(), *p = cmd ? strstr(cmd, "/autobatch=") : NULL;
	char *end;

	if (!p)
		return;

	p += 11;
	g_difficulty = (int32_t) strtol(p, &end, 10);
	p = end;
	if (*p == ',')
		++p;
	g_seed = (uint32_t) strtoul(p, &end, 10);
	p = end;
	if (*p == ',')
		++p;
	g_minutes = (int32_t) strtol(p, &end, 10);
	if (g_difficulty < 1 || g_difficulty > 10)
		g_difficulty = 5;
	if (g_minutes <= 0)
		g_minutes = 180;

	g_active = 1;
	g_started = RpClockNow();
	g_watch = RpSetTimer(NULL, 0, 100, (void *) watch);
}

int32_t AutoBatchActive(void)
{
	return g_active;
}

uint32_t AutoBatchTickMs(uint32_t normal)
{
	return g_active ? 250 : normal;
}

int32_t AutoBatchNewGame(struct control_state *st)
{
	int32_t i;

	if (!g_active)
		return 0;

	if (st->hwnd != g_new_dlg) {
		g_new_dlg = st->hwnd;
		g_new_step = 0;
	}

	for (i = 0; i < st->field_count; ++i) {
		char text[16];
		if (g_new_step == 0 && st->fields[i].is_choice && st->fields[i].id == 9020) {
			RngSetSeed(g_seed);
			StrPrintf(text, "#%d", g_difficulty - 1);
			ControlSetField(st, i, text);
			g_new_step = 1;
			return 1;
		}

		if (g_new_step == 1 && !st->fields[i].is_choice && st->fields[i].id == 9021) {
			StrPrintf(text, "%u", g_seed);
			ControlSetField(st, i, text);
			g_new_step = 2;
			return 1;
		}
	}

	return 0;
}

#endif
