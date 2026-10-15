#include "about.h"

#include <stddef.h>

#include "../app/app.h"
#include "../ui/cliplabelview.h"
#include "../base/cstr.h"
#include "../ui/dlgwin.h"
#include "../draw/drawable.h"
#include "../app/event.h"
#include "../ui/frametemplate.h"
#include "../data/gfxdev.h"
#include "../ui/labeledcontrol.h"
#include "../app/license.h"
#include "../base/log.h"
#include "../draw/mediabase.h"
#include "../base/memory.h"
#include "../draw/offscreendc.h"
#include "../draw/palette.h"
#include "../draw/pictserver.h"
#include "../draw/screendc.h"
#include "../ui/scrollview.h"
#include "../draw/server.h"
#include "../data/strrsrc.h"
#include "../base/sysinfo.h"
#include "../ui/view.h"
#include "../draw/windc.h"
#include "../ui/window.h"

const struct modal_dialog_ops ABOUT_OPS_TABLE = {
	{
		{about_destroy},
		evt_handler_is_handled,
		evt_handler_set_handled_for,
		evt_handler_set_handled,
		evt_handler_clear_handled_for,
		evt_handler_clear_handled,
		about_on_event,
		evt_handler_on_idle,
		evt_handler_on_idle_spare,
	},
	modal_dialog_begin,
	modal_dialog_accept,
};

void AboutInit(struct about *self)
{
	ModalDialogInit(&self->base, 10, false, true);
	self->base.base.base.ops = &ABOUT_OPS_TABLE.base.base;

	int32_t cmd_ids[2] = {CMD_CONTROL_ACTION, CMD_CLOSE_WINDOW};
	self->strings = StrRsrcLoad(g_text_lib, 10, true);

	if (!self->strings || StrRsrcId(self->strings) != 10)
		LogError(LOG_ERROR, "About strings not found");

	self->details_shown = 0;
	self->base.title = StrRsrcGetCStr(self->strings, 0);

	struct frame_template *ft = FrameTemplateCreate(g_text_lib, 10);
	struct app *app = GetApp();

	if (APP_OPS(app)->GetMode(app) == LICENSE_DEMO)
		FrameTemplateSetItemValue(ft, 0, 12);

	self->base.content = FrameTemplateBuild(ft, NULL, &self->base.base.eh_event, NULL, NULL);
	FrameTemplateDestroy(&ft);
	AboutFillSysInfo(self, self->base.content);

	struct rect bounds;
	bounds.top = 16;
	bounds.left = 235;
	bounds.right = 432;
	bounds.bottom = 187;
	AboutBuildSysInfoDisplay(self, &bounds, self->base.content);
	EvtHandlerBindHandler(&self->base.base, 2, cmd_ids);
}

struct about *AboutCreate(void)
{
	struct about *self = MemAlloc(1, sizeof(struct about));
	AboutInit(self);

	return self;
}

