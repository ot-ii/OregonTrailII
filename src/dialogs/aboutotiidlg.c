#include "aboutotiidlg.h"

#include <stddef.h>

#include "../ui/button.h"
#include "../ui/cliplabelview.h"
#include "../base/cstr.h"
#include "../app/event.h"
#include "../data/gfxdev.h"
#include "../ui/mediaview.h"
#include "../base/memory.h"
#include "../ui/picview.h"
#include "../base/rng.h"
#include "../data/strrsrc.h"
#include "../base/sysinfo.h"
#include "../screens/textlist.h"
#include "../ui/textview.h"
#include "../ui/view.h"

static const int32_t g_credit_clip_base[11] = {
	22110, 22600, 22400, 22940, 22100, 22210, 22300, 22410, 22700, 22900, 22930,
};

static const int32_t g_credit_variant[265] = {
	1000, 1001, 1003, 1004, 1006, 1007, 1008, 1009, 1011, 1012, 1014, 1015, 1016, 1017, 1018, 1019,
	1020, 1021, 1022, 1023, 1024, 1025, 1026, 1027, 1028, 1029, 1030, 1031, 1032, 1033, 1034, 1035,
	1036, 1037, 1040, 1041, 1043, 1044, 1045, 1046, 1050, 1051, 1052, 1053, 1054, 1055, 1056, 1057,
	1058, 1059, 1060, 1061, 1063, 1064, 1065, 1066, 1067, 1068, 1069, 1071, 1072, 1073, 1074, 1075,
	1076, 1077, 1078, 1079, 1081, 1082, 1083, 1084, 1085, 1087, 1088, 1090, 1091, 1092, 1093, 1094,
	1095, 1096, 1097, 1098, 1099, 1100, 1101, 1102, 1103, 1104, 1107, 1108, 1109, 1110, 1112, 1113,
	1114, 1116, 1119, 1121, 1122, 1123, 1124, 1125, 1126, 1127, 1128, 1129, 1130, 1133, 1135, 1137,
	1139, 1140, 1141, 1142, 1143, 1146, 1147, 1148, 1149, 1150, 1152, 1153, 1154, 1155, 1156, 1158,
	1159, 1161, 1165, 1166, 1167, 1169, 1170, 1173, 1175, 1177, 1178, 1179, 1182, 1183, 1185, 1186,
	1187, 1188, 1189, 1190, 1191, 1192, 1193, 1194, 1195, 1196, 1197, 1199, 1200, 1201, 1205, 1207,
	1209, 1210, 1214, 1215, 1216, 1217, 1218, 1219, 1220, 1222, 1223, 1225, 1226, 1229, 1230, 1232,
	1234, 1236, 1237, 1241, 1242, 1243, 1245, 1247, 1249, 1250, 1252, 1253, 1256, 1257, 1259, 1261,
	1262, 1263, 1266, 1268, 1270, 1272, 1275, 1277, 1281, 1284, 1285, 1286, 1287, 1288, 1290, 1292,
	1293, 1294, 1295, 1296, 1297, 1298, 1301, 1302, 1303, 1305, 1307, 1309, 1312, 1313, 1316, 1317,
	1318, 1319, 1320, 1323, 1326, 1327, 1328, 1330, 1334, 1335, 1337, 1338, 1339, 1340, 1341, 1342,
	1343, 1345, 1346, 1347, 1350, 1352, 1353, 1354, 1355, 1357, 1358, 1359, 1360, 1361, 1362, 1363,
	1364, 1365, 1366, 1367, 1368, 1369, 1370, 1371, 1372,
};

static const int32_t g_credit_variant_count = 264;

const struct about_otii_dlg_ops ABOUT_OTII_DLG_OPS_TABLE = {
	{
		{
			{about_otii_dlg_destroy},
			evt_handler_is_handled,
			evt_handler_set_handled_for,
			evt_handler_set_handled,
			evt_handler_clear_handled_for,
			evt_handler_clear_handled,
			about_otii_dlg_on_event,
			evt_handler_on_idle,
			about_otii_dlg_on_idle_spare,
		},
		about_otii_dlg_begin,
		about_otii_dlg_accept,
	},
};

static void split_left(struct rect *rc)
{
	int32_t half_width = ((rc->right - rc->left) - 2) / 2;
	rc->right = rc->left + half_width;
}

static void split_right(struct rect *rc)
{
	int32_t top = rc->top;
	int32_t left = rc->right + 2;
	int32_t width = rc->right - rc->left;
	int32_t height = rc->bottom - rc->top;
	rc->top = top;
	rc->left = left;
	rc->right = left + width;
	rc->bottom = top + height;
}

