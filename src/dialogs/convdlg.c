#include "convdlg.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../app/app.h"
#include "../data/assets.h"
#include "../ui/button.h"
#include "../base/byteorder.h"
#include "../draw/canvas.h"
#include "../ui/clipview.h"
#include "../base/cstr.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "../game/game.h"
#include "../data/gfxdev.h"
#include "../base/log.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../draw/objbase.h"
#include "../app/otiiapp.h"
#include "../game/party.h"
#include "../ui/pictctrl.h"
#include "../draw/pictserver.h"
#include "../ui/picview.h"
#include "../base/rng.h"
#include "../data/rsrc.h"
#include "../screens/sceneassets.h"
#include "../draw/server.h"
#include "../draw/shape.h"
#include "../media/sndobj.h"
#include "../base/str.h"
#include "../data/strrsrc.h"
#include "../ui/textview.h"
#include "../base/timer.h"
#include "../ui/view.h"

enum {
	PORTRAIT_TABLE_STRLST = 2601,
};

enum {
	TREE_SPEAKER = 0,
	TREE_START_ALT = 1,
	TREE_START_SPANISH = 2,
	TREE_CAN_TRADE = 3,
	TREE_NODES = 4,
};

static bool g_conv_active;
static const uint8_t g_mouth_frame[40] = {
	0, 1, 2, 8, 3, 4, 5, 6, 7, 5, 8, 9, 9, 10, 11, 12, 13, 14, 25, 26,
	27, 15, 16, 17, 30, 18, 19, 20, 21, 16, 22, 23, 24, 25, 26, 27, 15, 28, 29, 24,
};

struct strrsrc *g_portrait_table = NULL;
static int32_t g_last_conv_shown = 0;
static struct point g_speaker_pos[200];
uint8_t g_slot_flags[1200];
static int32_t g_last_conv_id = 0;
static int32_t g_last_conv_node = 0;

