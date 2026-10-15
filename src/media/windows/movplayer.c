#include "../movplayer.h"

#include <stddef.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include "../framesource.h"
#include "../../data/image.h"
#include "../../base/memory.h"

struct mov_player {
	struct frame_source *src;
	HWND hwnd;
	int32_t shown;
	bool playing;
	DWORD started;
	int32_t position;
	BITMAPINFO *bmi;
	uint8_t *bits;
	int32_t stride;
};

static const char MOV_CLASS[] = "OTIIMovWnd";

static void present(struct mov_player *p)
{
	int32_t w = FrameSourceWidth(p->src);
	int32_t h = FrameSourceHeight(p->src);
	const struct image *img = FrameSourceImage(p->src);

	if (img->format == IMAGE_INDEXED8) {
		for (int32_t y = 0; y < h; ++y)
			memcpy(p->bits + (size_t) (h - 1 - y) * p->stride, img->pixels + (size_t) y * img->stride, (size_t) w);

	} else {
		for (int32_t y = 0; y < h; ++y) {
			uint8_t *row = p->bits + (size_t) (h - 1 - y) * p->stride;
			const uint8_t *src = img->pixels + (size_t) y * img->stride;

			for (int32_t x = 0; x < w; ++x) {
				row[3 * x + 0] = src[4 * x + 0];
				row[3 * x + 1] = src[4 * x + 1];
				row[3 * x + 2] = src[4 * x + 2];
			}
		}
	}
}

static void decode_to(struct mov_player *p, int32_t target)
{
	bool changed = false;
	int32_t frames = FrameSourceFrameCount(p->src);

	if (target >= frames)
		target = frames - 1;

	if (target < p->shown) {
		p->shown = -1;
		FrameSourceRewind(p->src);
	}

	while (p->shown < target) {
		++p->shown;

		if (FrameSourceDecode(p->src, p->shown) == FRAME_NOT_READ)
			continue;

		changed = true;
	}

	if (changed) {
		present(p);

		if (p->hwnd)
			InvalidateRect(p->hwnd, NULL, FALSE);
	}
}

static LRESULT CALLBACK mov_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	struct mov_player *p = (struct mov_player *) GetWindowLongPtrA(hwnd, GWLP_USERDATA);

	switch (msg) {
		case WM_ERASEBKGND:
			return 1;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			RECT rc;
			HDC dc = BeginPaint(hwnd, &ps);
			GetClientRect(hwnd, &rc);

			if (p && p->bits) {
				SetStretchBltMode(dc, COLORONCOLOR);
				StretchDIBits(dc, 0, 0, rc.right, rc.bottom, 0, 0, FrameSourceWidth(p->src), FrameSourceHeight(p->src),
					p->bits, p->bmi, DIB_RGB_COLORS, SRCCOPY);

			} else {
				FillRect(dc, &rc, (HBRUSH) GetStockObject(BLACK_BRUSH));
			}

			EndPaint(hwnd, &ps);

			return 0;
		}
	}

	return DefWindowProcA(hwnd, msg, wp, lp);
}

struct mov_player *MovPlayerCreate(const char *path, void *parent, void *instance)
{
	static bool registered;
	struct mov_player *p = MemAlloc(1, sizeof(struct mov_player));

	p->src = FrameSourceOpen(path);

	if (!p->src) {
		MemFree(p);
		return NULL;
	}

	int32_t w = FrameSourceWidth(p->src);
	int32_t h = FrameSourceHeight(p->src);
	const uint8_t (*palette)[3] = FrameSourcePalette(p->src);
	bool depth8 = palette != NULL;
	p->stride = ((w * (depth8 ? 8 : 24) + 31) / 32) * 4;
	p->bmi = MemAlloc(1, sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD));
	p->bits = MemAlloc((size_t) p->stride, (size_t) h);

	p->bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	p->bmi->bmiHeader.biWidth = w;
	p->bmi->bmiHeader.biHeight = h;
	p->bmi->bmiHeader.biPlanes = 1;
	p->bmi->bmiHeader.biBitCount = (WORD) (depth8 ? 8 : 24);
	p->bmi->bmiHeader.biCompression = BI_RGB;

	if (depth8) {
		p->bmi->bmiHeader.biClrUsed = 256;

		for (int i = 0; i < 256; ++i) {
			p->bmi->bmiColors[i].rgbRed = palette[i][0];
			p->bmi->bmiColors[i].rgbGreen = palette[i][1];
			p->bmi->bmiColors[i].rgbBlue = palette[i][2];
		}
	}

	p->shown = -1;
	decode_to(p, FrameSourceFrameAt(p->src, 0) < 0 ? 0 : FrameSourceFrameAt(p->src, 0));

	if (!registered) {
		WNDCLASSA wc;
		memset(&wc, 0, sizeof(wc));
		wc.lpfnWndProc = mov_wndproc;
		wc.hInstance = (HINSTANCE) instance;
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.lpszClassName = MOV_CLASS;
		registered = RegisterClassA(&wc) != 0;
	}

	p->hwnd = CreateWindowExA(0, MOV_CLASS, "", WS_CHILD | WS_VISIBLE, 0, 0, w, h, (HWND) parent, NULL,
		(HINSTANCE) instance, NULL);

	if (!p->hwnd) {
		MovPlayerDestroy(&p);
		return NULL;
	}

	SetWindowLongPtrA(p->hwnd, GWLP_USERDATA, (LONG_PTR) p);

	return p;
}

void MovPlayerDestroy(struct mov_player **ctx_mod)
{
	if (!ctx_mod || !*ctx_mod)
		return;

	struct mov_player *p = *ctx_mod;
	*ctx_mod = NULL;

	if (p->hwnd) {
		SetWindowLongPtrA(p->hwnd, GWLP_USERDATA, 0);
		DestroyWindow(p->hwnd);
	}

	FrameSourceDestroy(&p->src);
	MemFree(p->bmi);
	MemFree(p->bits);
	MemFree(p);
}

void *MovPlayerHwnd(const struct mov_player *p)
{
	return p ? p->hwnd : NULL;
}

int32_t MovPlayerPosition(const struct mov_player *p)
{
	if (!p)
		return 0;

	if (p->playing) {
		int32_t now = (int32_t) (timeGetTime() - p->started);
		int32_t duration = FrameSourceDurationMs(p->src);

		return now < duration ? now : duration;
	}

	return p->position;
}

void MovPlayerPlay(struct mov_player *p)
{
	if (!p || p->playing)
		return;

	if (p->position >= FrameSourceDurationMs(p->src))
		return;

	p->started = timeGetTime() - (DWORD) p->position;
	p->playing = true;
}

void MovPlayerPause(struct mov_player *p)
{
	if (!p || !p->playing)
		return;

	p->position = MovPlayerPosition(p);
	p->playing = false;
}

bool MovPlayerPlaying(const struct mov_player *p)
{
	return p && p->playing;
}

void MovPlayerTick(struct mov_player *p)
{
	if (!p || !p->playing)
		return;

	int32_t ms = MovPlayerPosition(p);
	int32_t frame = FrameSourceFrameAt(p->src, ms);

	if (frame >= 0)
		decode_to(p, frame);

	if (ms >= FrameSourceDurationMs(p->src)) {
		p->position = FrameSourceDurationMs(p->src);
		p->playing = false;
	}
}

bool MovPlayerSound(struct mov_player *p, uint8_t **pcm, int32_t *bytes, int32_t *rate, int32_t *channels,
	int32_t *bits)
{
	return p && FrameSourceSound(p->src, pcm, bytes, rate, channels, bits);
}
