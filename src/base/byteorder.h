#pragma once

#include <stdint.h>

int32_t ReadBe16(const uint8_t *p);
uint32_t ReadBeU16(const uint8_t *p);
int32_t ReadBe32(const uint8_t *p);
uint32_t ReadBeU32(const uint8_t *p);
int32_t ReadLe16(const uint8_t *p);
uint32_t ReadLeU16(const uint8_t *p);
int32_t ReadLe32(const uint8_t *p);
uint32_t ReadLeU32(const uint8_t *p);
uint64_t ReadLeU64(const uint8_t *p);

void WriteBe16(uint8_t *p, int32_t v);
void WriteBe32(uint8_t *p, uint32_t v);
void WriteLe16(uint8_t *p, int32_t v);
void WriteLe32(uint8_t *p, uint32_t v);
void WriteLeU64(uint8_t *p, uint64_t v);
