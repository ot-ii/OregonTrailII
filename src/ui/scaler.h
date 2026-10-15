#pragma once

#include <stdbool.h>
#include <stdint.h>

struct point;
struct rect;

#define SCALER_MAX 4
#define SCALER_MENU_FIRST 6301
#define SCALER_MENU_CURSOR 6305

int32_t ScalerFloorDiv(int32_t value, int32_t scale);
int32_t ScalerLargestFit(int32_t game_w, int32_t game_h, int32_t frame_w, int32_t frame_h, int32_t work_w,
	int32_t work_h);

void ScalerProcessInit(void);
int32_t ScalerScale(void);
void ScalerWindowCreated(void *hwnd, int32_t game_w, int32_t game_h);
void *ScalerAttach(void *hwnd, void *window_dc);
void ScalerDetach(void *hwnd);
void ScalerPresent(void *hwnd, bool full);
void ScalerPresentAll(void);
void ScalerScreenToDc(void *dc, struct point *pt);
void ScalerDcToScreen(void *dc, struct point *pt);
void ScalerScreenToWindow(void *hwnd, struct point *pt);
int32_t ScalerToGame(int32_t client_value);
void ScalerRectToClient(struct rect *r);
void ScalerChild(void *child, int32_t x, int32_t y, int32_t w, int32_t h, void *font);
void ScalerSet(int32_t scale, bool remember);
void ScalerForce(int32_t scale);
int32_t ScalerInitial(void);
void ScalerMenuAppend(void *popup);
void ScalerMenuUpdate(void *popup);
bool ScalerMenuCommand(void *hwnd, int32_t id);
intptr_t ScalerDpiChanged(void *hwnd, uintptr_t wparam, intptr_t lparam);
void ScalerSizeMove(void *hwnd, bool dragging);
bool ScalerDpiScaledSize(void *hwnd, uint32_t dpi, int32_t *cx, int32_t *cy);
int32_t ScalerDpiPx(void *hwnd, int32_t px);
bool ScalerGameSize(void *hwnd, int32_t *w, int32_t *h);
void *ScalerCursor(void *base);
void ScalerCursorForget(void *base);
int32_t ScalerCursorScale(void);
int32_t ScalerMetric(void *hwnd, int32_t index);

#ifdef OTII_TEST
int32_t ScalerTestPresents(void);
int32_t ScalerTestCovers(void *hwnd, int32_t x, int32_t y);
#endif
