#include "../testframework.h"

#ifdef OTII_TEST

#include <string.h>

#include <windows.h>

#include "../../src/features/autopilot.h"
#include "../../src/features/controlstate.h"
#include "../../src/game/game.h"
#include "../../src/base/rng.h"

static UINT_PTR g_timer;
static DWORD g_started;
static bool g_started_pilot;

static void CALLBACK wait(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	static struct control_state st;

	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;
	if (GetTickCount() - g_started > 600000u) {
		KillTimer(NULL, g_timer);
		TCheck(false, "a game reached the trail", __FILE__, __LINE__, "timed out");
		TestRunGameSuites();
		return;
	}
	if (!ControlGetState(&st))
		return;
	if (!g_started_pilot) {
		for (int32_t i = 0; i < st.option_count; ++i)
			if (!strcmp(st.options[i].label, "New Game")) {
				RngSetSeed(TSeed());
				AutoPilotStart();
				g_started_pilot = true;
				break;
			}
		return;
	}
	if (g_game && g_game->party && g_game->date && g_game->days_traveled > 0 && st.screen == CONTROL_LANDMARK) {
		KillTimer(NULL, g_timer);
		AutoPilotStop();
		TestRunGameSuites();
	}
}

void TestGameStart(void)
{
	g_started = GetTickCount();
	g_timer = SetTimer(NULL, 0, 200, wait);
}

#endif
