// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include <combaseapi.h>
#include <mfapi.h>
#include <shellapi.h>
#include <malloc.h>
#include "DJDefs.h"
#include "DJPrefs.h"
#include "DJFileScanner.h"
#include "DJTrackLists.h"

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

// Path to the file that the Auto DJ is now playing.
WCHAR g_pszNowPlayingPath[MAX_PATH + 1] = { '\0' };
StartStopPositions g_startStopPositions = { 0,0 };
HANDLE g_hTimeWatcherThread = NULL;
HANDLE g_hStopTimeWatcherEvent = NULL;

void CheckForInterruption() {
	int listPos = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETLISTPOS);
	const WCHAR* fileBeingPlayed = (const WCHAR*)::SendMessage(g_hWinampWindow, WM_WA_IPC, listPos, IPC_GETPLAYLISTFILEW);
	// If it's a different file from what we're expecting it to be, then the real DJ has
	// started something manually. We need to calculate a new stop time.
	if (fileBeingPlayed && _wcsicmp(g_pszNowPlayingPath, fileBeingPlayed))
		GetStartStopPositions(fileBeingPlayed, &g_startStopPositions);
}

void PlayNextTrack() {
	WCHAR* pszTrackPath = GetNextTrack();
	WCHAR szBuffer[MAX_PATH + 1];
	wcscpy_s(szBuffer, g_szMusicRootPath);
	int len = wcslen(szBuffer);
	if(szBuffer[len-1]!='\\')
		wcscat_s(szBuffer, L"\\");
	wcscat_s(szBuffer, pszTrackPath);
	wcscpy_s(g_pszNowPlayingPath, szBuffer);
	GetStartStopPositions(szBuffer, &g_startStopPositions);
	enqueueFileWithMetaStructW w;
	w.filename = szBuffer;
	w.length = 100;
	w.title = pszTrackPath;
	::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_DELETE);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, (WPARAM)&w, IPC_PLAYFILEW);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_STARTPLAY);
	::SendMessage(g_hWinampWindow, WM_WA_IPC, g_startStopPositions.startPos, IPC_JUMPTOTIME);
	free(pszTrackPath);
}

DWORD WINAPI TimeWatcher(LPVOID pParam) {
	while (::WaitForSingleObject(g_hStopTimeWatcherEvent, 250) == WAIT_TIMEOUT) {
		if (g_startStopPositions.stopPos) {
			DWORD time = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETOUTPUTTIME);
			if (time) {
				time = (DWORD)(time * g_nTimeScaler);
				if (time > g_startStopPositions.stopPos)
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
			CheckForInterruption();
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
	::CoUninitialize();
}