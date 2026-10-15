#include "../ini.h"

#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "../file.h"
#include "../../app/replay.h"
#include "../str.h"

static char g_ini_path[MAX_PATH];
static char g_data_ini_path[MAX_PATH];
static bool g_data_ini_known;

static const char MISSING[] = "\x01";

const char *IniPath(void)
{
	if (!g_ini_path[0]) {
		FileGetExeDir(g_ini_path, sizeof(g_ini_path));
		StrCat(g_ini_path, "OregonII.ini");
	}

	return g_ini_path;
}

const char *IniDataPath(void)
{
	if (!g_data_ini_known) {
		char rsrc[MAX_PATH], exe_dir[MAX_PATH], dir[MAX_PATH], full_dir[MAX_PATH], full_exe[MAX_PATH];

		g_data_ini_known = true;
		g_data_ini_path[0] = 0;

		GetPrivateProfileStringA("cdrom", "rsrcpath", ".\\", rsrc, sizeof(rsrc), IniPath());
		FileGetExeDir(exe_dir, sizeof(exe_dir));

		if (rsrc[0] && rsrc[1] != ':' && rsrc[0] != '\\' && rsrc[0] != '/') {
			StrCopy(dir, exe_dir);
			StrCat(dir, rsrc);

		} else {
			StrCopy(dir, rsrc);
		}

		size_t len = strlen(dir);
		if (len && dir[len - 1] != '\\' && dir[len - 1] != '/')
			StrCat(dir, "\\");

		if (!GetFullPathNameA(dir, sizeof(full_dir), full_dir, NULL) ||
			!GetFullPathNameA(exe_dir, sizeof(full_exe), full_exe, NULL) || !_stricmp(full_dir, full_exe))
			return NULL;

		StrCopy(g_data_ini_path, full_dir);
		StrCat(g_data_ini_path, "OregonII.ini");

		if (!FileExists(g_data_ini_path))
			g_data_ini_path[0] = 0;
	}

	return g_data_ini_path[0] ? g_data_ini_path : NULL;
}

static const char *layer_with(const char *base, const char *top, const char *section, const char *key, char *out,
	size_t size)
{
	if (top) {
		GetPrivateProfileStringA(section, key, MISSING, out, (DWORD) size, top);
		if (strcmp(out, MISSING))
			return top;
	}

	if (base && !(!_stricmp(section, "cdrom") && !_stricmp(key, "rsrcpath"))) {
		GetPrivateProfileStringA(section, key, MISSING, out, (DWORD) size, base);
		if (strcmp(out, MISSING))
			return base;
	}

	return NULL;
}

bool IniReadLayered(const char *base, const char *top, const char *section, const char *key, char *out, size_t size)
{
	if (!out || !size)
		return false;

	if (layer_with(base, top, section, key, out, size))
		return true;

	out[0] = 0;
	return false;
}

void IniGetString(const char *section, const char *key, const char *def, char *out, size_t size)
{
	if (!out || !size)
		return;

	if (!IniReadLayered(IniDataPath(), IniPath(), section, key, out, size))
		StrCopyN(out, size, def ? def : "");

	out[size - 1] = 0;
	RpIniString(out, (uint32_t) size, (uint32_t) strlen(out));
	out[size - 1] = 0;
}

int32_t IniGetInt(const char *section, const char *key, int32_t def)
{
	char text[64];
	const char *file = layer_with(IniDataPath(), IniPath(), section, key, text, sizeof(text));
	int32_t value = file ? (int32_t) GetPrivateProfileIntA(section, key, def, file) : def;

	return RpQuery(RPQ_INI_INT, value);
}

bool IniSetString(const char *section, const char *key, const char *value)
{
	if (!_stricmp(section, "cdrom") && key && !_stricmp(key, "rsrcpath"))
		g_data_ini_known = false;

	return RpWritePrivateProfileString(section, key, value, IniPath());
}

bool IniSetInt(const char *section, const char *key, int32_t value)
{
	char text[16];
	StrPrintf(text, "%d", value);

	return IniSetString(section, key, text);
}
