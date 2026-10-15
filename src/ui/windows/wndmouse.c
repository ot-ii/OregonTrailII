#include "../wndmouse.h"

#include <stdbool.h>
#include <stddef.h>

#include "../../app/event.h"
#include "../../base/geom.h"
#include "../window.h"

void WndHandleLButtonDown(void *hwnd, int32_t a2, int32_t horizontal, int32_t vertical, int32_t key_flags)
{
	(void) a2;
	(void) key_flags;

	struct point pos = PointMake(horizontal, vertical);
	struct window *win = WindowFromHwnd(hwnd);

	if (!win)
		return;

	WINDOW_OPS(win)->HandleMouse(win, true, &pos, MOUSE_DOWN, 0, 0);
}

void WndHandleLButtonUp(void *hwnd, int32_t horizontal, int32_t vertical, int32_t key_flags)
{
	(void) key_flags;

	struct point pos = PointMake(horizontal, vertical);
	struct window *win = WindowFromHwnd(hwnd);

	if (!win)
		return;

	WINDOW_OPS(win)->HandleMouse(win, false, &pos, MOUSE_UP, 0, 0);
}

void WndHandleMouseMove(void *hwnd, int32_t horizontal, int32_t vertical, int32_t key_flags)
{
	struct point pos = PointMake(horizontal, vertical);
	struct window *win = WindowFromHwnd(hwnd);

	if (!win)
		return;

	WINDOW_OPS(win)->HandleMouse(win, key_flags & 1, &pos, MOUSE_MOVE, 0, 0);
}
