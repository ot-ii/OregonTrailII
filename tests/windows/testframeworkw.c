#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <string.h>

#include <windows.h>

#include "../../src/ui/framewin.h"

static DWORD g_crash_rva;

static int caught(EXCEPTION_POINTERS *e, DWORD *code)
{
	*code = e->ExceptionRecord->ExceptionCode;
	g_crash_rva = (DWORD) ((char *) e->ExceptionRecord->ExceptionAddress - (char *) GetModuleHandleA(NULL));
	return EXCEPTION_EXECUTE_HANDLER;
}

bool TestCallGuarded(void (*fn)(void *), void *arg, uint32_t *code, uint32_t *rva)
{
	DWORD c = 0;
	bool ok = true;

	__try {
		fn(arg);
	} __except (caught(GetExceptionInformation(), &c)) {
		ok = false;
	}
	*code = c;
	*rva = g_crash_rva;
	return ok;
}

void TestRaiseFatal(void)
{
	RaiseException(TEST_FATAL_EXCEPTION, EXCEPTION_NONCONTINUABLE, 0, NULL);
}

const char *TestCommandLine(void)
{
	return GetCommandLineA();
}

void TestExeBase(char *path, size_t size)
{
	char *dot;

	if (!GetModuleFileNameA(NULL, path, (DWORD) size)) {
		path[0] = 0;
		return;
	}
	path[size - 1] = 0;
	dot = strrchr(path, '.');
	if (dot)
		*dot = 0;
}

void TestExitProcess(int32_t code)
{
	ExitProcess((UINT) code);
}

#define SNAP_MAX 256

static HWND g_snap[SNAP_MAX];
static int g_snap_n;

static BOOL CALLBACK snap_child(HWND child, LPARAM lparam)
{
	(void) lparam;

	if (g_snap_n < SNAP_MAX)
		g_snap[g_snap_n++] = child;
	return TRUE;
}

void TestSnapChildWindows(void)
{
	HWND hwnd = g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;

	g_snap_n = 0;
	if (hwnd)
		EnumChildWindows(hwnd, snap_child, 0);
}

static BOOL CALLBACK list_child(HWND child, LPARAM lparam)
{
	void (*out)(const char *line) = (void (*)(const char *)) lparam;
	char cls[64];
	char line[256];
	RECT r;
	int i;

	for (i = 0; i < g_snap_n && g_snap[i] != child; ++i)
		;
	if (i == g_snap_n)
		return TRUE;
	GetClassNameA(child, cls, sizeof(cls));
	GetWindowRect(child, &r);
	MapWindowPoints(NULL, GetParent(child), (POINT *) &r, 2);
	snprintf(line, sizeof(line), "CHILD %s %s at %ld,%ld %ldx%ld", cls, IsWindowVisible(child) ? "visible" : "hidden",
		r.left, r.top, r.right - r.left, r.bottom - r.top);
	out(line);
	return TRUE;
}

void TestListChildWindows(void (*out)(const char *line))
{
	HWND hwnd = g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;

	if (hwnd)
		EnumChildWindows(hwnd, list_child, (LPARAM) out);
}

#endif
