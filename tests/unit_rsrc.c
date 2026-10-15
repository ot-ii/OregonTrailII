#include "testframework.h"

#ifdef OTII_TEST

#include <string.h>

#include "../src/base/byteorder.h"
#include "../src/data/rsrc.h"

#define GUARD 0xEE

struct buf {
	uint8_t mem[96];
	int32_t size;
	int32_t end;
};

static void fill(struct buf *b, const uint8_t *data, int32_t n)
{
	memset(b->mem, GUARD, sizeof(b->mem));
	memcpy(b->mem, data, (size_t) n);
	b->size = n;
	b->end = 0;
}

static bool guard_ok(const struct buf *b)
{
	for (int32_t i = b->size; i < (int32_t) sizeof(b->mem); ++i)
		if (b->mem[i] != GUARD)
			return false;
	return b->end == 0;
}

static int32_t unpack(struct buf *b, const char *fmt)
{
	struct rsrc *r = RsrcCreate("TEST", 0, b->size, RSRC_PACKED);

	memcpy(RsrcData(r), b->mem, (size_t) b->size);
	int32_t err = RsrcUnpack(r, (const uint8_t *) fmt);
	memcpy(b->mem, RsrcData(r), (size_t) b->size);
	b->end = RsrcData(r)[b->size];
	RsrcDestroy(&r);
	return err;
}

static int32_t w(const struct buf *b, int32_t off)
{
	return (int16_t) ReadLe16(b->mem + off);
}

static int32_t l(const struct buf *b, int32_t off)
{
	return ReadLe32(b->mem + off);
}

TEST(unit_rsrc, words_and_longs)
{
	static const uint8_t data[] = {0x12, 0x34, 0x01, 0x02, 0x03, 0x04, 0x7F, 0xFF, 0xAB};
	static const char fmt[] = {2, 3, 5, 1, 0};
	struct buf b;

	fill(&b, data, sizeof(data));
	CHECK_EQ(unpack(&b, fmt), 0);
	CHECK_EQ(w(&b, 0), 0x1234);
	CHECK_EQ(l(&b, 2), 0x01020304);
	CHECK_EQ(w(&b, 6), 0x7FFF);
	CHECK_EQ(b.mem[8], 0xAB);
	CHECK(guard_ok(&b));
}

TEST(unit_rsrc, counted_lists)
{
	static const uint8_t data23[] = {0x00, 0x03, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
	static const uint8_t data21[] = {0x00, 0x02, 0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
	static const char fmt23[] = {23, 2, 0};
	static const char fmt21[] = {21, 2, 0};
	struct buf b;

	fill(&b, data23, sizeof(data23));
	CHECK_EQ(unpack(&b, fmt23), 0);
	CHECK_EQ(w(&b, 0), 3);
	CHECK_EQ(w(&b, 2), 1);
	CHECK_EQ(w(&b, 6), 3);
	CHECK(guard_ok(&b));
	fill(&b, data21, sizeof(data21));
	CHECK_EQ(unpack(&b, fmt21), 0);
	CHECK_EQ(w(&b, 0), 2);
	CHECK_EQ(w(&b, 6), 3);
	CHECK(guard_ok(&b));
}

TEST(unit_rsrc, groups)
{
	static const uint8_t data[] = {0x00, 0x02, 0x00, 0x0A, 0x00, 0x00, 0x01, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x02,
		0x00};
	static const char fmt[] = {23, 22, 2, 3, 20, 0};
	struct buf b;

	fill(&b, data, sizeof(data));
	CHECK_EQ(unpack(&b, fmt), 0);
	CHECK_EQ(w(&b, 2), 10);
	CHECK_EQ(l(&b, 4), 256);
	CHECK_EQ(w(&b, 8), 11);
	CHECK_EQ(l(&b, 10), 512);
	CHECK(guard_ok(&b));
}

TEST(unit_rsrc, pascal_strings_and_blocks)
{
	static const uint8_t data[] = {2, 'H', 'i', 0x00, 0x03, 'a', 'b', 'c', 0x01, 0x00};
	static const char fmt[] = {9, 11, 2, 0};
	struct buf b;

	fill(&b, data, sizeof(data));
	CHECK_EQ(unpack(&b, fmt), 0);
	CHECK_EQ(b.mem[1], 'H');
	CHECK_EQ(w(&b, 3), 3);
	CHECK_EQ(w(&b, 8), 256);
	CHECK(guard_ok(&b));
}

TEST(unit_rsrc, repeat_to_end)
{
	static const uint8_t data[] = {0x00, 0x01, 0x00, 0x02, 0x00, 0x03};
	static const char fmt[] = {24, 2, 20, 0};
	struct buf b;

	fill(&b, data, sizeof(data));
	unpack(&b, fmt);
	CHECK_EQ(w(&b, 0), 1);
	CHECK_EQ(w(&b, 4), 3);
	CHECK(guard_ok(&b));
}

TEST(unit_rsrc, short_resources)
{
	static const struct { uint8_t data[16]; int32_t n; char fmt[8]; const char *what; } cases[] = {
		{{0x00, 0x05, 0x00, 0x01, 0x00, 0x02}, 6, {23, 2, 0}, "list of 5 words, room for 2"},
		{{0x00, 0x05, 0x00, 0x01, 0x00}, 5, {23, 2, 0}, "list ends half-way through a word"},
		{{0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00}, 7, {23, 22, 2, 3, 20, 0}, "records cut inside a long"},
		{{0x00}, 1, {2, 0}, "a word in one byte"},
		{{0x00, 0x00, 0x00}, 3, {3, 0}, "a long in three bytes"},
		{{0x00, 0x01, 0x00}, 3, {24, 2, 20, 0}, "repeat with an odd byte left"},
		{{0}, 0, {23, 2, 0}, "an empty resource"},
		{{0x7F, 0xFF}, 2, {23, 2, 0}, "a count of 32767 and nothing after it"},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		struct buf b;
		TCase("%s", cases[i].what);
		fill(&b, cases[i].data, cases[i].n);
		unpack(&b, cases[i].fmt);
		CHECK(guard_ok(&b));
	}
}

const struct test_reg TESTS_unit_rsrc[] = {
	TEST_ENTRY(unit_rsrc, words_and_longs),
	TEST_ENTRY(unit_rsrc, counted_lists),
	TEST_ENTRY(unit_rsrc, groups),
	TEST_ENTRY(unit_rsrc, pascal_strings_and_blocks),
	TEST_ENTRY(unit_rsrc, repeat_to_end),
	TEST_ENTRY(unit_rsrc, short_resources),
	TEST_END
};

#endif
