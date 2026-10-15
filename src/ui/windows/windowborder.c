#include "../windowborder.h"

#include <stdbool.h>
#include <string.h>

#include <windows.h>
#include <windowsx.h>

#include "../../base/geom.h"
#include "../../base/memory.h"
#include "../../app/replay.h"
#include "../scaler.h"
#include "../../base/str.h"

static BOOL g_draw_caption_text;

static int get_custom_nc_rect(HWND hWnd, RECT *lprc);
static int get_sizebox_rect(HWND hWnd, int32_t grip_index, RECT *lprc);
static int get_border_rect(HWND hWnd, RECT *lprc);
static int get_grip_rect_corner(HWND hWnd, RECT *lprc);
static int get_grip_rect_h(HWND hWnd, RECT *lprc);
static int show_window_sysmenu(HWND hWnd);
static void update_sysmenu_items(HWND hWnd, HMENU hMenu);
static void draw_window_gripper(HDC hdc, HWND hWnd, int down);
static void draw_window_border(HDC hdc, HWND hWnd, int corner, int pressed);
static void draw_size_grip_triangle(HDC hdc, RECT *cell, int shape);

static BOOL track_grip_click(HWND hWnd, int32_t grip_type, RECT *lprc)
{
	MSG msg;
	int inside = 1;

	draw_window_border(NULL, hWnd, grip_type == 8, inside);
	SetCapture(hWnd);

	while (1) {
		if (RpPeekMessage(&msg, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE)) {
			switch (msg.message) {
				case WM_LBUTTONUP:
					if (inside)
						draw_window_border(NULL, hWnd, grip_type == 8, inside == 0);

					ReleaseCapture();

					return PtInRect(lprc, msg.pt);
				case WM_MOUSEMOVE:
					if (PtInRect(lprc, msg.pt)) {
						if (!inside) {
							inside = 1;
							draw_window_border(NULL, hWnd, grip_type == 8, inside);
						}

					} else if (inside) {
						inside = 0;
						draw_window_border(NULL, hWnd, grip_type == 8, inside);
					}
					break;
			}
		}
	}
}

