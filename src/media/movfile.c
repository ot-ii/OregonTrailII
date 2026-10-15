#include "movfile.h"

#include <stddef.h>
#include <string.h>

#include "../base/byteorder.h"
#include "../base/file.h"
#include "../base/memory.h"

#define FCC_RAW 0x72617720u
#define FCC_TWOS 0x74776F73u
#define FCC_IMA4 0x696D6134u

static uint32_t fcc(const char *s)
{
	return ReadBeU32((const uint8_t *) s);
}

static void default_palette(uint8_t (*pal)[3])
{
	static const uint8_t cube[6] = {0xFF, 0xCC, 0x99, 0x66, 0x33, 0x00};
	static const uint8_t ramp[10] = {0xEE, 0xDD, 0xBB, 0xAA, 0x88, 0x77, 0x55, 0x44, 0x22, 0x11};
	int32_t n = 0;

	for (int32_t r = 0; r < 6; ++r)
		for (int32_t g = 0; g < 6; ++g)
			for (int32_t b = 0; b < 6; ++b)
				if (n < 215) {
					pal[n][0] = cube[r];
					pal[n][1] = cube[g];
					pal[n][2] = cube[b];
					++n;
				}

	for (int32_t c = 0; c < 4; ++c)
		for (int32_t k = 0; k < 10; ++k) {
			pal[n][0] = (c == 0 || c == 3) ? ramp[k] : 0;
			pal[n][1] = (c == 1 || c == 3) ? ramp[k] : 0;
			pal[n][2] = (c == 2 || c == 3) ? ramp[k] : 0;
			++n;
		}

	pal[255][0] = pal[255][1] = pal[255][2] = 0;
}

struct atom {
	const uint8_t *body;
	uint32_t size;
};

static bool find(const uint8_t *p, const uint8_t *end, const char *type, struct atom *out)
{
	while (end - p >= 8) {
		uint32_t size = ReadBeU32(p);

		if (size < 8 || size > (uint32_t) (end - p))
			return false;

		if (!memcmp(p + 4, type, 4)) {
			out->body = p + 8;
			out->size = size - 8;
			return true;
		}

		p += size;
	}

	return false;
}

struct tables {
	struct atom stsd;
	struct atom stts;
	struct atom stsc;
	struct atom stsz;
	struct atom stco;
};

static void free_track(struct mov_track *t)
{
	MemFree(t->offset);
	MemFree(t->size);
	MemFree(t->time);
	MemFree(t->chunk_offset);
	MemFree(t->chunk_samples);
	memset(t, 0, sizeof(*t));
}

static bool read_chunks(struct mov_track *t, const struct tables *tb)
{
	if (tb->stco.size < 8)
		return false;

	int32_t n = ReadBe32(tb->stco.body + 4);

	if (n <= 0 || (uint32_t) n > (tb->stco.size - 8) / 4)
		return false;

	t->chunk_offset = MemAlloc((size_t) n, sizeof(uint32_t));
	t->chunk_samples = MemAlloc((size_t) n, sizeof(int32_t));
	t->chunks = n;

	for (int32_t i = 0; i < n; ++i)
		t->chunk_offset[i] = ReadBeU32(tb->stco.body + 8 + 4 * i);

	if (tb->stsc.size < 8)
		return false;

	int32_t runs = ReadBe32(tb->stsc.body + 4);

	if (runs <= 0 || (uint32_t) runs > (tb->stsc.size - 8) / 12)
		return false;

	for (int32_t r = 0; r < runs; ++r) {
		const uint8_t *e = tb->stsc.body + 8 + 12 * r;
		int32_t first = ReadBe32(e) - 1;
		int32_t last = r + 1 < runs ? ReadBe32(e + 12) - 1 : n;
		int32_t per = ReadBe32(e + 4);

		for (int32_t i = first; i < last && i < n; ++i)
			if (i >= 0)
				t->chunk_samples[i] = per;
	}

	return true;
}

