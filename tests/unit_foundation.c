#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/base/byteorder.h"
#include "../src/base/cstr.h"
#include "../src/data/datafile.h"
#include "../src/base/date.h"
#include "../src/base/file.h"
#include "../src/base/geom.h"
#include "../src/base/list.h"
#include "../src/base/memory.h"
#include "../src/base/rng.h"
#include "../src/base/str.h"

static int32_t g_checks;
static int32_t g_failures;

#define CHECK(cond) check((cond), #cond, __LINE__)

static void check(bool ok, const char *what, int32_t line)
{
	g_checks++;

	if (!ok) {
		g_failures++;
		printf("FAIL line %d: %s\n", line, what);
	}
}

static uint64_t g_ref_calls;

static int32_t ref_uniform(int32_t range)
{
	const uint32_t span = 1u << 30;
	uint32_t limit = span - span % (uint32_t) range, v;

	do {
		v = ((uint32_t) rand() << 15) | (uint32_t) rand();
		g_ref_calls += 2;
	} while (v >= limit);

	return (int32_t) (v % (uint32_t) range);
}

static int32_t ref_rand_range_avg(int32_t lo, int32_t hi, int32_t count)
{
	int32_t range = hi - lo + 1;
	int32_t sum = 0;

	if (range <= 0)
		return lo;

	if (count <= 0)
		count = 1;

	int16_t rolls_left = (int16_t) count;

	while (rolls_left--)
		sum += ref_uniform(range);

	return lo + sum / count;
}

static void test_rng(void)
{
	static const uint32_t SEEDS[] = {0, 1, 12345, 0xDEADBEEF, 0x7FFFFFFF, 1760000000u};

	for (size_t s = 0; s < sizeof(SEEDS) / sizeof(SEEDS[0]); s++) {
		srand(SEEDS[s]);
		RngSetSeed(SEEDS[s]);
		g_ref_calls = 0;

		int32_t mismatches = 0;

		for (int32_t i = 0; i < 10000; i++) {
			int32_t lo = (int32_t) (i % 7) - 3;
			int32_t hi = lo + (i * 37) % 40000;
			int32_t count = 1 + i % 4;

			if (i % 1000 == 999)
				hi = lo + 1000000000;

			if (RandRangeAvg(lo, hi, count) != ref_rand_range_avg(lo, hi, count))
				mismatches++;
		}

		CHECK(mismatches == 0);
		CHECK(RngCalls() == g_ref_calls);
	}

	RngSetSeed(777);
	for (int32_t i = 0; i < 123; i++)
		RandRangeAvg(1, 100, 1);

	uint64_t calls = RngCalls();
	int32_t next = RandRangeAvg(1, 1000, 1);

	RngRestore(777, calls);
	CHECK(RandRangeAvg(1, 1000, 1) == next);
	CHECK(RandRangeAvg(5, 4, 1) == 5);

	int32_t picked[5];
	RngSetSeed(42);
	RandSelect(1, 10, 5, picked);

	for (int32_t i = 0; i < 5; i++) {
		CHECK(picked[i] >= 1 && picked[i] <= 10);

		for (int32_t j = 0; j < i; j++)
			CHECK(picked[i] != picked[j]);
	}
}

static void test_date(void)
{
	struct date d = DateMake(13, 1, 1847);
	CHECK(d.month == 1 && d.day == 1 && d.year == 1848);

	d = DateMake(2, 30, 1848);
	CHECK(d.month == 3 && d.day == 1 && d.year == 1848);

	d = DateMake(3, 0, 1847);
	CHECK(d.month == 2 && d.day == 28 && d.year == 1847);

	d = DateMake(1, -1, 1848);
	CHECK(d.month == 12 && d.day == 30 && d.year == 1847);

	DateSetDay(&d, d.day + 400);
	CHECK(d.year == 1849 && d.month == 2 && d.day == 2);

	d = DateMake(4, 1, 1848);
	CHECK(DateDayOfWeek(&d) == 6);

	struct date a = DateMake(5, 1, 1848), b = DateMake(5, 2, 1848);
	CHECK(DateLess(&a, &b) && !DateLess(&b, &a) && DateLessEqual(&a, &a) && DateGreater(&b, &a));
	CHECK(DateDaysInMonth(2, 1848) == 29 && DateDaysInMonth(12, 1847) == 31 && DateDaysInMonth(13, 1847) == 0);
}

