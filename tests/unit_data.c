#include "testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/data/assets.h"
#include "../src/base/file.h"
#include "../src/data/gfxdev.h"
#include "../src/data/mecclib.h"
#include "../src/data/rsrc.h"
#include "../src/base/str.h"
#include "testsupport.h"

struct result {
	int32_t resources, packed, no_format, truncated, guard_hit, missing;
};

static struct mecclib *chain_lib(struct assets *front, int32_t i)
{
	int32_t n = AssetsSourceCount(front);

	return i < n ? AssetsSourceMeccLib(front, n - 1 - i) : NULL;
}

static const char *lib_name(const struct mecclib *lib)
{
	const char *path = MeccLibPath(lib);
	const char *slash;

	if (!path)
		return "?";
	slash = strrchr(path, '\\');
	return slash ? slash + 1 : path;
}

static int32_t check_unpack(struct rsrc *r, const char *type, bool *guard_ok)
{
	char fmt_name[16];
	struct rsrc *fmt;
	struct rsrc *copy;
	int32_t before, size = RsrcSize(r);

	*guard_ok = true;
	StrPrintf(fmt_name, "T%s", type);
	fmt = AssetsGetElement(g_data_lib, fmt_name, 0, false);
	if (!fmt)
		return -1;
	copy = RsrcCreate(RsrcName(r), RsrcId(r), size, RsrcStorage(r));
	memcpy(RsrcData(copy), RsrcData(r), (size_t) size);
	RsrcData(copy)[size] = 0;
	before = RsrcUnpackTruncations();
	RsrcUnpack(copy, RsrcData(fmt));
	if (RsrcSize(copy) != size || RsrcData(copy)[size] != 0)
		*guard_ok = false;
	RsrcDestroy(&copy);
	RsrcDestroy(&fmt);
	return RsrcUnpackTruncations() - before;
}

struct walk {
	struct mecclib *lib;
	struct assets *front;
	const char *label;
	struct result *res;
};

static void check_element(const char *type, int32_t id, int32_t storage, int32_t size, int64_t offset, void *opaque)
{
	struct walk *w = opaque;
	struct result *res = w->res;
	struct rsrc *r = w->front ? AssetsGetElement(w->front, type, id, false) : MeccLibGetElement(w->lib, type, id);

	(void) storage;
	(void) size;
	(void) offset;
	++res->resources;
	TCase("%s %s %d", w->label, type, id);
	if (!r) {
		++res->missing;
		CHECK_MSG(0, "GetElement returned nothing");
		return;
	}
	if (RsrcStorage(r) == RSRC_PACKED) {
		bool guard_ok;
		int32_t cut;
		++res->packed;
		cut = check_unpack(r, type, &guard_ok);
		if (cut < 0) {
			++res->no_format;

		} else {
			CHECK_MSG(guard_ok, "the unpack wrote past the resource's %d bytes", RsrcSize(r));
			if (!guard_ok)
				++res->guard_hit;
			if (cut) {
				++res->truncated;
				if (!strcmp(type, "SHNT") && id == 150 && !w->front)
					TKnown("data-shnt150-short");
				CHECK_MSG(cut == 0, "shorter than its counts: the unpack stopped at the end %d time(s), size %d",
					cut, RsrcSize(r));
				TKnown(NULL);
			}
		}
	}
	RsrcDestroy(&r);
}

static void check_library(struct mecclib *lib, struct assets *front, const char *label_in, struct result *res)
{
	char label[160];

	snprintf(label, sizeof(label), "%s %s", label_in, lib_name(lib));
	memset(res, 0, sizeof(*res));
	if (!MeccLibIsAvailable(lib)) {
		TNote("%s: no directory (not open)", label);
		return;
	}
	struct walk w = {lib, front, label, res};
	MeccLibEnumerate(lib, check_element, &w);
	TCase("%s", label);
	TNote("%s: %d resources, %d unpacked with a format, %d cut short, %d missing", label, res->resources,
		res->packed - res->no_format, res->truncated, res->missing);
}