static int draw_window_titlebar(HDC hdc, HWND hWnd, RECT *caption, int32_t vert_title, int32_t gripper,
	int32_t corner_grip, int32_t edge_grip, int32_t active)
{
	COLORREF text_color;
	COLORREF caption_color;
	RECT edge_rect;
	RECT caption_rect;

	unsigned int bar_size = (unsigned int) (uintptr_t) GetPropA(hWnd, "ibSize");
	COLORREF frame_color = GetSysColor(COLOR_WINDOWFRAME);

	if (active) {
		text_color = GetSysColor(COLOR_CAPTIONTEXT);
		caption_color = GetSysColor(COLOR_ACTIVECAPTION);

	} else {
		if (g_draw_caption_text) {
			text_color = GetSysColor(COLOR_INACTIVECAPTIONTEXT);

		} else {
			text_color = GetSysColor(COLOR_CAPTIONTEXT);
		}

		caption_color = GetSysColor(COLOR_INACTIVECAPTION);
	}

	SetBkMode(hdc, TRANSPARENT);
	SelectObject(hdc, GetStockObject(NULL_BRUSH));
	SelectObject(hdc, GetStockObject(NULL_PEN));
	caption_rect = *caption;

	if (gripper) {
		if (vert_title) {
			caption_rect.top += bar_size;

		} else {
			caption_rect.left += bar_size;
		}
	}

	if (edge_grip) {
		if (vert_title) {
			caption_rect.bottom -= bar_size;

		} else {
			caption_rect.right -= bar_size;
		}
	}

	if (corner_grip) {
		if (vert_title) {
			caption_rect.bottom -= bar_size;

		} else {
			caption_rect.right -= bar_size;
		}
	}

	if (vert_title) {
		edge_rect.left = caption->right - 1;
		edge_rect.right = caption->right;
		edge_rect.top = caption->top;
		edge_rect.bottom = caption->bottom;

	} else {
		edge_rect.left = caption->left;
		edge_rect.right = caption->right;
		edge_rect.bottom = caption->bottom;
		edge_rect.top = edge_rect.bottom - 1;
	}

	SetBkColor(hdc, frame_color);
	ExtTextOutA(hdc, 0, 0, ETO_OPAQUE, &edge_rect, NULL, 0, NULL);
	HGDIOBJ brush = CreateSolidBrush(caption_color);
	brush = SelectObject(hdc, brush);
	SelectObject(hdc, GetStockObject(NULL_PEN));

	if (vert_title) {
		Rectangle(hdc, caption_rect.left, caption_rect.top, caption_rect.right, caption_rect.bottom + 1);

	} else {
		Rectangle(hdc, caption_rect.left, caption_rect.top, caption_rect.right + 1, caption_rect.bottom);
	}

	brush = SelectObject(hdc, brush);
	DeleteObject(brush);
	int title_len = GetWindowTextLengthA(hWnd);

	if (g_draw_caption_text) {
		char *title = MemAlloc((size_t) title_len + 2, 1);

		if (title) {
			TEXTMETRICA metrics;
			SIZE extent;
			LOGFONTA logfont;
			LONG along_pos;
			int cross_pos;
			HGDIOBJ font;

			GetWindowTextA(hWnd, title, title_len + 1);
			int old_bk_mode = SetBkMode(hdc, TRANSPARENT);
			text_color = SetTextColor(hdc, text_color);
			memset(&logfont, 0, sizeof(LOGFONTA));
			logfont.lfHeight = -(LONG) (bar_size - 3);
			logfont.lfCharSet = 0;
			logfont.lfQuality = 0;
			logfont.lfClipPrecision = CLIP_LH_ANGLES | CLIP_STROKE_PRECIS;

			if (bar_size >= 0x14)
				logfont.lfWeight = FW_BOLD;

			if (vert_title) {
				StrCopy(logfont.lfFaceName, "Arial");
				logfont.lfPitchAndFamily = 36;
				logfont.lfEscapement = 900;
				logfont.lfOrientation = 900;
				font = CreateFontIndirectA(&logfont);
				font = SelectObject(hdc, font);
				GetTextExtentPointA(hdc, title, title_len, &extent);
				along_pos = caption_rect.bottom - (caption_rect.bottom - caption_rect.top - extent.cx) / 2;
				cross_pos = caption_rect.left - 1 + (caption_rect.right - caption_rect.left - extent.cy) / 2;
				GetTextMetricsA(hdc, &metrics);

				if ((metrics.tmPitchAndFamily & TMPF_VECTOR) || (metrics.tmPitchAndFamily & TMPF_TRUETYPE)) {
					ExtTextOutA(hdc, cross_pos, caption_rect.bottom >= along_pos ? along_pos : caption_rect.bottom,
						ETO_CLIPPED, &caption_rect, title, title_len, NULL);
				}

				font = SelectObject(hdc, font);
				DeleteObject(font);

			} else {
				logfont.lfPitchAndFamily = 32;
				font = CreateFontIndirectA(&logfont);
				font = SelectObject(hdc, font);
				GetTextExtentPointA(hdc, title, title_len, &extent);
				along_pos = (caption_rect.right - caption_rect.left - extent.cx) / 2 + caption_rect.left;
				cross_pos = (caption_rect.bottom - caption_rect.top - extent.cy) / 2 + caption_rect.top;
				ExtTextOutA(hdc, caption_rect.left <= along_pos ? along_pos : caption_rect.left, cross_pos,
					ETO_CLIPPED, &caption_rect, title, title_len, NULL);
				font = SelectObject(hdc, font);
				DeleteObject(font);
			}

			text_color = SetTextColor(hdc, text_color);
			SetBkMode(hdc, old_bk_mode);
			MemFree(title);
		}
	}

	if (gripper)
		draw_window_gripper(hdc, hWnd, 0);

	if (corner_grip)
		draw_window_border(hdc, hWnd, 1, 0);

	if (edge_grip)
		draw_window_border(hdc, hWnd, 0, 0);

	return 1;
}