void AboutOtiiDlgInit(struct about_otii_dlg *self, int32_t a2)
{
	(void) a2;

	ModalDialogInit(&self->base, -2, true, true);
	self->base.base.base.ops = &ABOUT_OTII_DLG_OPS_TABLE.base.base.base;

	struct point media_pos = {0, 0};
	int32_t handlers[2];
	handlers[0] = CMD_CONTROL_ACTION;
	handlers[1] = CMD_CLOSE_WINDOW;

	struct rect rect = RectMake(3, 380, 439, 637);
	self->base.title = CStrCreateFrom("About Oregon Trail II");
	self->base.content = (struct view *) PicViewCreatePoint(-2, &media_pos, 22045);

	struct strrsrc *credits = StrRsrcLoad(g_text_lib, 975, true);
	StrRsrcRemapHighChars(credits, 0);
	struct text_list *credits_list = TextListCreate(-2, &rect, credits, 2, 2);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) credits_list);

	rect.top = 373;
	rect.left = 235;
	rect.right = 385;
	rect.bottom = 438;

	struct strrsrc *version_lines = StrRsrcLoad(g_text_lib, 10, true);
	StrRsrcRemapHighChars(version_lines, 0);
	struct strrsrc *version_text = StrRsrcCreate();
	struct cstr *line = StrRsrcGetCStr(version_lines, 0);
	StrRsrcAppend(version_text, CStrGet(line));
	CStrAssignBuf(line, "Version 1.2");
	StrRsrcAppend(version_text, CStrGet(line));
	CStrDestroy(&line);
	line = StrRsrcGetCStr(version_lines, 2);
	StrRsrcAppend(version_text, CStrGet(line));
	CStrDestroy(&line);
	line = StrRsrcGetCStr(version_lines, 3);
	StrRsrcAppend(version_text, CStrGet(line));
	CStrDestroy(&line);
	line = StrRsrcGetCStr(version_lines, 4);
	StrRsrcAppend(version_text, CStrGet(line));
	CStrDestroy(&line);
	StrRsrcDestroy(&version_lines);

	struct text_view *version_view = TextViewCreateStrs(-2, &rect, version_text, 5, 1, 1, 0, 0, 0, 0, 255, 0, 1);

	int32_t clip_id = g_credit_clip_base[RandRangeAvg(0, 10, 1)] << 16;
	clip_id |= 1000;
	media_pos.h = 35;
	media_pos.v = 86;
	self->media = MediaViewCreate(-2, &media_pos, clip_id, 22046, &media_pos, 22045);
	self->clip_id = clip_id;
	self->media_bg_id = 22046;
	self->media_fg_id = 22045;
	self->pt_a = media_pos;
	self->pt_b = media_pos;
	self->sysinfo_view = BuildSysinfoLabels(VIEW_OPS(self->media)->GetBounds((struct view *) self->media));

	struct rect media_bounds = VIEW_OPS(self->media)->GetBounds((struct view *) self->media);
	struct clip_button *toggle_button = ClipButtonCreate(-2, &self->base.base.eh_event, &media_bounds,
		CMD_CONTROL_ACTION, 0, 0);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) toggle_button);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) self->media);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, self->sysinfo_view);
	self->media_visible = true;
	VIEW_OPS(self->sysinfo_view)->Hide(self->sysinfo_view, false, true);
	VIEW_OPS(self->base.content)->AddChild(self->base.content, (struct view *) version_view);
	EvtHandlerBindHandler(&self->base.base, 2, handlers);
	EvtHandlerAddIdleHandler(&self->base.base, false, 900);
}

struct about_otii_dlg *AboutOtiiDlgCreate(int32_t a2)
{
	struct about_otii_dlg *self = MemAlloc(1, sizeof(struct about_otii_dlg));
	AboutOtiiDlgInit(self, a2);

	return self;
}

void AboutOtiiDlgDestroy(struct about_otii_dlg **ctx_mod)
{
	ObjectDestroy(ctx_mod);
}

void about_otii_dlg_destroy(struct object *obj)
{
	obj->ops = &MODAL_DIALOG_OPS_TABLE.base.base;
	modal_dialog_destroy(obj);
}

void about_otii_dlg_on_idle_spare(struct evt_handler *handler, void *param)
{
	(void) param;
	struct about_otii_dlg *self = (struct about_otii_dlg *) handler;

	if (!self->media_visible)
		return;

	int32_t clip_id = g_credit_clip_base[RandRangeAvg(0, 10, 1)] << 16;
	int32_t variant = g_credit_variant[RandRangeAvg(0, g_credit_variant_count, 1)];

	if (RandRangeAvg(0, 1, 1))
		variant += 500;

	clip_id |= variant;
	MediaViewLoadSource(self->media, clip_id);
	self->clip_id = clip_id;
}

void about_otii_dlg_accept(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void about_otii_dlg_begin(struct modal_dialog *self, intptr_t param)
{
	(void) self;
	(void) param;
}

void about_otii_dlg_on_event(struct evt_handler *handler, struct cmd_event *evt)
{
	struct about_otii_dlg *self = (struct about_otii_dlg *) handler;

	switch (evt->command) {
	case CMD_CONTROL_ACTION:
		if (self->media_visible) {
			VIEW_OPS(self->media)->Hide((struct view *) self->media, false, true);
			VIEW_OPS(self->sysinfo_view)->Show(self->sysinfo_view);

		} else {
			VIEW_OPS(self->sysinfo_view)->Hide(self->sysinfo_view, false, true);
			VIEW_OPS(self->media)->Show((struct view *) self->media);
		}

		self->media_visible = !self->media_visible;
		evt->handled = true;
		break;

	case CMD_CLOSE_WINDOW:
		self->base.result = DLG_OK;
		evt->handled = true;
		break;
	}
}

struct view *BuildSysinfoLabels(struct rect rc)
{
	struct strrsrc *machine_list;
	struct strrsrc *system_list;
	struct sys_info sysinfo;
	SysInfoInit(&sysinfo);

	struct view *container = ViewCreateId(-2, &rc);
	SysInfoBuildLists(&sysinfo, &system_list, &machine_list);
	split_left(&rc);
	struct clip_label_view *label = ClipLabelViewCreateStrs(-2, &rc, system_list, 7, 2, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(container)->AddChild(container, (struct view *) label);
	split_right(&rc);
	label = ClipLabelViewCreateStrs(-2, &rc, machine_list, 7, 0, 1, 0, 0, 255, 0, 1);
	VIEW_OPS(container)->AddChild(container, (struct view *) label);

	sys_info_destroy(&sysinfo.base);

	return container;
}
