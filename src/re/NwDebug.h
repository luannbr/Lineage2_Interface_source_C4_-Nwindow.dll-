#pragma once
// Proof mode (hook builds only): with [Debug] Marker=1 in NWindowDecomp.ini our code draws
// marks the stock code never draws, to check in game that the hooks are live:
//   - a line at the top of the screen with the hook count and the build time
//     (NCVirtualWndMain::DispatchWndMsg, after the windows are painted)
//   - '#' instead of '*' in password fields and a red caret (NCEditBox::OnPaint)
// With [Hooks] Enabled=0 nothing of ours runs, so no mark shows even with Marker=1; with
// Disable=NCEditBox::* only the edit box marks go away. Not stock behavior: the call sites
// use NW_DEBUG_MARKER(), which is 0 outside hook builds.

// NW_GAME_SOURCE (CMake option NWD_GAME_SOURCES): 1 when src/nwindow is compiled in; 0 for the
// infrastructure-only build (src/re alone), which has no NCGDevice to draw with.
#ifndef NW_GAME_SOURCE
#define NW_GAME_SOURCE 0
#endif

class NCGDevice;

namespace nw {

bool DebugMarker();                          // [Debug] Marker=1 (and hooks enabled)
void DrawDebugMarker(NCGDevice* Device);     // the line at the top of the screen (NW_GAME_SOURCE only)

extern int g_hooksInstalled;                 // set at startup (NwMain.cpp)
extern SYSTEMTIME g_buildTime;               // link time of NWindowDecomp.dll, local time

}  // namespace nw

#if NW_HOOK_MODE
#define NW_DEBUG_MARKER() (::nw::DebugMarker())
#else
#define NW_DEBUG_MARKER() 0
#endif
