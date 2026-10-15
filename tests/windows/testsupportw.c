#include "../testsupport.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>

#include "../../src/features/controlstate.h"
#include "../../src/ui/framewin.h"
#include "../../src/base/memory.h"
#include "../../src/base/str.h"
#include "../testframework.h"

static char g_prefer[96];
static char g_seen[4096];
static int32_t g_seen_count;
static UINT_PTR g_dismiss_timer;
static int32_t g_in_dismiss;
static bool g_dismiss_off;

void TestDismisser(bool on)
{
	g_dismiss_off = !on;
}

bool TestMakeDir(const char *path)
{
	return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

struct test_thread {
	HANDLE handle;
	void (*fn)(void *);
	void *arg;
};

static DWORD WINAPI test_thread_main(LPVOID param)
{
	struct test_thread *t = param;

	t->fn(t->arg);
	return 0;
}

struct test_thread *TestThreadStart(void (*fn)(void *), void *arg)
{
	struct test_thread *t = MemAlloc(1, sizeof(struct test_thread));

	t->fn = fn;
	t->arg = arg;
	t->handle = CreateThread(NULL, 0, test_thread_main, t, 0, NULL);
	if (!t->handle) {
		MemFree(t);
		return NULL;
	}
	return t;
}

void TestThreadJoin(struct test_thread **t)
{
	if (!*t)
		return;
	WaitForSingleObject((*t)->handle, INFINITE);
	CloseHandle((*t)->handle);
	MemFree(*t);
	*t = NULL;
}

static void remember(const struct control_state *st)
{
	char line[256];
	int32_t n;

	n = snprintf(line, sizeof(line), "%s: %s", st->title, st->text_count > 0 ? st->text[0] : "");
	if (n < 0)
		return;
	if (strlen(g_seen) + strlen(line) + 2 < sizeof(g_seen)) {
		if (g_seen[0])
			StrCat(g_seen, "|");
		StrCat(g_seen, line);
	}
	++g_seen_count;
}

static bool click_label(const struct control_state *st, const char *label, int32_t first, int32_t last)
{
	for (int32_t i = first; i < last; ++i)
		if (!_stricmp(st->options[i].label, label)) {
			ControlClick(st, st->options[i].index);
			return true;
		}
	return false;
}

static void CALLBACK dismiss(HWND hwnd, UINT msg, UINT_PTR id, DWORD time)
{
	static const char *const usual[] = {"OK", "Continue", "Back to travel", "Yes", "Done", "Close", "Cancel"};
	const int32_t usual_count = (int32_t) (sizeof(usual) / sizeof(usual[0]));
	struct control_state *st;
	int32_t i, last;

	(void) hwnd;
	(void) msg;
	(void) id;
	(void) time;
	if (g_in_dismiss >= 6 || g_dismiss_off)
		return;
	++g_in_dismiss;
	st = MemAlloc(1, sizeof(*st));
	if (!ControlGetState(st) || (st->screen != CONTROL_DIALOG && st->screen != CONTROL_EVENT) ||
			(last = st->side_option_first) <= 0) {
		MemFree(st);
		--g_in_dismiss;
		return;
	}
	remember(st);
	if (!strcmp(st->title, "Conversation") && !(g_prefer[0] && click_label(st, g_prefer, 0, last))) {
		ControlClick(st, st->options[last - 1].index);

	} else if (!(g_prefer[0] && click_label(st, g_prefer, 0, last))) {
		for (i = 0; i < usual_count; ++i)
			if (click_label(st, usual[i], 0, last))
				break;
		if (i == usual_count) {
			for (i = 0; i < last; ++i)
				if (strcmp(st->options[i].label, "Map") && strcmp(st->options[i].label, "Journal") &&
						strcmp(st->options[i].label, "Party health") && strcmp(st->options[i].label, "Supplies")) {
					ControlClick(st, st->options[i].index);
					break;
				}
		}
	}
	MemFree(st);
	--g_in_dismiss;
}

void TestPump(int32_t ms)
{
	DWORD until = GetTickCount() + (DWORD) ms;
	MSG msg;

	if (!g_dismiss_timer)
		g_dismiss_timer = SetTimer(NULL, 0, 60, dismiss);
	do {
		while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}
		Sleep(5);
	} while ((LONG) (GetTickCount() - until) < 0);
}

void TestPrefer(const char *label)
{
	StrCopy(g_prefer, label ? label : "");
}

void TestScreensClear(void)
{
	g_seen[0] = 0;
	g_seen_count = 0;
}

const char *TestScreensSeen(void)
{
	return g_seen;
}

int32_t TestScreenCount(void)
{
	return g_seen_count;
}

bool TestScreenshot(const char *name)
{
	HWND hwnd = g_parent_win ? (HWND) g_parent_win->base.hwnd : NULL;
	RECT c;
	HDC win, mem;
	HBITMAP bmp;
	BITMAPINFOHEADER bi;
	BITMAPFILEHEADER bf;
	char path[MAX_PATH], *dot;
	unsigned char *bits;
	int w, h, stride;
	bool ok = false;
	FILE *f;

	if (!hwnd || !GetClientRect(hwnd, &c) || c.right <= 0 || c.bottom <= 0)
		return false;
	w = c.right;
	h = c.bottom;
	stride = (w * 3 + 3) & ~3;
	win = GetDC(hwnd);
	mem = CreateCompatibleDC(win);
	bmp = CreateCompatibleBitmap(win, w, h);
	SelectObject(mem, bmp);
	BitBlt(mem, 0, 0, w, h, win, 0, 0, SRCCOPY);
	memset(&bi, 0, sizeof(bi));
	bi.biSize = sizeof(bi);
	bi.biWidth = w;
	bi.biHeight = h;
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bits = (unsigned char *) malloc((size_t) stride * (size_t) h);
	if (bits && GetDIBits(mem, bmp, 0, (UINT) h, bits, (BITMAPINFO *) &bi, DIB_RGB_COLORS)) {
		GetModuleFileNameA(NULL, path, sizeof(path));
		if ((dot = strrchr(path, '.')) != NULL)
			*dot = 0;
		StrCat(path, "_");
		StrCat(path, name);
		StrCat(path, ".bmp");
		memset(&bf, 0, sizeof(bf));
		bf.bfType = 0x4D42;
		bf.bfOffBits = sizeof(bf) + sizeof(bi);
		bf.bfSize = bf.bfOffBits + (DWORD) stride * (DWORD) h;
		if (!fopen_s(&f, path, "wb") && f) {
			fwrite(&bf, sizeof(bf), 1, f);
			fwrite(&bi, sizeof(bi), 1, f);
			fwrite(bits, (size_t) stride, (size_t) h, f);
			fclose(f);
			ok = true;
		}
	}
	free(bits);
	DeleteObject(bmp);
	DeleteDC(mem);
	ReleaseDC(hwnd, win);
	return ok;
}

#endif