const struct conv_dlg_ops CONV_DLG_OPS_TABLE = {
	{
		{
			{conv_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			conv_dlg_on_event,
			evt_handler_on_idle,
			evt_handler_on_idle_spare,
		},
		conv_dlg_begin,
		conv_dlg_accept,
	},
};

static void lipsync_tick_thunk(void *dlg);

static int8_t *tree_nodes(struct rsrc *tree)
{
	return (int8_t *) (RsrcData(tree) + TREE_NODES);
}

int32_t ConvGetSpeaker(int32_t conv_id)
{
	int32_t result = 0;
	int32_t index = 0;

	if (conv_id > 100000) {
		index = conv_id % 100;
		conv_id /= 100;
		struct strrsrc *lines = StrRsrcLoad(g_text_lib, conv_id + CONV_TEXT_BASE, true);

		if (!lines)
			return 1;

		const char *row = StrRsrcGetAt(lines, index);

		if (!row) {
			StrRsrcDestroy(&lines);
			return 1;
		}

		sscanf(row, "%" SCNd32 "\t", &result);
		StrRsrcDestroy(&lines);

	} else {
		struct rsrc *rsrc = AssetsGetElement(g_text_lib, "CONVR", conv_id, true);

		if (!rsrc)
			return 1;

		if (RsrcStorage(rsrc) == RSRC_TEXT)
			return 1;

		result = RsrcData(rsrc)[TREE_SPEAKER];
		RsrcDestroy(&rsrc);
	}

	return (int16_t) result;
}

int32_t ConvGetPortraitId(int32_t conv_id)
{
	if (!g_portrait_table)
		g_portrait_table = StrRsrcLoad(g_data_lib, PORTRAIT_TABLE_STRLST, true);

	int32_t index = ConvGetSpeaker(conv_id) - 1;
	const char *entry = StrRsrcGetAt(g_portrait_table, index);

	if (!entry)
		return 0;

	return (int16_t) atoi(entry);
}

static void rep_down(struct party *party)
{
	party->reputation = party->reputation - 1;

	if (party->reputation > 100)
		party->reputation = 100;

	if (party->reputation < -50)
		party->reputation = -50;
}

static void rep_up(struct party *party)
{
	party->reputation = party->reputation + 1;

	if (party->reputation > 100)
		party->reputation = 100;

	if (party->reputation < -50)
		party->reputation = -50;
}

static void size_rect(struct rect *dst, const struct rect *src)
{
	int32_t width = src->right - src->left;
	int32_t height = src->bottom - src->top;
	dst->top = 0;
	dst->left = 0;
	dst->right = width;
	dst->bottom = height;
}

static void norm_rect(struct rect *r)
{
	int32_t width = r->right - r->left;
	int32_t height = r->bottom - r->top;
	r->top = 0;
	r->left = 0;
	r->right = width;
	r->bottom = height;
}

static void set_rect(struct rect *r, int32_t top, int32_t left, int32_t bottom, int32_t right)
{
	r->top = top;
	r->left = left;
	r->right = right;
	r->bottom = bottom;
}

static void extend_rect(struct rect *rect, const struct point *extent)
{
	int32_t new_right = rect->left + extent->h;
	int32_t new_bottom = rect->top + extent->v;
	rect->right = new_right;
	rect->bottom = new_bottom;
}

static void move_rect_to(struct rect *rect, const struct point *anchor)
{
	int32_t width = rect->right - rect->left;
	int32_t height = rect->bottom - rect->top;
	int32_t new_right = anchor->h + width;
	int32_t new_bottom = anchor->v + height;
	rect->top = anchor->v;
	rect->left = anchor->h;
	rect->right = new_right;
	rect->bottom = new_bottom;
}

static void offset_rect(struct rect *r, int32_t dh, int32_t dv)
{
	r->left += dh;
	r->right += dh;
	r->top += dv;
	r->bottom += dv;
}

void ConvDlgInit(struct conv_dlg *self, int32_t conv_id, int32_t mode, int32_t region)
{
	ModalDialogInit(&self->base, 1, false, true);
	self->base.base.base.ops = &CONV_DLG_OPS_TABLE.base.base.base;

	struct rect text_rect;
	struct rect cel_rect;
	struct point pos;
	int32_t handlers[2];

	g_conv_active = false;
	self->strings = NULL;
	self->tree = NULL;
	self->speech = NULL;
	self->sprite = NULL;
	self->canvas_bg = NULL;
	self->node = -1;

	if (conv_id == g_last_conv_shown)
		rep_down(GetGameObj()->party);
	else
		rep_up(GetGameObj()->party);

	g_last_conv_shown = conv_id;

	if (conv_id > 100000) {
		self->node = conv_id % 100;
		conv_id /= 100;
		self->mode = 3;

	} else {
		self->mode = mode;
	}

	self->conv_id = (int16_t) conv_id;

	if (conv_id > 0 && conv_id < 1200)
		g_slot_flags[conv_id] = 1;

	if (self->mode == 3)
		ConvDlgLoadTree(self);
	else
		ConvDlgLoadStrings(self);

	if (!self->tree)
		return;

	const uint8_t *header = RsrcData(self->tree);
	int32_t speaker = header[TREE_SPEAKER];
	int32_t start_alt = (int8_t) header[TREE_START_ALT];
	int32_t start_spanish = (int8_t) header[TREE_START_SPANISH];

	if (!self->mode) {
		if (GetGameObj()->party->skills[SKILL_SPANISH] && start_spanish)
			self->node = start_spanish;
		else if (GetGameObj()->destination == DEST_SALT_LAKE && start_alt)
			self->node = start_alt;
		else
			self->node = 0;
	}

	self->can_trade = (int8_t) header[TREE_CAN_TRADE];

	self->base.title = CStrCreateFrom("Conversation");
	pos.h = 0;
	pos.v = 0;
	self->base.content = (struct view *) PicViewCreatePoint(0, &pos, 180);

	self->sprite = ServerCreateVirtual((struct server *) g_pict_server, speaker + 6000, true);

	if (!self->sprite) {
		self->sprite = ServerCreateVirtual((struct server *) g_pict_server, 6001, true);
		speaker = 1;
	}

	if (!g_speaker_pos[speaker].h && !g_speaker_pos[speaker].v) {
		self->origin = g_pict_server->pt;
		g_speaker_pos[speaker] = self->origin;

	} else {
		self->origin = g_speaker_pos[speaker];
	}

	struct obj_base *backdrop = ServerCreateVirtualPacked((struct server *) g_pict_server, RegionalAssetId2000(),
		region, true);
	struct canvas *frame = OBJ_BASE_OPS(backdrop)->GetRect(backdrop, &cel_rect);
	size_rect(&self->bg_rect, &cel_rect);
	self->canvas_bg = CanvasCreateRect(&self->bg_rect, 8, 255);
	CANVAS_OPS(self->canvas_bg)->StretchBlit(self->canvas_bg, frame, &cel_rect, &self->bg_rect, 0, &self->bg_rect);
	ReleaseDrawable((struct media_base *) backdrop, false);
	offset_rect(&self->bg_rect, 7, 8);
	SHAPE_OPS(self->canvas_bg)->SetRect((struct shape *) self->canvas_bg, &self->bg_rect);
	self->clip_bg = ClipViewCreate(0, &self->bg_rect, (struct shape *) self->canvas_bg, false);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) self->clip_bg);
	offset_rect(&self->bg_rect, -7, -8);

	frame = DRAWABLE_OPS(self->sprite)->GetRectAt(self->sprite, 0, &cel_rect);
	struct point bg_pos = RectTopLeft(&self->bg_rect);
	CANVAS_OPS(self->canvas_bg)->BlitImage(self->canvas_bg, frame, &cel_rect, &bg_pos, &self->bg_rect, false);
	DRAWABLE_OPS(self->sprite)->GetRectAt(self->sprite, 1, &cel_rect);
	norm_rect(&cel_rect);
	size_rect(&self->face_rect, &cel_rect);
	self->canvas_face = CanvasCreateRect(&self->face_rect, 8, 255);
	move_rect_to(&self->face_rect, &self->origin);
	CANVAS_OPS(self->canvas_face)->StretchBlit(self->canvas_face, self->canvas_bg, &self->face_rect, &cel_rect, 0,
		NULL);
	offset_rect(&self->face_rect, 7, 8);
	SHAPE_OPS(self->canvas_face)->SetRect((struct shape *) self->canvas_face, &self->face_rect);
	self->face_view = ClipViewCreate(0, &self->face_rect, (struct shape *) self->canvas_face, false);
	VIEW_OPS(self->clip_bg)->AddChild((struct view *) self->clip_bg, (struct view *) self->face_view);

	DRAWABLE_OPS(self->sprite)->GetSizeAt(self->sprite, &pos, 1);
	extend_rect(&self->face_rect, &pos);
	move_rect_to(&self->face_rect, &self->origin);

	set_rect(&text_rect, 1, self->bg_rect.right + 30, 112, self->bg_rect.right + 270);
	struct cstr *body_text = CStrCreate();
	self->body_view = TextViewCreate(0, &text_rect, body_text, 4, 0, 1, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) self->body_view);

	int32_t button_index;
	struct cstr *title;

	for (button_index = 0; button_index < 6; button_index++) {
		pos.h = self->bg_rect.right + 35;
		pos.v = 15 * (2 * (button_index + 1)) + 82;
		title = CStrCreateFrom("Title");
		self->response_btn[button_index] = PictCtrlCreate(button_index, &self->base.base.eh_event, &pos, 901, 0, 0,
			title, 3, 1, 1);
		VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) self->response_btn[button_index]);
	}

	pos.h = self->bg_rect.right + 35;
	pos.v = 120;
	self->continue_id = button_index;
	title = CStrCreateFrom("OK");
	self->continue_btn = PictCtrlCreate(self->continue_id, &self->base.base.eh_event, &pos, 901, 0, 0, title, 3, 1, 1);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) self->continue_btn);

	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;
	EvtHandlerBindHandler(&self->base.base, 2, handlers);

	DRAWABLE_OPS(self->sprite)->GetSizeAt(self->sprite, &pos, 1);
	extend_rect(&self->bg_rect, &pos);
	move_rect_to(&self->bg_rect, &self->origin);
}

