#pragma once

#include <stdbool.h>
#include <stdint.h>

struct file;

#define MOV_FCC_SMC 0x736D6320u
#define MOV_FCC_CVID 0x63766964u

struct mov_track {
	bool present;
	uint32_t fourcc;
	uint32_t timescale;
	int32_t delay_ms;
	int32_t media_start;
	int32_t media_length;
	int32_t samples;
	uint32_t *offset;
	uint32_t *size;
	int32_t *time;
	int32_t chunks;
	uint32_t *chunk_offset;
	int32_t *chunk_samples;
	int32_t width;
	int32_t height;
	int32_t depth;
	bool has_palette;
	uint8_t palette[256][3];
	int32_t channels;
	int32_t bits;
	int32_t rate;
};

struct mov_file {
	struct file *f;
	uint32_t timescale;
	int32_t duration_ms;
	struct mov_track video;
	struct mov_track sound;
	uint8_t *frame_buf;
	uint32_t frame_cap;
};

bool MovFileOpen(const char *path, struct mov_file *m);
void MovFileClose(struct mov_file *m);
int32_t MovFileFrameAt(const struct mov_file *m, int32_t ms);
bool MovFileReadFrame(struct mov_file *m, int32_t index, const uint8_t **data, uint32_t *size);
bool MovFileReadSound(struct mov_file *m, uint8_t **pcm, int32_t *bytes, int32_t *rate, int32_t *channels,
	int32_t *bits);
