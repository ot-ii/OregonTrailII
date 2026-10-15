#pragma once

#include <stdint.h>

void WndHandleLButtonDown(void *hwnd, int32_t a2, int32_t horizontal, int32_t vertical, int32_t key_flags);
void WndHandleLButtonUp(void *hwnd, int32_t horizontal, int32_t vertical, int32_t key_flags);
void WndHandleMouseMove(void *hwnd, int32_t horizontal, int32_t vertical, int32_t key_flags);