static bool read_video(struct mov_track *t, const struct tables *tb)
{
	if (tb->stsd.size < 8 + 86)
		return false;

	const uint8_t *e = tb->stsd.body + 8;
	t->fourcc = ReadBeU32(e + 4);

	if (t->fourcc != MOV_FCC_SMC && t->fourcc != MOV_FCC_CVID)
		return false;

	t->width = (int32_t) ReadBeU16(e + 32);
	t->height = (int32_t) ReadBeU16(e + 34);
	t->depth = ReadBe16(e + 82);

	if (t->width <= 0 || t->height <= 0 || t->width > 2048 || t->height > 2048)
		return false;

	if ((t->depth & 0x1F) == 8) {
		uint32_t esz = ReadBeU32(e);
		int32_t ctab = ReadBe16(e + 84);
		default_palette(t->palette);

		if (ctab == 0 && esz >= 86 + 8 && esz <= tb->stsd.size - 8) {
			int32_t count = (int32_t) ReadBeU16(e + 86 + 6) + 1;

			if (count > 256)
				count = 256;

			for (int32_t i = 0; i < count && 86 + 8 + 8 * (uint32_t) (i + 1) <= esz; ++i) {
				const uint8_t *ce = e + 86 + 8 + 8 * i;
				int32_t idx = (int32_t) ReadBeU16(ce) & 0xFF;
				t->palette[idx][0] = ce[2];
				t->palette[idx][1] = ce[4];
				t->palette[idx][2] = ce[6];
			}
		}

		t->has_palette = true;
	}

	if (tb->stsz.size < 12)
		return false;

	int32_t fixed = ReadBe32(tb->stsz.body + 4);
	t->samples = ReadBe32(tb->stsz.body + 8);

	if (t->samples <= 0 || t->samples > 1000000 || (!fixed && (uint32_t) t->samples > (tb->stsz.size - 12) / 4))
		return false;

	t->offset = MemAlloc((size_t) t->samples, sizeof(uint32_t));
	t->size = MemAlloc((size_t) t->samples, sizeof(uint32_t));
	t->time = MemAlloc((size_t) t->samples, sizeof(int32_t));

	for (int32_t i = 0; i < t->samples; ++i)
		t->size[i] = fixed ? (uint32_t) fixed : ReadBeU32(tb->stsz.body + 12 + 4 * i);

	if (!read_chunks(t, tb))
		return false;

	int32_t sample = 0;

	for (int32_t c = 0; c < t->chunks && sample < t->samples; ++c) {
		uint32_t pos = t->chunk_offset[c];

		for (int32_t s = 0; s < t->chunk_samples[c] && sample < t->samples; ++s) {
			t->offset[sample] = pos;
			pos += t->size[sample];
			++sample;
		}
	}

	if (tb->stts.size < 8)
		return false;

	int32_t runs = ReadBe32(tb->stts.body + 4);

	if ((uint32_t) runs > (tb->stts.size - 8) / 8)
		return false;

	int32_t when = 0;
	sample = 0;

	for (int32_t k = 0; k < runs; ++k) {
		int32_t count = ReadBe32(tb->stts.body + 8 + 8 * k);
		int32_t delta = ReadBe32(tb->stts.body + 12 + 8 * k);

		for (int32_t i = 0; i < count && sample < t->samples; ++i) {
			t->time[sample++] = when;
			when += delta;
		}
	}

	while (sample < t->samples)
		t->time[sample++] = when;

	return true;
}

static bool read_sound(struct mov_track *t, const struct tables *tb)
{
	if (tb->stsd.size < 8 + 36)
		return false;

	const uint8_t *e = tb->stsd.body + 8;
	t->fourcc = ReadBeU32(e + 4);

	if (t->fourcc != FCC_RAW && t->fourcc != FCC_TWOS && t->fourcc != FCC_IMA4)
		return false;

	t->channels = (int32_t) ReadBeU16(e + 24);
	t->bits = (int32_t) ReadBeU16(e + 26);
	t->rate = (int32_t) ((ReadBeU32(e + 32) + 0x8000) >> 16);

	if (t->channels < 1 || t->channels > 2 || t->rate <= 0)
		return false;

	if (t->fourcc != FCC_IMA4 && t->bits != 8 && t->bits != 16)
		return false;

	if (!read_chunks(t, tb))
		return false;

	t->samples = 0;

	for (int32_t c = 0; c < t->chunks; ++c)
		t->samples += t->chunk_samples[c];

	return t->samples > 0;
}

static void read_edits(struct mov_track *t, const struct atom *trak, uint32_t movie_scale)
{
	struct atom edts, elst;

	if (!find(trak->body, trak->body + trak->size, "edts", &edts) ||
		!find(edts.body, edts.body + edts.size, "elst", &elst) || elst.size < 8)
		return;

	int32_t n = ReadBe32(elst.body + 4);

	for (int32_t i = 0; i < n && 8 + 12 * (uint32_t) (i + 1) <= elst.size; ++i) {
		const uint8_t *e = elst.body + 8 + 12 * i;
		uint32_t length = ReadBeU32(e);
		int32_t media = ReadBe32(e + 4);

		if (media == -1) {
			if (movie_scale)
				t->delay_ms += (int32_t) ((double) length * 1000.0 / movie_scale);

			continue;
		}

		t->media_start = media;

		if (movie_scale && t->timescale)
			t->media_length = (int32_t) ((double) length * t->timescale / movie_scale);

		break;
	}
}

