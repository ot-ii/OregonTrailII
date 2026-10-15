#pragma once

#include <stdbool.h>
#include <stdint.h>

struct point;

enum replay_mode {
	RP_OFF,
	RP_RECORD,
	RP_REPLAY,
};

enum replay_query {
	RPQ_CURSOR = 1,
	RPQ_KEYSTATE,
	RPQ_INI_INT,
	RPQ_INI_STRING,
	RPQ_MESSAGEBOX,
	RPQ_TIME,
	RPQ_VIDEO_MODE,
	RPQ_SOUND_STATUS,
	RPQ_SOUND_POS,
	RPQ_SOUND_AMPLITUDE,
	RPQ_CLI_LINE,
	RPQ_CHAT,
	RPQ_PARTY_COUNT,
	RPQ_PARTY_ENTRY,
	RPQ_FILE_DIALOG,
	RPQ_ACTIVE_WINDOW,
	RPQ_SOUND_STARTED,
	RPQ_CROWD,
	RPQ_PARTY_STARTED,
};

int32_t RpMode(void);
void RpFlush(void);
void RpCheckpoint(uint32_t hash, int32_t day, int32_t cash, uint32_t rng_calls);
void RpSetCheckpoint(void (*fn)(void));
const char *RpFirstArgument(void);

uint32_t RpGetTickCount(void);
uint32_t RpClockNow(void);
uint32_t RpTime(void);
bool RpGetCursorPos(struct point *pt);
void RpSetVirtualCursor(int32_t x, int32_t y, int32_t ms);
int32_t RpGetKeyState(int32_t key);
int32_t RpSoundHandle(int32_t handle);
void RpNoteScale(int32_t scale);
int32_t RpQuery(int32_t kind, int32_t value);
bool RpQueryBytes(int32_t kind, void *buf, int32_t size, bool have);
void RpTraceRand(int32_t lo, int32_t hi, int32_t count, int32_t result);

uint32_t RpGetPrivateProfileString(const char *app, const char *key, const char *def, char *out, uint32_t size,
	const char *file);
uint32_t RpGetPrivateProfileInt(const char *app, const char *key, int32_t def, const char *file);
uint32_t RpIniString(char *out, uint32_t size, uint32_t n);
bool RpWritePrivateProfileString(const char *app, const char *key, const char *value, const char *file);
int32_t RpMessageBox(void *hwnd, const char *text, const char *caption, uint32_t type);
int32_t RpTrackPopupMenu(void *menu, uint32_t flags, int32_t x, int32_t y, void *hwnd);

bool RpPeekMessage(void *msg, void *hwnd, uint32_t first, uint32_t last, uint32_t remove);
intptr_t RpDispatchMessage(const void *msg);
uintptr_t RpSetTimer(void *hwnd, uintptr_t id, uint32_t elapse, void *proc);
int32_t RpWindowEnter(void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam);
void RpWindowLeave(int32_t token);

int32_t GameTicks(void);

void RpSetSoundStop(void (*stop)(int32_t handle));
void RpSetScalerHooks(void (*present_all)(void), void (*force)(int32_t scale));
int32_t RpReplayScale(void);
