#include "cursormgr.h"

#include <stddef.h>

#include "cursor.h"
#include "../data/gfxdev.h"
#include "../base/list.h"
#include "../base/memory.h"

struct cursor_mgr {
	int32_t unused;
};

static struct obarray *g_cursor_list;
static struct cursor *g_cursor_active;
static struct cursor *g_cursor_wait;
static int32_t g_cursor_cur_id;
static bool g_cursor_busy;

struct cursor_mgr *CursorMgrCreate(void)
{
	struct cursor_mgr *ctx = MemAlloc(1, sizeof(struct cursor_mgr));

	g_cursor_list = ObarrayCreate(NULL);
	g_cursor_cur_id = 0;
	g_cursor_active = CursorCreateKind(0);
	ObarrayAppend(g_cursor_list, g_cursor_active);
	CURSOR_OPS(g_cursor_active)->Set(g_cursor_active);
	g_cursor_wait = CursorCreateKind(3);

	return ctx;
}

void CursorMgrDestroy(struct cursor_mgr **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct cursor_mgr *ctx = *ctx_mod;
	*ctx_mod = NULL;

	int32_t index = ObarrayCount(g_cursor_list);

	while (index > 0) {
		struct cursor *cursor = ObarrayGetAt(g_cursor_list, index - 1);

		if (cursor)
			ObjectDelete(cursor);

		--index;
	}

	ObarrayDestroy(&g_cursor_list);
	CursorDestroy(&g_cursor_wait);
	g_cursor_active = NULL;

	MemFree(ctx);
}

void CursorSetId(int32_t id)
{
	g_cursor_cur_id = id;
	g_cursor_active = ObarrayGetAt(g_cursor_list, CursorFindOrCreate(id));

	if (!g_cursor_busy && g_cursor_active)
		CURSOR_OPS(g_cursor_active)->Set(g_cursor_active);
}

int32_t CursorFindOrCreate(int32_t id)
{
	int32_t index = ObarrayCount(g_cursor_list);

	while (index > 0) {
		struct cursor *cursor = ObarrayGetAt(g_cursor_list, index - 1);

		if (cursor && cursor->cursor_id == id)
			return index - 1;

		--index;
	}

	struct cursor *cursor;

	if (id < 5) {
		cursor = CursorCreateKind(id);

	} else {
		cursor = CursorCreateLib(g_data_lib, id);
	}

	if (cursor->cursor_id) {
		ObarrayAppend(g_cursor_list, cursor);
		return ObarrayCount(g_cursor_list) - 1;
	}

	ObjectDelete(cursor);

	return 0;
}

void CursorRemoveById(int32_t id)
{
	int32_t index = ObarrayCount(g_cursor_list);

	while (index > 1) {
		struct cursor *cursor = ObarrayGetAt(g_cursor_list, index - 1);

		if ((cursor && cursor->cursor_id == id) || id == -1) {
			ObarrayRemoveAt(g_cursor_list, index - 1);
			ObjectDelete(cursor);
		}

		--index;
	}
}

void CursorSetBusy(bool sel)
{
	g_cursor_busy = sel;

	if (sel) {
		CURSOR_OPS(g_cursor_wait)->Set(g_cursor_wait);

	} else {
		CURSOR_OPS(g_cursor_active)->Set(g_cursor_active);
	}
}