static void test_cstr(void)
{
	struct cstr *s = CStrCreateInt(-1234567, true);
	CHECK(strcmp(CStrGet(s), "-1,234,567") == 0);
	CStrDestroy(&s);

	s = CStrCreateInt(INT32_MIN, false);
	CHECK(strcmp(CStrGet(s), "-2147483648") == 0);
	CStrDestroy(&s);

	s = CStrCreateFrom("The %n crossed the %r.");
	CHECK(CStrReplaceAll(s, "%n", "Smith party"));
	CHECK(CStrReplaceAll(s, "%r", "Kansas River"));
	CHECK(strcmp(CStrGet(s), "The Smith party crossed the Kansas River.") == 0);
	CHECK(CStrLength(s) == (int32_t) strlen(CStrGet(s)));
	CHECK(!CStrReplaceAll(s, "zzz", "y"));

	CHECK(CStrGetWord(s, 2, 0));
	CHECK(strcmp(CStrGet(s), "Smith") == 0);
	CStrDestroy(&s);

	s = CStrCreateFrom("hello world");
	struct cstr *w = CStrCreate();
	CHECK(CStrSubstr2(s, -1, 5, w) && strcmp(CStrGet(w), "world") == 0);
	CHECK(CStrSubstr(s, 1, 5) && strcmp(CStrGet(s), "hello") == 0);
	CStrDestroy(&w);
	CStrDestroy(&s);

	s = CStrCreateFrom("-42");
	int32_t n = 0;
	CHECK(CStrToInt(s, &n) && n == -42);
	CStrDestroy(&s);

	struct cstr *a = CStrCreateFrom("abc"), *b = CStrCreateFrom("abd"), *c = CStrCreateFrom("ab");
	CHECK(CStrLess(a, b) && CStrGreater(b, a) && CStrLess(c, a) && CStrGreater(a, c) && !CStrLess(a, a));
	CStrDestroy(&a);
	CStrDestroy(&b);
	CStrDestroy(&c);

	char small[6];
	StrCopy(small, "truncated");
	CHECK(strcmp(small, "trunc") == 0);
	StrPrintf(small, "%d", 1234567);
	CHECK(strcmp(small, "12345") == 0);
}

struct item {
	struct list_node node;
	int32_t value;
};

static int32_t g_freed;

static void free_item(void *p)
{
	g_freed++;
	MemFree(p);
}

static void test_lists(void)
{
	struct list list;
	struct item items[4];
	ListInit(&list);

	for (int32_t i = 0; i < 4; i++) {
		memset(&items[i], 0, sizeof(items[i]));
		items[i].value = i;
		ListInsertTail(&list, &items[i].node);
	}

	ListMoveBefore(&list, &items[3].node, &items[0].node);
	CHECK(((struct item *) ListNodeAt(&list, 0))->value == 3);
	ListRemove(&list, &items[1].node);
	CHECK(list.count == 3 && ((struct item *) ListNodeAt(&list, 2))->value == 2);

	struct oblist *ol = OblistCreate(free_item);
	struct item *x = MemAlloc(1, sizeof(struct item));
	struct item *y = MemAlloc(1, sizeof(struct item));
	OblistPushBack(ol, x);
	OblistPushBack(ol, y);
	CHECK(OblistFirst(ol) == x && OblistNext(ol, true) == y && OblistNext(ol, true) == NULL);
	CHECK(OblistCurrent(ol) == y && OblistPrev(ol, false) == x);
	CHECK(OblistRemove(ol, x) && g_freed == 1 && OblistCount(ol) == 1);
	OblistDestroy(&ol);
	CHECK(g_freed == 2 && ol == NULL);

	struct obarray *arr = ObarrayCreate(NULL);
	int32_t v[3] = {1, 2, 3};
	ObarrayAppend(arr, &v[0]);
	ObarrayAppend(arr, &v[1]);
	CHECK(!ObarrayAddUnique(arr, &v[0]) && ObarrayAddUnique(arr, &v[2]));
	ObarrayRemoveAt(arr, 0);
	CHECK(ObarrayCount(arr) == 2 && ObarrayGetAt(arr, 0) == &v[1] && ObarrayIndexOf(arr, &v[2]) == 1);
	ObarrayDestroy(&arr);
}

