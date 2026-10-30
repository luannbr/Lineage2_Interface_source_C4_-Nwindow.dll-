/*=============================================================================
    NwDebug.cpp - proof mode marks (see NwDebug.h). Not stock behavior.
=============================================================================*/
#include "re/NwLog.h"
#include "re/NwDebug.h"
#if NW_GAME_SOURCE
#include "nwindow/NCGDevice.h"
#endif

namespace nw {

int g_hooksInstalled = 0;
SYSTEMTIME g_buildTime = {};

bool DebugMarker()
{
    return GetConfig().debugMarker && GetConfig().hooksEnabled;
}

#if NW_GAME_SOURCE
// yellow text on a dark box, centered at the top of the screen; the first frame is logged
// (position, screen and text size) so the log shows that the paint path ran
void DrawDebugMarker(NCGDevice* Device)
{
    if (!Device)
        return;
    TCHAR Text[160];
    appSprintf(Text, TEXT("NWindowDecomp ativo: %d funcoes nossas, build %02d/%02d %02d:%02d"),
               g_hooksInstalled, g_buildTime.wDay, g_buildTime.wMonth, g_buildTime.wHour,
               g_buildTime.wMinute);
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    INT X = (GSEKScreenX - Size.cx) / 2;
    INT Y = 40;
    Device->BeginPaint(0, 0);
    Device->DrawRect(X - 6, Y - 3, Size.cx + 12, Size.cy + 6, 0xC0000000);
    Device->DrawText(X, Y, 0xFFFFFF00, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();

    static bool bLogged = false;
    if (!bLogged)
    {
        bLogged = true;
        Log("[Debug] marker drawn at %d,%d: screen %dx%d, text %dx%d, canvas %08lx", X, Y, GSEKScreenX,
            GSEKScreenY, Size.cx, Size.cy, (DWORD)Device->Canvas);
    }
}
#endif  // NW_GAME_SOURCE (no game source, no painter: nothing calls it)

}  // namespace nw
