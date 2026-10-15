#pragma once

#include <stdbool.h>
#include <stdint.h>

struct canvas;
struct view;

extern struct canvas *g_title_canvas;

void TitleAnimStart(struct view *root);
void TitleAnimStop(void);
bool TitleAnimRunning(void);
int32_t TitleAnimFramesDrawn(void);
