#include "../appvideo.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <shellapi.h>
#include <mmsystem.h>
#include <vfw.h>

#include "../../features/autopilot.h"
#include "../../draw/canvas.h"
#include "../../ui/control.h"
#include "../../features/crowdcontrol.h"
#include "../../ui/edit.h"
#include "../event.h"
#include "../evthandler.h"
#include "../../ui/framewin.h"
#include "../../base/list.h"
#include "../../ui/menu.h"
#include "../../ui/menubar.h"
#include "../../ui/menuitem.h"
#include "../../draw/palette.h"
#include "../replay.h"
#include "../../ui/scaler.h"
#include "../../draw/screendc.h"
#include "../../features/twitchcontrol.h"
#include "../../features/twitchparty.h"
#include "../../ui/view.h"
#include "../../draw/windc.h"
#include "../../ui/window.h"
#include "../../ui/windowborder.h"
#include "../../ui/windowpos.h"
#include "../../ui/wndmouse.h"

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "vfw32.lib")

#ifndef WM_GETDPISCALEDSIZE
#define WM_GETDPISCALEDSIZE 0x02E4
#endif

static LRESULT CALLBACK mecc_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

const struct app_ops APP_VIDEO_OPS_TABLE = {
	{
		{app_video_destroy},
		evt_handler_is_handled,
		evt_handler_set_handled_for,
		evt_handler_set_handled,
		evt_handler_clear_handled_for,
		evt_handler_clear_handled,
		app_on_event,
		evt_handler_on_idle,
		evt_handler_on_idle_spare,
	},
	app_get_mode,
	app_get_doc_manager,
	app_video_pump_tick,
	app_get_app_path,
	app_noop13,
	app_register_object_types,
	app_on_activate,
	app_run_until_quit,
	app_get_resource_path,
	app_set_license,
	app_create_menu_bar,
	app_wait_game_ticks,
	app_on_deactivate,
	app_is_suspended,
	app_update_edit_menu_state,
	app_on_app_init,
	app_on_menus_ready,
	app_on_init_complete,
};

void AppVideoInit(struct app_video *self, const char *app_name)
{
	(void) app_name;

	AppInit(&self->base);
	self->base.base.base.ops = &APP_VIDEO_OPS_TABLE.base.base;

	CheckVideoMode();
}

void app_video_destroy(struct object *obj)
{
	obj->ops = &APP_OPS_TABLE.base.base;
	app_destroy(obj);
}

void RegisterWindowClasses(void)
{
	WNDCLASSA wc;

	wc.style = CS_OWNDC;
	wc.lpfnWndProc = mecc_wndproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 4;
	wc.hInstance = (HINSTANCE) g_hinstance;
	wc.hIcon = LoadIconA((HINSTANCE) g_hinstance, g_cmdline);
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "MECCWClass";
	RegisterClassA(&wc);

	wc.style = CS_OWNDC;
	wc.lpfnWndProc = mecc_wndproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0x22;
	wc.hInstance = (HINSTANCE) g_hinstance;
	wc.hIcon = NULL;
	wc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
	wc.lpszMenuName = NULL;
	wc.lpszClassName = "MECCDClass";
	RegisterClassA(&wc);

	if (__argc > 1) {
		for (int i = 1; i < __argc; i++) {
			if (__argv[i][0] == '/' || __argv[i][0] == '-') {
				if (__argv[i][1] == 'W') {
					g_use_dib_sections = false;
					break;
				}
			}
		}
	}
}

void app_video_pump_tick(struct app *self)
{
	(void) self;
	MSG msg;

	if (RpPeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		if (msg.message == WM_QUIT) {
			g_quit_requested = true;
			return;
		}

		TranslateMessage(&msg);
		RpDispatchMessage(&msg);

	} else {
		RunIdleProcessing(3);
	}
}

static void commit_typed_edits(struct view *v)
{
	if (!v)
		return;

	if (v->base.base.ops == &EDIT_OPS_TABLE.base.base.base.base) {
		struct edit *e = (struct edit *) v;

		if (e->base.hwnd && e->change_pending) {
			struct cmd_event *done = CmdEventCreateSpecs(CMD_EDIT_COMMITTED, &e->base.base.base.eh_event,
				&e->base.c_event);
			done->param1 = 0;
			e->change_pending = false;
			DispatchGlobalEvent(done);
		}
	}

	if (v->has_child_list && v->child_list) {
		for (int32_t i = 0; i < ObarrayCount(v->child_list); ++i)
			commit_typed_edits(ObarrayGetAt(v->child_list, i));
	}
}

static bool is_dialog_type(struct window *win)
{
	return win && (win->win_type == 2 || win->win_type == 3);
}

static void wnd_handle_char(HWND hwnd, int32_t key, int32_t lparam_lo)
{
	(void) lparam_lo;
	struct window *win = WindowFromHwnd(hwnd);

	if (!win)
		return;

	WINDOW_OPS(win)->HandleKey(win, key, 0);
}

