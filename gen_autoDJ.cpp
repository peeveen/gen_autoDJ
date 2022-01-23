// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <combaseapi.h>
#include <mfapi.h>
#include <shellapi.h>
#include <wchar.h>
#include <malloc.h>
#include "DJDefs.h"
#include "DJPrefs.h"
#include "DJFileScanner.h"
#include "DJTrackLists.h"
#include "DJWindows.h"
#include "DJData.h"

int init(void);
void config(void);
void quit(void);

// Resets PaceMaker tempo, pitch & speed settings to zeroes
#define PM_RESET (WM_APP+5)

// The instance handle of this DLL.
HINSTANCE g_hInstance = NULL;
// Handle to the Winamp window.
HWND g_hWinampWindow = NULL;

// Original WndProc that we have to swap back in at the end of proceedings.
WNDPROC g_pOriginalWndProc;
// This structure contains plugin information, version, name...
winampGeneralPurposePlugin plugin = { GPPHDR_VER,PLUGIN_NAME,init,config,quit,0,0 };

// This is an export function called by winamp which returns this plugin info.
// We wrap the code in 'extern "C"' to ensure the export isn't mangled if used in a CPP file.
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
	return &plugin;
}

HANDLE g_hTimeWatcherThread = NULL;
HANDLE g_hStopTimeWatcherEvent = NULL;
HANDLE g_hCurrentTrackMutex = NULL;
HANDLE g_hNextTrackMutex = NULL;
HANDLE g_hPlayNextMutex = NULL;
bool g_bNextTrackIsRequest = false;
__time64_t g_requestFileLastModifiedTime = 0;

void HandleInterruption(WCHAR* pszFileBeingPlayed) {
	g_currentTrackStartStopPositions = { MAXUINT32,MAXUINT32,MAXUINT32,MAXUINT32 };
	GetStartStopPositions(false, pszFileBeingPlayed, g_pszCurrentTrackPath, &g_currentTrackStartStopPositions, g_hCurrentTrackMutex);
}

void FindAndProcessNextTrack() {
	WCHAR* pszTrackPath = GetNextTrack(&g_bNextTrackIsRequest);
	if (pszTrackPath)
		GetStartStopPositions(true, pszTrackPath, g_pszNextTrackPath, &g_nextTrackStartStopPositions, g_hNextTrackMutex);
}

void PlayNextTrack() {
	::WaitForSingleObject(g_hPlayNextMutex, INFINITE);
	::WaitForSingleObject(g_hNextTrackMutex, INFINITE);
	WCHAR sz[MAX_PATH + 1] = { '\0' };
	wcscpy_s(sz, g_pszNextTrackPath);
	StartStopPositions posses = g_nextTrackStartStopPositions;
	::ReleaseMutex(g_hNextTrackMutex);

	enqueueFileWithMetaStructW w;
	w.filename = sz;
	w.length = 100;
	w.title = sz;

	::WaitForSingleObject(g_hCurrentTrackMutex, INFINITE);
	wcscpy_s(g_pszCurrentTrackPath, MAX_PATH, sz);
	g_currentTrackStartStopPositions = posses;
	::ReleaseMutex(g_hCurrentTrackMutex);

	::WaitForSingleObject(g_hNextTrackMutex, INFINITE);
	g_pszNextTrackPath[0] = '\0';
	g_nextTrackStartStopPositions = { MAXUINT32,MAXUINT32,MAXUINT32,MAXUINT32 };
	::ReleaseMutex(g_hNextTrackMutex);

	// Reset pitch on Pacemaker, if it is running.
	HWND pacemakerHwnd = ::FindWindowEx(NULL, NULL, NULL, L"PaceMaker Plug-in");
	if (pacemakerHwnd)
		::SendMessage(pacemakerHwnd, PM_RESET, 0, 0);

	::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_DELETE);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, (WPARAM)&w, IPC_PLAYFILEW);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_STARTPLAY);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, posses.startPos, IPC_JUMPTOTIME);
	::ReleaseMutex(g_hPlayNextMutex);

	::InvalidateRect(g_hDJWindow, NULL, FALSE);
}

void OnTrackStarted() {
	LRESULT listPos = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETLISTPOS);
	WCHAR* pszFileBeingPlayed = (WCHAR*)::SendMessage(g_hWinampWindow, WM_WA_IPC, listPos, IPC_GETPLAYLISTFILEW);
	// If it's a different file from what we're expecting it to be, then the real DJ has
	// started something manually. We need to calculate a new stop time.
	if (pszFileBeingPlayed) {
		WCHAR sz[MAX_PATH + 1] = { '\0' };
		::WaitForSingleObject(g_hCurrentTrackMutex, INFINITE);
		wcscpy_s(sz, g_pszCurrentTrackPath);
		::ReleaseMutex(g_hCurrentTrackMutex);

		if (_wcsicmp(sz, pszFileBeingPlayed))
			HandleInterruption(pszFileBeingPlayed);

		::WaitForSingleObject(g_hNextTrackMutex, INFINITE);
		DWORD stopPos = g_nextTrackStartStopPositions.stopPos;
		::ReleaseMutex(g_hNextTrackMutex);

		if (stopPos == MAXUINT32)
			FindAndProcessNextTrack();
	}
}

