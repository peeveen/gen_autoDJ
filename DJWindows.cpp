#include "pch.h"
#include <windowsx.h>
#include "DJDefs.h"
#include "DJGlobals.h"
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "DJData.h"
#include "DJPrefs.h"

// Window class names.
const WCHAR* g_pszDJWindowClassName = L"DJInfo";
const WCHAR* g_pszWindowCaption = L"AutoDJ";

// Application icon.
HICON g_hIcon = NULL;
// The window (and it's DC) containing the foreground.
HDC g_hDJWindowDC = NULL;
HWND g_hDJWindow = NULL;

void GetTimeString(WCHAR* szBuffer, int size, DWORD time, DWORD songLength, bool remaining = true) {
	if (time >= MAXUINT32-1)
		wcscpy_s(szBuffer, size, L"n/a");
	else {
		WCHAR sign[2] = L"\0";
		if (remaining) {
			sign[0] = '-';
			time = songLength - time;
		}
		int seconds = time / 1000;
		int mins = seconds / 60;
		int tenths = (time - (seconds * 1000)) / 100;
		seconds -= (mins * 60);
		wsprintf(szBuffer, L"%s%02d:%02d.%d", sign, mins,seconds, tenths);
	}
}

const WCHAR *GetFilename(const WCHAR* pszPath,int *pnChars) {
	const WCHAR *pszFilename=wcsrchr(pszPath, '\\');
	*pnChars = -1;
	if (pszFilename) {
		++pszFilename;
		*pnChars = wcslen(pszFilename);
		WCHAR sz[MAX_PATH + 1] = { '\0' };
		wcscpy_s(sz, pszFilename);
		_wcslwr_s(sz);
		WCHAR* ext = wcsstr(sz, L".m4a");
		if (ext)
			*pnChars = ext - sz;
		else {
			ext = wcsstr(sz, L".mp3");
			if (ext)
				*pnChars = ext - sz;
		}
	}
	else
		pszFilename = L"<nothing>";
	return pszFilename;
}