static int show_window_sysmenu(HWND hWnd)
{
	RECT border_rect;
	RECT window_rect;
	POINT menu_pos;

	if (!(GetWindowLongA(hWnd, GWL_STYLE) & 0x80000))
		return 0;

	HDC window_dc = GetWindowDC(hWnd);

	if (window_dc) {
		draw_window_gripper(window_dc, hWnd, 1);

		if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
			menu_pos.x = -1;
			menu_pos.y = 0;

		} else {
			menu_pos.x = 0;
			menu_pos.y = -1;
		}

		get_border_rect(hWnd, &border_rect);
		GetWindowRect(hWnd, &window_rect);
		OffsetRect(&border_rect, -window_rect.left, -window_rect.top);
		ClientToScreen(hWnd, &menu_pos);
		ClientToScreen(hWnd, (POINT *) &window_rect.right);
		LONG style = GetWindowLongA(hWnd, GWL_STYLE);
		SetWindowLongA(hWnd, GWL_STYLE, style | 0x80000);
		HMENU sys_menu = GetSystemMenu(hWnd, FALSE);
		update_sysmenu_items(hWnd, sys_menu);
		SetWindowLongA(hWnd, GWL_STYLE, style);
		TrackPopupMenu(sys_menu, 0, menu_pos.x, menu_pos.y, 0, hWnd, &window_rect);
		draw_window_gripper(window_dc, hWnd, 0);
		ReleaseDC(hWnd, window_dc);
	}

	return 1;
}

static int get_border_rect(HWND hWnd, RECT *lprc)
{
	int has_bar = (GetWindowLongA(hWnd, GWL_STYLE) & 0x80000) != 0;
	int vertical = (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) != 0;

	if (!get_custom_nc_rect(hWnd, lprc))
		return 0;

	if (!has_bar) {
		SetRectEmpty(lprc);

		return 0;
	}

	int bar_size = (int) (intptr_t) GetPropA(hWnd, "ibSize");

	if (vertical) {
		lprc->bottom = bar_size + lprc->top;

	} else {
		lprc->right = bar_size + lprc->left;
	}

	--lprc->bottom;
	--lprc->right;

	return 1;
}

static int get_grip_rect_corner(HWND hWnd, RECT *lprc)
{
	if (!(GetWindowLongA(hWnd, GWL_STYLE) & 0x20000)) {
		SetRectEmpty(lprc);

		return 0;
	}

	if (GetWindowLongA(hWnd, GWL_STYLE) & 0x10000)
		return get_sizebox_rect(hWnd, 2, lprc);

	return get_sizebox_rect(hWnd, 1, lprc);
}

static int get_grip_rect_h(HWND hWnd, RECT *lprc)
{
	if (GetWindowLongA(hWnd, GWL_STYLE) & 0x10000)
		return get_sizebox_rect(hWnd, 1, lprc);

	SetRectEmpty(lprc);

	return 0;
}

static int get_sizebox_rect(HWND hWnd, int32_t grip_index, RECT *lprc)
{
	unsigned int box_size = 0;

	if (!get_custom_nc_rect(hWnd, lprc))
		return 0;

	box_size = (unsigned int) (intptr_t) GetPropA(hWnd, "ibSize");

	if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
		lprc->bottom -= box_size * (grip_index - 1);
		lprc->top = lprc->bottom - box_size + 1;

	} else {
		lprc->right -= box_size * (grip_index - 1);
		lprc->left = lprc->right - box_size + 1;
	}

	return 1;
}