void AboutDestroy(struct about **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void about_destroy(struct object *obj)
{
	struct about *self = (struct about *) obj;

	StrRsrcDestroy(&self->strings);

	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

struct view *AboutBuildAboutLogo(struct about *self, const struct rect *view_rect)
{
	(void) self;
	struct view *result = NULL;
	struct obj_base *drawable = ServerCreateVirtual((struct server *) g_pict_server, 11, true);

	if (drawable) {
		struct point logo_size;
		OBJ_BASE_OPS(drawable)->GetSize(drawable, &logo_size);

		struct rect logo_rect = {0, 0, 0, 0};
		logo_rect.bottom = logo_rect.top + logo_size.v;
		logo_rect.right = logo_rect.left + logo_size.h;
		logo_rect.right = logo_rect.right - logo_rect.left;
		logo_rect.bottom = logo_rect.bottom - logo_rect.top;
		logo_rect.top = 0;
		logo_rect.left = logo_rect.top;

		struct offscreen_dc *offscreen = OffscreenDcCreateRect(&logo_rect, g_palette, 8);
		struct point origin = {0, 0};
		DRAWABLE_OPS(drawable)->Draw((struct drawable *) drawable, &offscreen->base, &origin, 0, 0);
		ReleaseDrawable((struct media_base *) drawable, false);
		result = (struct view *) ScrollViewCreate(15, view_rect, (struct canvas *) offscreen, &logo_rect, true);
	}

	return result;
}

void AboutToggleDetails(struct about *self)
{
	struct view *content = self->base.content;
	struct clip_label_view *label = (struct clip_label_view *) ViewFindByTypeId(content, EVCLS_VIEW, 13);
	struct cstr *text = StrRsrcGetCStr(self->strings, self->details_shown + 1);
	struct rect bounds = VIEW_OPS(label)->GetBounds((struct view *) label);
	ScreenDcFillRect(&self->base.dlg_win->base.windc->base, &bounds, 0);
	ClipLabelViewSetLabelAttr(label, text);

	struct view *v;

	if (self->details_shown == 1) {
		AboutPostToggleEvent(self, false);

		v = ViewFindByTypeId(content, EVCLS_SCROLLVIEW, 15);

		if (v)
			VIEW_OPS(v)->Hide(v, true, true);

		v = ViewFindByTypeId(content, EVCLS_VIEW, 16);
		VIEW_OPS(v)->Show(v);
		v = ViewFindByTypeId(content, EVCLS_VIEW, 17);
		VIEW_OPS(v)->Show(v);

	} else {
		v = ViewFindByTypeId(content, EVCLS_VIEW, 16);
		VIEW_OPS(v)->Hide(v, false, true);
		v = ViewFindByTypeId(content, EVCLS_VIEW, 17);
		VIEW_OPS(v)->Hide(v, false, true);

		v = ViewFindByTypeId(content, EVCLS_SCROLLVIEW, 15);

		if (v)
			VIEW_OPS(v)->Show(v);

		AboutPostToggleEvent(self, true);
	}
}

void AboutPostToggleEvent(struct about *self, bool which)
{
	struct event spec = EventMake(EVCLS_SCROLLVIEW, 15, -1);
	struct cmd_event *cmd = CmdEventCreateSpecsFlag(CMD_SCROLLVIEW_CTRL, &self->base.base.eh_event, &spec, false);
	cmd->param1 = which ? 0 : 1;
	cmd->param2 = 3;
	DispatchGlobalEvent(cmd);
}

void about_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct about *self = (struct about *) handler;

	if (evt->command == CMD_CONTROL_ACTION) {
		int32_t id = evt->spec_b.id;

		switch (id) {
			case 2:
				self->base.result = DLG_OK;
				break;
			case 3:
				switch (self->details_shown) {
					case 1:
						self->details_shown = 0;
						break;
					case 0:
						self->details_shown = 1;
						break;
				}

				AboutToggleDetails(self);
				break;
			case 12: {
				LicenseInitialize(1);

				if (LicenseGetType() != LICENSE_DEMO) {
					struct view *view = ViewFindByTypeId(self->base.content, 20007, 12);
					VIEW_OPS(self->base.content)->RemoveChild(self->base.content, &view->base.eh_event);
					ObjectDelete(view);
					AboutFillSysInfo(self, self->base.dlg_win->base.root_view);
				}

				struct window *win = &self->base.dlg_win->base;
				WINDOW_OPS(win)->Draw(win, NULL);
				break;
			}
		}

		evt->handled = true;

	} else {
		if (evt->command == CMD_CLOSE_WINDOW) {
			self->base.result = DLG_CANCEL;
			evt->handled = true;
		}
	}
}

void AboutFillSysInfo(struct about *self, struct view *content)
{
	(void) self;
	struct event spec = EventMake(EVCLS_LABELED_CONTROL, -1, -1);
	struct product_strings info;
	LicenseGetLicenseeInformation(&info);
	EventSetId(&spec, 8);
	struct view *ctrl = ViewFindByName(content, spec);

	if (ctrl) {
		LabeledControlSetTitle((struct labeled_control *) ctrl, CStrCreateFrom(info.not_implemented_0));
		spec.code = EVCLS_VIEW;
		EventSetId(&spec, 9);
		ctrl = ViewFindByName(content, spec);
		ClipLabelViewSetLabelAttr((struct clip_label_view *) ctrl, CStrCreateFrom(info.name_20));
		EventSetId(&spec, 10);
		ctrl = ViewFindByName(content, spec);
		ClipLabelViewSetLabelAttr((struct clip_label_view *) ctrl, CStrCreateFrom(info.licensee_40));
		EventSetId(&spec, 11);
		ctrl = ViewFindByName(content, spec);
		ClipLabelViewSetLabelAttr((struct clip_label_view *) ctrl, CStrCreateFrom(info.not_implemented_60));
	}
}

static void rect_left_half(struct rect *rect)
{
	int32_t mid = ((rect->right - rect->left) - 2) / 2;
	rect->right = rect->left + mid;
}

static void rect_next_right(struct rect *rect)
{
	int32_t top = rect->top;
	int32_t new_left = rect->right + 2;
	int32_t width = rect->right - rect->left;
	int32_t height = rect->bottom - rect->top;
	rect->top = top;
	rect->left = new_left;
	rect->right = new_left + width;
	rect->bottom = top + height;
}

void AboutBuildSysInfoDisplay(struct about *self, struct rect *rect, struct view *content)
{
	struct sys_info sysinfo;
	SysInfoInit(&sysinfo);

	struct view *view = AboutBuildAboutLogo(self, rect);

	if (view)
		VIEW_OPS(content)->AddChild(content, view);

	struct strrsrc *left_list;
	struct strrsrc *right_list;
	SysInfoBuildLists(&sysinfo, &left_list, &right_list);
	rect_left_half(rect);
	view = (struct view *) ClipLabelViewCreateStrs(16, rect, left_list, 1, 2, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, view);
	VIEW_OPS(view)->Hide(view, false, true);
	rect_next_right(rect);
	view = (struct view *) ClipLabelViewCreateStrs(17, rect, right_list, 1, 0, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(content)->AddChild(content, view);
	VIEW_OPS(view)->Hide(view, false, true);

	sys_info_destroy((struct object *) &sysinfo);
}
