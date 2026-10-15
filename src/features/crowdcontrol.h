#pragma once

#include <stdbool.h>
#include <stdint.h>

struct crowd_net;
struct crowd_config;

#define CROWD_CONTROL_MENU_ID 6093

void CrowdControlStart(void);
void CrowdControlStop(void);
void CrowdControlToggleWindow(void);

bool CrowdControlWritePack(const char *path);

const char *CrowdControlHandle(const char *payload, char *message, int32_t message_size);
int32_t CrowdControlTimedCount(void);
void CrowdControlAdvanceTimed(int32_t ms);
bool CrowdControlConnect(const struct crowd_config *cfg, bool sign_in);
int32_t CrowdControlState(char *text, int32_t size);
void CrowdControlDisconnect(void);
void CrowdControlSetQuiet(bool quiet);
const char *CrowdControlItemKey(int32_t index, const char **give_effect);
bool CrowdControlPackVariables(char *out, int32_t size);
void CrowdControlResendVariables(void);
void *CrowdControlWindowInfo(bool *fonts_loaded, bool *logo_loaded, int32_t *buttons_shown);
struct crowd_net *CrowdControlNet(void);
void CrowdControlTick(void);
void CrowdControlLog(const char *line);
int32_t CrowdControlLogVersion(void);
int32_t CrowdControlLogCount(void);
const char *CrowdControlLogEntry(int32_t index, int32_t *viewer_len, bool *failed);

uintptr_t CrowdControlStartTimer(void);
void CrowdControlStopTimer(uintptr_t timer);
void CrowdControlCloseWindow(void);
bool CrowdControlMainWindowEnabled(void);
