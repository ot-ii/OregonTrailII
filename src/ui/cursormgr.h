#pragma once

#include <stdbool.h>
#include <stdint.h>

enum cursor_id {
	CURSOR_ARROW = 0,
	CURSOR_IBEAM = 1,
	CURSOR_CROSS = 2,
	CURSOR_WAIT = 3,
	CURSOR_SCENE_BASE = 200,
	CURSOR_LEFT = 201,
	CURSOR_RIGHT = 202,
	CURSOR_UP = 203,
	CURSOR_DOWN = 204,
	CURSOR_HOUSE = 205,
	CURSOR_PAGE = 206,
	CURSOR_DOLLAR = 207,
	CURSOR_ROUND_SIGN = 208,
	CURSOR_EXIT = 209,
	CURSOR_TALK = 210,
	CURSOR_CROSSHAIR = 211,
};

struct cursor_mgr;

struct cursor_mgr *CursorMgrCreate(void);
void CursorMgrDestroy(struct cursor_mgr **ctx_mod);

void CursorSetId(int32_t id);
int32_t CursorFindOrCreate(int32_t id);
void CursorRemoveById(int32_t id);
void CursorSetBusy(bool sel);

extern struct cursor_mgr *g_cursor_mgr;
