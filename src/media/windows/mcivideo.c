#include "../mcivideo.h"

#include <stddef.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>
#include <vfw.h>

#include "../../app/app.h"
#include "../../base/byteorder.h"
#include "../../base/cstr.h"
#include "../../ui/ctrldesc.h"
#include "../../base/file.h"
#include "../../base/ini.h"
#include "../../base/memory.h"
#include "../movplayer.h"
#include "../../base/object.h"
#include "../../draw/palette.h"
#include "../../base/path.h"
#include "../../app/replay.h"
#include "../../ui/scaler.h"
#include "../../draw/screendc.h"
#include "../../base/str.h"
#include "../vfwprobe.h"
#include "../../draw/windc.h"
#include "../../ui/window.h"

#ifdef OTII_TEST
#include <intrin.h>

#include "../../../tests/testframework.h"
#endif

#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "winmm.lib")

static const int32_t g_mcivideo_handler_table[2] = {CMD_MOUSE, CMD_VIDEO_CTRL};

static char g_mci_error_text[0x80];
static DWORD g_mci_id = 0;
static WNDPROC g_mcivideo_prevproc;
static char g_movies_dir[MAX_PATH];

static LRESULT CALLBACK mci_video_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

const struct mci_video_ops MCI_VIDEO_OPS_TABLE = {
	{
		{
			{
				{mci_video_destroy},
				evt_handler_is_handled,
				control_set_handled_for,
				control_set_handled,
				control_clear_handled_for,
				control_clear_handled,
				mci_video_on_event,
				mci_video_on_idle,
				view_on_idle_spare,
			},
			view_add_child,
			view_get_bounds,
			view_get_clip_region,
			control_hide,
			view_remove_child,
			view_remove_from_parent,
			view_set_cursor_id,
			control_set_device,
			control_show,
			mci_video_draw_children,
			view_move_children,
		},
		control_set_value,
		control_on_command,
		control_focus_sibling,
		control_on_key_down,
		control_set_focus,
		mci_video_create_or_destroy,
	},
	mci_video_pause,
	mci_video_play,
	mci_video_set_volume,
	mci_video_stop,
	mci_video_post_event,
};

static bool mov_available(int32_t id)
{
	static int32_t folder = -1;
	char name[MAX_PATH];
	DWORD attrs;

	if (folder < 0) {
		struct app *app = GetApp();
		struct path *resource_path = APP_OPS(app)->GetResourcePath(app);
		struct cstr *dir = PathCopyPath(resource_path);
		ObjectDelete(resource_path);
		CStrAppendBuf(dir, "MOVIES");
		attrs = GetFileAttributesA(CStrGet(dir));
		folder = attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY);

		if (folder)
			StrCopy(g_movies_dir, CStrGet(dir));

		CStrDestroy(&dir);
	}

	if (!folder || id <= 0)
		return false;

	StrPrintf(name, "%s\\%d.MOV", g_movies_dir, id);
	attrs = GetFileAttributesA(name);

	return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