static void test_bytes(void)
{
	uint8_t b[8];
	WriteBe16(b, -2);
	CHECK(ReadBe16(b) == -2 && ReadBeU16(b) == 0xFFFE);
	WriteLe32(b, 0x12345678u);
	CHECK(b[0] == 0x78 && ReadLeU32(b) == 0x12345678u);
	WriteLeU64(b, 0x0102030405060708ull);
	CHECK(ReadLeU64(b) == 0x0102030405060708ull);

	struct rect r = RectMake(10, 10, 20, 30), o = RectMake(15, 25, 40, 50), out;
	CHECK(RectIntersect(&r, &o, &out) && out.top == 15 && out.left == 25 && out.bottom == 20 && out.right == 30);
	struct rect u = RectUnionOf(&r, &o);
	CHECK(u.top == 10 && u.left == 10 && u.bottom == 40 && u.right == 50);
}

static void test_datafile(void)
{
	const char *path = "unit_datafile.tmp";
	struct datafile *w = DataFileOpenWrite(path);
	CHECK(DataFileWriteHeader(w, 0x4F543221u, 0x0200) == DATAFILE_OK);
	CHECK(DataFileWriteBlock(w, "abcdefgh", 8) == DATAFILE_OK);
	CHECK(DataFileWriteBlock(w, "", 0) == DATAFILE_OK);
	DataFileClose(&w);

	size_t size = 0;
	uint8_t *bytes = FileReadAll(path, &size);
	CHECK(size == 10 + 4 + 8 + 4);
	CHECK(bytes && memcmp(bytes, "MECCOT2!\x02\x00\x08\x00\x00\x00" "abcdefgh", 22) == 0);
	MemFree(bytes);

	struct datafile *r = DataFileOpenRead(path);
	CHECK(DataFileReadHeader(r, 0x4F543221u, 0x0102, 0x0200) == DATAFILE_OK && DataFileVersion(r) == 0x0200);

	int32_t n = 0;
	uint8_t *block = DataFileReadBlock(r, &n);
	CHECK(n == 8 && memcmp(block, "abcdefgh", 8) == 0);
	MemFree(block);
	block = DataFileReadBlock(r, &n);
	CHECK(n == 0);
	MemFree(block);

	block = DataFileReadBlock(r, &n);
	CHECK(n == 4096 && block[0] == 0 && block[4095] == 0 && DataFileStatus(r) == DATAFILE_OK);
	MemFree(block);
	DataFileClose(&r);

	r = DataFileOpenRead(path);
	CHECK(DataFileReadHeader(r, 0x4F543221u, 0x0300, 0x0400) == DATAFILE_BAD_VERSION);
	CHECK(DataFileReadHeader(r, 0x12345678u, 0x0100, 0x0400) == DATAFILE_WRONG_TYPE);
	DataFileClose(&r);
	FileDelete(path);
}

int main(void)
{
	test_datafile();
	test_rng();
	test_date();
	test_cstr();
	test_lists();
	test_bytes();

	printf("unit_foundation (%d-bit): %d checks, %d failed\n", (int) (sizeof(void *) * 8), g_checks, g_failures);

	return g_failures;
}
