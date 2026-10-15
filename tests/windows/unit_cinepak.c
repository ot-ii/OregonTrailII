#include "../testframework.h"

#ifdef OTII_TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <vfw.h>

#include "../../src/media/cinepakframe.h"
#include "../../src/data/gfxdev.h"
#include "../../src/base/str.h"

static char g_videos[128][32];
static int g_video_count;

static void data_path(const char *rel, char *out, size_t size)
{
	if (!GfxDevFindDataFolder("OregonII", out, size))
		out[0] = 0;
	StrCatN(out, size, rel);
}

static void find_videos(void)
{
	WIN32_FIND_DATAA fd;
	char pattern[MAX_PATH];
	data_path("AVI\\*.avi", pattern, sizeof(pattern));
	HANDLE h = FindFirstFileA(pattern, &fd);

	g_video_count = 0;
	if (h == INVALID_HANDLE_VALUE)
		return;
	do {
		PAVISTREAM stream;
		char path[MAX_PATH], full[MAX_PATH];
		snprintf(path, sizeof(path), "AVI\\%s", fd.cFileName);
		data_path(path, full, sizeof(full));
		if (AVIStreamOpenFromFileA(&stream, full, streamtypeVIDEO, 0, OF_READ, NULL) == 0) {
			BITMAPINFOHEADER in;
			LONG size = sizeof(in);
			if (AVIStreamReadFormat(stream, 0, &in, &size) == 0 && in.biCompression == mmioFOURCC('c', 'v', 'i', 'd') &&
					g_video_count < 128)
				snprintf(g_videos[g_video_count++], sizeof(g_videos[0]), "%s", path);
			AVIStreamRelease(stream);
		}
	} while (FindNextFileA(h, &fd));
	FindClose(h);
}

struct clip {
	PAVISTREAM stream;
	BITMAPINFOHEADER in;
	BITMAPINFOHEADER out;
	LONG frames;
	unsigned char *packed;
	LONG packed_size;
	unsigned char *picture;
};

static int open_clip(const char *path, struct clip *c)
{
	LONG fmt_size = sizeof(c->in);
	char full[MAX_PATH];

	memset(c, 0, sizeof(*c));
	data_path(path, full, sizeof(full));
	if (AVIStreamOpenFromFileA(&c->stream, full, streamtypeVIDEO, 0, OF_READ, NULL) != 0)
		return 0;
	if (AVIStreamReadFormat(c->stream, 0, &c->in, &fmt_size) != 0)
		return 0;
	c->frames = AVIStreamLength(c->stream);
	c->out.biSize = sizeof(BITMAPINFOHEADER);
	c->out.biWidth = c->in.biWidth;
	c->out.biHeight = c->in.biHeight;
	c->out.biPlanes = 1;
	c->out.biBitCount = 24;
	c->out.biCompression = BI_RGB;
	c->out.biSizeImage = ((c->in.biWidth * 24 + 31) / 32) * 4 * c->in.biHeight;
	c->packed_size = 1 << 20;
	c->packed = (unsigned char *) malloc((size_t) c->packed_size);
	c->picture = (unsigned char *) malloc(c->out.biSizeImage);
	return c->packed && c->picture;
}

static void close_clip(struct clip *c)
{
	if (c->stream)
		AVIStreamRelease(c->stream);
	free(c->packed);
	free(c->picture);
}

static LRESULT driver(DWORD_PTR id, UINT msg, LPARAM lp1, LPARAM lp2)
{
	return (LRESULT) CinepakTestDriverProc((uintptr_t) id, msg, (intptr_t) lp1, (intptr_t) lp2);
}

static LRESULT decode_one(struct clip *c, LONG i, DWORD_PTR ours, HIC hic, unsigned char *picture)
{
	LONG bytes = 0;
	ICDECOMPRESS ic;

	if (AVIStreamRead(c->stream, i, 1, c->packed, c->packed_size, &bytes, NULL) != 0)
		return ICERR_ERROR;
	c->in.biSizeImage = (DWORD) bytes;
	memset(&ic, 0, sizeof(ic));
	ic.lpbiInput = &c->in;
	ic.lpInput = c->packed;
	ic.lpbiOutput = &c->out;
	ic.lpOutput = picture;
	if (!bytes)
		return ICERR_OK;
	if (hic)
		return ICDecompress(hic, 0, &c->in, c->packed, &c->out, picture);
	return driver(ours, ICM_DECOMPRESS, (LPARAM) &ic, sizeof(ic));
}

