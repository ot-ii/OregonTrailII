#include "../sysinfo.h"

#include <stddef.h>

#include <windows.h>

#include "../cstr.h"
#include "../../data/gfxdev.h"
#include "../log.h"
#include "../../app/replay.h"
#include "../str.h"
#include "../../data/strrsrc.h"

const struct object_ops SYS_INFO_OPS_TABLE = {
	sys_info_destroy,
};

void SysInfoInit(struct sys_info *self)
{
	ObjectInit(&self->base);
	self->base.ops = &SYS_INFO_OPS_TABLE;

	self->rsrc = StrRsrcLoad(g_text_lib, 26, true);

	if (!self->rsrc || StrRsrcId(self->rsrc) != 26)
		LogError(LOG_ERROR, "System information strings not found");

	SysInfoInitData(self);
}

void sys_info_destroy(struct object *obj)
{
	struct sys_info *self = (struct sys_info *) obj;

	StrRsrcDestroy(&self->rsrc);

	object_destroy(obj);
}

void SysInfoBuildLists(struct sys_info *self, struct strrsrc **out_list2, struct strrsrc **out_machine)
{
	*out_machine = SysInfoBuildMachineStrings(self);
	*out_list2 = SysInfoBuildList2(self);
}

struct strrsrc *SysInfoBuildList2(struct sys_info *self)
{
	struct strrsrc *list = StrRsrcCreate();

	for (int32_t i = 0; i < 6; ++i)
		StrRsrcAppend(list, StrRsrcGetAt(self->rsrc, i));

	return list;
}

static void append_int(struct strrsrc *list, int32_t value)
{
	struct cstr *s = CStrCreateInt(value, true);
	StrRsrcAppend(list, CStrGet(s));
	CStrDestroy(&s);
}

struct strrsrc *SysInfoBuildMachineStrings(struct sys_info *self)
{
	(void) self;
	SYSTEM_INFO sys_info;
	MEMORYSTATUS mem_status;
	char buf[32];

	GetSystemInfo(&sys_info);
	GlobalMemoryStatus(&mem_status);
	struct strrsrc *list = StrRsrcCreate();

#pragma warning(suppress: 4996)
	int value = (int) GetVersion();

	if (value & 0x80000000) {
		StrPrintf(buf, "%d.%d w/Win32s ", value & 0xFF, (value >> 8) & 0xFF);
		RpGetPrivateProfileString("Win32s", "Version", "(unknown)", buf, 0x1E, "Win32s.ini");

	} else {
		StrPrintf(buf, "NT %d.%d", value & 0xFF, (value >> 8) & 0xFF);
	}

	StrRsrcAppend(list, buf);

	const char *cpu_name;

	switch (sys_info.dwProcessorType) {
		case 386:   cpu_name = "Intel 80386"; break;
		case 486:   cpu_name = "Intel i486"; break;
		case 586:   cpu_name = "Intel Pentium"; break;
		case 860:   cpu_name = "Intel i860"; break;
		case 2000:  cpu_name = "Mips R2000"; break;
		case 3000:  cpu_name = "Mips R3000"; break;
		case 4000:  cpu_name = "Mips R4000"; break;
		case 21064: cpu_name = "DEC Alpha 21064"; break;
		default:    cpu_name = "(unknown)"; break;
	}

	StrRsrcAppend(list, cpu_name);

	append_int(list, (int32_t) (mem_status.dwTotalPhys + mem_status.dwTotalPageFile));
	append_int(list, (int32_t) GlobalCompact(0));
	append_int(list, (int32_t) (mem_status.dwAvailPageFile + mem_status.dwAvailPhys));

	HDC hdc = GetDC(NULL);
	int width = GetDeviceCaps(hdc, HORZRES);
	int height = GetDeviceCaps(hdc, VERTRES);
	int bits = GetDeviceCaps(hdc, BITSPIXEL);
	ReleaseDC(NULL, hdc);
	StrPrintf(buf, "%d x %d x %d", width, height, bits);
	StrRsrcAppend(list, buf);

	return list;
}

void SysInfoInitData(struct sys_info *self)
{
	(void) self;
}