struct conv_dlg *ConvDlgCreate(int32_t conv_id, int32_t mode, int32_t region)
{
	struct conv_dlg *self = MemAlloc(1, sizeof(struct conv_dlg));
	ConvDlgInit(self, conv_id, mode, region);

	return self;
}

void ConvDlgDestroy(struct conv_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void conv_dlg_destroy(struct object *obj)
{
	struct conv_dlg *self = (struct conv_dlg *) obj;

	if (self->strings)
		StrRsrcDestroy(&self->strings);

	if (self->tree)
		RsrcDestroy(&self->tree);

	if (self->sprite)
		ReleaseDrawable((struct media_base *) self->sprite, false);

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

int32_t ConvDlgGetRsrcId(struct conv_dlg *self)
{
	return RsrcData(self->tree)[TREE_SPEAKER] + 6000;
}

void ConvDlgBuildTradeList(struct conv_dlg *self, struct strrsrc *months, struct strrsrc *costs)
{
	char cash_buf[52];

	struct strrsrc *rebuilt = StrRsrcCreate();
	int32_t count = StrRsrcCount(self->strings);
	StrPrintf(cash_buf, "(You have $%5.2f)", GetGameObj()->party->cash);
	struct cstr *cash_text = CStrCreateFrom(cash_buf);
	struct rect header_rect = VIEW_OPS(self->response_btn[3])->GetBounds((struct view *) self->response_btn[3]);
	struct text_view *header = TextViewCreate(-2, &header_rect, cash_text, 3, 1, 1, 0, 0, 0, 0, 255, 0, 1);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) header);

	if (count) {
		for (int32_t line_index = 0; line_index < count; line_index++) {
			struct cstr *line = StrRsrcGetCStr(self->strings, line_index);
			const char *month = StrRsrcGetAt(months, line_index);
			const char *cost = StrRsrcGetAt(costs, line_index);
			CStrReplaceAll(line, "[month]", month);
			CStrReplaceAll(line, "[cost]", cost);
			StrRsrcAppend(rebuilt, CStrGet(line));
			CStrDestroy(&line);
		}
	}

	StrRsrcDestroy(&self->strings);
	self->strings = rebuilt;

	int8_t *nodes = tree_nodes(self->tree);
	count = StrRsrcCount(self->strings);

	if (count) {
		for (int32_t string_index = 0; string_index < count; string_index++) {
			const char *text = StrRsrcGetAt(self->strings, string_index);

			if (strstr(text, "-")) {
				for (int32_t link_index = 0; link_index < 6; link_index++) {
					int8_t *link = &nodes[6 * self->node];

					if (!link[link_index])
						break;

					if (link[link_index] == string_index) {
						while (link[link_index]) {
							link[link_index] = link[link_index + 1];
							link_index++;
						}
					}
				}
			}
		}
	}

	self->can_trade = 0;
}

