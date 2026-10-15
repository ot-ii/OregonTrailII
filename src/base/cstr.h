#pragma once

#include <stdbool.h>
#include <stdint.h>

struct cstr;

struct cstr *CStrCreate(void);
struct cstr *CStrCreateFrom(const char *s);
struct cstr *CStrCreateBytes(int32_t len, const char *buf);
struct cstr *CStrCreateCopy(const struct cstr *src);
struct cstr *CStrCreateCopyMax(const struct cstr *src, int32_t maxlen);
struct cstr *CStrCreateN(const char *s, int32_t n);
struct cstr *CStrCreateInt(int32_t value, bool grouping);
void CStrDestroy(struct cstr **ctx_mod);

const char *CStrGet(const struct cstr *ctx);
char *CStrGetMutable(struct cstr *ctx);
int32_t CStrLength(const struct cstr *ctx);

void CStrAppendBytes(struct cstr *ctx, const char *src, int32_t n);
struct cstr *CStrAssign(struct cstr *ctx, const struct cstr *src);
struct cstr *CStrAssignBuf(struct cstr *ctx, const char *s);
struct cstr *CStrAppendStr(struct cstr *ctx, const struct cstr *src);
struct cstr *CStrAppendBuf(struct cstr *ctx, const char *s);
void CStrAppend(struct cstr *ctx, const struct cstr *src);

struct cstr *CStrConcat(const struct cstr *a, const struct cstr *b);
struct cstr *CStrConcatBuf(const struct cstr *a, const char *b);
struct cstr *CStrConcatBufStr(const char *a, const struct cstr *b);
bool CStrEqual(const struct cstr *a, const struct cstr *b);
bool CStrEqualBuf(const struct cstr *a, const char *b);
bool CStrGreater(const struct cstr *a, const struct cstr *b);
bool CStrLess(const struct cstr *a, const struct cstr *b);

void CStrCopyToBuffer(const struct cstr *ctx, char *buf, int32_t size);
void CStrFromPascal(struct cstr *ctx, const char *pstr);
char CStrCharAt(const struct cstr *ctx, int32_t i);
int32_t CStrFindChar(const struct cstr *ctx, char c);
void CStrMacRomanToAnsi(struct cstr *ctx, int32_t mode);
void CStrToUpper(struct cstr *ctx);
void CStrToLower(struct cstr *ctx);
bool CStrToInt(const struct cstr *ctx, int32_t *out);

bool CStrReplaceAll(struct cstr *ctx, const char *find, const char *repl);
bool CStrReplaceObjs(struct cstr *ctx, const struct cstr *find, const struct cstr *repl);
bool CStrReplaceStr(struct cstr *ctx, const char *find, const struct cstr *repl);
bool CStrReplaceRsrc(struct cstr *ctx, const struct cstr *find, const char *repl);

bool CStrSubstr(struct cstr *ctx, int32_t mode, int32_t count);
bool CStrSubstr2(struct cstr *ctx, int32_t mode, int32_t count, struct cstr *out);
bool CStrGetWord(struct cstr *ctx, int32_t mode, char delim);
bool CStrGetWord2(struct cstr *ctx, int32_t mode, struct cstr *out, char delim);
bool CStrParseWord(struct cstr *ctx, int32_t mode, int32_t *pos, int32_t *len, char delim);