int32_t MciVideoFileSize(int32_t id)
{
	WIN32_FILE_ATTRIBUTE_DATA info;
	char name[MAX_PATH];
	bool is_mov = mov_available(id);
	struct app *app = GetApp();
	struct path *resource_path = APP_OPS(app)->GetResourcePath(app);
	struct cstr *dir = PathCopyPath(resource_path);

	ObjectDelete(resource_path);
	StrPrintf(name, is_mov ? "%sMOVIES\\%d.MOV" : "%sAVI\\%d.avi", CStrGet(dir), id);
	CStrDestroy(&dir);

	if (!GetFileAttributesExA(name, GetFileExInfoStandard, &info) || (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		return 0;

	return info.nFileSizeHigh ? 0x7FFFFFFF : (int32_t) (info.nFileSizeLow & 0x7FFFFFFF);
}

void MciVideoInitPath(struct mci_video *self, int32_t id, const struct event *event, const struct rect *rect,
	struct path *path_arg, bool arg_unused_8C, bool arg_autoplay)
{
	struct control *ctrl = &self->base;

	ControlInit(ctrl, CTRL_MCI_VIDEO, id, event, rect, 0, -1);
	ctrl->base.base.base.ops = &MCI_VIDEO_OPS_TABLE.base.base.base.base;

	self->probe = NULL;
	ctrl->hwnd = NULL;
	self->is_mov = false;
	self->mov = NULL;
	self->path = path_arg;
	self->ctrl_id = id;
	self->unused_8C = arg_unused_8C;
	self->autoplay = arg_autoplay;
	ctrl->owns_window = true;
	EvtHandlerBindHandler(&ctrl->base.base, 2, g_mcivideo_handler_table);
}

void MciVideoInit(struct mci_video *self, int32_t id, const struct event *event, const struct rect *rect,
	int32_t video_id, bool arg_unused_8C, bool arg_autoplay)
{
	struct control *ctrl = &self->base;

	ControlInit(ctrl, CTRL_MCI_VIDEO, id, event, rect, 0, -1);
	ctrl->base.base.base.ops = &MCI_VIDEO_OPS_TABLE.base.base.base.base;

	self->probe = NULL;
	ctrl->hwnd = NULL;
	self->ctrl_id = (int16_t) video_id;
#ifdef OTII_TEST
	if (self->ctrl_id <= 0)
		TCheck(false, "a video with a number", __FILE__, __LINE__, "video %d asked for by exe+0x%X", self->ctrl_id,
			(unsigned) ((char *) _ReturnAddress() - (char *) GetModuleHandleA(NULL)));
#endif

	self->is_mov = mov_available(self->ctrl_id);
	self->mov = NULL;
	struct cstr *file_name = CStrCreateInt(self->ctrl_id, false);
	CStrAppendBuf(file_name, self->is_mov ? ".MOV" : ".avi");

	struct cstr *rel_path = CStrCreateFrom(self->is_mov ? "MOVIES\\" : "AVI\\");
	CStrAppendStr(rel_path, file_name);
	CStrDestroy(&file_name);

	self->path = PathCreateStr(rel_path);
	CStrDestroy(&rel_path);
	self->unused_8C = arg_unused_8C;
	self->autoplay = arg_autoplay;
	self->is_playing = false;
	ctrl->owns_window = true;
	EvtHandlerBindHandler(&ctrl->base.base, 2, g_mcivideo_handler_table);
}

struct mci_video *MciVideoCreatePath(int32_t id, const struct event *event, const struct rect *rect,
	struct path *path_arg, bool arg_unused_8C, bool arg_autoplay)
{
	struct mci_video *self = MemAlloc(1, sizeof(struct mci_video));
	MciVideoInitPath(self, id, event, rect, path_arg, arg_unused_8C, arg_autoplay);

	return self;
}

struct mci_video *MciVideoCreate(int32_t id, const struct event *event, const struct rect *rect, int32_t video_id,
	bool arg_unused_8C, bool arg_autoplay)
{
	struct mci_video *self = MemAlloc(1, sizeof(struct mci_video));
	MciVideoInit(self, id, event, rect, video_id, arg_unused_8C, arg_autoplay);

	return self;
}

void MciVideoDestroy(struct mci_video **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

static bool g_clip_sound;

struct avi_audio {
	int32_t stream;
	int32_t streams;
	bool format_ok;
	int32_t rate;
	int32_t channels;
	int32_t bits;
	uint8_t *pcm;
	int32_t bytes;
	int32_t capacity;
};

static void avi_walk(const uint8_t *p, const uint8_t *end, struct avi_audio *a, bool in_strl)
{
	while (end - p >= 8) {
		uint32_t size = ReadLeU32(p + 4);
		const uint8_t *body = p + 8;

		if (size > (uint32_t) (end - body))
			size = (uint32_t) (end - body);

		if (!memcmp(p, "LIST", 4) || !memcmp(p, "RIFF", 4)) {
			if (size >= 4) {
				bool strl = !memcmp(body, "strl", 4);
				avi_walk(body + 4, body + size, a, strl);

				if (strl)
					++a->streams;
			}

		} else if (in_strl && !memcmp(p, "strh", 4) && size >= 4 && !memcmp(body, "auds", 4) && a->stream < 0) {
			a->stream = a->streams;

		} else if (in_strl && !memcmp(p, "strf", 4) && a->stream == a->streams && size >= 16 && !a->format_ok) {
			a->format_ok = ReadLeU16(body) == WAVE_FORMAT_PCM;
			a->channels = (int32_t) ReadLeU16(body + 2);
			a->rate = ReadLe32(body + 4);
			a->bits = (int32_t) ReadLeU16(body + 14);

		} else if (a->stream >= 0 && p[2] == 'w' && p[3] == 'b' && p[0] >= '0' && p[0] <= '9' && p[1] >= '0' &&
			p[1] <= '9' && (p[0] - '0') * 10 + (p[1] - '0') == a->stream && size) {
			if (a->bytes + (int32_t) size > a->capacity) {
				int32_t cap = (a->capacity ? a->capacity * 2 : 65536) + (int32_t) size;
				a->pcm = MemRealloc(a->pcm, (size_t) cap, 1);
				a->capacity = cap;
			}

			memcpy(a->pcm + a->bytes, body, size);
			a->bytes += (int32_t) size;
		}

		p = body + size + (size & 1);
	}
}

static bool clip_sound_load(const char *path)
{
	size_t n = 0;
	struct avi_audio a;
	bool ok = false;

	Xa2ClipUnload();

	uint8_t *file = FileReadAll(path, &n);

	if (!file)
		return false;

	if (n > 12 && !memcmp(file, "RIFF", 4) && !memcmp(file + 8, "AVI ", 4)) {
		memset(&a, 0, sizeof(a));
		a.stream = -1;
		avi_walk(file, file + n, &a, false);

		if (a.format_ok && a.bytes)
			ok = Xa2ClipLoad(a.pcm, a.bytes, a.rate, a.channels, a.bits);

		MemFree(a.pcm);
	}

	MemFree(file);

	return ok;
}

static void clip_sound_play(HWND video)
{
	if (g_clip_sound)
		Xa2ClipPlay(video ? (int32_t) MCIWndGetPosition(video) : 0);
}

static bool mov_sound_load(struct mov_player *player)
{
	uint8_t *pcm = NULL;
	int32_t bytes, rate, channels, bits;
	bool ok = false;

	Xa2ClipUnload();

	if (MovPlayerSound(player, &pcm, &bytes, &rate, &channels, &bits))
		ok = Xa2ClipLoad(pcm, bytes, rate, channels, bits);

	MemFree(pcm);

	return ok;
}

void mci_video_destroy(struct object *obj)
{
	struct mci_video *self = (struct mci_video *) obj;
	struct control *ctrl = &self->base;

	ControlRelinquishFocus(ctrl);
	EvtHandlerReleaseBinding(&ctrl->base.base, 2, g_mcivideo_handler_table);

	if (self->is_playing)
		mci_video_stop(self);

	Xa2ClipUnload();
	g_clip_sound = false;

	if (self->mov) {
		RemovePropA((HWND) ctrl->hwnd, "CCLO");
		SetWindowLongPtrA((HWND) ctrl->hwnd, GWLP_WNDPROC, (LONG_PTR) ctrl->orig_wndproc);
		MovPlayerDestroy(&self->mov);
		ctrl->hwnd = NULL;
	}

	if (ctrl->hwnd) {
		HWND hwnd = (HWND) ctrl->hwnd;
		RemovePropA(hwnd, "CCLO");
		RemovePropA(hwnd, "CCHI");
		g_mci_id = MCIWndGetDeviceID(hwnd);
		SendMessageA(hwnd, WM_CLOSE, 0, 0);
		MCIWndGetError(hwnd, g_mci_error_text, 0x80);
		mciSendCommandA(g_mci_id, MCI_CLOSE, 0, 0);
		ctrl->hwnd = NULL;
	}

	if (self->probe)
		VfwProbeDestroy(&self->probe);

	ObjectDelete(self->path);

	obj->ops = &CONTROL_OPS_TABLE.base.base.base;
	control_destroy(obj);
}

void mci_video_stop(struct mci_video *self)
{
	if (self->mov) {
		MovPlayerPause(self->mov);

	} else if (self->base.hwnd) {
		MCIWndStop((HWND) self->base.hwnd);
		MCIWndGetError((HWND) self->base.hwnd, g_mci_error_text, 0x80);
	}

	Xa2ClipStop();
	self->is_playing = false;
}

void mci_video_pause(struct mci_video *self)
{
	if (self->mov) {
		MovPlayerPause(self->mov);

	} else if (self->base.hwnd) {
		MCIWndPause((HWND) self->base.hwnd);
		MCIWndGetError((HWND) self->base.hwnd, g_mci_error_text, 0x80);
	}

	Xa2ClipStop();
}

void mci_video_play(struct mci_video *self)
{
	HWND hwnd;
	LRESULT error = 1;
	bool paused = true;

	if (self->mov) {
		MovPlayerPlay(self->mov);

		if (g_clip_sound)
			Xa2ClipPlay(MovPlayerPosition(self->mov));

	} else {
		hwnd = (HWND) self->base.hwnd;

		if (hwnd && (error = MCIWndPlay(hwnd)) != 0) {
			MCIWndGetError(hwnd, g_mci_error_text, 0x80);
			paused = false;
		}

		if (hwnd && !error)
			clip_sound_play(hwnd);
	}

	if (!self->is_playing) {
		self->is_playing = paused;
		EvtHandlerAddIdleHandler(&self->base.base.base, true, 0);
	}
}

void mci_video_set_volume(struct mci_video *self, int32_t volume)
{
	if (g_clip_sound) {
		Xa2ClipVolume(volume);
		volume = 0;
	}

	if (self->base.hwnd && !self->mov) {
		MCIWndSetVolume((HWND) self->base.hwnd, volume);
		MCIWndGetError((HWND) self->base.hwnd, g_mci_error_text, 0x80);
	}
}

void mci_video_draw_children(struct view *view, struct screen_dc *dc, const struct rect *invalid_rect)
{
	struct mci_video *self = (struct mci_video *) view;
	HWND hwnd = (HWND) self->base.hwnd;

	(void) dc;
	(void) invalid_rect;

	if (ViewIsVisible(view) && hwnd && self->autoplay) {
		if (self->mov) {
			MovPlayerPlay(self->mov);

			if (g_clip_sound)
				Xa2ClipPlay(MovPlayerPosition(self->mov));

		} else {
			MCIWndPlay(hwnd);

			if (!MCIWndGetError(hwnd, g_mci_error_text, 0x80))
				clip_sound_play(hwnd);
		}

		self->autoplay = false;
	}
}

void mci_video_on_event(struct evt_handler *handler, struct cmd_event *event)
{
	struct mci_video *self = (struct mci_video *) handler;

	if (!self->base.hwnd) {
		event->handled = true;
		return;
	}

	switch (event->command) {
		case CMD_MOUSE:
			event->handled = true;
			break;

		case CMD_VIDEO_CTRL:
			switch (event->param1) {
				case 0:
					MCI_VIDEO_OPS(self)->Play(self);
					break;

				case 1:
					MCI_VIDEO_OPS(self)->Stop(self);
					break;

				case 2:
					MCI_VIDEO_OPS(self)->Pause(self);
					break;

				case 3:
					MCI_VIDEO_OPS(self)->SetVolume(self, (int32_t) event->param2);
					break;
			}

			break;

		default:
			control_on_event(handler, event);
			break;
	}
}

void mci_video_on_idle(struct evt_handler *handler, void *task)
{
	struct mci_video *self = (struct mci_video *) handler;
	int32_t status = MCI_MODE_STOP;

	(void) task;

	if (self->mov) {
		MovPlayerTick(self->mov);
		status = RpQuery(RPQ_VIDEO_MODE, MovPlayerPlaying(self->mov) ? MCI_MODE_PLAY : MCI_MODE_STOP);

	} else if (self->probe && self->base.hwnd) {
		HWND hwnd = (HWND) self->base.hwnd;
		status = RpQuery(RPQ_VIDEO_MODE, (int32_t) MCIWndGetMode(hwnd, NULL, 0));
		MCIWndGetError(hwnd, g_mci_error_text, 0x80);
	}

	if (status != MCI_MODE_PLAY && status != MCI_MODE_SEEK)
		self->is_playing = false;

	if (!self->is_playing) {
		struct cmd_event *cmd = CmdEventCreateSpecsFlag(CMD_MEDIA_DONE, &self->base.base.base.eh_event,
			&self->base.c_event, false);
		cmd->param1 = self->ctrl_id;
		EvtHandlerRemoveIdleHandler(handler, true);
		DispatchGlobalEvent(cmd);
	}
}

static void place_and_subclass(struct mci_video *self, const struct rect *bounds)
{
	HWND hwnd = (HWND) self->base.hwnd;

	SetWindowPos(hwnd, 0, bounds->left, bounds->top, bounds->right - bounds->left, bounds->bottom - bounds->top,
		SWP_NOZORDER | SWP_NOACTIVATE);
	ScalerChild(hwnd, bounds->left, bounds->top, bounds->right - bounds->left, bounds->bottom - bounds->top, NULL);
	SetPropA(hwnd, "CCLO", (HANDLE) self);
	self->base.orig_wndproc = (void *) SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR) mci_video_subclass_wndproc);
	g_mcivideo_prevproc = (WNDPROC) self->base.orig_wndproc;
}

void mci_video_create_or_destroy(struct control *ctrl, struct window *desc)
{
	struct mci_video *self = (struct mci_video *) ctrl;

	if (!ctrl->owns_window)
		return;

	if (desc) {
		struct rect bounds;

		if (!ViewGetWinDc(&ctrl->base))
			return;

		if (self->is_mov && !self->mov) {
			char vfw_setting[16];

			IniGetString("game", "vfw", "on", vfw_setting, sizeof(vfw_setting));

			if (strcmp(vfw_setting, "on"))
				return;

			bounds = VIEW_OPS(ctrl)->GetBounds(&ctrl->base);
			struct cstr *file_path = PathCopyPath(self->path);
			struct app *app = GetApp();
			struct path *resource_path = APP_OPS(app)->GetResourcePath(app);
			struct cstr *full_path = PathCopyPath(resource_path);
			ObjectDelete(resource_path);
			CStrAppendStr(full_path, file_path);
			CStrDestroy(&file_path);
			self->mov = MovPlayerCreate(CStrGet(full_path), desc->hwnd, g_hinstance);
#ifdef OTII_TEST
			if (!self->mov)
				TCheck(false, "the movie opens", __FILE__, __LINE__, "%s", CStrGet(full_path));
#endif
			CStrDestroy(&full_path);

			if (!self->mov)
				return;

			ctrl->hwnd = MovPlayerHwnd(self->mov);
			g_clip_sound = mov_sound_load(self->mov);
			place_and_subclass(self, &bounds);

			return;
		}

		self->probe = VfwProbeCreate();
		VfwProbeCheck(self->probe);

		if (!VfwProbeIsAvail(self->probe)) {
			VfwProbeDestroy(&self->probe);
			return;
		}

		bounds = VIEW_OPS(ctrl)->GetBounds(&ctrl->base);
		struct cstr *file_path = PathCopyPath(self->path);
		struct app *app = GetApp();
		struct path *resource_path = APP_OPS(app)->GetResourcePath(app);
		struct cstr *full_path = PathCopyPath(resource_path);
		ObjectDelete(resource_path);
		CStrAppendStr(full_path, file_path);
		CStrDestroy(&file_path);

		HWND hwnd = MCIWndCreateA((HWND) desc->hwnd, (HINSTANCE) g_hinstance, WS_CHILD | WS_VISIBLE |
			MCIWNDF_NOPLAYBAR | MCIWNDF_NOAUTOSIZEWINDOW | MCIWNDF_NOERRORDLG, NULL);
		ctrl->hwnd = hwnd;
		MCIWndGetError(hwnd, g_mci_error_text, 0x80);
		SendMessageA(hwnd, MCIWNDM_OPENA, 0, (LPARAM) CStrGet(full_path));
		g_clip_sound = false;

		if (MCIWndGetError(hwnd, g_mci_error_text, 0x80)) {
#ifdef OTII_TEST
			TCheck(false, "the video opens", __FILE__, __LINE__, "%s: %s", CStrGet(full_path), g_mci_error_text);
#endif

		} else if (clip_sound_load(CStrGet(full_path))) {
			g_clip_sound = true;
			MCIWndUseTime(hwnd);
			MCIWndSendString(hwnd, "setaudio all off");
			MCIWndSetVolume(hwnd, 0);
		}

		CStrDestroy(&full_path);
		place_and_subclass(self, &bounds);
		SendMessageA(hwnd, MCIWNDM_SETPALETTE, (WPARAM) desc->windc->base.palette_obj->hpal, 0);

	} else {
		MCI_VIDEO_OPS(self)->Stop(self);
		Xa2ClipUnload();
		g_clip_sound = false;

		if (self->mov) {
			RemovePropA((HWND) ctrl->hwnd, "CCLO");
			SetWindowLongPtrA((HWND) ctrl->hwnd, GWLP_WNDPROC, (LONG_PTR) ctrl->orig_wndproc);
			MovPlayerDestroy(&self->mov);
			ctrl->hwnd = NULL;
		}

		if (ctrl->hwnd) {
			HWND hwnd = (HWND) ctrl->hwnd;
			DWORD param2 = 0;

			RemovePropA(hwnd, "CCLO");
			RemovePropA(hwnd, "CCHI");
			g_mci_id = (DWORD) SendMessageA(hwnd, MCIWNDM_GETDEVICEID, 0, 0);
			SendMessageA(hwnd, WM_CLOSE, 0, 0);
			MCIWndGetError(hwnd, g_mci_error_text, 0x80);
			mciSendCommandA(g_mci_id, MCI_CLOSE, MCI_WAIT, (DWORD_PTR) &param2);
			ctrl->hwnd = NULL;
		}
	}
}

struct view *CtrlFactoryMciVideo(struct ctrl_desc *desc)
{
	struct rect rect = desc->rect;

	struct mci_video *result = MciVideoCreate(desc->id, desc->event, &rect, desc->obj_type,
		(uint8_t) desc->style != 0, (uint8_t) desc->arg7 != 0);
	ControlApplyDescriptor(&result->base, desc);

	return &result->base.base;
}

void mci_video_post_event(struct mci_video *self, void *ctrl_hwnd, int32_t reserved, int32_t lparam_lo,
	int32_t lparam_hi, int32_t wparam)
{
	(void) ctrl_hwnd;
	(void) reserved;
	(void) lparam_lo;
	(void) lparam_hi;
	(void) wparam;

	struct cmd_event *cmd = CmdEventCreateSpecsFlag(self->base.command_id, &self->base.base.base.eh_event,
		&self->base.c_event, false);
	cmd->param1 = self->ctrl_id;
	cmd->param2 = self->base.command_p2;
	DispatchGlobalEvent(cmd);
}

intptr_t MciVideoHandleClick(struct mci_video *self, void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
	switch (msg) {
		case WM_LBUTTONDOWN:
			MCI_VIDEO_OPS(self)->PostEvent(self, self->base.hwnd, 0, (short) LOWORD(lparam), (short) HIWORD(lparam),
				(int32_t) wparam);
			return 0;
	}

	return CallWindowProcA((WNDPROC) self->base.orig_wndproc, (HWND) hwnd, msg, wparam, lparam);
}

static LRESULT CALLBACK mci_video_subclass_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct mci_video *video = (struct mci_video *) GetPropA(hwnd, "CCLO");

	if (video)
		return MciVideoHandleClick(video, hwnd, msg, wparam, lparam);

	return CallWindowProcA(g_mcivideo_prevproc, hwnd, msg, wparam, lparam);
}