void ConvDlgLoadStrings(struct conv_dlg *self)
{
	self->tree = AssetsGetElement(g_text_lib, "CONVR", self->conv_id, true);

	if (!self->tree)
		return;

	if (RsrcStorage(self->tree) == RSRC_TEXT)
		LogError(LOG_ERROR, "Attempt load a raw resource");
	else
		self->strings = StrRsrcLoad(g_text_lib, self->conv_id + CONV_TEXT_BASE, true);

	if (self->strings)
		StrRsrcRemapHighChars(self->strings, 0);
}

void ConvDlgLoadTree(struct conv_dlg *self)
{
	struct strrsrc *lines = StrRsrcLoad(g_text_lib, self->conv_id + CONV_TEXT_BASE, true);
	self->strings = StrRsrcCreate();
	self->tree = RsrcCreate("CONVR", (int16_t) self->conv_id, 6 * StrRsrcCount(lines) + 10, RSRC_NATIVE);

	if (self->node == -1) {
		for (;;) {
			self->node = RandRangeAvg(1, StrRsrcCount(lines), 1) - 1;

			if (!(self->conv_id == g_last_conv_id && self->node == g_last_conv_node))
				break;
		}
	}

	g_last_conv_id = (int16_t) self->conv_id;
	g_last_conv_node = (int16_t) self->node;

	uint8_t *record = RsrcData(self->tree);
	int8_t *nodes = tree_nodes(self->tree);
	record[TREE_START_ALT] = 0;
	record[TREE_START_SPANISH] = 0;
	record[TREE_CAN_TRADE] = 0;
	int32_t count = StrRsrcCount(lines);

	if (count) {
		for (int32_t line_index = 0; line_index < count; line_index++) {
			const char *line = StrRsrcGetAt(lines, line_index);

			if (self->node == line_index)
				record[TREE_SPEAKER] = (uint8_t) atoi(line);

			nodes[6 * line_index + 0] = -1;
			nodes[6 * line_index + 1] = 0;
			line = strchr(line, 9) + 1;
			StrRsrcAppend(self->strings, line);
		}
	}

	StrRsrcRemapHighChars(self->strings, 0);
	StrRsrcDestroy(&lines);
}

