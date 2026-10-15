#include "../app.h"

#include <stddef.h>
#include <string.h>

#include <windows.h>
#include <commdlg.h>

#include "../../dialogs/about.h"
#include "../../dialogs/alert.h"
#include "../../ui/button.h"
#include "../../draw/canvas.h"
#include "../../ui/cliplabelview.h"
#include "../../base/cstr.h"
#include "../../ui/cursormgr.h"
#include "../../data/docfile.h"
#include "../docmanager.h"
#include "../../draw/fontrsrc.h"
#include "../../draw/fonttable.h"
#include "../../data/gfxdev.h"
#include "../../base/ini.h"
#include "../../ui/labeledcontrol.h"
#include "../license.h"
#include "../../base/memory.h"
#include "../../ui/menu.h"
#include "../../ui/menubar.h"
#include "../../dialogs/modaldialog.h"
#include "../../draw/palette.h"
#include "../../base/path.h"
#include "../../ui/piccontrol.h"
#include "../../draw/pictserver.h"
#include "../../ui/picview.h"
#include "../registrar.h"
#include "../replay.h"
#include "../../draw/server.h"
#include "../../base/str.h"
#include "../../ui/view.h"
#include "../../ui/window.h"

#pragma comment(lib, "comdlg32.lib")

struct app *g_app;
bool g_quit_requested;
bool g_use_dib_sections;
void *g_hinstance;
struct path *g_rsrc_path;
int32_t g_palette_rsrc_id = -1;
struct palette *g_palette;
struct pict_server *g_pict_server;
struct cursor_mgr *g_cursor_mgr;
struct menu_bar *g_menubar;
struct alert *g_alert_handler;

static void *g_sound_server;
static void *(*g_sound_create)(struct assets *lib);
static void (*g_sound_resume)(void *server);
static void (*g_sound_release)(void *server);

void AppSetSoundServerHooks(void *(*create)(struct assets *lib), void (*resume)(void *server),
	void (*release)(void *server))
{
	g_sound_create = create;
	g_sound_resume = resume;
	g_sound_release = release;
}

struct app *GetApp(void)
{
	return g_app;
}

void AppOnActivate(void *hwnd, int32_t active, intptr_t lparam)
{
	(void) hwnd;
	(void) lparam;
	struct app *app = GetApp();

	if (active) {
		APP_OPS(app)->OnActivate(app);

	} else {
		APP_OPS(app)->OnDeactivate(app);
	}
}

static bool app_suspended_query(void)
{
	struct app *app = GetApp();

	return APP_OPS(app)->IsSuspended(app);
}

