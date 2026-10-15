//

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"
#include "../base/object.h"

struct window;
struct view;
struct control;
struct win_dc;
struct palette;
struct cstr;
struct event;
struct oblist;
struct doc_file;

struct window_ops {
	struct object_ops base;
	bool (*vf04)(struct window *self);
	bool (*vf08)(struct window *self);
	bool (*vf0C)(struct window *self);
	bool (*PostCloseEvent)(struct window *self);
	bool (*vf14)(struct window *self, int32_t param);
	void (*Draw)(struct window *self, const struct rect *rect);
	bool (*IsDialog)(struct window *self);
	void (*OnActivate)(struct window *self);
	void (*OnDeactivate)(struct window *self);
	struct cstr *(*GetText)(struct window *self);
	void (*PostMouseEvent)(struct window *self, int32_t command, int32_t param1, int32_t param2);
	void (*HandleKey)(struct window *self, int32_t key, int32_t mods);
	void (*HandleMouse)(struct window *self, bool down, const struct point *pt, int32_t kind, int32_t param2,
		int32_t param1);
};

struct window {
	struct object base;
	int32_t window_id;
	struct view *root_view;
	struct window *parent;
	int32_t unused_0E;
	int32_t win_type;
	struct rect rect;
	void *hwnd;
	struct win_dc *windc;
	bool shown;
	struct event *trigger_event;
	struct control *focus_ctrl;
	struct oblist *ctrl_list;
	struct view *hover_child;
};

#define WINDOW_OPS(p) ((const struct window_ops *) ((const struct object *) (p))->ops)

extern const struct window_ops WINDOW_OPS_TABLE;

void WindowInit(struct window *self, struct view *view);
void WindowInitSize(struct window *self, struct view *view, int32_t w, int32_t h);
void window_destroy(struct object *self);

bool window_vf04(struct window *self);
bool window_vf08(struct window *self);
bool window_vf0c(struct window *self);
bool window_post_close_event(struct window *self);
bool window_vf14(struct window *self, int32_t param);
void window_draw(struct window *self, const struct rect *rect);
bool window_is_dialog(struct window *self);
void window_on_activate(struct window *self);
void window_on_deactivate(struct window *self);
struct cstr *window_get_text(struct window *self);
void window_post_mouse_event(struct window *self, int32_t command, int32_t param1, int32_t param2);
void window_handle_key(struct window *self, int32_t key, int32_t mods);
void window_handle_mouse(struct window *self, bool down, const struct point *pt, int32_t kind, int32_t param2,
	int32_t param1);

void WindowHide(struct window *self);
struct point WindowCursorToClient(struct window *self);
struct window *WindowFromHwnd(void *hwnd);
int32_t WindowCenterMethod(struct window *self);
int32_t WindowCenter(struct window *self);
void WindowBringToFront(struct window *self);
void WindowIdleRelease(struct window *self);
struct view *WindowSetRootView(struct window *self, struct view *view);
void WindowValidate(struct window *self, const struct rect *rect);
void WindowInvalidate(struct window *self, const struct rect *rect);
struct control *WindowFindCtrlById(struct window *self, int32_t id);
struct view *WindowFindByTypeId(struct window *self, int32_t code, int32_t id);
void WindowSelectPalette(struct window *self, struct palette *palette_obj);
void WindowFocusNext(struct window *self);
void WindowFocusPrev(struct window *self);
bool WindowRemoveFocusCtrl(struct window *self, struct control *ctrl);
void WindowAddFocusCtrl(struct window *self, struct control *ctrl, int32_t focus_order);
bool WindowSetFocusCtrl(struct window *self, struct control *ctrl);
void WindowFocusIfEligible(struct window *self, struct control *ctrl);
void WindowCreate(struct window *self, int32_t win_type, struct cstr *title, uint32_t style, uint32_t ex_style,
	struct window *parent);
void WindowInsertCtrl(struct window *self, struct control *ctrl, int32_t focus_order);
void WindowSetTitleCStr(struct window *self, struct cstr *text);
void WindowSetTextCStr(struct window *self, struct cstr *text);
void WindowShowV(struct window *self);
void WindowSetState(struct window *self, struct doc_file *flag);

struct window *GetActiveWindowObj(void);
bool IsRegisteredWindow(const void *obj);
void NotifyWindowsPalette(struct palette *palette_obj);
void NotifyWindowsActivate(void);
void NotifyWindowsDeactivate(void);
struct view *SetFocusObject(struct view *obj, bool play_sound);

extern struct view *g_focus_object;
extern bool g_mouse_captured;