void conv_dlg_begin(struct modal_dialog *dlg, intptr_t param)
{
	(void) param;
	struct conv_dlg *self = (struct conv_dlg *) dlg;

	OtiiAppPauseMusic((struct otii_app *) g_app);
	OtiiAppPauseSfx((struct otii_app *) g_app);
	ConvDlgShowNode(self);
}

void conv_dlg_accept(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void conv_dlg_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct conv_dlg *self = (struct conv_dlg *) handler;

	int32_t button_id = (int16_t) evt->spec_b.id;

	if (self->speech) {
		ConvDlgLipsyncStop(self);
		self->speech = NULL;
		OtiiAppStopSpeech((struct otii_app *) g_app);
	}

	if (evt->command == CMD_CONTROL_ACTION) {
		if (self->continue_id == button_id) {
			self->base.result = DLG_CANCEL;

		} else {
			if (ConvDlgCountResponses(self, self->node) <= button_id) {
				self->base.result = self->can_trade;

			} else {
				int32_t link = ConvDlgGetResponseLink(self, self->node, button_id);
				self->node = ConvDlgGetResponseLink(self, link, 0);

				if (self->node >= 0) {
					ConvDlgShowNode(self);

				} else {
					if (++self->node == -1)
						self->base.result = DLG_CANCEL;
					else
						self->base.result = (int16_t) -self->node;
				}
			}
		}

		evt->handled = true;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW) {
			self->base.result = DLG_CANCEL;
			evt->handled = true;
		}
	}

	if (self->base.result != DLG_RUNNING) {
		OtiiAppResumeMusic((struct otii_app *) g_app);
		OtiiAppResumeSfx((struct otii_app *) g_app);
	}
}

int32_t ConvDlgCountResponses(struct conv_dlg *self, int32_t node)
{
	const int8_t *nodes = tree_nodes(self->tree);
	int32_t count;

	for (count = 0; count < 6; count++)
		if (!nodes[6 * node + count])
			break;

	return count;
}

int32_t ConvDlgGetResponseLink(struct conv_dlg *self, int32_t node, int32_t resp)
{
	const int8_t *nodes = tree_nodes(self->tree);

	return nodes[6 * node + resp];
}

