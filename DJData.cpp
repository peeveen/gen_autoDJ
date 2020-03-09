#include "pch.h"
#include <wchar.h>
#include "DJDefs.h"

// Path to the file that the Auto DJ is now playing.
WCHAR g_pszCurrentTrackPath[MAX_PATH + 1] = { '\0' };
WCHAR g_pszNextTrackPath[MAX_PATH + 1] = { '\0' };
StartStopPositions g_currentTrackStartStopPositions = { MAXUINT32,MAXUINT32,MAXUINT32,MAXUINT32 };
StartStopPositions g_nextTrackStartStopPositions = { MAXUINT32,MAXUINT32,MAXUINT32,MAXUINT32 };