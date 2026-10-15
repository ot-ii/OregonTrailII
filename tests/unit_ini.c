#include "testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include "../src/base/file.h"
#include "../src/base/ini.h"
#include "../src/base/str.h"

static char g_base[260], g_top[260];

static void write_file(const char *path, const char *text)
{
	FILE *f = fopen(path, "w");

	if (f) {
		fputs(text, f);
		fclose(f);
	}
}

static void make_layers(void)
{
	FileGetExeDir(g_base, sizeof(g_base));
	StrCopy(g_top, g_base);
	StrCat(g_base, "unit_ini_data.ini");
	StrCat(g_top, "unit_ini_exe.ini");

	write_file(g_base, "[window]\nscale=2\nleft=10\n[cdrom]\nrsrcpath=Z:\\elsewhere\\\n[sound]\nmusic=1\n");
	write_file(g_top, "[window]\nscale=3\nempty=\n[twitch]\nchannel=someone\n");
}

static void remove_layers(void)
{
	remove(g_base);
	remove(g_top);
}

TEST(unit_ini, executable_overrides_data_folder)
{
	char v[64];

	make_layers();

	CHECK(IniReadLayered(g_base, g_top, "window", "scale", v, sizeof(v)));
	CHECK_MSG(!strcmp(v, "3"), "scale %s", v);

	CHECK(IniReadLayered(g_base, g_top, "window", "left", v, sizeof(v)));
	CHECK_MSG(!strcmp(v, "10"), "left %s", v);

	CHECK(IniReadLayered(g_base, g_top, "sound", "music", v, sizeof(v)));
	CHECK_MSG(!strcmp(v, "1"), "music %s", v);

	CHECK(IniReadLayered(g_base, g_top, "twitch", "channel", v, sizeof(v)));
	CHECK_MSG(!strcmp(v, "someone"), "channel %s", v);

	remove_layers();
}

TEST(unit_ini, empty_value_still_overrides)
{
	char v[64];

	make_layers();
	write_file(g_base, "[window]\nempty=from data\n");

	CHECK(IniReadLayered(g_base, g_top, "window", "empty", v, sizeof(v)));
	CHECK_MSG(v[0] == 0, "empty '%s'", v);

	remove_layers();
}

TEST(unit_ini, missing_key)
{
	char v[64] = "x";

	make_layers();

	CHECK(!IniReadLayered(g_base, g_top, "window", "nothing", v, sizeof(v)));
	CHECK(v[0] == 0);
	CHECK(!IniReadLayered(NULL, NULL, "window", "scale", v, sizeof(v)));

	CHECK(IniReadLayered(NULL, g_top, "window", "scale", v, sizeof(v)));
	CHECK(!strcmp(v, "3"));
	CHECK(IniReadLayered(g_base, NULL, "window", "scale", v, sizeof(v)));
	CHECK(!strcmp(v, "2"));

	remove_layers();
}

TEST(unit_ini, rsrcpath_only_from_executable)
{
	char v[64];

	make_layers();

	CHECK(!IniReadLayered(g_base, g_top, "cdrom", "rsrcpath", v, sizeof(v)));

	remove_layers();
}

TEST(unit_ini, game_layers)
{
	const char *data = IniDataPath();
	char v[64];

	TNote("executable's ini %s, data folder's %s", IniPath(), data ? data : "(none)");
	CHECK(IniPath() != NULL);
	CHECK(!data || _stricmp(data, IniPath()));

	IniGetString("cdrom", "rsrcpath", "", v, sizeof(v));
	TNote("rsrcpath %s", v);
}

const struct test_reg TESTS_unit_ini[] = {
	TEST_ENTRY(unit_ini, executable_overrides_data_folder),
	TEST_ENTRY(unit_ini, empty_value_still_overrides),
	TEST_ENTRY(unit_ini, missing_key),
	TEST_ENTRY(unit_ini, rsrcpath_only_from_executable),
	TEST_ENTRY(unit_ini, game_layers),
	TEST_END
};

#endif