intptr_t CustomBorderWndProc(void *hwnd, uint32_t msg, uintptr_t wparam, intptr_t lparam)
{
	HWND hWnd = (HWND) hwnd;
	UINT Msg = msg;
	WPARAM wParam = wparam;
	LPARAM lParam = lparam;

	RECT window_rect;
	HDC hdc;
	int active;
	RECT nc_rect;
	RECT grip_rect;
	RECT hit_rect;
	POINT pt;
	RECT hit_corner_grip;
	RECT hit_border;
	RECT hit_h_grip;
	LONG style;
	RECT corner_grip;
	RECT h_grip;
	MINMAXINFO *minmax;
	int frame_cy = 0;
	int frame_cx = 0;
	RECT border;
	int grip_extent;
	LONG saved_style;
	LRESULT result;
	unsigned int title_size;

	switch (Msg) {
		case WM_SYSCHAR:
			if (((GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) || (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000)) &&
				wParam == 32)
			{
				show_window_sysmenu(hWnd);
				break;
			}
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
			saved_style = GetWindowLongA(hWnd, GWL_STYLE);
			SetWindowLongA(hWnd, GWL_STYLE, saved_style & 0xFFF7FFFF);
			result = DefWindowProcA(hWnd, Msg, wParam, lParam);
			SetWindowLongA(hWnd, GWL_STYLE, saved_style);

			return result;
		case WM_COMMAND:
			if (wParam >= 0xF000)
				PostMessageA(hWnd, 0x112, wParam, lParam);

			return DefWindowProcA(hWnd, Msg, wParam, lParam);
		case WM_GETMINMAXINFO:
			title_size = (unsigned int) (uintptr_t) GetPropA(hWnd, "ibSize");

			if (((GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) || (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000)) &&
				(GetWindowLongA(hWnd, GWL_STYLE) & 0x40000))
			{
				minmax = (MINMAXINFO *) lParam;

				if (GetWindowLongA(hWnd, GWL_STYLE) & 0x40000) {
					frame_cx = ScalerMetric(hWnd, 32);
					frame_cy = ScalerMetric(hWnd, 33);

				} else if (GetWindowLongA(hWnd, GWL_STYLE) & 0x800000) {
					frame_cx = ScalerMetric(hWnd, 5);
					frame_cy = ScalerMetric(hWnd, 6);
				}

				get_border_rect(hWnd, &border);
				get_grip_rect_corner(hWnd, &corner_grip);
				get_grip_rect_h(hWnd, &h_grip);
				grip_extent = corner_grip.right - corner_grip.left - corner_grip.left + corner_grip.right -
					border.left + border.right;

				if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
					minmax->ptMinTrackSize.x = (int) title_size + 2 * frame_cx - 1;
					minmax->ptMinTrackSize.y = grip_extent + 2 * (int) title_size;

				} else {
					minmax->ptMinTrackSize.x = grip_extent + 2 * (int) title_size;
					minmax->ptMinTrackSize.y = (int) title_size + 2 * frame_cy - 1;
				}
			}

			return DefWindowProcA(hWnd, Msg, wParam, lParam);
		case WM_NCCREATE:
			SetPropA(hWnd, "ibSize", (HANDLE) (intptr_t) (ScalerMetric(hWnd, 4) / 2 + 1));
			SetPropA(hWnd, "ibFlag", NULL);
#pragma warning(suppress: 4996)
			g_draw_caption_text = LOWORD(GetVersion()) >= 0x30A;
			style = GetWindowLongA(hWnd, GWL_STYLE);

			if (((style & 0x8000) || (style & 0x4000)) && (style & 0x400000)) {
				style &= ~0x400000;
				SetWindowLongA(hWnd, GWL_STYLE, style);
			}

			return DefWindowProcA(hWnd, Msg, wParam, lParam);
		case WM_NCDESTROY:
			RemovePropA(hWnd, "ibSize");
			RemovePropA(hWnd, "ibFlag");

			return DefWindowProcA(hWnd, Msg, wParam, lParam);
		case WM_NCCALCSIZE:
			result = DefWindowProcA(hWnd, Msg, wParam, lParam);

			if (!IsIconic(hWnd) &&
				((GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) || (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000)))
			{
				title_size = (unsigned int) (uintptr_t) GetPropA(hWnd, "ibSize");

				if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
					((RECT *) lParam)->left += title_size;

				} else {
					((RECT *) lParam)->top += title_size;
				}
			}

			return result;
		case WM_NCHITTEST:
			result = DefWindowProcA(hWnd, Msg, wParam, lParam);

			if (!IsIconic(hWnd) &&
				((GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) || (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000)) &&
				!result)
			{
				title_size = (unsigned int) (uintptr_t) GetPropA(hWnd, "ibSize");
				pt.x = GET_X_LPARAM(lParam);
				pt.y = GET_Y_LPARAM(lParam);
				get_custom_nc_rect(hWnd, &hit_rect);

				if (PtInRect(&hit_rect, pt)) {
					result = 2;
					get_border_rect(hWnd, &hit_border);
					get_grip_rect_corner(hWnd, &hit_corner_grip);
					get_grip_rect_h(hWnd, &hit_h_grip);

					if (PtInRect(&hit_border, pt))
						result = 3;

					if (PtInRect(&hit_corner_grip, pt)) {
						result = 8;

					} else if (PtInRect(&hit_h_grip, pt)) {
						result = 9;
					}
				}
			}

			if (result != 3)
				SetPropA(hWnd, "ibFlag", NULL);

			return result;
		case WM_NCLBUTTONDBLCLK:
			if (!IsIconic(hWnd) &&
				((GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) || (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000)) &&
				wParam == 3)
			{
				SendMessageA(hWnd, 0x10, 0, 0);
				break;
			}

			return DefWindowProcA(hWnd, Msg, wParam, lParam);
		case WM_NCLBUTTONDOWN:
			if (IsIconic(hWnd) ||
				((GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) == 0 && (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000) == 0))
			{
				return DefWindowProcA(hWnd, Msg, wParam, lParam);
			}

			switch (wParam) {
				case 3:
					if (!GetPropA(hWnd, "ibFlag") && show_window_sysmenu(hWnd)) {
						SetPropA(hWnd, "ibFlag", (HANDLE) 1);

					} else {
						SetPropA(hWnd, "ibFlag", NULL);
					}
					break;
				case 8:
					get_grip_rect_corner(hWnd, &grip_rect);

					if (track_grip_click(hWnd, (int32_t) wParam, &grip_rect))
						SendMessageA(hWnd, 0x112, 0xF020, lParam);
					break;
				case 9:
					get_grip_rect_h(hWnd, &grip_rect);

					if (track_grip_click(hWnd, (int32_t) wParam, &grip_rect)) {
						if (IsZoomed(hWnd)) {
							SendMessageA(hWnd, 0x112, 0xF120, lParam);

						} else {
							SendMessageA(hWnd, 0x112, 0xF030, lParam);
						}
					}
					break;
				default:
					return DefWindowProcA(hWnd, Msg, wParam, lParam);
			}
			break;
		case WM_NCPAINT:
		case WM_NCACTIVATE:
			if (IsIconic(hWnd))
				return DefWindowProcA(hWnd, Msg, wParam, lParam);

			result = DefWindowProcA(hWnd, Msg, wParam, lParam);

			if ((GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) || (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000)) {
				hdc = GetWindowDC(hWnd);
				get_custom_nc_rect(hWnd, &nc_rect);
				GetWindowRect(hWnd, &window_rect);
				OffsetRect(&nc_rect, -window_rect.left, -window_rect.top);

				if (Msg == WM_NCPAINT) {
					active = GetActiveWindow() == hWnd;

				} else {
					active = (int) wParam;
				}

				draw_window_titlebar(hdc, hWnd, &nc_rect, (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000u) >> 15,
					(GetWindowLongA(hWnd, GWL_STYLE) & 0x80000u) >> 19,
					(GetWindowLongA(hWnd, GWL_STYLE) & 0x20000u) >> 17,
					(GetWindowLongA(hWnd, GWL_STYLE) & 0x10000u) >> 16, active);
				ReleaseDC(hWnd, hdc);
			}

			return result;
		default:
			return DefWindowProcA(hWnd, Msg, wParam, lParam);
	}

	return 0;
}

