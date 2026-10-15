#include "alert.h"

#include <stddef.h>

#include "../base/cstr.h"
#include "../data/gfxdev.h"
#include "../base/memory.h"
#include "../app/replay.h"
#include "../data/strrsrc.h"

#define MB_OKCANCEL_         0x00000001u
#define MB_YESNOCANCEL_      0x00000003u
#define MB_YESNO_            0x00000004u
#define MB_ICONSTOP_         0x00000010u
#define MB_ICONQUESTION_     0x00000020u
#define MB_ICONEXCLAMATION_  0x00000030u
#define MB_ICONINFORMATION_  0x00000040u
#define MB_DEFBUTTON2_       0x00000100u
#define MB_DEFBUTTON3_       0x00000200u
#define MB_TASKMODAL_        0x00002000u

#define IDOK_     1
#define IDCANCEL_ 2
#define IDYES_    6
#define IDNO_     7

struct alert {
	struct cstr *title;
};

struct alert *AlertCreate(const char *s)
{
	struct alert *ctx = MemAlloc(1, sizeof(struct alert));
	ctx->title = CStrCreateFrom(s);

	return ctx;
}

void AlertDestroy(struct alert **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct alert *ctx = *ctx_mod;
	*ctx_mod = NULL;

	CStrDestroy(&ctx->title);
	MemFree(ctx);
}

int32_t AlertShow(struct alert *ctx, int32_t icon, const struct cstr *msg, int32_t buttons, int32_t def)
{
	uint32_t style = MB_TASKMODAL_;

	switch (icon) {
		case 1:
			style |= MB_ICONSTOP_;
			break;
		case 2:
			style |= MB_ICONEXCLAMATION_;
			break;
		case 3:
			style |= MB_ICONINFORMATION_;
			break;
		case 4:
			style |= MB_ICONQUESTION_;
			break;
	}

	int32_t default_button = def;

	switch (buttons) {
		case 0:
			break;
		case 1:
			style |= MB_OKCANCEL_;

			if (def == 3)
				default_button = 2;
			break;
		case 2:
			style |= MB_YESNO_;
			break;
		case 3:
			style |= MB_YESNOCANCEL_;
			break;
	}

	switch (default_button) {
		case 1:
			break;
		case 2:
			style |= MB_DEFBUTTON2_;
			break;
		case 3:
			style |= MB_DEFBUTTON3_;
			break;
	}

	int32_t answer = RpMessageBox(NULL, CStrGet(msg), CStrGet(ctx->title), style);

	switch (answer) {
		case IDOK_:
			return 0;
		case IDCANCEL_:
			return 1;
		case IDYES_:
			return 2;
		case IDNO_:
			return 3;
		default:
			return -1;
	}
}

int32_t AlertShowRsrc(struct alert *ctx, int32_t icon, int32_t rsrc_id, int32_t str_index, int32_t buttons,
	int32_t def)
{
	struct strrsrc *strings = StrRsrcLoad(g_text_lib, rsrc_id, true);
	struct cstr *message = strings ? StrRsrcGetCStr(strings, str_index) : NULL;

	if (!message)
		message = CStrCreate();

	int32_t result = AlertShow(ctx, icon, message, buttons, def);
	CStrDestroy(&message);
	StrRsrcDestroy(&strings);

	return result;
}