static bool read_trak(struct mov_file *m, const struct atom *trak)
{
	struct atom mdia, mdhd, hdlr, minf, stbl;
	struct tables tb;
	struct mov_track *t;

	if (!find(trak->body, trak->body + trak->size, "mdia", &mdia) ||
		!find(mdia.body, mdia.body + mdia.size, "mdhd", &mdhd) || mdhd.size < 20 ||
		!find(mdia.body, mdia.body + mdia.size, "hdlr", &hdlr) || hdlr.size < 12 ||
		!find(mdia.body, mdia.body + mdia.size, "minf", &minf) ||
		!find(minf.body, minf.body + minf.size, "stbl", &stbl))
		return false;

	uint32_t kind = ReadBeU32(hdlr.body + 8);

	if (kind == fcc("vide") && !m->video.present) {
		t = &m->video;

	} else if (kind == fcc("soun") && !m->sound.present) {
		t = &m->sound;

	} else {
		return false;
	}

	memset(&tb, 0, sizeof(tb));

	if (!find(stbl.body, stbl.body + stbl.size, "stsd", &tb.stsd) ||
		!find(stbl.body, stbl.body + stbl.size, "stts", &tb.stts) ||
		!find(stbl.body, stbl.body + stbl.size, "stsc", &tb.stsc) ||
		!find(stbl.body, stbl.body + stbl.size, "stsz", &tb.stsz) ||
		!find(stbl.body, stbl.body + stbl.size, "stco", &tb.stco))
		return false;

	t->timescale = ReadBeU32(mdhd.body + 12);

	if (!t->timescale)
		return false;

	if (!(t == &m->video ? read_video(t, &tb) : read_sound(t, &tb))) {
		free_track(t);
		return false;
	}

	read_edits(t, trak, m->timescale);
	t->present = true;

	return true;
}