TEST(unit_cinepak, decodes_game_videos)
{
	AVIFileInit();
	find_videos();
	CHECK_MSG(g_video_count > 0, "no Cinepak videos found");
	for (int v = 0; v < g_video_count; ++v) {
		struct clip c;
		ICOPEN open;
		DWORD_PTR ours;
		LONG failed = 0, lit = 0;

		TCase("%s", g_videos[v]);
		if (!open_clip(g_videos[v], &c)) {
			CHECK_MSG(0, "%s does not open", g_videos[v]);
			close_clip(&c);
			continue;
		}
		CHECK_EQ(c.in.biCompression, mmioFOURCC('c', 'v', 'i', 'd'));
		memset(&open, 0, sizeof(open));
		open.dwSize = sizeof(open);
		open.fccType = ICTYPE_VIDEO;
		open.fccHandler = c.in.biCompression;
		open.dwFlags = ICMODE_DECOMPRESS;
		ours = (DWORD_PTR) driver(0, DRV_OPEN, 0, (LPARAM) &open);
		CHECK(ours != 0);
		CHECK_EQ(driver(ours, ICM_DECOMPRESS_QUERY, (LPARAM) &c.in, (LPARAM) &c.out), ICERR_OK);
		CHECK_EQ(driver(ours, ICM_DECOMPRESS_BEGIN, (LPARAM) &c.in, (LPARAM) &c.out), ICERR_OK);
		for (LONG i = 0; i < c.frames; ++i) {
			if (decode_one(&c, i, ours, 0, c.picture) != ICERR_OK)
				++failed;
			for (DWORD k = 0; k < c.out.biSizeImage; k += 97)
				if (c.picture[k] > 16) {
					++lit;
					break;
				}
		}
		driver(ours, ICM_DECOMPRESS_END, 0, 0);
		driver(ours, DRV_CLOSE, 0, 0);
		TNote("%s: %ldx%ld, %ld frames, %ld failed, %ld with a picture", g_videos[v], (long) c.in.biWidth,
			(long) c.in.biHeight, (long) c.frames, (long) failed, (long) lit);
		CHECK_MSG(c.frames > 0, "%s has no frames", g_videos[v]);
		CHECK_EQ(failed, 0);
		CHECK_MSG(lit > c.frames / 2, "most frames are blank (%ld of %ld have a picture)", (long) lit, (long) c.frames);
		close_clip(&c);
	}
	AVIFileExit();
}

#ifndef _WIN64
TEST(unit_cinepak, matches_windows)
{
	AVIFileInit();
	find_videos();
	CHECK_MSG(g_video_count > 0, "no Cinepak videos found");
	for (int v = 0; v < g_video_count; ++v) {
		struct clip c;
		HIC hic;
		ICOPEN open;
		DWORD_PTR ours;
		unsigned char *theirs;
		unsigned long long pixels = 0, close = 0;
		int worst = 0;

		TCase("%s", g_videos[v]);
		if (!open_clip(g_videos[v], &c)) {
			CHECK_MSG(0, "%s does not open", g_videos[v]);
			close_clip(&c);
			continue;
		}
		hic = ICOpen(ICTYPE_VIDEO, mmioFOURCC('c', 'v', 'i', 'd'), ICMODE_DECOMPRESS);
		if (!hic) {
			TNote("no Windows Cinepak decoder here: nothing to compare");
			close_clip(&c);
			continue;
		}
		theirs = (unsigned char *) malloc(c.out.biSizeImage);
		memset(&open, 0, sizeof(open));
		open.dwSize = sizeof(open);
		open.fccType = ICTYPE_VIDEO;
		open.dwFlags = ICMODE_DECOMPRESS;
		ours = (DWORD_PTR) driver(0, DRV_OPEN, 0, (LPARAM) &open);
		driver(ours, ICM_DECOMPRESS_BEGIN, (LPARAM) &c.in, (LPARAM) &c.out);
		ICDecompressBegin(hic, &c.in, &c.out);
		for (LONG i = 0; i < c.frames && theirs; ++i) {
			if (decode_one(&c, i, ours, 0, c.picture) != ICERR_OK || decode_one(&c, i, 0, hic, theirs) != ICERR_OK)
				continue;
			for (DWORD k = 0; k < c.out.biSizeImage; ++k) {
				int d = abs((int) c.picture[k] - (int) theirs[k]);
				if (d > worst)
					worst = d;
				if (d <= 4)
					++close;
				++pixels;
			}
		}
		ICDecompressEnd(hic);
		ICClose(hic);
		driver(ours, DRV_CLOSE, 0, 0);
		TNote("%s: %.3f%% of colour values within 4 of Windows', worst difference %d", g_videos[v],
			pixels ? 100.0 * (double) close / (double) pixels : 0.0, worst);
		CHECK(pixels > 0);
		CHECK_MSG(close * 1000 >= pixels * 999, "only %.3f%% within 4 of Windows' picture",
			pixels ? 100.0 * (double) close / (double) pixels : 0.0);
		free(theirs);
		close_clip(&c);
	}
	AVIFileExit();
}
#endif

const struct test_reg TESTS_unit_cinepak[] = {
	TEST_ENTRY(unit_cinepak, decodes_game_videos),
#ifndef _WIN64
	TEST_ENTRY(unit_cinepak, matches_windows),
#endif
	TEST_END
};

#endif