void ConvDlgShowNode(struct conv_dlg *self)
{
	g_conv_active = true;
	struct cstr *label = StrRsrcGetCStr(self->strings, (int16_t) self->node);
	TextViewSetSource(self->body_view, label);
	int32_t count = ConvDlgCountResponses(self, self->node);

	if (count == 1 && ConvDlgGetResponseLink(self, self->node, 0) < 0) {
		count = 0;
		VIEW_OPS(self->continue_btn)->Show((struct view *) self->continue_btn);

	} else {
		VIEW_OPS(self->continue_btn)->Hide((struct view *) self->continue_btn, false, false);
	}

	int32_t shown = count;
	int32_t trade_slot;

	if (self->can_trade && shown < 4) {
		trade_slot = shown;
		++shown;

	} else {
		trade_slot = -1;
	}

	for (int32_t button_index = 0; button_index < 6; button_index++) {
		struct view *btn = (struct view *) self->response_btn[button_index];

		if (button_index >= shown) {
			VIEW_OPS(btn)->Hide(btn, false, false);

		} else {
			if (button_index == trade_slot) {
				if (self->conv_id == 349 || self->conv_id == 893)
					label = CStrCreateFrom("Offer something.");
				else
					label = CStrCreateFrom("Would you like to trade?");

			} else {
				int32_t link = ConvDlgGetResponseLink(self, self->node, button_index);
				label = StrRsrcGetCStr(self->strings, (int16_t) link);
			}

			ButtonSetData((struct button *) btn, label, false);
			VIEW_OPS(btn)->Show(btn);
		}
	}

	int32_t speech_id = (int16_t) (self->node + 20 * self->conv_id + 1);

	if (self->mode == 3 && self->node > 0)
		speech_id = (int16_t) (speech_id + 1);

	struct otii_app *app = (struct otii_app *) g_app;
	OtiiAppPlaySpeech(app, speech_id, false, false);
	self->speech = app->speech_stream;

	if (self->speech) {
		SND_OBJ_OPS(self->speech)->Start(self->speech);
		ConvDlgLipsyncStart(self, speech_id);
	}
}

void ConvDlgLipsyncStart(struct conv_dlg *self, int32_t speech_id)
{
	self->lipsync = AssetsGetElement(g_data_lib, "markr", speech_id, false);

	if (!self->lipsync)
		return;

	RsrcLoadAndUnpack(self->lipsync);
	const uint8_t *base = RsrcData(self->lipsync);
	self->marker_count = base;
	self->markers = base + 2;
	self->marker_index = 0;
	self->speech_primed = false;
	self->tick_timer = TimerCreate(55, lipsync_tick_thunk, self);
}

void ConvDlgLipsyncStop(struct conv_dlg *self)
{
	if (!self->lipsync)
		return;

	TimerDestroy(&self->tick_timer);
	RsrcDestroy(&self->lipsync);
	self->marker_count = NULL;
	self->markers = NULL;
}

static void lipsync_tick_thunk(void *dlg)
{
	ConvDlgLipsyncTick((struct conv_dlg *) dlg);
}

static int32_t marker_at(struct conv_dlg *self, int32_t i)
{
	intptr_t offset = (intptr_t) 2 + (intptr_t) 4 * i;

	if (i < 0 || offset + 4 > RsrcSize(self->lipsync))
		return INT32_MAX;

	return ReadLe32(self->markers + 4 * i);
}

static int32_t marker_position(struct conv_dlg *self, int32_t n)
{
	return marker_at(self, 2 * n);
}

static int32_t marker_shape(struct conv_dlg *self, int32_t n)
{
	return marker_at(self, 2 * n + 1);
}

void ConvDlgLipsyncTick(struct conv_dlg *self)
{
	bool changed = false;

	if (!self->speech)
		return;

	if (g_conv_active) {
		g_conv_active = false;
		SND_OBJ_OPS(self->speech)->Finish(self->speech);
	}

	if (!((struct otii_app *) g_app)->speech_stream) {
		ConvDlgLipsyncStop(self);
		self->speech = NULL;
		return;
	}

	if (!self->speech_primed) {
		OtiiAppLipsyncAdvanceSpeech((struct otii_app *) g_app);
		self->speech_primed = true;
		return;
	}

	int32_t marker_count = ReadLe16(self->marker_count);

	if (SND_OBJ_OPS(self->speech)->GetStatus(self->speech) != 1) {
		self->marker_index = marker_count;
		changed = true;

	} else {
		int32_t pos = SND_OBJ_OPS(self->speech)->GetPosition(self->speech);

		while (marker_position(self, self->marker_index) <= pos) {
			changed = true;
			++self->marker_index;

			if (marker_count == self->marker_index)
				break;
		}
	}

	if (marker_count <= self->marker_index)
		TimerDestroy(&self->tick_timer);

	if (changed && self->marker_index > 0)
		ConvDlgSetMouth(self, (uint32_t) marker_shape(self, self->marker_index - 1));
}

