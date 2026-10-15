#include "testframework.h"

#ifdef OTII_TEST

#include <ctype.h>
#include <string.h>

#include "../src/base/file.h"
#include "../src/base/memory.h"
#include "../src/base/str.h"

#ifdef CROWD_CONTROL

#include "../src/features/crowdcontrol.h"
#include "../src/features/crowdnet.h"
#include "../src/third_party/mty_json.h"

static char *read_file(const char *path)
{
	size_t size;

	return FileReadAll(path, &size);
}

static void state_path(const char *name, char *path, size_t size)
{
	FileGetExeDir(path, size);
	StrCatN(path, size, "test_state\\");
	StrCatN(path, size, name);
}

TEST(unit_crowd, token_claims)
{
	int64_t exp;
	char name[64];

	CHECK(CrowdNetTokenClaims("eyJhbGciOiJub25lIn0.eyJleHAiOjQxMDI0NDQ4MDAsIm5hbWUiOiJTa2lwcHkgVGVzdCJ9.sig", &exp,
		name, sizeof(name)));
	CHECK_EQ(exp, INT64_C(4102444800));
	CHECK(!strcmp(name, "Skippy Test"));
	CHECK(!CrowdNetTokenClaims("nonsense", &exp, name, sizeof(name)));
	CHECK(!CrowdNetTokenClaims("a.%%%.c", &exp, name, sizeof(name)));
	CHECK(!CrowdNetTokenClaims("", &exp, name, sizeof(name)));
}

static int32_t member_count(const MTY_JSON *obj)
{
	uint64_t iter = 0;
	const char *key;
	int32_t n = 0;

	while (MTY_JSONObjGetNextKey(obj, &iter, &key))
		++n;

	return n;
}

TEST(unit_crowd, pack_file)
{
	char path[512], repo[512];
	char *text, *committed;
	MTY_JSON *root;
	const MTY_JSON *game;
	const char *id;
	uint64_t ei = 0;
	int32_t count = 0;

	state_path("crowd_pack_generated.json", path, sizeof(path));
	CHECK(CrowdControlWritePack(path));
	text = read_file(path);
	CHECK(text != NULL);
	if (!text)
		return;
	root = MTY_JSONParse(text);
	CHECK(root != NULL);
	game = MTY_JSONObjGetItem(MTY_JSONObjGetItem(root, "effects"), "game");
	CHECK(MTY_JSONGetType(game) == MTY_JSON_OBJECT);
	CHECK(MTY_JSONGetType(MTY_JSONObjGetItem(MTY_JSONObjGetItem(root, "meta"), "connector")) == MTY_JSON_ARRAY);
	while (MTY_JSONObjGetNextKey(game, &ei, &id)) {
		const MTY_JSON *e = MTY_JSONObjGetItem(game, id);
		const MTY_JSON *params = MTY_JSONObjGetItem(e, "parameters");
		const char *pkey;
		uint64_t pi = 0;

		TCase("%s", id);
		++count;
		CHECK(MTY_JSONObjGetStringPtr(e, "name") != NULL);
		CHECK(MTY_JSONGetType(MTY_JSONObjGetItem(e, "price")) == MTY_JSON_NUMBER);
		CHECK(MTY_JSONArrayGetLength(MTY_JSONObjGetItem(e, "category")) == 1);
		CHECK(MTY_JSONArrayGetLength(MTY_JSONObjGetItem(e, "group")) == 1);
		while (MTY_JSONObjGetNextKey(params, &pi, &pkey)) {
			const MTY_JSON *options = MTY_JSONObjGetItem(MTY_JSONObjGetItem(params, pkey), "options");
			const char *okey;
			uint64_t oi = 0;

			CHECK(member_count(options) > 0);
			while (MTY_JSONObjGetNextKey(options, &oi, &okey)) {
				const char *name = MTY_JSONObjGetStringPtr(MTY_JSONObjGetItem(options, okey), "name");

				CHECK(name && strncmp(name, "ailment ", 8) != 0);
				CHECK_MSG(name && !islower((uint8_t) name[0]), "%s", name ? name : "");
			}
		}
	}
	TCase("");
	CHECK(count >= 25);
	CHECK(strstr(text, "\"Random Bad Event\"") != NULL);
	CHECK(strstr(text, "\"Meat (Salt Pork)\"") != NULL);
	CHECK(strstr(text, "\"Gunpowder (25-lb. Kegs)\"") != NULL);
	{
		const MTY_JSON *vars = MTY_JSONObjGetItem(MTY_JSONObjGetItem(MTY_JSONObjGetItem(root, "effects"), "meta"),
			"variables");
		CHECK(MTY_JSONGetType(vars) == MTY_JSON_OBJECT && member_count(vars) == 6);
		CHECK(MTY_JSONObjGetStringPtr(vars, "leader_name") != NULL);
		CHECK(MTY_JSONObjGetStringPtr(vars, "member6_name") != NULL);
	}
	CHECK(strstr(text, "\"{{member2_name}}\"") != NULL);
	CHECK(strstr(text, "\"{{leader_name}}\"") != NULL);
	CHECK(strstr(text, "\"rabies\"") != NULL);
	CHECK(strstr(text, "\"The Wagon Leader\"") != NULL);
	TNote("%d effects in the pack", count);
	state_path("OregonTrailII.pack.json", repo, sizeof(repo));
	committed = read_file(repo);
	CHECK_MSG(committed && !strcmp(committed, text), "crowdcontrol/OregonTrailII.pack.json differs from what the game "
		"writes: copy %s over it", path);
	MemFree(committed);
	MTY_JSONDestroy(&root);
	MemFree(text);
}

const struct test_reg TESTS_unit_crowd[] = {
	TEST_ENTRY(unit_crowd, token_claims),
	TEST_ENTRY(unit_crowd, pack_file),
	TEST_END
};

#else

const struct test_reg TESTS_unit_crowd[] = {
	TEST_END
};

#endif

#endif
