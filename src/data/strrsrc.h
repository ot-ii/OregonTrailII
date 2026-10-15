#pragma once

#include <stdbool.h>
#include <stdint.h>

struct assets;
struct cstr;
struct strrsrc;

struct strrsrc *StrRsrcCreate(void);
struct strrsrc *StrRsrcLoad(struct assets *lib, int32_t id, bool fatal);
struct strrsrc *StrRsrcCreateCopy(const struct strrsrc *src);
void StrRsrcDestroy(struct strrsrc **ctx_mod);

int32_t StrRsrcCount(const struct strrsrc *ctx);
int32_t StrRsrcId(const struct strrsrc *ctx);

const char *StrRsrcGetAt(const struct strrsrc *ctx, int32_t index);
struct cstr *StrRsrcGetCStr(const struct strrsrc *ctx, int32_t index);
void StrRsrcCopyTo(const struct strrsrc *ctx, int32_t index, char *buf, int32_t max);
int32_t StrRsrcStrlenAt(const struct strrsrc *ctx, int32_t index);
int32_t StrRsrcIndexOf(const struct strrsrc *ctx, const char *s);

void StrRsrcAppend(struct strrsrc *ctx, const char *s);
void StrRsrcAppendAll(struct strrsrc *ctx, const struct strrsrc *other);
void StrRsrcRemapHighChars(struct strrsrc *ctx, int32_t mode);
