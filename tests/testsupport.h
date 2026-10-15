#pragma once

#ifdef OTII_TEST

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool TestSave(const char *name);
bool TestLoad(const char *name);
void TestStatePath(const char *name, char *path, size_t size);

void TestPump(int32_t ms);

bool TestScreenshot(const char *name);

void TestPrefer(const char *label);
void TestDismisser(bool on);
void TestScreensClear(void);
const char *TestScreensSeen(void);
int32_t TestScreenCount(void);

struct test_state {
	double cash;
	int32_t food;
	int32_t members, alive;
	int32_t health[6], max_health[6], state[6], ailments[6];
	int32_t animals, spare_animals, wagons;
	int32_t inventory_items;
	int32_t weight;
	int32_t date_key;
	int32_t days_traveled, travel_state, pace, rations, morale;
	int32_t node;
};

void TestCapture(struct test_state *s);
void TestDiff(const struct test_state *before, const struct test_state *after, char *out, int32_t size);
int32_t TestCheckInvariants(const struct test_state *s, const char *when);

int32_t TestItemCount(const char *name);
bool TestSetItem(const char *name, int32_t units);

bool TestMakeDir(const char *path);
struct test_thread;
struct test_thread *TestThreadStart(void (*fn)(void *), void *arg);
void TestThreadJoin(struct test_thread **t);

#endif
