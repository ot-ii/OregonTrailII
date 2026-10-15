#include "../autosave.h"

#include <inttypes.h>
#include <stddef.h>

#include <windows.h>

#include "../app.h"
#include "../../base/cstr.h"
#include "../docmanager.h"
#include "../../base/file.h"
#include "../../ui/framewin.h"
#include "../../game/game.h"
#include "../../game/gamedoc.h"
#include "../../base/ini.h"
#include "../otiiapp.h"
#include "../../base/path.h"
#include "../replay.h"
#include "../../base/rng.h"
#include "../../base/str.h"
#include "../../ui/window.h"

static uintptr_t g_autosave_timer;
static int32_t g_autosave_enabled = -1;
static bool g_autosave_busy;
static int32_t g_saved_node = -1;
static int32_t g_saved_day = -1;
static char g_autosave_status[160] = "no autosave yet";

static void autosave_file(char *out, size_t size, const char *name)
{
	FileGetExeDir(out, size);
	StrCatN(out, size, name);
}

bool AutosaveEnabled(void)
{
	if (g_autosave_enabled < 0)
		g_autosave_enabled = IniGetInt("options", "autosave", 1) ? 1 : 0;
	return g_autosave_enabled != 0;
}

void AutosaveSetEnabled(bool enabled)
{
	g_autosave_enabled = enabled ? 1 : 0;
	IniSetString("options", "autosave", g_autosave_enabled ? "1" : "0");
	if (g_autosave_enabled)
		AutosaveSkipCurrent();
}

void AutosaveSkipCurrent(void)
{
	struct game *game = GetGameObj();

	if (game) {
		g_saved_node = game->cur_node_id;
		g_saved_day = game->days_traveled;
	}
}

const char *AutosaveStatus(void)
{
	return g_autosave_status;
}

static bool autosave_allowed(void)
{
	struct otii_app *app = (struct otii_app *) g_app;
	struct game *game = GetGameObj();

	if (!app || !game || !game->party || !game->cur_node)
		return false;
	if (app->app_state != APP_STATE_AT_NODE)
		return false;
	if (GetActiveWindowObj() != (struct window *) g_parent_win)
		return false;
	if (!app->base.base.docmgr || !DocManagerGetSingleDoc(app->base.base.docmgr))
		return false;
	return true;
}

static void autosave_now(void)
{
	struct game *game = GetGameObj();

	if (RpMode() == RP_REPLAY)
		return;
	char temp_path[MAX_PATH], save_path[MAX_PATH], prev_path[MAX_PATH];
	struct cstr *path_text;
	int32_t status;
	SYSTEMTIME now;
	char name[64];

	StrPrintf(name, "autosave_%" PRIu32 ".tmp", RngSeed());
	autosave_file(temp_path, sizeof(temp_path), name);
	StrPrintf(name, "autosave_%" PRIu32 ".ot2", RngSeed());
	autosave_file(save_path, sizeof(save_path), name);
	StrPrintf(name, "autosave_%" PRIu32 "-previous.ot2", RngSeed());
	autosave_file(prev_path, sizeof(prev_path), name);
	if (!temp_path[0])
		return;

	DeleteFileA(temp_path);
	path_text = CStrCreateFrom(temp_path);
	status = GameDocWriteCopy(PathCreateStr(path_text));
	CStrDestroy(&path_text);

	GetLocalTime(&now);
	if (status) {
		DeleteFileA(temp_path);
		StrPrintf(g_autosave_status, "autosave FAILED (status %d) at %02d:%02d:%02d",
			(int) status, now.wHour, now.wMinute, now.wSecond);
		return;
	}
	MoveFileExA(save_path, prev_path, MOVEFILE_REPLACE_EXISTING);
	MoveFileExA(temp_path, save_path, MOVEFILE_REPLACE_EXISTING);
	StrPrintf(g_autosave_status, "last autosave at %02d:%02d:%02d, node %d, day %d on the trail -> %s",
		now.wHour, now.wMinute, now.wSecond, (int) game->cur_node_id, (int) game->days_traveled, save_path);
}

static void CALLBACK autosave_tick(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	struct game *game;

	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;
	if (g_autosave_busy || !AutosaveEnabled() || !autosave_allowed())
		return;
	game = GetGameObj();
	if (game->cur_node_id == g_saved_node && game->days_traveled == g_saved_day)
		return;
	g_autosave_busy = true;
	g_saved_node = game->cur_node_id;
	g_saved_day = game->days_traveled;
	autosave_now();
	g_autosave_busy = false;
}

void AutosaveStart(void)
{
	if (!g_autosave_timer)
		g_autosave_timer = RpSetTimer(NULL, 0, 1000, (void *) autosave_tick);
}