void ConvDlgSetMouth(struct conv_dlg *self, uint32_t shape)
{
	struct rect bounds;

	if ((int32_t) shape < 0 || (int32_t) shape >= (int32_t) sizeof(g_mouth_frame))
		return;

	int32_t idx = (int8_t) g_mouth_frame[shape] + 1;
	struct canvas *cel = DRAWABLE_OPS(self->sprite)->GetRectAt(self->sprite, idx, &bounds);
	struct point pos = RectTopLeft(&bounds);
	CANVAS_OPS(self->canvas_face)->BlitImage(self->canvas_face, cel, &bounds, &pos, &bounds, false);
	struct view *face = (struct view *) self->face_view;
	VIEW_OPS(face)->DrawChildren(face, ViewGetWinDc(face), &self->face_rect);
}

struct rsrc *ConvDlgParseConvTable(struct conv_dlg *self, struct rsrc *src)
{
	int32_t speaker;
	int32_t start_alt;
	int32_t start_spanish;
	int32_t can_trade;
	int32_t id;
	char field1[512];
	char text_buf[512];
	char links[32];

	self->strings = StrRsrcCreate();
	int32_t capacity = 10;
	char *line = (char *) RsrcData(src);
	struct rsrc *tbl = RsrcCreate("CONVR", RsrcId(src), 6 * capacity + 10, RSRC_NATIVE);
	uint8_t *rec = RsrcData(tbl);
	int8_t *body = (int8_t *) (rec + TREE_NODES);
	int32_t count = 0;
	bool done = false;
	bool have_header = false;

	while (!done) {
		char *i;

		for (i = line; *i != 13 && *i; ++i)
			continue;

		if (!*i)
			done = true;

		*i = 9;

		if (!have_header) {
			sscanf(line, " %" SCNd32 " %" SCNd32 " %" SCNd32 " %" SCNd32, &speaker, &start_alt, &start_spanish,
				&can_trade);
			rec[TREE_SPEAKER] = (uint8_t) speaker;
			rec[TREE_START_ALT] = (uint8_t) (start_alt - 1);
			rec[TREE_START_SPANISH] = (uint8_t) (start_spanish - 1);
			rec[TREE_CAN_TRADE] = (uint8_t) can_trade;
			have_header = true;

		} else {
			if (capacity == count) {
				++capacity;
				RsrcResize(tbl, 6 * capacity + 10);
				rec = RsrcData(tbl);
				body = (int8_t *) (rec + TREE_NODES);
			}

			int32_t ret = sscanf(line, "%" SCNd32 " %511[^\t] %31[^\t] %511[^\t]", &id, field1, links, text_buf);

			if (ret < 4)
				break;

			int32_t n = 0;
			char *scan = strchr(links, ',');

			if (!scan) {
				body[6 * count + n] = (int8_t) (atoi(links) - 1);
				n++;

			} else {
				int32_t remaining = atoi(links);
				scan++;

				while (remaining--) {
					while (!isdigit((unsigned char) *scan))
						++scan;

					body[6 * count + n] = (int8_t) (atoi(scan) - 1);
					n++;
					scan = strchr(scan, ' ');
				}
			}

			body[6 * count + n] = 0;
			StrRsrcAppend(self->strings, text_buf);
			++count;
		}

		line = i + 1;
	}

	if (capacity > count)
		RsrcResize(tbl, 6 * count + 10);

	return tbl;
}
