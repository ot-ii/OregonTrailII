#include "testframework.h"

#ifdef OTII_TEST

#include <limits.h>
#include <string.h>

#include "../src/base/cstr.h"
#include "../src/base/str.h"

static bool is(const struct cstr *s, const char *want)
{
	const char *p = CStrGet(s);
	return p && (int32_t) strlen(want) == CStrLength(s) && !memcmp(p, want, (size_t) CStrLength(s));
}

static const char *text(const struct cstr *s, char *buf, int32_t size)
{
	const char *p = CStrGet(s);
	int32_t n = CStrLength(s) < size - 1 ? CStrLength(s) : size - 1;
	if (n < 0)
		n = 0;
	if (p)
		memcpy(buf, p, (size_t) n);
	buf[n] = 0;
	return buf;
}

#define CHECK_STR(s, want) do { char t_buf_[256]; \
	TCheck(is((s), (want)), #s " == \"" want "\"", __FILE__, __LINE__, "got \"%s\"", text((s), t_buf_, 256)); } while (0)

TEST(unit_str, construct)
{
	struct cstr *empty = CStrCreate(), *a = CStrCreateFrom("Oregon"), *b = CStrCreateN("Trail II", 5);
	struct cstr *c = CStrCreateCopy(a), *d = CStrCreateCopyMax(a, 3);

	CHECK_EQ(CStrLength(empty), 0);
	CHECK_STR(a, "Oregon");
	CHECK_STR(b, "Trail");
	CHECK_STR(c, "Oregon");
	CHECK_STR(d, "Ore");
	CStrDestroy(&empty);
	CStrDestroy(&a);
	CStrDestroy(&b);
	CStrDestroy(&c);
	CStrDestroy(&d);
}

TEST(unit_str, from_int)
{
	static const struct { int32_t v; bool grouping; const char *want; } cases[] = {
		{0, false, "0"}, {0, true, "0"}, {7, true, "7"}, {-7, false, "-7"}, {999, true, "999"},
		{1000, true, "1,000"}, {1000, false, "1000"}, {32767, true, "32,767"}, {-1234567, true, "-1,234,567"},
		{2147483647, true, "2,147,483,647"}, {2147483647, false, "2147483647"},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		char buf[64];
		struct cstr *s = CStrCreateInt(cases[i].v, cases[i].grouping);
		TCase("%d%s", cases[i].v, cases[i].grouping ? " grouped" : "");
		CHECK_MSG(is(s, cases[i].want), "got \"%s\", want \"%s\"", text(s, buf, 64), cases[i].want);
		CStrDestroy(&s);
	}
}

TEST(unit_str, from_int_min)
{
	struct cstr *s = CStrCreateInt(INT32_MIN, true), *t = CStrCreateInt(INT32_MIN, false);

	CHECK_STR(s, "-2,147,483,648");
	CHECK_STR(t, "-2147483648");
	CStrDestroy(&s);
	CStrDestroy(&t);
}

TEST(unit_str, to_int)
{
	static const struct { const char *s; bool ok; int32_t v; } cases[] = {
		{"0", true, 0}, {"42", true, 42}, {"-42", true, -42}, {"1848", true, 1848}, {"32767", true, 32767},
		{"", true, 0}, {"12a", false, 0}, {"a12", false, 0}, {" 12", false, 0}, {"--3", false, 0},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		struct cstr *s = CStrCreateFrom(cases[i].s);
		int32_t v = -999;
		bool ok = CStrToInt(s, &v);
		TCase("\"%s\"", cases[i].s);
		CHECK_EQ(!!ok, cases[i].ok);
		if (cases[i].ok)
			CHECK_EQ(v, cases[i].v);
		CStrDestroy(&s);
	}
}

TEST(unit_str, compare_and_concat)
{
	struct cstr *a = CStrCreateFrom("wagon"), *b = CStrCreateFrom("wagon"), *c = CStrCreateFrom("wheel");
	struct cstr *ab = CStrConcat(a, c);
	struct cstr *ax = CStrConcatBuf(a, " tongue");

	CHECK(CStrEqual(a, b));
	CHECK(!(CStrEqual(a, c)));
	CHECK(!CStrEqual(a, c));
	CHECK(CStrEqualBuf(a, "wagon"));
	CHECK(!CStrEqualBuf(a, "Wagon"));
	CHECK(CStrLess(a, c));
	CHECK(CStrGreater(c, a));
	CHECK_STR(ab, "wagonwheel");
	CHECK_STR(ax, "wagon tongue");
	CStrAppendBuf(a, "s");
	CHECK_STR(a, "wagons");
	CStrAssign(a, c);
	CHECK_STR(a, "wheel");
	CStrDestroy(&a);
	CStrDestroy(&b);
	CStrDestroy(&c);
	CStrDestroy(&ab);
	CStrDestroy(&ax);
}

TEST(unit_str, case_and_find)
{
	struct cstr *s = CStrCreateFrom("Fort Laramie");

	CHECK_EQ(CStrFindChar(s, 'L'), 5);
	CHECK_EQ(CStrCharAt(s, 0), 'F');
	CStrToUpper(s);
	CHECK_STR(s, "FORT LARAMIE");
	CStrToLower(s);
	CHECK_STR(s, "fort laramie");
	TNote("FindChar('z') on \"fort laramie\" = %d", CStrFindChar(s, 'z'));
	CStrDestroy(&s);
}

TEST(unit_str, replace_all)
{
	static const struct { const char *s, *find, *repl, *want; } cases[] = {
		{"@1 has died.", "@1", "Mary", "Mary has died."},
		{"@1 and @1", "@1", "Jo", "Jo and Jo"},
		{"no match", "@2", "x", "no match"},
		{"aaa", "a", "bb", "bbbbbb"},
		{"abcabc", "abc", "", ""},
		{"x", "longer", "y", "x"},
	};

	for (int32_t i = 0; i < (int32_t) (sizeof(cases) / sizeof(cases[0])); ++i) {
		char buf[128];
		struct cstr *s = CStrCreateFrom(cases[i].s);
		TCase("\"%s\" %s->%s", cases[i].s, cases[i].find, cases[i].repl);
		CStrReplaceAll(s, cases[i].find, cases[i].repl);
		CHECK_MSG(is(s, cases[i].want), "got \"%s\", want \"%s\"", text(s, buf, 128), cases[i].want);
		CStrDestroy(&s);
	}
}

TEST(unit_str, substr_and_words)
{
	struct cstr *s = CStrCreateFrom("one two three"), *out = CStrCreate();

	CHECK(CStrSubstr2(s, 0, 3, out));
	CHECK_STR(out, "one");
	CHECK(CStrSubstr2(s, 5, 3, out));
	CHECK_STR(out, "two");
	CHECK(CStrSubstr2(s, -1, 5, out));
	CHECK_STR(out, "three");
	CHECK(CStrSubstr2(s, 9, 100, out));
	CHECK_STR(out, "three");
	CHECK(!CStrSubstr2(s, 50, 2, out));
	CHECK(CStrGetWord2(s, 2, out, ' '));
	CHECK_STR(out, "two");
	CHECK(CStrGetWord2(s, 3, out, ' '));
	CHECK_STR(out, "three");
	CHECK(!CStrGetWord2(s, 4, out, ' '));
	CStrDestroy(&s);
	CStrDestroy(&out);
}

TEST(unit_str, safe_copy_truncates)
{
	char tiny[8], guard_after[8];
	char big[32];
	int32_t n;

	memset(guard_after, 0x5A, sizeof(guard_after));
	StrCopy(tiny, "Independence");
	CHECK(!strcmp(tiny, "Indepen"));
	StrCopy(tiny, "abc");
	StrCat(tiny, "defghijk");
	CHECK(!strcmp(tiny, "abcdefg"));
	StrCopyN(big, 1, "x");
	CHECK_EQ(big[0], 0);
	StrCopyN(big, sizeof(big), NULL);
	CHECK_EQ(big[0], 0);
	n = StrPrintf(tiny, "%d-%d", 1848, 1849);
	CHECK(!strcmp(tiny, "1848-18"));
	CHECK_EQ(n, 9);
	memset(tiny, 'q', sizeof(tiny));
	StrCat(tiny, "z");
	CHECK_EQ(tiny[7], 0);
	CHECK_EQ(guard_after[0], 0x5A);
}

const struct test_reg TESTS_unit_str[] = {
	TEST_ENTRY(unit_str, construct),
	TEST_ENTRY(unit_str, from_int),
	TEST_ENTRY(unit_str, from_int_min),
	TEST_ENTRY(unit_str, to_int),
	TEST_ENTRY(unit_str, compare_and_concat),
	TEST_ENTRY(unit_str, case_and_find),
	TEST_ENTRY(unit_str, replace_all),
	TEST_ENTRY(unit_str, substr_and_words),
	TEST_ENTRY(unit_str, safe_copy_truncates),
	TEST_END
};

#endif