TEST(unit_data, game_view_unpacks)
{
	struct mecclib *lib;
	struct result res;
	char label[64];

	CHECK(g_data_lib != NULL);
	CHECK(g_text_lib != NULL);
	if (!g_data_lib || !g_text_lib)
		return;
	for (int32_t i = 0; (lib = chain_lib(g_data_lib, i)) != NULL; ++i) {
		snprintf(label, sizeof(label), "data[%d]", i);
		check_library(lib, g_data_lib, label, &res);
	}
	for (int32_t i = 0; (lib = chain_lib(g_text_lib, i)) != NULL; ++i) {
		snprintf(label, sizeof(label), "text[%d]", i);
		check_library(lib, g_text_lib, label, &res);
	}
}

static uint32_t fnv(const uint8_t *p, int32_t n)
{
	uint32_t h = 2166136261u;

	for (int32_t i = 0; i < n; ++i)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

struct dump {
	FILE *f;
	struct assets *front;
	const char *label;
	int32_t index;
};

static void dump_element(const char *type, int32_t id, int32_t storage, int32_t size, int64_t offset, void *opaque)
{
	struct dump *d = opaque;
	struct rsrc *r = AssetsGetElement(d->front, type, id, false);

	(void) storage;
	(void) size;
	(void) offset;
	if (!r) {
		fprintf(d->f, "%s[%d] %s %d missing\n", d->label, d->index, type, id);
		return;
	}
	uint32_t raw = fnv(RsrcData(r), RsrcSize(r));
	int32_t err = -1;
	if (RsrcStorage(r) == RSRC_PACKED) {
		char fmt_name[16];
		StrPrintf(fmt_name, "T%s", type);
		struct rsrc *fmt = AssetsGetElement(g_data_lib, fmt_name, 0, false);
		if (fmt) {
			RsrcDestroy(&fmt);
			err = RsrcLoadAndUnpack(r);
		}
	}
	fprintf(d->f, "%s[%d] %s %d %d %d %08x %d %08x\n", d->label, d->index, type, id, RsrcStorage(r), RsrcSize(r), raw,
		err, fnv(RsrcData(r), RsrcSize(r)));
	RsrcDestroy(&r);
}

static void dump_chain(FILE *f, struct assets *front, const char *label)
{
	struct mecclib *lib;

	for (int32_t i = 0; (lib = chain_lib(front, i)) != NULL; ++i) {
		if (!MeccLibIsAvailable(lib))
			continue;
		struct dump d = {f, front, label, i};
		MeccLibEnumerate(lib, dump_element, &d);
	}
}

TEST(unit_data, dump_for_port)
{
	char path[512];
	FILE *f;

	if (!g_data_lib || !g_text_lib)
		return;
	FileGetExeDir(path, sizeof(path));
	StrCat(path, "test_state");
	TestMakeDir(path);
	StrCat(path, "\\rsrc_dump_c.txt");
	f = fopen(path, "w");
	CHECK(f != NULL);
	if (!f)
		return;
	dump_chain(f, g_data_lib, "data");
	dump_chain(f, g_text_lib, "text");
	fclose(f);
	TNote("written %s", path);
}

TEST(unit_data, each_file_unpacks)
{
	struct mecclib *lib;
	struct result res;
	char label[64];
	int32_t n;

	if (!g_data_lib)
		return;
	n = AssetsSourceCount(g_data_lib);
	CHECK_MSG(n >= 2, "the data chain has %d librar%s; the .DT2 patch should sit in front of the .DAT", n,
		n == 1 ? "y" : "ies");
	for (int32_t i = 0; (lib = chain_lib(g_data_lib, i)) != NULL; ++i) {
		snprintf(label, sizeof(label), "data[%d]", i);
		check_library(lib, NULL, label, &res);
	}
}

const struct test_reg TESTS_unit_data[] = {
	TEST_ENTRY(unit_data, game_view_unpacks),
	TEST_ENTRY(unit_data, dump_for_port),
	TEST_ENTRY(unit_data, each_file_unpacks),
	TEST_END
};

#endif
