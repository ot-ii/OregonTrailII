#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "evthandler.h"

struct app;
struct doc_manager;
struct cstr;
struct path;

struct app_ops {
	struct evt_handler_ops base;
	int32_t (*GetMode)(struct app *self);
	struct doc_manager *(*GetDocManager)(struct app *self);
	void (*PumpTick)(struct app *self);
	struct cstr *(*GetAppPath)(struct app *self);
	void (*Noop13)(struct app *self);
	void (*RegisterObjectTypes)(struct app *self);
	void (*OnActivate)(struct app *self);
	void (*RunUntilQuit)(struct app *self);
	struct path *(*GetResourcePath)(struct app *self);
	void (*SetLicense)(struct app *self, int32_t type);
	void (*CreateMenuBar)(struct app *self);
	void (*WaitGameTicks)(struct app *self, int32_t ticks);
	void (*OnDeactivate)(struct app *self);
	bool (*IsSuspended)(struct app *self);
	void (*UpdateEditMenuState)(struct app *self, int32_t command);
	void (*OnAppInit)(struct app *self);
	void (*OnMenusReady)(struct app *self);
	void (*OnInitComplete)(struct app *self);
};

struct app {
	struct evt_handler base;
	struct doc_manager *docmgr;
	struct cstr *app_path;
	int32_t license_mode;
	bool suspended;
};

#define APP_OPS(p) ((const struct app_ops *) ((const struct object *) (p))->ops)

extern const struct app_ops APP_OPS_TABLE;

void AppInit(struct app *self);
void app_destroy(struct object *self);

void app_on_event(struct evt_handler *self, struct cmd_event *event);
int32_t app_get_mode(struct app *self);
struct doc_manager *app_get_doc_manager(struct app *self);
struct cstr *app_get_app_path(struct app *self);
void app_noop13(struct app *self);
void app_register_object_types(struct app *self);
void app_on_activate(struct app *self);
void app_run_until_quit(struct app *self);
struct path *app_get_resource_path(struct app *self);
void app_set_license(struct app *self, int32_t type);
void app_create_menu_bar(struct app *self);
void app_wait_game_ticks(struct app *self, int32_t ticks);
void app_on_deactivate(struct app *self);
bool app_is_suspended(struct app *self);
void app_update_edit_menu_state(struct app *self, int32_t command);
void app_on_app_init(struct app *self);
void app_on_menus_ready(struct app *self);
void app_on_init_complete(struct app *self);

bool AppLoadPalette(struct app *self, int32_t palette_id);
void AppInitAndRun(struct app *self, const char *cmdline);

void AppOnActivate(void *hwnd, int32_t active, intptr_t lparam);
struct app *GetApp(void);

extern struct app *g_app;
extern bool g_quit_requested;
extern void *g_hinstance;
extern const char *g_cmdline;
extern struct path *g_rsrc_path;
extern int32_t g_palette_rsrc_id;

struct assets;

void AppSetSoundServerHooks(void *(*create)(struct assets *lib), void (*resume)(void *server),
	void (*release)(void *server));

struct app *OtiiAppCreate(void);
