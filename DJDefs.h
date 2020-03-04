#pragma once
#include "pch.h"

// Plugin version (don't touch this!)
#define GPPHDR_VER 0x10

// Plugin title
#define PLUGIN_NAME "AutoDJ Plugin"

// main structure with plugin information, version, name...
typedef struct {
	int version;             // version of the plugin structure
	const char* description; // name/title of the plugin 
	int(*init)();            // function which will be executed on init event
	void(*config)();         // function which will be executed on config event
	void(*quit)();           // function which will be executed on quit event
	HWND hwndParent;         // hwnd of the Winamp client main window (stored by Winamp when dll is loaded)
	HINSTANCE hDllInstance;  // hinstance of this plugin DLL. (stored by Winamp when dll is loaded) 
} winampGeneralPurposePlugin;

// WinAmp messages.
#define WM_WA_IPC WM_USER
#define IPC_PLAYFILEW 1100
#define IPC_DELETE 101
#define IPC_STARTPLAY 102
#define IPC_ISPLAYING 104
#define IPC_JUMPTOTIME 106
#define IPC_PLAYING_FILEW 13003
#define IPC_GETLISTPOS 125
#define IPC_GETPLAYLISTFILEW 214
#define IPC_CB_MISC 603
#define IPC_GETOUTPUTTIME 105
#define IPC_CB_MISC_STATUS 2

typedef struct {
	const wchar_t* filename;
	const wchar_t* title;
	int length;
} enqueueFileWithMetaStructW;

typedef struct StartStopPositions {
	DWORD startPos;
	DWORD stopPos;
	DWORD audioStopPos;
	DWORD cdgStopPos;
} StartStopPositions;

// The CDG data packet structure.
typedef struct CDGPacket {
	BYTE command;
	BYTE instruction;
	BYTE unused1[2];
	BYTE data[16];
	BYTE unused2[4];
} CDGPacket;