//

#pragma once

#if defined(OTII_AUTO_MODE) && !defined(CONTROL)
#define CONTROL 1
#endif

#if defined(OTII_TWITCH) && !defined(CONTROL)
#define CONTROL 1
#endif

#if defined(CLI_CONTROL) && !defined(CONTROL)
#define CONTROL 1
#endif

#ifdef CONTROL

#include <stdbool.h>
#include <stdint.h>

#include "../base/geom.h"

struct button;
struct combo_box;
struct edit;

enum control_screen {
	CONTROL_NONE,
	CONTROL_TRAVEL,
	CONTROL_EVENT,
	CONTROL_LANDMARK,
	CONTROL_DIALOG,
	CONTROL_TOWN,
};

struct control_option {
	int32_t index;
	char label[96];
	int32_t command;
	int32_t code;
	int32_t labelled;
	struct button *button;
	int32_t x, y;
	void *hwnd;
};

struct control_field {
	int32_t index;
	int32_t is_choice;
	int32_t choice_count;
	char text[128];
	char label[64];
	int32_t id;
	void *hwnd;
	struct combo_box *combo;
	struct edit *edit;
};

struct control_state {
	int32_t screen;
	void *hwnd;
	char title[128];
	int32_t has_game;
	int32_t year, month, day;
	char location[96];
	int32_t event_id;
	int32_t text_count;
	char text[24][1024];
	int32_t field_count;
	struct control_field fields[24];
	int32_t option_count;
	struct control_option options[64];
	int32_t side_text_first;
	int32_t side_option_first;
	int32_t info_count;
	char info[12][256];
	int32_t table_count;
	char table[32][200];
	struct rect text_box[24];
	void *text_hwnd[24];
};

int32_t ControlGetState(struct control_state *out);

int32_t ControlClick(const struct control_state *st, int32_t index);

int32_t ControlClickAt(const struct control_state *st, int32_t x, int32_t y);

int32_t ControlSetField(const struct control_state *st, int32_t index, const char *text);

int32_t ControlFieldChoices(const struct control_state *st, int32_t index, char (*names)[64], int32_t max);

int32_t ControlClose(const struct control_state *st);

int32_t ControlMenuItems(int32_t *ids, char (*names)[64], int32_t max);
int32_t ControlMenu(int32_t id);

int32_t ControlDump(char *buf, int32_t size);

int32_t ControlDebugText(char *buf, int32_t size);

int32_t ControlExportAnimals(const char *dir, char *report, int32_t size);

#endif