void GetWindowOuterRect(void *hwnd, struct rect *rect)
{
	HWND hWnd = (HWND) hwnd;
	RECT r;

	r.left = rect->left;
	r.top = rect->top;
	r.right = rect->right;
	r.bottom = rect->bottom;

	int inflate_x = 0;
	int inflate_y = 0;
	unsigned int title_size = (unsigned int) (uintptr_t) GetPropA(hWnd, "ibSize");

	if (GetWindowLongA(hWnd, GWL_STYLE) & 0x40000) {
		inflate_x = ScalerMetric(hWnd, 0x20);
		inflate_y = ScalerMetric(hWnd, 0x21);

	} else if (GetWindowLongA(hWnd, GWL_STYLE) & 0x80) {
		inflate_x = ScalerMetric(hWnd, 5) + ScalerMetric(hWnd, 7);
		inflate_y = ScalerMetric(hWnd, 6) + ScalerMetric(hWnd, 8);

	} else if (GetWindowLongA(hWnd, GWL_STYLE) & 0x800000) {
		inflate_x = ScalerMetric(hWnd, 5);
		inflate_y = ScalerMetric(hWnd, 6);
	}

	InflateRect(&r, inflate_x, inflate_y);

	if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
		r.left = r.left - title_size;

	} else if (GetWindowLongA(hWnd, GWL_STYLE) & 0x4000) {
		r.top = r.top - title_size;
	}

	rect->left = r.left;
	rect->top = r.top;
	rect->right = r.right;
	rect->bottom = r.bottom;
}