static const int32_t ima_index[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
static const int32_t ima_step[89] = {
	7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107,
	118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876,
	963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
	5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086,
	29794, 32767};

static void ima_packet(const uint8_t *p, int16_t *out, int32_t stride)
{
	int32_t pred = ReadBe16(p) & ~0x7F;
	int32_t index = (int32_t) ReadBeU16(p) & 0x7F;

	if (index > 88)
		index = 88;

	for (int32_t i = 0; i < 64; ++i) {
		int32_t code = (i & 1) ? (p[2 + i / 2] >> 4) : (p[2 + i / 2] & 0x0F);
		int32_t step = ima_step[index];
		int32_t diff = step >> 3;

		if (code & 4)
			diff += step;

		if (code & 2)
			diff += step >> 1;

		if (code & 1)
			diff += step >> 2;

		pred += (code & 8) ? -diff : diff;

		if (pred > 32767)
			pred = 32767;

		if (pred < -32768)
			pred = -32768;

		index += ima_index[code];

		if (index < 0)
			index = 0;

		if (index > 88)
			index = 88;

		out[i * stride] = (int16_t) pred;
	}
}

bool MovFileOpen(const char *path, struct mov_file *m)
{
	uint8_t head[16];
	int64_t pos = 0;
	uint8_t *moov = NULL;
	uint32_t moov_size = 0;
	struct atom mvhd, trak;

	memset(m, 0, sizeof(*m));
	m->f = FileOpen(path, FILE_READ);

	if (!m->f)
		return false;

	int64_t end = FileSize(m->f);

	while (pos + 8 <= end) {
		FileSeek(m->f, pos);

		if (FileRead(m->f, head, 8) != 8)
			break;

		uint32_t size = ReadBeU32(head);

		if (size < 8 || pos + (int64_t) size > end)
			break;

		if (!memcmp(head + 4, "moov", 4)) {
			moov_size = size - 8;
			moov = MemAlloc((size_t) moov_size + 1, 1);

			if ((uint32_t) FileRead(m->f, moov, (int32_t) moov_size) != moov_size) {
				MemFree(moov);
				moov = NULL;
			}

			break;
		}

		pos += (int64_t) size;
	}

	if (!moov) {
		MovFileClose(m);
		return false;
	}

	if (find(moov, moov + moov_size, "mvhd", &mvhd) && mvhd.size >= 20) {
		m->timescale = ReadBeU32(mvhd.body + 12);

		if (m->timescale)
			m->duration_ms = (int32_t) ((double) ReadBeU32(mvhd.body + 16) * 1000.0 / m->timescale);
	}

	const uint8_t *p = moov;
	const uint8_t *stop = moov + moov_size;

	while (find(p, stop, "trak", &trak)) {
		read_trak(m, &trak);
		p = trak.body + trak.size;
	}

	MemFree(moov);

	if (!m->video.present) {
		MovFileClose(m);
		return false;
	}

	return true;
}

void MovFileClose(struct mov_file *m)
{
	if (m->f)
		FileClose(&m->f);

	free_track(&m->video);
	free_track(&m->sound);
	MemFree(m->frame_buf);
	memset(m, 0, sizeof(*m));
}

int32_t MovFileFrameAt(const struct mov_file *m, int32_t ms)
{
	const struct mov_track *t = &m->video;

	if (!t->present || ms < t->delay_ms)
		return -1;

	int32_t media = t->media_start + (int32_t) ((double) (ms - t->delay_ms) * t->timescale / 1000.0);

	if (t->media_length && media > t->media_start + t->media_length)
		media = t->media_start + t->media_length;

	int32_t lo = 0;
	int32_t hi = t->samples - 1;

	if (t->time[0] > media)
		return 0;

	while (lo < hi) {
		int32_t mid = (lo + hi + 1) / 2;

		if (t->time[mid] <= media) {
			lo = mid;

		} else {
			hi = mid - 1;
		}
	}

	return lo;
}

bool MovFileReadFrame(struct mov_file *m, int32_t index, const uint8_t **data, uint32_t *size)
{
	const struct mov_track *t = &m->video;

	if (index < 0 || index >= t->samples)
		return false;

	uint32_t n = t->size[index];

	if (n > m->frame_cap) {
		m->frame_buf = MemRealloc(m->frame_buf, n, 1);
		m->frame_cap = n;
	}

	if (!FileSeek(m->f, (int64_t) t->offset[index]) || (uint32_t) FileRead(m->f, m->frame_buf, (int32_t) n) != n)
		return false;

	*data = m->frame_buf;
	*size = n;

	return true;
}

bool MovFileReadSound(struct mov_file *m, uint8_t **pcm, int32_t *bytes, int32_t *rate, int32_t *channels,
	int32_t *bits)
{
	struct mov_track *t = &m->sound;
	bool ima = t->fourcc == FCC_IMA4;
	int32_t out = 0;
	uint8_t *chunk = NULL;

	*pcm = NULL;
	*bytes = 0;

	if (!t->present)
		return false;

	int32_t sample_bytes = ima ? 2 * t->channels : t->channels * (t->bits / 8);
	int32_t total = t->samples * sample_bytes;
	uint8_t *buf = MemAlloc(total ? (size_t) total : 1, 1);

	for (int32_t c = 0; c < t->chunks; ++c) {
		int32_t frames = t->chunk_samples[c];
		uint32_t in = ima ? (uint32_t) ((frames + 63) / 64) * 34 * t->channels : (uint32_t) frames * sample_bytes;

		if (frames <= 0)
			continue;

		chunk = MemRealloc(chunk, in, 1);

		if (!FileSeek(m->f, (int64_t) t->chunk_offset[c]) || (uint32_t) FileRead(m->f, chunk, (int32_t) in) != in)
			break;

		if (frames * sample_bytes > total - out)
			frames = (total - out) / sample_bytes;

		if (ima) {
			int32_t packets = (frames + 63) / 64;
			int16_t tmp[64 * 2];

			for (int32_t k = 0; k < packets; ++k) {
				int32_t n = frames - 64 * k < 64 ? frames - 64 * k : 64;

				for (int32_t ch = 0; ch < t->channels; ++ch)
					ima_packet(chunk + 34 * (k * t->channels + ch), tmp + ch, t->channels);

				for (int32_t s = 0; s < n * t->channels; ++s) {
					buf[out++] = (uint8_t) (tmp[s] & 0xFF);
					buf[out++] = (uint8_t) ((tmp[s] >> 8) & 0xFF);
				}
			}

		} else if (t->bits == 8) {
			int32_t n = frames * t->channels;

			for (int32_t s = 0; s < n; ++s)
				buf[out++] = t->fourcc == FCC_TWOS ? (uint8_t) (chunk[s] ^ 0x80) : chunk[s];

		} else {
			int32_t n = frames * t->channels;

			for (int32_t s = 0; s < n; ++s) {
				buf[out++] = chunk[2 * s + 1];
				buf[out++] = chunk[2 * s];
			}
		}
	}

	MemFree(chunk);

	int32_t first = t->media_start * sample_bytes;
	int32_t count = t->media_length ? t->media_length * sample_bytes : out - first;

	if (first < 0 || first > out)
		first = 0;

	if (count > out - first)
		count = out - first;

	if (first)
		memmove(buf, buf + first, (size_t) count);

	*pcm = buf;
	*bytes = count;
	*rate = t->rate;
	*channels = t->channels;
	*bits = ima ? 16 : t->bits;

	return count > 0;
}
