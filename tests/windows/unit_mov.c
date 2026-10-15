#include "../testframework.h"

#ifdef OTII_TEST

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <vfw.h>

#include "../../src/media/cinepakframe.h"
#include "../../src/base/memory.h"
#include "../../src/media/movfile.h"
#include "../../src/media/smcdecoder.h"
#include "../../src/data/gfxdev.h"
#include "../../src/base/str.h"

static char g_movies[160][32];
static int g_movie_count;

static const char *data_path(const char *rel)
{
	static char out[MAX_PATH];

	if (!GfxDevFindDataFolder("OregonII", out, sizeof(out)))
		out[0] = 0;
	StrCat(out, rel);
	return out;
}

static void find_movies(void)
{
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA(data_path("MOVIES\\*.MOV"), &fd);

	g_movie_count = 0;
	if (h == INVALID_HANDLE_VALUE)
		return;
	do {
		if (g_movie_count < 160)
			snprintf(g_movies[g_movie_count++], sizeof(g_movies[0]), "MOVIES\\%s", fd.cFileName);
	} while (FindNextFileA(h, &fd));
	FindClose(h);
}

static void decode_all(struct mov_file *m, unsigned char *rgb, long *failed, long *lit)
{
	int w = m->video.width, h = m->video.height, x, y;
	struct smc_decoder *smc = m->video.fourcc == MOV_FCC_SMC ? SmcDecoderCreate(w, h) : NULL;
	struct cinepak_frame *cvid = smc ? NULL : CinepakFrameCreate(w, h);

	*failed = *lit = 0;
	for (int i = 0; i < m->video.samples; ++i) {
		const uint8_t *data;
		uint32_t size;
		bool ok = MovFileReadFrame(m, i, &data, &size) && (smc ? SmcDecoderDecode(smc, data, size)
			: CinepakFrameDecode(cvid, data, (int32_t) size));
		if (!ok)
			++(*failed);
		for (y = 0; y < h; ++y)
			for (x = 0; x < w; ++x) {
				unsigned char *o = rgb + 3 * ((size_t) y * w + x);
				if (smc) {
					const uint8_t *c = m->video.palette[smc->pixels[(size_t) y * smc->stride + x]];
					o[0] = c[0];
					o[1] = c[1];
					o[2] = c[2];

				} else {
					int32_t stride;
					const uint8_t *src = CinepakFrameRgb(cvid, &stride) + 3 * ((size_t) y * stride + x);
					o[0] = src[0];
					o[1] = src[1];
					o[2] = src[2];
				}
			}
		for (y = 1; y < w * h; y += 61)
			if (memcmp(rgb, rgb + 3 * y, 3)) {
				++(*lit);
				break;
			}
	}
	SmcDecoderDestroy(&smc);
	CinepakFrameDestroy(&cvid);
}

static void write_side_by_side(const char *name, const unsigned char *rgb, const unsigned char *avi, int avi_stride,
	int bpp, int w, int h)
{
	char path[MAX_PATH];
	BITMAPFILEHEADER fh;
	BITMAPINFOHEADER ih;
	int stride = ((2 * w * 24 + 31) / 32) * 4, x, y;
	unsigned char *row = (unsigned char *) calloc((size_t) stride, 1);
	FILE *f;

	snprintf(path, sizeof(path), "test_state\\mov_%.*s.bmp", (int) (strchr(name, '.') - name), name);
	if (!row || fopen_s(&f, path, "wb") || !f) {
		free(row);
		return;
	}
	memset(&fh, 0, sizeof(fh));
	memset(&ih, 0, sizeof(ih));
	fh.bfType = 0x4D42;
	fh.bfOffBits = sizeof(fh) + sizeof(ih);
	fh.bfSize = fh.bfOffBits + (DWORD) (stride * h);
	ih.biSize = sizeof(ih);
	ih.biWidth = 2 * w;
	ih.biHeight = h;
	ih.biPlanes = 1;
	ih.biBitCount = 24;
	fwrite(&fh, sizeof(fh), 1, f);
	fwrite(&ih, sizeof(ih), 1, f);
	for (y = h - 1; y >= 0; --y) {
		for (x = 0; x < w; ++x) {
			const unsigned char *a = rgb + 3 * ((size_t) y * w + x);
			const unsigned char *b = avi + (size_t) (h - 1 - y) * avi_stride + x * bpp;
			row[3 * x + 0] = a[2];
			row[3 * x + 1] = a[1];
			row[3 * x + 2] = a[0];
			row[3 * (w + x) + 0] = b[0];
			row[3 * (w + x) + 1] = b[1];
			row[3 * (w + x) + 2] = b[2];
		}
		fwrite(row, (size_t) stride, 1, f);
	}
	fclose(f);
	free(row);
}

