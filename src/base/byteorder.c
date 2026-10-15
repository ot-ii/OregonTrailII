#include "byteorder.h"

int32_t ReadBe16(const uint8_t *p)
{
	return (int16_t) (uint16_t) (p[0] << 8 | p[1]);
}

uint32_t ReadBeU16(const uint8_t *p)
{
	return (uint32_t) (p[0] << 8 | p[1]);
}

int32_t ReadBe32(const uint8_t *p)
{
	return (int32_t) ReadBeU32(p);
}

uint32_t ReadBeU32(const uint8_t *p)
{
	return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3];
}

int32_t ReadLe16(const uint8_t *p)
{
	return (int16_t) (uint16_t) (p[1] << 8 | p[0]);
}

uint32_t ReadLeU16(const uint8_t *p)
{
	return (uint32_t) (p[1] << 8 | p[0]);
}

int32_t ReadLe32(const uint8_t *p)
{
	return (int32_t) ReadLeU32(p);
}

uint32_t ReadLeU32(const uint8_t *p)
{
	return (uint32_t) p[3] << 24 | (uint32_t) p[2] << 16 | (uint32_t) p[1] << 8 | p[0];
}

uint64_t ReadLeU64(const uint8_t *p)
{
	return (uint64_t) ReadLeU32(p + 4) << 32 | ReadLeU32(p);
}

void WriteBe16(uint8_t *p, int32_t v)
{
	p[0] = (uint8_t) (v >> 8);
	p[1] = (uint8_t) v;
}

void WriteBe32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t) (v >> 24);
	p[1] = (uint8_t) (v >> 16);
	p[2] = (uint8_t) (v >> 8);
	p[3] = (uint8_t) v;
}

void WriteLe16(uint8_t *p, int32_t v)
{
	p[0] = (uint8_t) v;
	p[1] = (uint8_t) (v >> 8);
}

void WriteLe32(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t) v;
	p[1] = (uint8_t) (v >> 8);
	p[2] = (uint8_t) (v >> 16);
	p[3] = (uint8_t) (v >> 24);
}

void WriteLeU64(uint8_t *p, uint64_t v)
{
	WriteLe32(p, (uint32_t) v);
	WriteLe32(p + 4, (uint32_t) (v >> 32));
}
