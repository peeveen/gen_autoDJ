#include "pch.h"
#include "DJPrefs.h"
#include <malloc.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>

WCHAR** g_ppszTrackPaths=NULL;
int g_nTracks=0;
bool g_bSeeded = false;

void ReadTrackFile(const WCHAR* pszPath, WCHAR*** pppszList, int* pnCount) {
	FILE* pFile = NULL;
	*pnCount = 0;
	*pppszList = (WCHAR**)malloc(0);
	if (*pppszList) {
		errno_t error = _wfopen_s(&pFile, pszPath, L"rt, ccs=UTF-8");
		if (pFile && !error) {
			static WCHAR szBuffer[MAX_PATH + 1];
			while (fgetws(szBuffer, MAX_PATH, pFile)) {
				WCHAR* pszNewLine = wcsrchr(szBuffer, '\n');
				if (pszNewLine)
					*pszNewLine = '\0';
				int len = wcslen(szBuffer);
				++* pnCount;
				WCHAR **reallocedStringArray = (WCHAR**)realloc(*pppszList, sizeof(WCHAR*) * (*pnCount));
				if (reallocedStringArray) {
					*pppszList = reallocedStringArray;
					++len;
					WCHAR* pszNewString = (WCHAR*)malloc(sizeof(WCHAR) * len);
					if (pszNewString) {
						wcscpy_s(pszNewString, len, szBuffer);
						(*pppszList)[(*pnCount) - 1] = pszNewString;
					}
				}
			}
			fclose(pFile);
		}
	}
}

void WriteTrackFile(const WCHAR* pszPath, WCHAR** ppszList, int nCount) {
	FILE* pFile = NULL;
	errno_t error = _wfopen_s(&pFile, pszPath, L"wt");
	if (pFile && !error) {
		for (int f = 0; f < nCount; ++f) {
			if (ppszList[f]) {
				fputws(ppszList[f], pFile);
				fputws(L"\n", pFile);
			}
		}
		fclose(pFile);
	}
}

WCHAR* GetNextRequestedTrack() {
	WCHAR** ppszRequestedTrackPaths;
	int nRequestedTracks;
	ReadTrackFile(g_szRequestedTracksFilePath, &ppszRequestedTrackPaths, &nRequestedTracks);
	if (nRequestedTracks) {
		srand((UINT)time(NULL));
		int nTrackIndex = 0; // rand() % nRequestedTracks;
		WCHAR* pszTrack = _wcsdup(ppszRequestedTrackPaths[nTrackIndex]);
		free(ppszRequestedTrackPaths[nTrackIndex]);
		ppszRequestedTrackPaths[nTrackIndex] = NULL;
		WriteTrackFile(g_szRequestedTracksFilePath, ppszRequestedTrackPaths, nRequestedTracks);
		for (int f = 0; f < nRequestedTracks; ++f)
			free(ppszRequestedTrackPaths[f]);
		free(ppszRequestedTrackPaths);
		return pszTrack;
	}
	return NULL;
}

void ReadTrackList() {
	ReadTrackFile(g_szTracksFilePath, &g_ppszTrackPaths, &g_nTracks);
}

void FreeTrackList() {
	for (int f = 0; f < g_nTracks; ++f)
		free(g_ppszTrackPaths[f]);
	free(g_ppszTrackPaths);
}

WCHAR* GetNextTrack(bool *pbRequest) {
	WCHAR* pszTrack = GetNextRequestedTrack();
	if (!pszTrack) {
		*pbRequest = false;
		if (!g_nTracks) {
			FreeTrackList();
			ReadTrackList();
		}
		if (g_nTracks) {
			if (!g_bSeeded) {
				srand((UINT)time(NULL));
				g_bSeeded = true;
			}
			int nTrackIndex = rand() % g_nTracks;
			pszTrack = _wcsdup(g_ppszTrackPaths[nTrackIndex]);
			free(g_ppszTrackPaths[nTrackIndex]);
			g_ppszTrackPaths[nTrackIndex] = NULL;
			memcpy(g_ppszTrackPaths + nTrackIndex, g_ppszTrackPaths + nTrackIndex + 1, sizeof(WCHAR*) * ((g_nTracks - nTrackIndex) - 1));
			--g_nTracks;
		}
	}
	else
		*pbRequest = true;
	return pszTrack;
}