TEST(unit_mov, opens_every_movie)
{
	find_movies();
	CHECK_MSG(g_movie_count > 0, "no MOVIES\\*.MOV (copy the 1.3 release's DATA\\MOVIES next to the data)");
	for (int v = 0; v < g_movie_count; ++v) {
		struct mov_file m;
		TCase("%s", g_movies[v]);
		CHECK_MSG(MovFileOpen(data_path(g_movies[v]), &m), "%s does not open", g_movies[v]);
		CHECK(m.video.samples > 0);
		CHECK(m.duration_ms > 0);
		CHECK_MSG(m.video.fourcc == MOV_FCC_SMC || m.video.fourcc == MOV_FCC_CVID, "%s: codec %08X", g_movies[v],
			m.video.fourcc);
		CHECK(MovFileFrameAt(&m, m.duration_ms) >= 0);
		MovFileClose(&m);
	}
}

TEST(unit_mov, decodes_every_frame)
{
	find_movies();
	CHECK(g_movie_count > 0);
	for (int v = 0; v < g_movie_count; ++v) {
		struct mov_file m;
		unsigned char *rgb;
		long failed, lit;
		TCase("%s", g_movies[v]);
		if (!MovFileOpen(data_path(g_movies[v]), &m)) {
			CHECK_MSG(0, "%s does not open", g_movies[v]);
			continue;
		}
		rgb = (unsigned char *) malloc((size_t) m.video.width * (size_t) m.video.height * 3);
		decode_all(&m, rgb, &failed, &lit);
		TNote("%s: %c%c%c%c %dx%d, %d frames, %ld failed, %ld with a picture", g_movies[v],
			(int) (m.video.fourcc >> 24), (int) (m.video.fourcc >> 16) & 0xFF, (int) (m.video.fourcc >> 8) & 0xFF,
			(int) m.video.fourcc & 0xFF, m.video.width, m.video.height, m.video.samples, failed, lit);
		CHECK_EQ(failed, 0L);
		if (m.video.samples > 1)
			CHECK_MSG(lit * 2 >= m.video.samples, "most frames are blank (%ld of %d have a picture)", lit,
				m.video.samples);
		free(rgb);
		MovFileClose(&m);
	}
}

TEST(unit_mov, decodes_sound)
{
	int with_sound = 0;

	find_movies();
	CHECK(g_movie_count > 0);
	for (int v = 0; v < g_movie_count; ++v) {
		struct mov_file m;
		uint8_t *pcm = NULL;
		int32_t bytes = 0, expected, rate = 0, channels = 0, bits = 0;
		TCase("%s", g_movies[v]);
		if (!MovFileOpen(data_path(g_movies[v]), &m))
			continue;
		if (m.sound.present) {
			++with_sound;
			CHECK_MSG(MovFileReadSound(&m, &pcm, &bytes, &rate, &channels, &bits), "%s: the sound does not decode",
				g_movies[v]);
			expected = (m.sound.media_length ? m.sound.media_length : m.sound.samples) * channels * (bits / 8);
			TNote("%s: sound %d bytes, %d Hz, %d bits, %d channel(s)", g_movies[v], bytes, rate, bits, channels);
			CHECK_EQ(bytes, expected);
			CHECK(rate > 8000 && rate < 48000);
			MemFree(pcm);
		}
		MovFileClose(&m);
	}
	CHECK_MSG(with_sound > 0, "no movie has a sound track");
}