bool ShouldPlayNextTrack() {
	bool should = false;
	::WaitForSingleObject(g_hPlayNextMutex, INFINITE);
	::WaitForSingleObject(g_hCurrentTrackMutex, INFINITE);
	DWORD stopPos = g_currentTrackStartStopPositions.stopPos;
	::ReleaseMutex(g_hCurrentTrackMutex);

	if (stopPos != MAXUINT32) {
		DWORD time = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETOUTPUTTIME);
		if (time > 0) {
			time = (DWORD)(time * g_nTimeScaler);
			should = time > stopPos;
		}
	}
	if (!should) {
		LRESULT isPlayingResult = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_ISPLAYING);
		::WaitForSingleObject(g_hNextTrackMutex, INFINITE);
		bool hasNextTrack = wcslen(g_pszNextTrackPath) != 0;
		::ReleaseMutex(g_hNextTrackMutex);
		should = hasNextTrack && !isPlayingResult;
	}
	::ReleaseMutex(g_hPlayNextMutex);
	return should;
}

bool RequestFileHasBeenModified() {
	struct _stat64i32 stat;
	bool testOnly = (g_requestFileLastModifiedTime == 0);
	if (!_wstat(g_szRequestedTracksFilePath, &stat)) {
		if (stat.st_mtime != g_requestFileLastModifiedTime) {
			g_requestFileLastModifiedTime = stat.st_mtime;
			return true && !testOnly;
		}
	}
	else if (testOnly)
		g_requestFileLastModifiedTime = 1;
	return false;
}

DWORD WINAPI TimeWatcher(LPVOID pParam) {
	while (::WaitForSingleObject(g_hStopTimeWatcherEvent, 250) == WAIT_TIMEOUT) {
		if (RequestFileHasBeenModified() && !g_bNextTrackIsRequest)
			FindAndProcessNextTrack();
		if (ShouldPlayNextTrack())
			PlayNextTrack();
	}
	return 0;
}

void StartTimeWatcher() {
	g_hStopTimeWatcherEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hTimeWatcherThread = ::CreateThread(NULL, 0, TimeWatcher, NULL, 0, NULL);
}

void StopTimeWatcher() {
	::SetEvent(g_hStopTimeWatcherEvent);
	::WaitForSingleObject(g_hTimeWatcherThread, INFINITE);
	::CloseHandle(g_hStopTimeWatcherEvent);
}

LRESULT CALLBACK AutoDJWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_WA_IPC:
		switch (lParam) {
		case IPC_PLAYING_FILEW: {
			// This message indicates that a new file has started playing. It might be the one
			// we asked for, or it might be one that the "real" DJ has started manually. We need
			// to check.
			OnTrackStarted();
			break;
		}
		}
		break;
	}
	return ::CallWindowProc(g_pOriginalWndProc, hwnd, uMsg, wParam, lParam);
}

int init() {
	g_hInstance = plugin.hDllInstance;
	g_hWinampWindow = plugin.hwndParent;
	g_pOriginalWndProc = (WNDPROC)::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)AutoDJWndProc);

	g_hPlayNextMutex = ::CreateMutex(NULL, FALSE, NULL);
	if (g_hPlayNextMutex) {
		g_hCurrentTrackMutex = ::CreateMutex(NULL, FALSE, NULL);
		if (g_hCurrentTrackMutex) {
			g_hNextTrackMutex = ::CreateMutex(NULL, FALSE, NULL);
			if (g_hNextTrackMutex) {
				HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
				if (SUCCEEDED(hr)) {
					hr = ::MFStartup(MF_VERSION);
					if (SUCCEEDED(hr)) {
						ReadPrefs();
						ReadTrackList();
						StartTimeWatcher();
						CreateWindows();
					}
				}
			}
		}
	}

	return 0;
}

void config() {
	::ShellExecute(g_hWinampWindow, L"edit", g_szINIPath, NULL, NULL, SW_SHOWDEFAULT);
}

void quit() {
	StopTimeWatcher();
	FreeTrackList();
	::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)g_pOriginalWndProc);
	::MFShutdown();

	DestroyWindows();
	::CoUninitialize();
	if (g_hCurrentTrackMutex)
		::CloseHandle(g_hCurrentTrackMutex);
	if (g_hNextTrackMutex)
		::CloseHandle(g_hNextTrackMutex);
	if (g_hPlayNextMutex)
		::CloseHandle(g_hPlayNextMutex);
}