static void draw_window_gripper(HDC hdc, HWND hWnd, int down)
{
	RECT border;
	RECT fill;
	RECT edge;
	int owns_dc;

	unsigned int title_size = (unsigned int) (uintptr_t) GetPropA(hWnd, "ibSize");

	if (!hdc) {
		owns_dc = 1;
		hdc = GetWindowDC(hWnd);

	} else {
		owns_dc = 0;
	}

	if (hdc) {
		COLORREF face_color = GetNearestColor(hdc, 0xC0C0C0);
		COLORREF frame_color = GetSysColor(6);
		get_border_rect(hWnd, &border);
		GetWindowRect(hWnd, &edge);
		OffsetRect(&border, -edge.left, -edge.top);
		fill = border;

		if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
			edge = border;
			edge.bottom++;
			edge.top = edge.bottom - 1;

		} else {
			edge = border;
			edge.right++;
			edge.left = edge.right - 1;
		}

		SetBkColor(hdc, face_color);
		ExtTextOutA(hdc, 0, 0, 2, &fill, NULL, 0, NULL);
		SetBkColor(hdc, frame_color);
		ExtTextOutA(hdc, 0, 0, 2, &edge, NULL, 0, NULL);

		if (title_size > 4) {
			fill.top = border.top + ((title_size - 1) >> 1);
			fill.bottom = fill.top + 3;
			fill.left += 3;
			--fill.right;
			SetBkColor(hdc, 0x808080);
			ExtTextOutA(hdc, 0, 0, 2, &fill, NULL, 0, NULL);
			edge = fill;
			OffsetRect(&edge, -1, -1);
			SetBkColor(hdc, 0);
			ExtTextOutA(hdc, 0, 0, 2, &edge, NULL, 0, NULL);
			InflateRect(&edge, -1, -1);
			SetBkColor(hdc, 0xFFFFFF);
			ExtTextOutA(hdc, 0, 0, 2, &edge, NULL, 0, NULL);
		}

		if (down)
			InvertRect(hdc, &border);

		if (owns_dc)
			ReleaseDC(hWnd, hdc);
	}
}

static void update_sysmenu_items(HWND hWnd, HMENU hMenu)
{
	UINT minimize_state;
	UINT restore_state;
	UINT maximize_state;
	UINT move_state;
	UINT size_state;

	size_state = move_state = minimize_state = maximize_state = restore_state = MF_GRAYED;

	if ((GetWindowLongA(hWnd, GWL_STYLE) & WS_MAXIMIZEBOX) || IsIconic(hWnd))
		maximize_state = MF_ENABLED;

	if (GetWindowLongA(hWnd, GWL_STYLE) & WS_MINIMIZEBOX)
		minimize_state = MF_ENABLED;

	if (IsZoomed(hWnd))
		restore_state = MF_ENABLED;

	if ((GetWindowLongA(hWnd, GWL_STYLE) & WS_THICKFRAME) && !IsIconic(hWnd) && !IsZoomed(hWnd))
		size_state = MF_ENABLED;

	if (!IsZoomed(hWnd) && !IsIconic(hWnd) && (GetWindowLongA(hWnd, GWL_STYLE) & WS_CAPTION))
		move_state = MF_ENABLED;

	EnableMenuItem(hMenu, 0xF010, move_state);
	EnableMenuItem(hMenu, 0xF000, size_state);
	EnableMenuItem(hMenu, 0xF020, minimize_state);
	EnableMenuItem(hMenu, 0xF030, maximize_state);
	EnableMenuItem(hMenu, 0xF120, restore_state);
}

