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

void HandleInterruption(const WCHAR *pszFileBeingPlayed) {
	g_currentTrackStartStopPositions = { MAXUINT32,MAXUINT32 };
	wcscpy_s(g_pszCurrentTrackPath, MAX_PATH, pszFileBeingPlayed);
	GetStartStopPositions(false,true,pszFileBeingPlayed, &g_currentTrackStartStopPositions);
	::InvalidateRect(g_hDJWindow, NULL, FALSE);
}

void FindAndProcessNextTrack() {
	WCHAR* pszTrackPath = GetNextTrack();
	if (pszTrackPath) {
		wcscpy_s(g_pszNextTrackPath, MAX_PATH, pszTrackPath);
		GetStartStopPositions(true,true,g_pszNextTrackPath, &g_nextTrackStartStopPositions);
		free(pszTrackPath);
	}
}

void PlayNextTrack() {
	enqueueFileWithMetaStructW w;
	w.filename = g_pszNextTrackPath;
	w.length = 100;
	w.title = g_pszNextTrackPath;
	wcscpy_s(g_pszCurrentTrackPath, MAX_PATH, g_pszNextTrackPath);
	g_currentTrackStartStopPositions = g_nextTrackStartStopPositions;
	g_nextTrackStartStopPositions = { MAXUINT32-1,MAXUINT32-1 };
	::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_DELETE);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, (WPARAM)&w, IPC_PLAYFILEW);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_STARTPLAY);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, g_currentTrackStartStopPositions.startPos, IPC_JUMPTOTIME);
	FindAndProcessNextTrack();
	::InvalidateRect(g_hDJWindow, NULL, FALSE);
}

void OnTrackStarted() {
	LRESULT listPos = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETLISTPOS);
	const WCHAR* pszFileBeingPlayed = (const WCHAR*)::SendMessage(g_hWinampWindow, WM_WA_IPC, listPos, IPC_GETPLAYLISTFILEW);
	// If it's a different file from what we're expecting it to be, then the real DJ has
	// started something manually. We need to calculate a new stop time.
	if (pszFileBeingPlayed) {
		if (_wcsicmp(g_pszCurrentTrackPath, pszFileBeingPlayed))
			HandleInterruption(pszFileBeingPlayed);
		if(g_nextTrackStartStopPositions.stopPos==MAXUINT32)
			FindAndProcessNextTrack();
	}
}

DWORD WINAPI TimeWatcher(LPVOID pParam) {
	while (::WaitForSingleObject(g_hStopTimeWatcherEvent, 250) == WAIT_TIMEOUT) {
		if (g_currentTrackStartStopPositions.stopPos <MAXUINT32-1) {
			DWORD time = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETOUTPUTTIME);
			if (time) {
				time = (DWORD)(time * g_nTimeScaler);
				if (time > g_currentTrackStartStopPositions.stopPos)
					PlayNextTrack();
			}
		}
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
		case IPC_CB_MISC:
			// If the music is ever somehow stopped, start it again (unless we're quitting).
			if (wParam == IPC_CB_MISC_STATUS) {
				if (::WaitForSingleObject(g_hStopTimeWatcherEvent, 0) == WAIT_TIMEOUT) {
					LRESULT isPlayingResult = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_ISPLAYING);
					if (!isPlayingResult)
						PlayNextTrack();
				}
			}
			break;
		}
		break;
	}
	return ::CallWindowProc(g_pOriginalWndProc, hwnd, uMsg, wParam, lParam);
}

int init() {
	g_hInstance = plugin.hDllInstance;
	g_hWinampWindow = plugin.hwndParent;
	g_pOriginalWndProc = (WNDPROC)::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)AutoDJWndProc);

	HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED(hr)) {
		hr = ::MFStartup(MF_VERSION);
		if (SUCCEEDED(hr)) {
			ReadPrefs();
			ReadTrackList();
			StartTimeWatcher();
		}
	}

	CreateWindows();

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
}