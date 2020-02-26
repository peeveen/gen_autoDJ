#pragma once
#include "pch.h"

bool ReadPrefs();

extern WCHAR g_szINIPath[];
extern WCHAR g_szTracksFilePath[];
extern WCHAR g_szRequestedTracksFilePath[];
extern int g_nStartThreshold;
extern int g_nStopThreshold;
extern double g_nTimeScaler;