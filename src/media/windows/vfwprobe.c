#include "../vfwprobe.h"

#include <stddef.h>
#include <string.h>

#include <windows.h>
#include <vfw.h>

#include "../../base/ini.h"
#include "../../base/memory.h"
#include "../../app/replay.h"

#pragma comment(lib, "vfw32.lib")

bool g_vfw_available = false;
int32_t g_vfw_check_count = 0;

struct vfw_probe *VfwProbeCreate(void)
{
	return MemAlloc(1, sizeof(struct vfw_probe));
}

void VfwProbeDestroy(struct vfw_probe **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct vfw_probe *ctx = *ctx_mod;
	*ctx_mod = NULL;

	MemFree(ctx);
}

bool VfwProbeIsAvail(struct vfw_probe *self)
{
	(void) self;

	return g_vfw_available;
}

void VfwProbeCheck(struct vfw_probe *self)
{
	char setting_text[64];

	(void) self;

	if (!g_vfw_available) {
		IniGetString("game", "vfw", "on", setting_text, sizeof(setting_text));

		if (strcmp(setting_text, "on"))
			return;

		uint32_t version = (VideoForWindowsVersion() >> 16) & 0xFFFF;

		if (version < 266) {
			MessageBeep(MB_ICONSTOP);
			RpMessageBox(NULL, "Video for Windows version is too old", "MCIPlay Error", MB_ICONSTOP);
			return;
		}

		g_vfw_available = true;
	}

	++g_vfw_check_count;
}

void VfwProbeRelease(struct vfw_probe *self)
{
	(void) self;

	--g_vfw_check_count;
}
