#include "../cinepakframe.h"

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <vfw.h>

#include "../../base/memory.h"

#pragma comment(lib, "vfw32.lib")

#define FCC_CVID mmioFOURCC('c', 'v', 'i', 'd')
#define FCC_CVID_UPPER mmioFOURCC('C', 'V', 'I', 'D')

struct cinepak_driver {
	struct cinepak_frame *frame;
	BITMAPINFOHEADER out;
};

static int accepts(const BITMAPINFOHEADER *in, const BITMAPINFOHEADER *out)
{
	if (!in || (in->biCompression != FCC_CVID && in->biCompression != FCC_CVID_UPPER) || in->biWidth <= 0)
		return 0;

	if (!out)
		return 1;

	return out->biCompression == BI_RGB && (out->biBitCount == 24 || out->biBitCount == 32) &&
		out->biWidth == in->biWidth && abs(out->biHeight) == abs(in->biHeight);
}

static LRESULT decompress(struct cinepak_driver *d, ICDECOMPRESS *ic)
{
	const BITMAPINFOHEADER *in = ic->lpbiInput;
	const BITMAPINFOHEADER *out = ic->lpbiOutput;

	if (!CinepakFrameSetSize(d->frame, in->biWidth, abs(in->biHeight)))
		return ICERR_MEMORY;

	LONG size = (LONG) in->biSizeImage;

	if (!size && ic->lpInput) {
		const BYTE *p = (const BYTE *) ic->lpInput + 1;
		size = (p[0] << 16) | (p[1] << 8) | p[2];
	}

	if (!CinepakFrameDecode(d->frame, (const uint8_t *) ic->lpInput, size))
		return ICERR_BADFORMAT;

	if (out && ic->lpOutput)
		CinepakFrameWriteOut(d->frame, out->biWidth, out->biHeight, out->biBitCount, (uint8_t *) ic->lpOutput);

	return ICERR_OK;
}

static LRESULT CALLBACK cinepak_driver_proc(DWORD_PTR id, HDRVR drv, UINT msg, LPARAM lp1, LPARAM lp2)
{
	struct cinepak_driver *d = (struct cinepak_driver *) id;

	(void) drv;

	switch (msg) {
		case DRV_LOAD:
		case DRV_FREE:
		case DRV_ENABLE:
		case DRV_DISABLE:
			return 1;

		case DRV_OPEN: {
			ICOPEN *open = (ICOPEN *) lp2;

			if (open && open->fccType != ICTYPE_VIDEO)
				return 0;

			struct cinepak_driver *nd = MemAlloc(1, sizeof(struct cinepak_driver));
			nd->frame = CinepakFrameCreateEmpty();

			if (open)
				open->dwError = ICERR_OK;

			return (LRESULT) nd;
		}

		case DRV_CLOSE:
			if (d) {
				CinepakFrameDestroy(&d->frame);
				MemFree(d);
			}

			return 1;

		case DRV_QUERYCONFIGURE:
			return 0;

		case ICM_GETINFO: {
			ICINFO *info = (ICINFO *) lp1;

			if (!info || (DWORD) lp2 < sizeof(ICINFO))
				return 0;

			memset(info, 0, sizeof(ICINFO));
			info->dwSize = sizeof(ICINFO);
			info->fccType = ICTYPE_VIDEO;
			info->fccHandler = FCC_CVID;
			info->dwVersion = 1;
			info->dwVersionICM = ICVERSION;
			MultiByteToWideChar(CP_ACP, 0, "Cinepak", -1, info->szName, 16);
			MultiByteToWideChar(CP_ACP, 0, "Cinepak (built in)", -1, info->szDescription, 128);

			return sizeof(ICINFO);
		}

		case ICM_DECOMPRESS_QUERY:
			return accepts((BITMAPINFOHEADER *) lp1, (BITMAPINFOHEADER *) lp2) ? ICERR_OK : ICERR_BADFORMAT;

		case ICM_DECOMPRESS_GET_FORMAT: {
			const BITMAPINFOHEADER *in = (BITMAPINFOHEADER *) lp1;
			BITMAPINFOHEADER *out = (BITMAPINFOHEADER *) lp2;

			if (!accepts(in, NULL))
				return ICERR_BADFORMAT;

			if (!out)
				return sizeof(BITMAPINFOHEADER);

			memset(out, 0, sizeof(BITMAPINFOHEADER));
			out->biSize = sizeof(BITMAPINFOHEADER);
			out->biWidth = in->biWidth;
			out->biHeight = abs(in->biHeight);
			out->biPlanes = 1;
			out->biBitCount = 24;
			out->biCompression = BI_RGB;
			out->biSizeImage = (DWORD) (((in->biWidth * 24 + 31) / 32) * 4 * abs(in->biHeight));

			return ICERR_OK;
		}

		case ICM_DECOMPRESS_BEGIN:
			if (!d || !accepts((BITMAPINFOHEADER *) lp1, (BITMAPINFOHEADER *) lp2))
				return ICERR_BADFORMAT;

			d->out = *(BITMAPINFOHEADER *) lp2;

			return ICERR_OK;

		case ICM_DECOMPRESS:
			if (!d || !lp1)
				return ICERR_BADPARAM;

			return decompress(d, (ICDECOMPRESS *) lp1);

		case ICM_DECOMPRESS_END:
			return ICERR_OK;

		default:
			return msg < DRV_USER ? 0 : ICERR_UNSUPPORTED;
	}
}

#ifdef OTII_TEST
intptr_t CinepakTestDriverProc(uintptr_t id, uint32_t msg, intptr_t lp1, intptr_t lp2)
{
	return cinepak_driver_proc((DWORD_PTR) id, NULL, msg, lp1, lp2);
}
#endif

bool CinepakInstall(void)
{
	bool ok = ICInstall(ICTYPE_VIDEO, FCC_CVID, (LPARAM) cinepak_driver_proc, NULL, ICINSTALL_FUNCTION) != 0;

	ICInstall(ICTYPE_VIDEO, FCC_CVID_UPPER, (LPARAM) cinepak_driver_proc, NULL, ICINSTALL_FUNCTION);

	return ok;
}