static int get_custom_nc_rect(HWND hWnd, RECT *lprc)
{
	int title_size = (int) (intptr_t) GetPropA(hWnd, "ibSize");

	if (!(GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) && !(GetWindowLongA(hWnd, GWL_STYLE) & 0x4000)) {
		SetRectEmpty(lprc);

		return 0;
	}

	GetWindowRect(hWnd, lprc);

	if (GetWindowLongA(hWnd, GWL_STYLE) & 0x40000) {
		lprc->left += ScalerMetric(hWnd, 32);
		lprc->top += ScalerMetric(hWnd, 33);
		lprc->right -= ScalerMetric(hWnd, 32);
		lprc->bottom -= ScalerMetric(hWnd, 33);

	} else if (GetWindowLongA(hWnd, GWL_STYLE) & 0x80) {
		lprc->left += ScalerMetric(hWnd, 7) + ScalerMetric(hWnd, 5);
		lprc->top += ScalerMetric(hWnd, 8) + ScalerMetric(hWnd, 6);
		lprc->right -= ScalerMetric(hWnd, 7) + ScalerMetric(hWnd, 5);
		lprc->bottom -= ScalerMetric(hWnd, 8) + ScalerMetric(hWnd, 6);

	} else {
		if (GetWindowLongA(hWnd, GWL_STYLE) & 0x800000) {
			lprc->left += ScalerMetric(hWnd, 5);
			lprc->top += ScalerMetric(hWnd, 6);
			lprc->right -= ScalerMetric(hWnd, 5);
			lprc->bottom -= ScalerMetric(hWnd, 6);
		}
	}

	if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
		lprc->right = lprc->left + title_size;

	} else {
		lprc->bottom = lprc->top + title_size;
	}

	return 1;
}

