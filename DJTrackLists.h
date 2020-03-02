#pragma once
#include "pch.h"

extern WCHAR* GetNextTrack(bool *pbRequest);
extern void ReadTrackList();
extern void FreeTrackList();