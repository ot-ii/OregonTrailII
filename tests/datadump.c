#include <stdio.h>
#include <string.h>

#include "../src/data/assets.h"
#include "../src/data/gfxdev.h"
#include "../src/data/mecclib.h"
#include "../src/data/rsrc.h"
#include "../src/base/str.h"

struct dump {
	FILE *f;
	struct assets *front;
	const char *label;
	int32_t index;
};

static uint32_t fnv(const uint8_t *p, int32_t n)
{
	uint32_t h = 2166136261u;

	for (int32_t i = 0; i < n; i++)
		h = (h ^ p[i]) * 16777619u;

	return h;
}

static void dump_element(const char *name, int32_t id, int32_t storage, int32_t size, int64_t offset, void *opaque)
{
	struct dump *d = opaque;
	struct rsrc *r = AssetsGetElement(d->front, name, id, false);

	if (!r) {
		fprintf(d->f, "%s[%d] %s %d missing\n", d->label, d->index, name, id);
		return;
	}

	uint32_t raw = fnv(RsrcData(r), RsrcSize(r));
	int32_t err = -1;

	if (RsrcStorage(r) == RSRC_PACKED) {
		char recipe_name[16];
		StrPrintf(recipe_name, "T%s", name);

		struct rsrc *recipe = AssetsGetElement(g_data_lib, recipe_name, 0, false);

		if (recipe) {
			RsrcDestroy(&recipe);
			err = RsrcLoadAndUnpack(r);
		}
	}

	fprintf(d->f, "%s[%d] %s %d %d %d %08x %d %08x\n", d->label, d->index, name, id, RsrcStorage(r), RsrcSize(r),
		raw, err, fnv(RsrcData(r), RsrcSize(r)));

	RsrcDestroy(&r);
	(void) storage;
	(void) size;
	(void) offset;
}

static void dump_chain(FILE *f, struct assets *front, const char *label)
{
	int32_t n = AssetsSourceCount(front);

	for (int32_t i = n - 1; i >= 0; i--) {
		struct mecclib *lib = AssetsSourceMeccLib(front, i);

		if (!lib || !MeccLibIsAvailable(lib))
			continue;

		struct dump d = {f, front, label, n - 1 - i};
		MeccLibEnumerate(lib, dump_element, &d);
	}
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		printf("datadump <data folder> <out file>\n");
		return 2;
	}

	if (!GfxDevOpenLibraries("OregonII", argv[1])) {
		printf("the data files did not open from %s\n", argv[1]);
		return 1;
	}

	FILE *f = fopen(argv[2], "w");

	if (!f)
		return 1;

	dump_chain(f, g_data_lib, "data");
	dump_chain(f, g_text_lib, "text");
	fclose(f);

	printf("written %s; unpacks cut short: %d\n", argv[2], RsrcUnpackTruncations());
	GfxDevCloseLibraries();

	return 0;
}