static void draw_window_border(HDC hdc, HWND hWnd, int corner, int pressed)
{
	RECT edge_rect;
	RECT grip_rect;
	int owns_dc;
	unsigned int thickness;

	unsigned int title_size = (unsigned int) (uintptr_t) GetPropA(hWnd, "ibSize");

	if (title_size >= 0x14) {
		thickness = 2;

	} else {
		thickness = 1;
	}

	if (!hdc) {
		owns_dc = 1;
		hdc = GetWindowDC(hWnd);

	} else {
		owns_dc = 0;
	}

	if (hdc) {
		COLORREF frame_color = GetSysColor(6);

		if (corner) {
			get_grip_rect_corner(hWnd, &grip_rect);

		} else {
			get_grip_rect_h(hWnd, &grip_rect);
		}

		GetWindowRect(hWnd, &edge_rect);
		OffsetRect(&grip_rect, -edge_rect.left, -edge_rect.top);
		edge_rect = grip_rect;

		if (GetWindowLongA(hWnd, GWL_STYLE) & 0x8000) {
			edge_rect = grip_rect;
			edge_rect.top--;
			edge_rect.bottom = edge_rect.top + 1;
			grip_rect.right--;

		} else {
			edge_rect = grip_rect;
			edge_rect.left--;
			edge_rect.right = edge_rect.left + 1;
			grip_rect.bottom--;
		}

		SetBkColor(hdc, frame_color);
		ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);
		SetBkColor(hdc, 0xC0C0C0);
		ExtTextOutA(hdc, 0, 0, 2, &grip_rect, NULL, 0, NULL);

		if (!pressed) {
			SetBkColor(hdc, 0xFFFFFF);
			edge_rect = grip_rect;
			edge_rect.right = edge_rect.left + 1;
			ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);
			edge_rect = grip_rect;
			edge_rect.bottom = edge_rect.top + 1;
			ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);
			SetBkColor(hdc, 0x808080);
			edge_rect = grip_rect;
			edge_rect.left = edge_rect.right - 1;
			ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);

			if (title_size > 0x14) {
				edge_rect.left--;
				edge_rect.top++;
				ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);
			}

			edge_rect = grip_rect;
			edge_rect.top = edge_rect.bottom - 1;
			ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);

			if (title_size > 0x14) {
				edge_rect.top--;
				edge_rect.left++;
				ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);
			}

			grip_rect.left++;
			grip_rect.top++;
			grip_rect.right -= thickness;
			grip_rect.bottom -= thickness;

		} else {
			SetBkColor(hdc, 0x808080);
			edge_rect = grip_rect;
			edge_rect.right = edge_rect.left + thickness;
			ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);
			edge_rect = grip_rect;
			edge_rect.bottom = edge_rect.top + thickness;
			ExtTextOutA(hdc, 0, 0, 2, &edge_rect, NULL, 0, NULL);
			grip_rect.left += 2 * thickness;
			grip_rect.top += 2 * thickness;
		}

		int slack = ScalerMetric(hWnd, 4) / 2 - (grip_rect.right - grip_rect.left);

		if (slack < 1)
			InflateRect(&grip_rect, slack / 2 - 1, slack / 2 - 1);

		if (corner) {
			draw_size_grip_triangle(hdc, &grip_rect, 1);

		} else if (IsZoomed(hWnd)) {
			draw_size_grip_triangle(hdc, &grip_rect, 2);

		} else {
			draw_size_grip_triangle(hdc, &grip_rect, 0);
		}

		if (owns_dc)
			ReleaseDC(hWnd, hdc);
	}
}

static void draw_size_grip_triangle(HDC hdc, RECT *cell, int shape)
{
	int offset;
	int base_y;
	RECT clipping_rect;

	int half_height = (cell->bottom - cell->top) >> 1;
	SetBkColor(hdc, 0);
	int center_x = cell->left + ((cell->right - cell->left + 1) >> 1);

	switch (shape) {
		case 0:
			base_y = cell->top + ((cell->bottom - cell->top - 1) >> 2);

			for (offset = 1; offset <= half_height; ++offset) {
				clipping_rect.left = center_x - offset;
				clipping_rect.right = center_x + offset - 1;
				clipping_rect.top = base_y + offset;
				clipping_rect.bottom = clipping_rect.top + 1;
				ExtTextOutA(hdc, 0, 0, 2, &clipping_rect, NULL, 0, NULL);
			}
			break;
		case 1:
			base_y = cell->bottom - ((cell->bottom - cell->top - 1) >> 2);

			for (offset = half_height; offset > 0; --offset) {
				clipping_rect.left = center_x - offset;
				clipping_rect.right = center_x + offset - 1;
				clipping_rect.top = base_y - offset;
				clipping_rect.bottom = clipping_rect.top + 1;
				ExtTextOutA(hdc, 0, 0, 2, &clipping_rect, NULL, 0, NULL);
			}
			break;
		default:
			base_y = cell->top + ((cell->bottom - cell->top - 1) >> 3) - 2;

			for (offset = 1; offset <= half_height; ++offset) {
				clipping_rect.left = center_x - offset;
				clipping_rect.right = center_x + offset - 1;
				clipping_rect.top = base_y + offset;
				clipping_rect.bottom = clipping_rect.top + 1;
				ExtTextOutA(hdc, 0, 0, 2, &clipping_rect, NULL, 0, NULL);
			}

			base_y += 2 * (half_height + 1);

			for (offset = half_height; offset > 0; --offset) {
				clipping_rect.left = center_x - offset;
				clipping_rect.right = center_x + offset - 1;
				clipping_rect.top = base_y - offset;
				clipping_rect.bottom = clipping_rect.top + 1;
				ExtTextOutA(hdc, 0, 0, 2, &clipping_rect, NULL, 0, NULL);
			}
			break;
	}
}
