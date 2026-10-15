#pragma once

#include <stdbool.h>
#include <stdint.h>

struct crowd_net;

struct crowd_config {
	char server[256];
	char api[256];
	char app_id[128];
	char secret[256];
	char scopes[256];
	char pack_id[128];
	char token_file[260];
	char user_agent[64];
	bool auto_session;
	bool no_browser;
	int32_t ping_idle_ms;
	int32_t pong_wait_ms;
};

enum crowd_state {
	CROWD_OFF = 0,
	CROWD_CONNECTING,
	CROWD_SIGN_IN,
	CROWD_SIGNED_IN,
	CROWD_SESSION,
};

struct crowd_net *CrowdNetCreate(const struct crowd_config *config);
void CrowdNetDestroy(struct crowd_net **ctx_mod);
bool CrowdNetPoll(struct crowd_net *ctx, char *buf, int32_t size);
int32_t CrowdNetState(struct crowd_net *ctx, char *text, int32_t size);
void CrowdNetRespond(struct crowd_net *ctx, const char *request_id, const char *status, const char *message,
	int32_t time_remaining_ms);
void CrowdNetReport(struct crowd_net *ctx, const char *ids_json, const char *status);
void CrowdNetPackMetadata(struct crowd_net *ctx, const char *values_json);
void CrowdNetSignIn(struct crowd_net *ctx);
void CrowdNetSignOut(struct crowd_net *ctx);
void CrowdNetSession(struct crowd_net *ctx, bool on);
void CrowdNetSendRaw(struct crowd_net *ctx, const char *json);
int32_t CrowdNetConnections(struct crowd_net *ctx);
int64_t CrowdNetTokenExpiry(struct crowd_net *ctx);
bool CrowdNetTokenClaims(const char *token, int64_t *exp, char *name, int32_t name_size);
bool CrowdNetTokenString(const char *token, const char *claim, char *out, int32_t size);
bool CrowdNetCodeChallenge(const char *verifier, char *out, int32_t size);
