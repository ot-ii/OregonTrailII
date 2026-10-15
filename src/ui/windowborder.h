#pragma once

#include <stdint.h>

struct rect;

intptr_t CustomBorderWndProc(void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);
void GetWindowOuterRect(void *hwnd, struct rect *rect);
