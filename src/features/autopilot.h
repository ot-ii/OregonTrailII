#pragma once

#ifdef OTII_AUTO_MODE

#include <stdint.h>

#include "controlstate.h"

struct ford_wagon;

void AutoPilotStart(void);
void AutoPilotStop(void);
int32_t AutoPilotRunning(void);

#define AUTOPILOT_MENU_ID 6097
#define AUTOPILOT_NEXT_GAME_MENU_ID 6095
#define AUTOPILOT_SCORE_GOODS_MENU_ID 6094

void AutoPilotAppendMenu(void *popup);
void AutoPilotMenuUpdate(void *popup);
int32_t AutoPilotMenuCommand(void *hwnd, int32_t id);

void AutoBatchStart(void);
int32_t AutoBatchActive(void);
uint32_t AutoBatchTickMs(uint32_t normal);
int32_t AutoBatchNewGame(struct control_state *st);

#ifdef OTII_TEST
#include <stdbool.h>

struct inv_item;

int32_t AutoPilotTestMealHelping(int32_t level);
int32_t AutoPilotTestFoodDaysAt(int32_t level);
int32_t AutoPilotTestFoodShortEndgame(int32_t left);
int32_t AutoPilotTestArrivingLate(int32_t left);
int32_t AutoPilotTestWantedPace(void);
int32_t AutoPilotTestWantedRations(void);
int32_t AutoPilotTestDaysToGo(void);
int32_t AutoPilotTestStuckGunOk(int32_t food_days_n, int32_t days_left, int32_t refusals, int32_t days_to_gun_town_n,
	int32_t can_afford);
int32_t AutoPilotTestGunTowns(int32_t *landmarks, int32_t *days);
int32_t AutoPilotTestStuckTradeOk(struct inv_item *item, int32_t qty, int32_t refusals);
int32_t AutoPilotTestCanPullWithout(struct inv_item *item, int32_t qty);
void AutoPilotTestSteerRafts(bool on);
int32_t AutoPilotTestRaftRuns(void);
int32_t AutoPilotTestRaftSteering(void);
int32_t AutoPilotTestKeepWaiting(int32_t waits);
int32_t AutoPilotTestStrained(void);
int32_t AutoPilotTestLoadExcess(void);
int32_t AutoPilotTestDump(int32_t lb);
#endif

#endif