static void wnd_handle_command(HWND hwnd, int32_t id, HWND ctl_hwnd, int32_t notify)
{
	if (ctl_hwnd) {
		struct control *control = GetPropA(ctl_hwnd, "CCLO");

		if (control)
			CONTROL_OPS(control)->OnCommand(control, hwnd, id, ctl_hwnd, notify);

		return;
	}

	if (id == DEBUG_SCREEN_MENU_ID) {
		DebugScreenToggle();
		return;
	}

#ifdef OTII_AUTO_MODE
	if (AutoPilotMenuCommand(hwnd, id))
		return;
#endif

#ifdef OTII_TWITCH
	if (id == TWITCH_OPTIONS_MENU_ID) {
		TwitchControlToggleWindow();
		return;
	}
#endif

#ifdef CROWD_CONTROL
	if (id == CROWD_CONTROL_MENU_ID) {
		CrowdControlToggleWindow();
		return;
	}
#endif

#ifdef TWITCH_PARTY
	if (id == TWITCH_PARTY_MENU_ID) {
		TwitchPartyShowWindow();
		return;
	}
#endif

	if (id == DISCORD_MENU_ID) {
		ShellExecuteA(hwnd, "open", DISCORD_URL, NULL, NULL, SW_SHOWNORMAL);
		return;
	}

	if (AudioDeviceMenuCommand(id))
		return;

	if (ScalerMenuCommand(hwnd, id))
		return;

	if (id == FORCE_SAVE_MENU_ID) {
		ForceSaveRun();
		return;
	}

	if (id == PLAY_INTRO_MENU_ID) {
		PlayIntroRun();
		return;
	}

	if (id == AUTOSAVE_MENU_ID) {
		AutosaveSetEnabled(!AutosaveEnabled());
		CheckMenuItem(GetMenu(hwnd), AUTOSAVE_MENU_ID, MF_BYCOMMAND | (AutosaveEnabled() ? MF_CHECKED : MF_UNCHECKED));
		return;
	}

	struct window *win = WindowFromHwnd(hwnd);

	if (!win || win->win_type != 1) {
		CustomBorderWndProc(hwnd, WM_COMMAND, ((uintptr_t) (uint16_t) notify << 16) | (uint16_t) id,
			(intptr_t) ctl_hwnd);
		return;
	}

	struct menu *out_menu = NULL;
	int32_t out_index = 0;
	struct menu_item *item = MenuBarFindItem(g_menubar, id, &out_menu, &out_index);

	if (item)
		MenuItemFireCommand(item);
}

static LRESULT wnd_handle_paint(HWND hwnd)
{
	PAINTSTRUCT paint;
	struct window *win = WindowFromHwnd(hwnd);
	BeginPaint(hwnd, &paint);

	if (win)
		WINDOW_OPS(win)->Draw(win, NULL);

	EndPaint(hwnd, &paint);
	ScalerPresent(hwnd, true);

	return is_dialog_type(win) ? DefDlgProcA(hwnd, WM_PAINT, 0, 0) : CustomBorderWndProc(hwnd, WM_PAINT, 0, 0);
}

static void wnd_handle_close(HWND hwnd)
{
	struct window *win = WindowFromHwnd(hwnd);

	if (win && WINDOW_OPS(win)->PostCloseEvent(win))
		ObjectDelete(win);
}

static HPALETTE window_hpal(struct window *win)
{
	return (HPALETTE) win->windc->base.palette_obj->hpal;
}

static LRESULT wnd_handle_palettechanged(HWND hwnd, HWND changer)
{
	struct window *win = WindowFromHwnd(hwnd);

	if (win && changer != hwnd) {
		HDC hdc = GetDC(hwnd);
		HPALETTE old_pal = SelectPalette(hdc, window_hpal(win), FALSE);
		UINT changed = RealizePalette(hdc);
		SelectPalette(hdc, old_pal, FALSE);
		ReleaseDC(hwnd, hdc);

		if (changed)
			InvalidateRect(hwnd, NULL, FALSE);
	}

	return is_dialog_type(win) ? DefDlgProcA(hwnd, WM_PALETTECHANGED, (WPARAM) changer, 0) :
		CustomBorderWndProc(hwnd, WM_PALETTECHANGED, (uintptr_t) changer, 0);
}

static int32_t wnd_realize_palette(HWND hwnd)
{
	struct window *win = WindowFromHwnd(hwnd);

	if (!win)
		return 0;

	HDC hdc = GetDC(hwnd);
	HPALETTE old_pal = SelectPalette(hdc, window_hpal(win), FALSE);

	if (RealizePalette(hdc))
		InvalidateRect(hwnd, NULL, FALSE);

	SelectPalette(hdc, old_pal, FALSE);
	ReleaseDC(hwnd, hdc);

	return 1;
}

static LRESULT dlg_handle_erasebkgnd(HWND hwnd, WPARAM wparam)
{
	struct window *win = WindowFromHwnd(hwnd);

	if (win) {
		if (win->win_type == 2 || win->win_type == 3)
			return DefDlgProcA(hwnd, WM_ERASEBKGND, wparam, 0);
	}

	return 1;
}