TEST(unit_mov, matches_avi_clips)
{
	int compared = 0;

	AVIFileInit();
	find_movies();
	for (int v = 0; v < g_movie_count; ++v) {
		char avi[MAX_PATH];
		const char *name = strchr(g_movies[v], '\\') + 1;
		PAVISTREAM stream;
		struct mov_file m;
		AVISTREAMINFOA info;
		PGETFRAME gf;
		snprintf(avi, sizeof(avi), "AVI\\%.*s.AVI", (int) (strchr(name, '.') - name), name);
		TCase("%s", g_movies[v]);
		if (GetFileAttributesA(data_path(avi)) == INVALID_FILE_ATTRIBUTES || !MovFileOpen(data_path(g_movies[v]), &m))
			continue;
		if (AVIStreamOpenFromFileA(&stream, data_path(avi), streamtypeVIDEO, 0, OF_READ, NULL) != 0) {
			MovFileClose(&m);
			continue;
		}
		AVIStreamInfoA(stream, &info, sizeof(info));
		LONG avi_ms = AVIStreamSampleToTime(stream, AVIStreamLength(stream));
		LONG last = AVIStreamLength(stream) - 1;
		gf = AVIStreamGetFrameOpen(stream, (LPBITMAPINFOHEADER) AVIGETFRAMEF_BESTDISPLAYFMT);
		TNote("%s: %d ms, %s: %ld ms", g_movies[v], m.duration_ms, avi, (long) avi_ms);
		if (labs(avi_ms - m.duration_ms) <= avi_ms / 10 + 200 && gf &&
				(LONG) (info.rcFrame.right - info.rcFrame.left) == m.video.width &&
				(LONG) (info.rcFrame.bottom - info.rcFrame.top) == m.video.height) {
			BITMAPINFOHEADER *bi = (BITMAPINFOHEADER *) AVIStreamGetFrame(gf, last);
			unsigned char *rgb = (unsigned char *) malloc((size_t) m.video.width * (size_t) m.video.height * 3);
			long failed, lit;
			decode_all(&m, rgb, &failed, &lit);
			if (bi && bi->biBitCount >= 24) {
				int bpp = bi->biBitCount / 8, stride = ((bi->biWidth * bi->biBitCount + 31) / 32) * 4, x, y;
				const unsigned char *bits = (const unsigned char *) bi + bi->biSize + bi->biClrUsed * 4;
				double diff = 0;
				for (y = 0; y < m.video.height; ++y)
					for (x = 0; x < m.video.width; ++x) {
						const unsigned char *a = bits + (size_t) (m.video.height - 1 - y) * stride + x * bpp;
						const unsigned char *b = rgb + 3 * ((size_t) y * m.video.width + x);
						diff += abs(a[2] - b[0]) + abs(a[1] - b[1]) + abs(a[0] - b[2]);
					}
				diff /= 3.0 * m.video.width * m.video.height;
				TNote("%s: last picture differs from the AVI's by %.1f a colour value on average", g_movies[v], diff);
				write_side_by_side(name, rgb, bits, stride, bpp, m.video.width, m.video.height);
				CHECK_MSG(diff < 40.0, "%s: the last picture is unlike the AVI clip's (%.1f)", g_movies[v], diff);
				++compared;
			}
			free(rgb);
		}
		if (gf)
			AVIStreamGetFrameClose(gf);
		AVIStreamRelease(stream);
		MovFileClose(&m);
	}
	AVIFileExit();
	TNote("%d movies compared with their AVI clips", compared);
}

const struct test_reg TESTS_unit_mov[] = {
	TEST_ENTRY(unit_mov, opens_every_movie),
	TEST_ENTRY(unit_mov, decodes_every_frame),
	TEST_ENTRY(unit_mov, decodes_sound),
	TEST_ENTRY(unit_mov, matches_avi_clips),
	TEST_END
};

#endif