void TextOut(const WCHAR* pszText, int x, int y, COLORREF color, int nChars=-1) {
	RECT rect;
	::GetClientRect(g_hDJWindow, &rect);
	RECT textRect = { x,y,rect.right,rect.bottom };
	::SetTextColor(g_hDJWindowDC, color);
	::DrawText(g_hDJWindowDC, pszText, nChars, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
}

void TimeOut(DWORD time, DWORD songLength, int x, int y, COLORREF color, bool remaining = true) {
	static WCHAR szTime[100];
	GetTimeString(szTime, 99, time, songLength, remaining);
	TextOut(szTime, x, y, color);
}

void KaraokeTimeOut(StartStopPositions *pPosses, int x, int y, COLORREF color, bool remaining = true) {
	if (pPosses->cdgStopPos != MAXUINT32) {
		static WCHAR szAudioStopTime[100];
		static WCHAR szCDGStopTime[100];
		GetTimeString(szAudioStopTime, 99, pPosses->audioStopPos, pPosses->songLength, remaining);
		GetTimeString(szCDGStopTime, 99, pPosses->cdgStopPos, pPosses->songLength, remaining);
		static WCHAR szMessage[100];
		wsprintf(szMessage, L"(Audio: %s, CDG: %s)", szAudioStopTime, szCDGStopTime);
		TextOut(szMessage, x, y, color);
	}
}

void DrawInfo() {
	COLORREF yellow = RGB(255, 255, 50);
	COLORREF white = RGB(255, 255, 255);
	COLORREF cyan = RGB(50, 255, 255);
	COLORREF green = RGB(50, 255, 50);
	COLORREF magenta = RGB(255,50,255);

	RECT rect;
	::GetClientRect(g_hDJWindow, &rect);
	HBRUSH hBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
	int rectWidth = rect.right - rect.left;
	::FillRect(g_hDJWindowDC, &rect, hBrush);

	TextOut(L"Now Playing", 10, 10, yellow);

	int nChars = -1;
	const WCHAR* filename = GetFilename(g_pszCurrentTrackPath, &nChars);
	TextOut(filename, 10, 30, white, nChars);

	TimeOut(g_currentTrackStartStopPositions.startPos, g_currentTrackStartStopPositions.songLength, 60, 50, cyan, false);
	TextOut(L"Start:", 10, 50, green);

	TimeOut(g_currentTrackStartStopPositions.stopPos, g_currentTrackStartStopPositions.songLength, 60, 70, cyan, g_bShowStopTimeFromEnd);
	KaraokeTimeOut(&g_currentTrackStartStopPositions, 130, 70, magenta);
	TextOut(L"Stop:", 10, 70, green);

	TextOut(L"Next Track", 10, 100, yellow);

	filename = GetFilename(g_pszNextTrackPath, &nChars);
	TextOut(filename, 10, 120, white, nChars);

	TimeOut(g_nextTrackStartStopPositions.startPos, g_nextTrackStartStopPositions.songLength, 60, 140, cyan, false);
	TextOut(L"Start:", 10, 140, green);

	TimeOut(g_nextTrackStartStopPositions.stopPos, g_nextTrackStartStopPositions.songLength, 60, 160, cyan, g_bShowStopTimeFromEnd);
	KaraokeTimeOut(&g_nextTrackStartStopPositions, 130, 160, magenta, g_bShowStopTimeFromEnd);
	TextOut(L"Stop:", 10, 160, green);
}

LRESULT CALLBACK DJWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static PAINTSTRUCT ps;
	switch (uMsg)
	{
	case WM_NCHITTEST: {
		LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
		if (hit == HTCLIENT)
			hit = HTCAPTION;
		return hit;
	}
	case WM_CLOSE:
		return 1;
	case WM_PAINT: {
		::BeginPaint(g_hDJWindow, &ps);
		DrawInfo();
		::EndPaint(g_hDJWindow, &ps);
	}
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

ATOM RegisterQueueWindowClass() {
	if (!g_hIcon)
		g_hIcon = ::LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_APPICON));
	if (g_hIcon) {
		WNDCLASSEX wndClass;
		::ZeroMemory(&wndClass, sizeof(WNDCLASSEX));
		wndClass.cbSize = sizeof(WNDCLASSEX);
		wndClass.style = CS_NOCLOSE | CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = (WNDPROC)DJWindowProc;
		wndClass.hInstance = g_hInstance;
		wndClass.hIcon = g_hIcon;
		wndClass.hIcon = wndClass.hIconSm = g_hIcon;
		wndClass.lpszClassName = g_pszDJWindowClassName;
		return ::RegisterClassEx(&wndClass);
	}
	return 0;
}

bool CreateQueueWindow() {
	int width = 600;
	int height = 230;
	g_hDJWindow = CreateWindowEx(
		WS_EX_APPWINDOW,
		g_pszDJWindowClassName,
		g_pszWindowCaption,
		WS_VISIBLE | WS_THICKFRAME,
		CW_USEDEFAULT, CW_USEDEFAULT,
		width, height,
		// We don't want this window to minimize/restore along with WinAmp, so we don't
		// tell Windows that this is a child of WinAmp.
		NULL,//g_hWinampWindow,
		NULL,
		g_hInstance,
		NULL);
	if (g_hDJWindow) {
		g_hDJWindowDC = ::GetDC(g_hDJWindow);
		if (g_hDJWindowDC) {
			::SetBkColor(g_hDJWindowDC, RGB(0,0,0));
			return true;
		}
	}
	return false;
}

void UnregisterWindowClasses() {
	::UnregisterClass(g_pszDJWindowClassName, g_hInstance);
	if (g_hIcon)
		::DeleteObject(g_hIcon);
}

bool CreateWindows() {
	if (RegisterQueueWindowClass() &&
		CreateQueueWindow()) {
		return true;
	}
	return false;
}

void CloseWindow(HWND hWnd, HDC hDC) {
	if (hDC)
		::ReleaseDC(hWnd, hDC);
	if (hWnd) {
		::CloseWindow(hWnd);
		::DestroyWindow(hWnd);
	}
}

void DestroyWindows() {
	CloseWindow(g_hDJWindow, g_hDJWindowDC);
	UnregisterWindowClasses();
}