static LRESULT mecc_wndproc_body(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct window *win = WindowFromHwnd(hwnd);

	if ((msg == WM_EXITSIZEMOVE || msg == WM_DESTROY) && g_parent_win && hwnd == g_parent_win->base.hwnd)
		WindowPosSave(hwnd);

	if ((msg == WM_EXITSIZEMOVE || msg == WM_DESTROY) && g_health_window &&
		hwnd == ((struct window *) g_health_window)->hwnd)
		WindowPosSaveAs(hwnd, "health");

	if (msg == WM_INITMENUPOPUP)
		AudioDeviceMenuOnInitPopup((void *) wparam);

#ifdef TWITCH_PARTY
	if (msg == WM_INITMENUPOPUP)
		TwitchPartyMenuUpdate((void *) wparam);
#endif

#ifdef OTII_AUTO_MODE
	if (msg == WM_INITMENUPOPUP)
		AutoPilotMenuUpdate((void *) wparam);
#endif

	if (msg == WM_INITMENUPOPUP)
		ScalerMenuUpdate((void *) wparam);

	if (msg == WM_GETDPISCALEDSIZE) {
		SIZE *size = (SIZE *) lparam;
		int32_t cx = size->cx;
		int32_t cy = size->cy;
		bool answered = ScalerDpiScaledSize(hwnd, (uint32_t) wparam, &cx, &cy);
		size->cx = cx;
		size->cy = cy;

		if (answered)
			return TRUE;
	}

	if (msg == WM_ENTERSIZEMOVE || msg == WM_EXITSIZEMOVE)
		ScalerSizeMove(hwnd, msg == WM_ENTERSIZEMOVE);

	if (msg == WM_DPICHANGED)
		return ScalerDpiChanged(hwnd, wparam, lparam);

	switch (msg) {
		case WM_CHAR:
			wnd_handle_char(hwnd, (int8_t) wparam, (int16_t) lparam);
			return 0;
		case WM_LBUTTONDOWN:
			if (win)
				commit_typed_edits(win->root_view);

			WndHandleLButtonDown(hwnd, 0, ScalerToGame((int16_t) lparam), ScalerToGame((int16_t) HIWORD(lparam)),
				(int32_t) wparam);
			return 0;
		case WM_LBUTTONUP:
			WndHandleLButtonUp(hwnd, ScalerToGame((int16_t) lparam), ScalerToGame((int16_t) HIWORD(lparam)),
				(int32_t) wparam);
			return 0;
		case WM_MOUSEMOVE:
			WndHandleMouseMove(hwnd, ScalerToGame((int16_t) lparam), ScalerToGame((int16_t) HIWORD(lparam)),
				(int32_t) wparam);
			return 0;
		case WM_COMMAND:
			wnd_handle_command(hwnd, (uint16_t) wparam, (HWND) lparam, HIWORD(wparam));
			return 0;
		case WM_PAINT:
			wnd_handle_paint(hwnd);
			return 0;
		case WM_CLOSE:
			wnd_handle_close(hwnd);
			return 0;
		case WM_PALETTECHANGED:
			wnd_handle_palettechanged(hwnd, (HWND) wparam);
			return 0;
		case WM_QUERYNEWPALETTE:
			return (uint16_t) wnd_realize_palette(hwnd);
		case WM_ERASEBKGND:
			return dlg_handle_erasebkgnd(hwnd, wparam);
		case WM_ACTIVATEAPP:
			AppOnActivate(hwnd, (int32_t) wparam, lparam);
			return 0;
		default:
			if (is_dialog_type(win))
				return DefDlgProcA(hwnd, msg, wparam, lparam);

			return CustomBorderWndProc(hwnd, msg, wparam, lparam);
	}
}

static LRESULT CALLBACK mecc_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	int32_t token = RpWindowEnter(hwnd, msg, wparam, lparam);

	if (token < 0)
		return 0;

	LRESULT result = mecc_wndproc_body(hwnd, msg, wparam, lparam);
	RpWindowLeave(token);

	return result;
}

bool CheckVideoMode(void)
{
	bool low_color = false;
	HDC hdc = GetDC(GetDesktopWindow());
	int caps = GetDeviceCaps(hdc, BITSPIXEL);

	if (caps < 8)
		low_color = true;

	ReleaseDC(GetDesktopWindow(), hdc);

	if (low_color) {
		RpMessageBox(NULL, "This product requires a video mode that supports at least 256 colors.", "Video Mode",
			MB_ICONEXCLAMATION);
		FatalAppExitA(0, "Inadequate video mode.");
	}

	uint32_t vfw_version = (VideoForWindowsVersion() >> 16) & 0xFFFF;

	if (vfw_version < 0x10A) {
		RpMessageBox(NULL, "This product requires Video for Windows version 1.10 for optimal performance.",
			"Video for Windows", MB_ICONEXCLAMATION);
	}

	return true;
}

const char *GetFirstCmdlineArg(void)
{
	return RpFirstArgument();
}
