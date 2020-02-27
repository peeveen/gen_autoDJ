#include "pch.h"
#include <stdio.h>
#include <wchar.h>
#include "DJGlobals.h"

#define PREF_BUFFER_SIZE (MAX_PATH*2)

// Path to the INI file
WCHAR g_szINIPath[MAX_PATH + 1] = { '\0' };
// Path the file containing the paths of all tracks we want to play.
WCHAR g_szTracksFilePath[PREF_BUFFER_SIZE] = { '\0' };
// Path the file containing the paths of all tracks that have been specifically requested.
WCHAR g_szRequestedTracksFilePath[PREF_BUFFER_SIZE] = { '\0' };
// Amplitude point from which to start songs.
int g_nStartThreshold = 10000;
// Amplitude point at which to stop songs.
int g_nStopThreshold = 15000;
// Amplitude point at which to stop karaoke songs.
int g_nKaraokeStopThreshold = 4000;
// Stupid scaler for WinAmp time bug.
double g_nTimeScaler = 1.00466;

void TrimLeading(WCHAR* pszString) {
	WCHAR* pszPointer = pszString;
	while (*pszPointer == ' ' || *pszPointer == '\t')
		++pszPointer;
	wcscpy_s(pszString, PREF_BUFFER_SIZE, pszPointer);
}

void SetPref(WCHAR* pszPrefLine, const WCHAR* pszPrefName, void* pDestVal, void (*pFunc)(const WCHAR*, void*)) {
	size_t len = wcslen(pszPrefName);
	if (!wcsncmp(pszPrefLine, pszPrefName, len)) {
		wcscpy_s(pszPrefLine, PREF_BUFFER_SIZE, pszPrefLine + len);
		TrimLeading(pszPrefLine);
		while (wcslen(pszPrefLine) && pszPrefLine[0] == '=')
			++pszPrefLine;
		TrimLeading(pszPrefLine);
		WCHAR* pszNewLine = wcsrchr(pszPrefLine, '\n');
		if (pszNewLine)
			*pszNewLine = '\0';
		if (wcslen(pszPrefLine))
			pFunc(pszPrefLine, pDestVal);
	}
}

void SetBoolValue(const WCHAR* pszPrefValue, void* pbDestVal) {
	*(bool*)pbDestVal = wcslen(pszPrefValue) && (pszPrefValue[0] == 't' || pszPrefValue[0] == 'y');
}

void SetStringValue(const WCHAR* pszPrefValue, void* pszDestVal) {
	wcscpy_s((WCHAR*)pszDestVal, PREF_BUFFER_SIZE, pszPrefValue);
}

void SetIntValueRadix(const WCHAR* pszPrefValue, int* pnDestVal, int radix) {
	*(int*)pnDestVal = wcstol(pszPrefValue, NULL, radix);
}

void SetIntValue(const WCHAR* pszPrefValue, void* pnDestVal) {
	SetIntValueRadix(pszPrefValue, (int*)pnDestVal, 10);
}

void SetHexIntValue(const WCHAR* pszPrefValue, void* pnDestVal) {
	SetIntValueRadix(pszPrefValue, (int*)pnDestVal, 16);
}

void SetInt(WCHAR* pszPrefLine, const WCHAR* pszPrefName, int* pnDestVal, int nMin, int nMax, bool hex = false) {
	SetPref(pszPrefLine, pszPrefName, pnDestVal, hex ? SetHexIntValue : SetIntValue);
	if (*pnDestVal < nMin)
		*pnDestVal = nMin;
	else if (*pnDestVal > nMax)
		*pnDestVal = nMax;
}

void SetBool(WCHAR* pszPrefLine, const WCHAR* pszPrefName, bool* pbDestVal) {
	SetPref(pszPrefLine, pszPrefName, pbDestVal, SetBoolValue);
}

void SetString(WCHAR* pszPrefLine, const WCHAR* pszPrefName, WCHAR* pszDestVal) {
	SetPref(pszPrefLine, pszPrefName, pszDestVal, SetStringValue);
}

void SetStartThreshold(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"startamplitudethreshold", &g_nStartThreshold, 0, 32767);
}

void SetStopThreshold(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"stopamplitudethreshold", &g_nStopThreshold, 0, 32767);
}

void SetKaraokeStopThreshold(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"karaokestopamplitudethreshold", &g_nKaraokeStopThreshold, 0, 32767);
}

void SetTracksFilePath(WCHAR* pszPrefLine) {
	SetString(pszPrefLine, L"tracksfilepath", g_szTracksFilePath);
}

void SetRequestedTracksFilePath(WCHAR* pszPrefLine) {
	SetString(pszPrefLine, L"requestedtracksfilepath", g_szRequestedTracksFilePath);
}

bool SetINIPath() {
	g_szINIPath[0] = '\0';
	if (GetModuleFileName(g_hInstance, g_szINIPath, MAX_PATH)) {
		WCHAR* pszLastSlash = wcsrchr(g_szINIPath, '\\');
		if (pszLastSlash) {
			*(pszLastSlash + 1) = '\0';
			wcscat_s(g_szINIPath, L"gen_autoDJ.ini");
			return true;
		}
	}
	return false;
}

bool ReadPrefs() {
	if (SetINIPath()) {
		FILE* pFile = NULL;
		errno_t error = _wfopen_s(&pFile, g_szINIPath, L"rt, ccs=UTF-8");
		if (pFile && !error) {
			WCHAR szBuffer[PREF_BUFFER_SIZE+1];
			while (fgetws(szBuffer, PREF_BUFFER_SIZE, pFile)) {
				_wcslwr_s(szBuffer);
				TrimLeading(szBuffer);
				SetTracksFilePath(szBuffer);
				SetRequestedTracksFilePath(szBuffer);
				SetStartThreshold(szBuffer);
				SetStopThreshold(szBuffer);
				SetKaraokeStopThreshold(szBuffer);
			}
			fclose(pFile);
			return true;
		}
	}
	return false;
}