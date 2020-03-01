#include "pch.h"
#include <windowsx.h>
#include "DJDefs.h"
#include "DJGlobals.h"
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "DJData.h"

// Window class names.
const WCHAR* g_pszDJWindowClassName = L"DJInfo";
const WCHAR* g_pszWindowCaption = L"AutoDJ";

// Application icon.
HICON g_hIcon = NULL;
// The window (and it's DC) containing the foreground.
HDC g_hDJWindowDC = NULL;
HWND g_hDJWindow = NULL;

void GetTimeString(WCHAR* szBuffer, int size, DWORD time) {
	if (time >= MAXUINT32-1)
		wcscpy_s(szBuffer, size, L"n/a");
	else {
		int seconds = time / 1000;
		int mins = seconds / 60;
		seconds -= (mins * 60);
		int tenths = (time - (seconds * 1000)) / 100;
		wsprintf(szBuffer, L"%02d:%02d.%d", mins,seconds, tenths);
	}
}

const WCHAR *GetFilename(const WCHAR* pszPath) {
	const WCHAR *pszFilename=wcsrchr(pszPath, '\\');
	if (pszFilename)
		++pszFilename;
	else
		pszFilename = L"<nothing>";
	return pszFilename;
}

void DrawInfo() {
	const int spacing = 20;
	const int timeIndent = 50;
	RECT rect;
	::GetClientRect(g_hDJWindow, &rect);
	HBRUSH hBrush = (HBRUSH)::GetStockObject(BLACK_BRUSH);
	int rectWidth = rect.right - rect.left;
	::FillRect(g_hDJWindowDC, &rect, hBrush);
	RECT textRect = { 10,10,rectWidth - 20,28 };
	::SetTextColor(g_hDJWindowDC, RGB(255, 255, 50));
	::DrawText(g_hDJWindowDC, L"Now Playing", -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.top += spacing;
	textRect.bottom += spacing;
	::SetTextColor(g_hDJWindowDC, RGB(255, 255, 255));
	const WCHAR* filename = GetFilename(g_pszCurrentTrackPath);
	::DrawText(g_hDJWindowDC,  filename, -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_PATH_ELLIPSIS);
	static WCHAR szTime[100];
	GetTimeString(szTime, 99, g_currentTrackStartStopPositions.startPos);
	textRect.top += spacing;
	textRect.bottom += spacing;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 50));
	::DrawText(g_hDJWindowDC, L"Start:", -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.left += timeIndent;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 255));
	::DrawText(g_hDJWindowDC, szTime, -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.left -= timeIndent;

	GetTimeString(szTime, 99, g_currentTrackStartStopPositions.stopPos);
	textRect.top += spacing;
	textRect.bottom += spacing;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 50));
	::DrawText(g_hDJWindowDC, L"Stop:", -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.left += timeIndent;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 255));
	::DrawText(g_hDJWindowDC, szTime, -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.left -= timeIndent;

	textRect.top += spacing+10;
	textRect.bottom += spacing+10;
	::SetTextColor(g_hDJWindowDC, RGB(255, 255, 50));
	::DrawText(g_hDJWindowDC, L"Next Track", -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_PATH_ELLIPSIS);
	textRect.top += spacing;
	textRect.bottom += spacing;
	::SetTextColor(g_hDJWindowDC, RGB(255, 255, 255));
	filename = GetFilename(g_pszNextTrackPath);
	::DrawText(g_hDJWindowDC, filename, -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_PATH_ELLIPSIS);
	GetTimeString(szTime, 99, g_nextTrackStartStopPositions.startPos);
	textRect.top += spacing;
	textRect.bottom += spacing;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 50));
	::DrawText(g_hDJWindowDC, L"Start:", -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.left += timeIndent;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 255));
	::DrawText(g_hDJWindowDC, szTime, -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.left -= timeIndent;

	GetTimeString(szTime, 99, g_nextTrackStartStopPositions.stopPos);
	textRect.top += spacing;
	textRect.bottom += spacing;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 50));
	::DrawText(g_hDJWindowDC, L"Stop:", -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);
	textRect.left += timeIndent;
	::SetTextColor(g_hDJWindowDC, RGB(50, 255, 255));
	::DrawText(g_hDJWindowDC, szTime, -1, &textRect, DT_NOPREFIX | DT_TOP | DT_LEFT | DT_END_ELLIPSIS);

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