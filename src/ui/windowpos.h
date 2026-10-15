#pragma once

#include <stdbool.h>

void WindowPosSave(void *hwnd);
bool WindowPosRestore(void *hwnd);

void WindowPosSaveAs(void *hwnd, const char *name);
bool WindowPosRestoreAs(void *hwnd, const char *name);

void WindowPlaceBeside(void *wnd, void *anchor, bool right, bool match_height);
