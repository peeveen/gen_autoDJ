#pragma once
#include "pch.h"

bool CreateWindows();
void DestroyWindows();

extern HDC g_hDJWindowDC;
extern HWND g_hDJWindow;