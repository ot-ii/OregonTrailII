#include "gfxdev.h"

#include <string.h>

#include "assets.h"
#include "../base/file.h"
#include "../base/ini.h"
#include "mecclib.h"
#include "rsrc.h"
#include "../base/str.h"

struct assets *g_text_lib;
struct assets *g_data_lib;

static void with_trailing_slash(char *folder, size_t size)
{
	size_t len = strlen(folder);

	if (len && folder[len - 1] != '\\' && folder[len - 1] != '/' && len + 1 < size) {
		folder[len] = '\\';
		folder[len + 1] = 0;
	}
}

static bool data_file_in(const char *folder, const char *data_name)
{
	char path[260];

	if (!folder[0])
		return false;

	StrCopy(path, folder);
	with_trailing_slash(path, sizeof(path));
	StrCat(path, data_name);
	StrCat(path, ".Dat");

	return FileExists(path);
}

bool GfxDevFindDataFolder(const char *data_name, char *folder, size_t size)
{
	char from_ini[260];
	IniGetString("cdrom", "rsrcpath", ".\\", from_ini, sizeof(from_ini));

	char candidate[260];

	if (from_ini[0] && from_ini[1] != ':' && from_ini[0] != '\\' && from_ini[0] != '/') {
		FileGetExeDir(candidate, sizeof(candidate));
		StrCat(candidate, from_ini);

	} else {
		StrCopy(candidate, from_ini);
	}

	if (data_file_in(candidate, data_name)) {
		StrCopyN(folder, size, candidate);
		with_trailing_slash(folder, size);
		return true;
	}

	char exe_dir[260];
	FileGetExeDir(exe_dir, sizeof(exe_dir));

	if (data_file_in(exe_dir, data_name)) {
		StrCopyN(folder, size, exe_dir);
		return true;
	}

	return false;
}

static struct mecclib *open_library(const char *data_folder, const char *prefix, const char *data_name,
	const char *ext)
{
	char exe_dir[260], path[260];
	FileGetExeDir(exe_dir, sizeof(exe_dir));

	const char *dirs[2] = {exe_dir, data_folder};

	for (int32_t i = 0; i < 2; i++) {
		if (!dirs[i] || !dirs[i][0])
			continue;

		StrCopy(path, dirs[i]);
		with_trailing_slash(path, sizeof(path));
		StrCat(path, prefix);
		StrCat(path, data_name);
		StrCat(path, ext);

		if (FileExists(path))
			return MeccLibCreate(path);
	}

	return NULL;
}

static bool has_element(struct assets *lib, const char *name, int32_t id)
{
	struct rsrc *r = AssetsGetElement(lib, name, id, false);
	bool found = r != NULL;
	RsrcDestroy(&r);

	return found;
}

bool GfxDevOpenLibraries(const char *data_name, const char *data_folder)
{
	GfxDevCloseLibraries();

	g_text_lib = AssetsCreate();

	struct mecclib *en2 = open_library(data_folder, "", data_name, ".En2");

	if (en2)
		AssetsAddMeccLib(g_text_lib, en2, 0);

	struct mecclib *eng = open_library(data_folder, "", data_name, ".Eng");

	if (eng)
		AssetsAddMeccLib(g_text_lib, eng, ASSETS_PRIMARY);

	g_data_lib = AssetsCreate();

	struct mecclib *dt2 = open_library(data_folder, "", data_name, ".Dt2");

	if (dt2)
		AssetsAddMeccLib(g_data_lib, dt2, 0);

	static const char *const FST_DIRS[] = {"", "FST1\\", "FST2\\", "FST3\\"};

	for (int32_t i = 0; i < 4; i++) {
		struct mecclib *fst = open_library(data_folder, FST_DIRS[i], data_name, ".Fst");

		if (fst) {
			AssetsAddMeccLib(g_data_lib, fst, 0);
			break;
		}
	}

	char dat_path[260];
	StrCopy(dat_path, data_folder ? data_folder : "");
	with_trailing_slash(dat_path, sizeof(dat_path));
	StrCat(dat_path, data_name);
	StrCat(dat_path, ".Dat");
	AssetsAddMeccLib(g_data_lib, MeccLibCreate(dat_path), ASSETS_PRIMARY);

	if (!AssetsIsAvailable(g_data_lib) || !has_element(g_data_lib, "CBMP", 601))
		return false;

	if (!AssetsIsAvailable(g_text_lib) || !has_element(g_text_lib, "CONVR", 90))
		return false;

	return true;
}

void GfxDevCloseLibraries(void)
{
	RsrcCacheClear();
	AssetsDestroy(&g_text_lib);
	AssetsDestroy(&g_data_lib);
}