const struct app_ops APP_OPS_TABLE = {
	{
		{app_destroy},
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
	NULL,
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

void AppInit(struct app *self)
{
	EvtHandlerInit(&self->base, EVCLS_APP, 0);
	self->base.base.ops = &APP_OPS_TABLE.base.base;

	g_app = self;
	self->suspended = false;

	EvtSetSuspendedQuery(app_suspended_query);
}

void app_destroy(struct object *obj)
{
	struct app *self = (struct app *) obj;

	MenuBarDestroy(&g_menubar);

	if (self->docmgr)
		DocManagerCloseAll(self->docmgr);

	DocManagerDestroy(&self->docmgr);
	AlertDestroy(&g_alert_handler);
	CleanupFontTable();

	if (g_pict_server) {
		ServerReleaseAll((struct server *) g_pict_server);
		PictServerDestroy(&g_pict_server);
	}

	if (g_sound_server) {
		if (g_sound_release)
			g_sound_release(g_sound_server);

		g_sound_server = NULL;
	}

	CursorSetId(CURSOR_ARROW);

	CursorMgrDestroy(&g_cursor_mgr);
	CStrDestroy(&self->app_path);
	GfxDevCloseLibraries();
	PaletteDestroy(&g_palette);
	RegistrarDestroy(&g_registrar_singleton);
	EvtHandlerUnbindHandler(&self->base);

	obj->ops = &EVT_HANDLER_OPS_TABLE.base;
	evt_handler_destroy(obj);
}

void app_on_app_init(struct app *self)
{
	(void) self;
}

void app_on_menus_ready(struct app *self)
{
	(void) self;
}

void app_on_init_complete(struct app *self)
{
	(void) self;
}

void AppInitAndRun(struct app *self, const char *cmdline)
{
	int32_t cmd_ids[8] = {CMD_QUIT, CMD_CLOSE_WINDOW, CMD_ABOUT, CMD_UNDO, CMD_CUT, CMD_COPY, CMD_PASTE, CMD_CLEAR};

	self->app_path = CStrCreateFrom(cmdline);
	struct path *rsrc_path = APP_OPS(self)->GetResourcePath(self);
	bool available = GfxDevOpenLibraries(cmdline, CStrGet(rsrc_path->path));
	PathDestroy(&rsrc_path);

	if (!available)
		return;

	g_pict_server = PictServerCreate();
	g_cursor_mgr = CursorMgrCreate();

	if (g_sound_create)
		g_sound_server = g_sound_create(g_data_lib);

	g_palette_rsrc_id = 1008;
	g_palette = PaletteCreateLoad(g_data_lib, g_palette_rsrc_id, true);
	struct font_rsrc *font_rsrc = FontRsrcCreate(g_data_lib, 128);
	InitFontTable(font_rsrc);
	FontRsrcDestroy(&font_rsrc);
	g_alert_handler = AlertCreate("");
	LicenseInitialize(0);
	self->license_mode = LicenseGetType();
	APP_OPS(self)->OnAppInit(self);
	self->docmgr = DocManagerCreate();
	APP_OPS(self)->CreateMenuBar(self);
	APP_OPS(self)->OnMenusReady(self);
	EvtHandlerBindHandler(&self->base, 8, cmd_ids);
	APP_OPS(self)->OnInitComplete(self);
}

void app_noop13(struct app *self)
{
	(void) self;
}

void app_on_event(struct evt_handler *handler, struct cmd_event *ev)
{
	struct app *self = (struct app *) handler;
	int32_t command = ev->command;

	switch (command) {
		case CMD_ABOUT: {
			struct about *dlg = AboutCreate();
			ModalDialogRun(&dlg->base, 0, GetActiveWindowObj());
			AboutDestroy(&dlg);

			if (APP_OPS(self)->GetMode(self) == LICENSE_DEMO)
				APP_OPS(self)->SetLicense(self, LicenseGetType());

			ev->handled = true;
			break;
		}
		case CMD_UNDO:
		case CMD_CUT:
		case CMD_COPY:
		case CMD_PASTE:
		case CMD_CLEAR: {
			struct window *window = GetActiveWindowObj();

			if (window)
				WINDOW_OPS(window)->PostMouseEvent(window, command, 0, 0);

			ev->handled = true;
			break;
		}
		case CMD_QUIT:
			g_quit_requested = true;

			if (DocManagerAnyModified(self->docmgr)) {
				switch (DocManagerCloseProceed(self->docmgr)) {
					case 0:
						DocManagerFlushModifiedV(self->docmgr);
						DocManagerCloseAll(self->docmgr);
						break;
					case 1:
						DocManagerCloseAll(self->docmgr);
						break;
					default:
						g_quit_requested = false;
						break;
				}
			}

			ev->handled = true;
			break;
		case 31010:
			break;
	}
}

void app_register_object_types(struct app *self)
{
	(void) self;

	RegistrarRegister(256, CtrlCreateFromDesc);
	RegistrarRegister(259, CreateControlFromTemplate);
	RegistrarRegister(268, PicControlCreateFromDesc);
	RegistrarRegister(269, CreatePicView);
	RegistrarRegister(270, CreateLabeledControlFromTemplate);
	RegistrarRegister(271, CreateGroupFromTemplate);
	RegistrarRegister(273, CreateClipLabelView);
}

void app_on_activate(struct app *self)
{
	self->suspended = false;
	NotifyWindowsActivate();

	if (g_sound_server && g_sound_resume)
		g_sound_resume(g_sound_server);
}

void app_run_until_quit(struct app *self)
{
	do {
		APP_OPS(self)->PumpTick(self);

	} while (!g_quit_requested);
}

bool AppLoadPalette(struct app *self, int32_t palette_id)
{
	(void) self;
	bool loaded = false;

	if (g_palette_rsrc_id != palette_id) {
		struct assets *lib = g_data_lib;

		if (lib) {
			struct palette *palette = PaletteCreateLoad(lib, palette_id, true);

			if (palette->palette_built) {
				g_palette_rsrc_id = palette_id;
				NotifyWindowsPalette(palette);
				PaletteDestroy(&g_palette);
				g_palette = palette;
				loaded = true;

			} else {
				PaletteDestroy(&palette);
			}
		}
	}

	return loaded;
}

void app_create_menu_bar(struct app *self)
{
	(void) self;
	struct menu *body = MenuCreate(MENU_RSRC_BAR, g_text_lib, NULL);

	if (g_menubar)
		MenuBarDestroy(&g_menubar);

	g_menubar = MenuBarCreate(body);
}

void app_on_deactivate(struct app *self)
{
	(void) self;
	NotifyWindowsDeactivate();
}

void app_update_edit_menu_state(struct app *self, int32_t command)
{
	if (!command || command == MENU_ITEM_FILE) {
		if (DocManagerIsEmpty(self->docmgr)) {
			MenuBarEnableItem(g_menubar, MENU_ITEM_NEW_GAME, true);
			MenuBarEnableItem(g_menubar, MENU_ITEM_LOAD_GAME, true);

		} else {
			MenuBarEnableItem(g_menubar, MENU_ITEM_NEW_GAME, false);
			MenuBarEnableItem(g_menubar, MENU_ITEM_LOAD_GAME, false);
		}

		struct doc_file *single_doc = DocManagerGetSingleDoc(self->docmgr);

		if (single_doc) {
			MenuBarEnableItem(g_menubar, MENU_ITEM_END_GAME, true);
			MenuBarEnableItem(g_menubar, MENU_ITEM_SAVE_GAME, true);

			if (DocFileIsDirty(single_doc)) {
				MenuBarEnableItem(g_menubar, MENU_ITEM_SAVE, true);

			} else {
				MenuBarEnableItem(g_menubar, MENU_ITEM_SAVE, false);
			}

		} else {
			MenuBarEnableItem(g_menubar, MENU_ITEM_END_GAME, false);
			MenuBarEnableItem(g_menubar, MENU_ITEM_SAVE, false);
			MenuBarEnableItem(g_menubar, MENU_ITEM_SAVE_GAME, false);
		}
	}
}

static void find_data_folder_dialog(char *folder, size_t size)
{
#ifdef OTII_TWITCH
	(void) folder;
	(void) size;
#else
	char data_name[64];
	char picked[MAX_PATH];
	OPENFILENAMEA ofn;

	StrPrintf(data_name, "%s.Dat", g_cmdline);

	for (;;) {
		char message[512];

		picked[0] = 0;
		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFilter = "Oregon Trail II data (OregonII.Dat)\0OregonII.Dat\0Data files (*.dat)\0*.dat\0";
		ofn.lpstrFile = picked;
		ofn.nMaxFile = sizeof(picked);
		ofn.lpstrTitle = "Where is OregonII.Dat?";
		ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

		if (!GetOpenFileNameA(&ofn))
			return;

		char *slash = strrchr(picked, '\\');

		if (slash && !_stricmp(slash + 1, data_name)) {
			slash[1] = 0;
			StrCopyN(folder, size, picked);
			IniSetString("cdrom", "rsrcpath", folder);
			return;
		}

		StrPrintf(message, "The game's data file must be named %s.\n\nPlease pick that file.", data_name);
		RpMessageBox(NULL, message, "Oregon Trail II", MB_OK | MB_ICONEXCLAMATION);
	}
#endif
}

struct path *app_get_resource_path(struct app *self)
{
	(void) self;

	if (!g_rsrc_path) {
		char folder[MAX_PATH] = {0};

		if (!GfxDevFindDataFolder(g_cmdline, folder, sizeof(folder)))
			find_data_folder_dialog(folder, sizeof(folder));

		struct cstr *path_str = CStrCreateFrom(folder);
		g_rsrc_path = PathCreateStr(path_str);
		CStrDestroy(&path_str);
	}

	return PathCreateCopy(g_rsrc_path);
}

bool app_is_suspended(struct app *self)
{
	return self->suspended;
}

int32_t app_get_mode(struct app *self)
{
	return self->license_mode;
}

struct doc_manager *app_get_doc_manager(struct app *self)
{
	return self->docmgr;
}

struct cstr *app_get_app_path(struct app *self)
{
	return self->app_path;
}

void app_set_license(struct app *self, int32_t type)
{
	self->license_mode = type;
}

void app_wait_game_ticks(struct app *self, int32_t ticks)
{
	int32_t deadline = GameTicks() + ticks + 1;

	do {
		APP_OPS(self)->PumpTick(self);

	} while (!g_quit_requested && GameTicks() < deadline);
}
