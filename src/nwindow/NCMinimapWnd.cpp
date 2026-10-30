/*=============================================================================
    NCMinimapWnd.cpp - the map window (C4: seventeen maps, a world map in six pieces, party markers). Stock:
    0x100b8980 (WindowProc), 0x100b8bd0 .. 0x100b8ca0 (the left button, the maps' sizes), 0x100b92f0 .. 0x100b9b90 (the
    buttons, the coordinates, the zone to the map), 0x100ba110 (the tables), 0x100ba530 (the tool tips), 0x100bac60 ..
    0x100bb2f0 (mouse, positions, marks, the zoom out button), 0x100bbac0 .. 0x100bd170 (create, paint, construct, destroy).
    The stock helpers that are ECX-agnostic (the map tables) are called with the window's `this`, which they ignore.
=============================================================================*/
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NCMapCoverWnd.h"
#include "nwindow/NCRadarWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCQuestGaraTreeWnd.h"
#include "nwindow/NMapBase24.h"
#include "nwindow/NWinBStock.h"

// the tables of the stock image (hook mode: they live in its .data; nothing writes them)
typedef INT NMinimapIntTable[17];
typedef FLOAT NMinimapFloatTable[17];
typedef INT NMinimapTownTable[16];
typedef FString NMinimapTownNames[16];
#if NW_HOOK_MODE
#define GMapWidths      NW_GLOBAL(NMinimapIntTable, 0x1025CCE0)    // 1804, 653, 768, 606 ...
#define GMapHeights     NW_GLOBAL(NMinimapIntTable, 0x1025CD28)    // 2624, 626, 706, 647 ...
#define GTownPosX       NW_GLOBAL(NMinimapTownTable, 0x1025CB40)   // where the 16 town buttons sit on the world map
#define GTownPosY       NW_GLOBAL(NMinimapTownTable, 0x1025CB80)
#define GMapScaleX      NW_GLOBAL(NMinimapFloatTable, 0x1025CBC0)
#define GMapScaleY      NW_GLOBAL(NMinimapFloatTable, 0x1025CC08)
#define GMapOriginX     NW_GLOBAL(NMinimapIntTable, 0x1025CC50)
#define GMapOriginY     NW_GLOBAL(NMinimapIntTable, 0x1025CC98)
#define GTownMapNames   NW_GLOBAL(NMinimapTownNames, 0x102E9300)   // "map_gludin_village" ... turned into the textures' names by
                                                                   // LoadMapData
#else
// the standalone's copies (src/standalone/NwGlobals.cpp, tools/gen_globals.py), with the stock initial values; each map table has
// the 17 maps and, as in the stock image, a trailing 0
extern INT GMapWidths[18];
extern INT GMapHeights[18];
extern INT GTownPosX[16];
extern INT GTownPosY[16];
extern FLOAT GMapScaleX[18];
extern FLOAT GMapScaleY[18];
extern INT GMapOriginX[18];
extern INT GMapOriginY[18];
extern FString GTownMapNames[16];
#endif

// the player's world position as the stock reads it inline (the first viewport's controller's pawn; 0 without one):
// UE2 inlined `GetMyLocation()` into the map's functions, one coordinate at a time
static FORCEINLINE FVector MyLocation()
{
    FVector Result(0.0f, 0.0f, 0.0f);
    APlayerController* Controller = GConsole->Engine->Client->Viewports(0)->Actor;
    if (Controller && Controller->Pawn)
        Result = Controller->Pawn->Location;
    return Result;
}

static FORCEINLINE INT NAbs(INT Value)
{
    return Value < 0 ? -Value : Value;
}

// the desktop's tracked quest (0x100B90F0: the id the quest window's list has picked, the console itself when there is
// none) and a quest's name (0x10067510: NULL when the id is unknown): functions of the console
static FORCEINLINE INT CurrentQuestID()
{
    return ((NConsoleLeftFns*)GConsole)->GetTrackedQuestID();
}
static FORCEINLINE TCHAR* GetQuestName(INT QuestID)
{
    return GConsole->GetQuestName(QuestID);
}

// 0x100BB860 - the party map's pairs array destructor (NMapBase24.cpp hooks it): the hook build calls the stock function, the
// standalone the template's own
static FORCEINLINE void DestroyPartyPairs(void* Party)
{
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x100BB860, Party);
#else
    reinterpret_cast<TMapPair24Array*>(Party)->~TMapPair24Array();
#endif
}

// the tool tip's nine pieces from the skin (the stock inlines this into both tool tips): the corners 8 x 8, the edges
// and the fill stretched to W x H
static FORCEINLINE void DrawToolTipSkin(NCGDevice* Device, NToolTipSkin* Skin, INT X, INT Y, INT W, INT H)
{
    if (!Skin)
        return;
    if (Skin->Tex[0])
        Device->DrawTexture(X, Y, 8, 8, 0, 0, 8, 8, Skin->Tex[0], 0xFF, 1);
    if (Skin->Tex[1])
        Device->DrawStretchedTex(X + 8, Y, W, 8, Skin->Tex[1], 1, 0xFF);
    if (Skin->Tex[2])
        Device->DrawTexture(W + X + 8, Y, 8, 8, 0, 0, 8, 8, Skin->Tex[2], 0xFF, 1);
    if (Skin->Tex[3])
        Device->DrawStretchedTex(X, Y + 8, 8, H, Skin->Tex[3], 1, 0xFF);
    if (Skin->Tex[4])
        Device->DrawStretchedTex(X + 8, Y + 8, W, H, Skin->Tex[4], 1, 0xFF);
    if (Skin->Tex[5])
        Device->DrawStretchedTex(W + X + 8, Y + 8, 8, H, Skin->Tex[5], 1, 0xFF);
    if (Skin->Tex[6])
        Device->DrawTexture(X, H + Y + 8, 8, 8, 0, 0, 8, 8, Skin->Tex[6], 0xFF, 1);
    if (Skin->Tex[7])
        Device->DrawStretchedTex(X + 8, H + Y + 8, W, 8, Skin->Tex[7], 1, 0xFF);
    if (Skin->Tex[8])
        Device->DrawTexture(W + X + 8, H + Y + 8, 8, 8, 0, 0, 8, 8, Skin->Tex[8], 0xFF, 1);
}

// 0x100bcf70 - the arrays empty (their constructors), the scroll at the corner; Mode and the click position are left
// to the allocator's zeroes (the stock constructor does not store them)
NCMinimapWnd::NCMinimapWnd()
{
    MyPosTex = NULL;
    MyX = 0;
    MyY = 0;
    ScrollX = 0;
    ScrollY = 0;
    SavedScrollX = 0;
    SavedScrollY = 0;
    Unknown1A8 = 0;
    MapIndex = 0;
    bDragging = 0;
    bShowMark = 0;
    bMyPosHover = 0;
    bMarkHover = 0;
}
NW_HOOK_CTOR(0x100bcf70, NCMinimapWnd)

// 0x100b8980 (vslot 25) - the cover's buttons: the zoom out and the 16 towns, each through its own slot (74, 75) that
// answers 1 when it took the command
INT NCMinimapWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if (wParam == NID_MINIMAP_ZOOMOUT)
            return ::nw::VirtualCall<INT>(this, 74) != 0;
        if (wParam == NID_MINIMAP_TOWN + 0)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 0) != 0;
        if (wParam == NID_MINIMAP_TOWN + 1)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 1) != 0;
        if (wParam == NID_MINIMAP_TOWN + 2)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 2) != 0;
        if (wParam == NID_MINIMAP_TOWN + 3)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 3) != 0;
        if (wParam == NID_MINIMAP_TOWN + 4)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 4) != 0;
        if (wParam == NID_MINIMAP_TOWN + 5)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 5) != 0;
        if (wParam == NID_MINIMAP_TOWN + 6)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 6) != 0;
        if (wParam == NID_MINIMAP_TOWN + 7)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 7) != 0;
        if (wParam == NID_MINIMAP_TOWN + 8)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 8) != 0;
        if (wParam == NID_MINIMAP_TOWN + 9)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 9) != 0;
        if (wParam == NID_MINIMAP_TOWN + 10)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 10) != 0;
        if (wParam == NID_MINIMAP_TOWN + 11)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 11) != 0;
        if (wParam == NID_MINIMAP_TOWN + 12)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 12) != 0;
        if (wParam == NID_MINIMAP_TOWN + 13)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 13) != 0;
        if (wParam == NID_MINIMAP_TOWN + 14)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 14) != 0;
        if (wParam == NID_MINIMAP_TOWN + 15)
            return ::nw::VirtualCall<INT>(this, 75, NID_MINIMAP_TOWN + 15) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100b8980, NCMinimapWnd, WindowProc)

// 0x100b8bd0 (vslot 68) - a press inside the map starts a drag: the click is kept, the cursor is the hand (3) and
// the console locks it
INT NCMinimapWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    INT X = (SHORT)LOWORD(lParam) - ScreenX;
    INT Y = (SHORT)HIWORD(lParam) - ScreenY;
    if (X >= 1 && X <= (INT)Width && Y >= 1 && Y <= (INT)Height)
    {
        ClickX = X;
        ClickY = Y;
        bDragging = 1;
        SetCursor(3, NULL);
        GConsole->LockCursor();
    }
    return 1;
}
NW_HOOK_METHOD(0x100b8bd0, NCMinimapWnd, OnLButtonDown)

// 0x100b8c50 (vslot 69)
INT NCMinimapWnd::OnLButtonUp(WPARAM wParam, LPARAM lParam)
{
    GConsole->UnlockCursor();
    bDragging = 0;
    return 1;
}
NW_HOOK_METHOD(0x100b8c50, NCMinimapWnd, OnLButtonUp)

// 0x100b8c80 - the map's width in pixels (0 for a number out of 0..16)
INT NCMinimapWnd::GetMapWidth(INT Map)
{
    if (Map >= 0 && Map < 17)
        return GMapWidths[Map];
    return 0;
}
NW_HOOK_METHOD(0x100b8c80, NCMinimapWnd, GetMapWidth)

// 0x100b8ca0 - the map's height in pixels (0 for a number out of 0..16)
INT NCMinimapWnd::GetMapHeight(INT Map)
{
    if (Map >= 0 && Map < 17)
        return GMapHeights[Map];
    return 0;
}
NW_HOOK_METHOD(0x100b8ca0, NCMinimapWnd, GetMapHeight)

// 0x100b92f0 (guard RearrangeMapDisplay) - on the world map the town buttons are shown and put where the scroll says (their
// positions are the tables'; the buttons are the window's children, so these are relative to it); on a town's map they
// are hidden
void NCMinimapWnd::RearrangeMapDisplay(INT Map)
{
    guard(NCMinimapWnd::RearrangeMapDisplay);
    INT i;
    if (Map == 0)
    {
        for (i = 0; i < TownButtons.Num(); i++)
        {
            if (!TownButtons(i)->IsShowWindow())
                TownButtons(i)->ShowWindow();
        }
        for (i = 0; i < TownPos.Num(); i++)
            TownButtons(i)->SetPosition(TownPos(i).X - ScrollX, TownPos(i).Y - ScrollY, 0x1A, 0x1A);
    }
    else
    {
        for (i = 0; i < TownButtons.Num(); i++)
        {
            if (TownButtons(i)->IsShowWindow())
                TownButtons(i)->HideWindow();
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x100b92f0, NCMinimapWnd, RearrangeMapDisplay)

// 0x100b9430 - a world position as map pixels: the first map adds its origin before the scale, the others subtract
// theirs (the scale is a float: the product of the int and the float is exact in the x87)
void NCMinimapWnd::WorldToMapPixels(INT* X, INT* Y, INT Map)
{
    if (Map == 0)
    {
        *X = (INT)((DOUBLE)(MapOrigins(0).X + *X) * MapScales(0).X);
        *Y = (INT)((DOUBLE)(MapOrigins(0).Y + *Y) * MapScales(0).Y);
        return;
    }
    *X = (INT)((DOUBLE)(*X - MapOrigins(Map).X) * MapScales(Map).X);
    *Y = (INT)((DOUBLE)(*Y - MapOrigins(Map).Y) * MapScales(Map).Y);
}
NW_HOOK_METHOD(0x100b9430, NCMinimapWnd, WorldToMapPixels)

// 0x100b9600 (guard RefreshButtonLoc) - after a scroll, on the world map: each town button shown where its position says
void NCMinimapWnd::RefreshButtonLoc()
{
    guard(NCMinimapWnd::RefreshButtonLoc);
    if (MapIndex == 0)
    {
        for (INT i = 0; i < TownPos.Num(); i++)
        {
            TownButtons(i)->ShowWindow();
            TownButtons(i)->SetPosition(TownPos(i).X - ScrollX, TownPos(i).Y - ScrollY, 0x1A, 0x1A);
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x100b9600, NCMinimapWnd, RefreshButtonLoc)

// 0x100b96d0 - the map a position belongs to: the zone the game data gives for it (the player's own zone when the
// position is the player's) by the table of the zones that have a town map, else the world map
INT NCMinimapWnd::GetMapIndex(INT X, INT Y, INT Z)
{
    INT Zone;
    if (X == (INT)MyLocation().X && Y == (INT)GConsole->GetMyLocation().Y && Z == (INT)GConsole->GetMyLocation().Z)
        Zone = GConsole->GetCurrentZoneID();
    else
        Zone = GConsole->GetZoneID((FLOAT)X, (FLOAT)Y, (FLOAT)Z);
    if (Zone >= 0)
    {
        if (Zone < 0x4B)
        {
            if (Zone == 0x4A)
                return 10;
            switch (Zone)
            {
            case 0x14:
                return 3;
            case 0x15:
                return 2;
            case 0x23:
                return 1;
            case 0x2B:
                return 9;
            case 0x30:
                return 5;
            case 0x3A:
                return 6;
            case 0x3E:
                return 0xD;
            case 0x40:
                return 0xE;
            }
        }
        else
        {
            switch (Zone)
            {
            case 0x4D:
                return 0xB;
            case 0x65:
                return 7;
            case 0x6B:
                return 4;
            case 0x71:
                return 8;
            case 0x76:
            case 0x7A:
                return 0xC;
            case 0x10D:
                return 0x10;
            case 0x123:
                return 0xF;
            }
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x100b96d0, NCMinimapWnd, GetMapIndex)

// 0x100b99d0 - where the player is on a map: the harbor's box (the mark at (100, 1334)) or the pawn's position by
// WorldToMapPixels
void NCMinimapWnd::GetMyMapPos(INT* OutX, INT* OutY, INT Map)
{
    INT PX = (INT)MyLocation().X;
    INT PY = (INT)MyLocation().Y;
    INT PZ = (INT)MyLocation().Z;
    if (PX >= 0x9E0B && PX <= 0xC3FC && PY >= 0x3AFAD && PY <= 0x3D4E7 && PZ <= -0x130A && PZ >= -0x1ADA)
    {
        *OutX = 100;
        *OutY = 0x536;
        return;
    }
    WorldToMapPixels(&PX, &PY, Map);
    *OutX = PX;
    *OutY = PY;
}
NW_HOOK_METHOD(0x100b99d0, NCMinimapWnd, GetMyMapPos)

// 0x100b9b30 - a world position on a map (the harbor's box has its own mark)
void NCMinimapWnd::WorldToMap(INT WX, INT WY, INT* OutX, INT* OutY, INT Map)
{
    if (WX >= 0x9E0B && WX <= 0xC3FC && WY >= 0x3AFAD && WY <= 0x3D4E7)
    {
        *OutX = 100;
        *OutY = 0x536;
        return;
    }
    *OutX = WX;
    *OutY = WY;
    WorldToMapPixels(OutX, OutY, Map);
}
NW_HOOK_METHOD(0x100b9b30, NCMinimapWnd, WorldToMap)

// 0x100b9b90 (slot 75) - a town button of the world map: its map (the ids start at map 1), the world map's scroll kept,
// the buttons hidden, the zoom out shown, the piece centered on the player when he is on that map
INT NCMinimapWnd::OnTownButton(UINT ID)
{
    if (ID > 0xB00FFF && ID < 0xB01010)
    {
        MapIndex = ID - 0xB00FFF;
        SavedScrollX = ScrollX;
        SavedScrollY = ScrollY;
        for (INT i = 0; i < TownButtons.Num(); i++)
            TownButtons(i)->HideWindow();
        ZoomOutButton->ShowWindow();
        INT PX = (INT)MyLocation().X;
        INT PY = (INT)MyLocation().Y;
        INT W = GetMapWidth(MapIndex);
        INT H = GetMapHeight(MapIndex);
        if (W > 0 && H > 0)
        {
            WorldToMapPixels(&PX, &PY, MapIndex);
            if (PX < 0 || PX > GMapWidths[MapIndex] || PY < 0 || PY > GMapHeights[MapIndex])
                ScrollX = 0;
            else
            {
                if (PX < 0xA4)
                    ScrollX = 0;
                else if (PX > W - 0xA4)
                    ScrollX = W - 0x148;
                else
                    ScrollX = PX - 0xA4;
                if (PY >= 0xA4)
                {
                    if (PY > H - 0xA4)
                        ScrollY = H - 0x148;
                    else
                        ScrollY = PY - 0xA4;
                    SetFocus();
                    return 1;
                }
            }
            ScrollY = 0;
            SetFocus();
            return 1;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x100b9b90, NCMinimapWnd, OnTownButton)

// 0x100ba110 - OnCreate's tables and buttons: the positions of the town buttons and the maps' scales and origins from
// the image's tables, a 26 x 26 button per town (ids 0xB01000 + n) and the textures of the maps: the world map's six
// pieces (named in localization.ini by the country, with a default) and the towns' ("<TownMap>.<name>": the strings of
// the image are overwritten by the full names)
void NCMinimapWnd::LoadMapData()
{
    INT i;
    for (i = 0; i < 16; i++)
    {
        FMinimapTarget P;
        P.X = GTownPosX[i];
        P.Y = GTownPosY[i];
        P.Z = 0;
        TownPos.AddItem(P);
    }
    for (i = 0; i < 17; i++)
    {
        FMinimapScale S;
        S.X = GMapScaleX[i];
        S.Y = GMapScaleY[i];
        MapScales.AddItem(S);
    }
    for (i = 0; i < 17; i++)
    {
        FMinimapTarget O;
        O.X = GMapOriginX[i];
        O.Y = GMapOriginY[i];
        O.Z = 0;
        MapOrigins.AddItem(O);
    }
    for (i = 0; i < TownPos.Num(); i++)
    {
        NCButton* Button = new NCButton(TEXT("L2UI_CH3.Minimap.MapButton_ZoomIn1"),
                                        TEXT("L2UI_CH3.Minimap.MapButton_ZoomIn2"), 0);
        Button->ID = NID_MINIMAP_TOWN + i;
        CreateChild(Button, NWS_VISIBLE | NWS_CLIPTOPARENT);
        Button->SetPosition(TownPos(i).X - ScrollX, TownPos(i).Y - ScrollY, 0x1A, 0x1A);
        TownButtons.AddItem(Button);
    }

    FString Name;
    TCHAR Key[256];
    appSprintf(Key, TEXT("%s_minimap"), GCountryName);
    if (!GConfig->GetString(GCountryName, Key, Name, TEXT("localization.ini")))
        Name = TEXT("L2Font.Minimap.ch4_worldmap");
    for (i = 1; i <= 6; i++)
        WorldTex[i - 1] = FindTexture(*FString::Printf(TEXT("%s%d"), *Name, i));
    MapTextures.AddItem(NULL);
    appSprintf(Key, TEXT("%s_TownMap"), GCountryName);
    if (!GConfig->GetString(GCountryName, Key, Name, TEXT("localization.ini")))
        Name = TEXT("L2Font.TownMap");
    for (i = 0; i < 16; i++)
    {
        GTownMapNames[i] = Name + TEXT(".") + GTownMapNames[i];
        MapTextures.AddItem(FindTexture(*GTownMapNames[i]));
    }
}
NW_HOOK_METHOD(0x100ba110, NCMinimapWnd, LoadMapData)

// 0x100bac60 (vslot 67) - a drag moves the map by the mouse's travel (kept inside it), then the hover tests of the two
// marks (the player's: 10 pixels around; the console's: the pin's box)
INT NCMinimapWnd::OnMouseMove(WPARAM wParam, LPARAM lParam)
{
    INT X = (SHORT)LOWORD(lParam) - ScreenX;
    INT Y = (SHORT)HIWORD(lParam) - ScreenY;
    if (bDragging && X > 0 && X <= (INT)Width && Y > 0 && Y <= (INT)Height)
    {
        INT DX = ClickX - X;
        INT DY = ClickY - Y;
        INT W = GetMapWidth(MapIndex);
        INT H = GetMapHeight(MapIndex);
        if (DX > 0)
        {
            ScrollX += DX;
            if (ScrollX > W - 0x148)
                ScrollX = W - 0x148;
        }
        else
        {
            ScrollX -= NAbs(DX);
            if (ScrollX < 0)
                ScrollX = 0;
        }
        if (DY > 0)
        {
            ScrollY += DY;
            if (ScrollY > H - 0x148)
                ScrollY = H - 0x148;
        }
        else
        {
            ScrollY -= NAbs(DY);
            if (ScrollY < 0)
                ScrollY = 0;
        }
        RefreshButtonLoc();
    }
    if (MyX >= 0xC && MyX <= (INT)Width - 0xC && MyY >= 0xC && MyY < (INT)Height - 0xC && MyX - 10 <= X &&
        X <= MyX + 10 && MyY - 10 <= Y && Y <= MyY + 10)
        bMyPosHover = 1;
    else
        bMyPosHover = 0;
    if (bShowMark && MarkPos.Num() > 0)
    {
        FMinimapTarget T = MarkPos(0);
        INT OutX;
        INT OutY;
        WorldToMap(T.X, T.Y, &OutX, &OutY, MapIndex);
        INT PX = OutX - ScrollX;
        INT PY = OutY - ScrollY;
        if (PX >= 5 && PX <= (INT)Width - 5 && PY >= 5 && PY < (INT)Height - 5 && PX <= X && X <= PX + 0x14 &&
            PY - 0x14 <= Y && Y <= PY)
            bMarkHover = 1;
        else
            bMarkHover = 0;
    }
    ClickX = X;
    ClickY = Y;
    return 1;
}
NW_HOOK_METHOD(0x100bac60, NCMinimapWnd, OnMouseMove)

// 0x100baee0 - the cover shows the window: the player's map (the world map or his town's), the player's mark centered
// (the piece kept inside the map)
void NCMinimapWnd::CenterOnPlayer()
{
    INT PX = (INT)MyLocation().X;
    INT PY = (INT)MyLocation().Y;
    INT PZ = (INT)MyLocation().Z;
    MapIndex = GetMapIndex(PX, PY, PZ);
    Unknown1A8 = 0;
    INT W = GetMapWidth(MapIndex);
    INT H = GetMapHeight(MapIndex);
    if (W > 0 && H > 0)
    {
        if (MapIndex == 0)
            ZoomOutButton->HideWindow();
        else
            ZoomOutButton->ShowWindow();
        INT MX;
        INT MY;
        GetMyMapPos(&MX, &MY, MapIndex);
        if (MX < 0xA4)
            ScrollX = 0;
        else if (MX > W - 0xA4)
            ScrollX = W - 0x148;
        else
            ScrollX = MX - 0xA4;
        if (MY < 0xA4)
            ScrollY = 0;
        else if (MY > H - 0xA4)
            ScrollY = H - 0x148;
        else
            ScrollY = MY - 0xA4;
        RearrangeMapDisplay(MapIndex);
    }
}
NW_HOOK_METHOD(0x100baee0, NCMinimapWnd, CenterOnPlayer)

// 0x100bb0b0 - show a place of the world: its map (bFollow 0: the world map's scroll kept first; 1: the world map unless
// it is the one shown), the piece centered on it (the harbor's box has its own mark)
void NCMinimapWnd::ShowPosition(INT X, INT Y, INT Z, UBOOL bFollow)
{
    INT Zone = GetMapIndex(X, Y, Z);
    if (bFollow == 0)
    {
        if (MapIndex == 0)
        {
            SavedScrollX = ScrollX;
            SavedScrollY = ScrollY;
        }
        MapIndex = Zone;
        Unknown1A8 = 0;
    }
    else if (MapIndex != Zone)
    {
        MapIndex = 0;
        Unknown1A8 = 0;
    }
    INT W = GetMapWidth(MapIndex);
    INT H = GetMapHeight(MapIndex);
    if (W > 0 && H > 0)
    {
        if (MapIndex == 0)
            ZoomOutButton->HideWindow();
        else
            ZoomOutButton->ShowWindow();
        INT PY;
        if (X < 0x9E0B || X > 0xC3FC || Y < 0x3AFAD || Y > 0x3D4E7 || Z > -0x130A || Z < -0x1ADA)
        {
            WorldToMapPixels(&X, &Y, MapIndex);
            PY = Y;
            if (X < 0xA4)
                ScrollX = 0;
            else if (X > W - 0xA4)
                ScrollX = W - 0x148;
            else
                ScrollX = X - 0xA4;
        }
        else
        {
            ScrollX = 0;
            PY = 0x536;
        }
        if (PY < 0xA4)
            ScrollY = 0;
        else if (PY > H - 0xA4)
            ScrollY = H - 0x148;
        else
            ScrollY = PY - 0xA4;
        RearrangeMapDisplay(MapIndex);
    }
}
NW_HOOK_METHOD(0x100bb0b0, NCMinimapWnd, ShowPosition)

// 0x100bb250 - a mark of the console (ShowRadar)
void NCMinimapWnd::AddTarget(INT X, INT Y, INT Z)
{
    INT i = Targets.Add();
    Targets(i).X = X;
    Targets(i).Y = Y;
    Targets(i).Z = Z;
}
NW_HOOK_METHOD(0x100bb250, NCMinimapWnd, AddTarget)

// 0x100bb290 - every mark at that position goes (the entry after a removed one is skipped, as in the stock loop)
void NCMinimapWnd::RemoveTarget(INT X, INT Y, INT Z)
{
    for (INT i = 0; i < Targets.Num(); i++)
    {
        FMinimapTarget T = Targets(i);
        if (T.X == X && T.Y == Y && T.Z == Z)
        {
#if NW_HOOK_MODE
            ::nw::StockCallAt<void>(0x101495B0, &Targets, i, 1);   // Targets.Remove(i, 1), the template's out-of-line copy
#else
            Targets.Remove(i, 1);
#endif
        }
    }
}
NW_HOOK_METHOD(0x100bb290, NCMinimapWnd, RemoveTarget)

// 0x100bb2f0 (slot 74) - the zoom out button, on a town's map: back to the world map, the scroll it had, the button
// hidden; the player's position shown
INT NCMinimapWnd::OnZoomOutButton()
{
    if (MapIndex != 0)
    {
        ScrollY = SavedScrollY;
        MapIndex = 0;
        ScrollX = SavedScrollX;
        ZoomOutButton->HideWindow();
        INT PX = (INT)GConsole->GetMyLocation().X;
        INT PY = (INT)GConsole->GetMyLocation().Y;
        INT PZ = (INT)GConsole->GetMyLocation().Z;
        if (Unknown1A8 == 0)
            ShowPosition(PX, PY, PZ, 1);
        RearrangeMapDisplay(MapIndex);
    }
    SetFocus();
    return 1;
}
NW_HOOK_METHOD(0x100bb2f0, NCMinimapWnd, OnZoomOutButton)

// 0x100bbac0 (vslot 37) - the textures, the zoom out button (24 x 24 at (301, 301)), the eight party markers and the tables
INT NCMinimapWnd::OnCreate()
{
    NCWnd::OnCreate();
    MyPosTex = FindTexture(TEXT("L2UI.MinimapWnd.MinimapPos"));
    TargetPosTex = FindTexture(TEXT("L2UI.MinimapWnd.TargetPos"));
    MarkTex = FindTexture(TEXT("L2UI_CH3.Minimap.MapIcon_Mark"));
    BarShadowTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.Bar_Shadow"));
    ShadowTex = FindTexture(TEXT("L2UI_CH3.Minimap.MapShadow"));
    ZoomOutButton = new NCButton(TEXT("L2UI_CH3.Minimap.MapButton_ZoomOut1"),
                                 TEXT("L2UI_CH3.Minimap.MapButton_ZoomOut2"), 0);
    ZoomOutButton->ID = NID_MINIMAP_ZOOMOUT;
    ZoomOutButton->SetPosition(0x12D, 0x12D, 0x18, 0x18);
    CreateChild(ZoomOutButton, NWS_VISIBLE);
    MoonTex = FindTexture(TEXT("L2UI_CH3.Minimap.Map_Moon"));
    SunTex = FindTexture(TEXT("L2UI_CH3.Minimap.Map_Sun"));
    for (INT i = 0; i < 8; i++)
    {
        PartyMarkers[i] = new NCButton(TEXT("L2UI_CH3.Minimap.minimap_party"), TEXT("L2UI_CH3.Minimap.minimap_party"), 2);
        PartyMarkers[i]->SetPosition(0, 0, 0x10, 0x10);
        CreateChild(PartyMarkers[i], NWS_VISIBLE | NWS_UNKNOWN20);
        PartyMarkers[i]->HideWindow();
    }
    LoadMapData();
    return 1;
}
NW_HOOK_METHOD(0x100bbac0, NCMinimapWnd, OnCreate)

// 0x100bd170 (vslot 38) - the buttons go (each leaves its parent's list first), then every array is emptied
INT NCMinimapWnd::OnDestroy()
{
    guard(NCMinimapWnd::OnDestroy);
    INT i;
    if (ZoomOutButton)
    {
        RemoveChildWindow(ZoomOutButton->Entry);
        delete ZoomOutButton;
        ZoomOutButton = NULL;
    }
    for (i = 0; i < TownButtons.Num(); i++)
    {
        if (TownButtons(i))
        {
            RemoveChildWindow(TownButtons(i)->Entry);
            delete TownButtons(i);
            TownButtons(i) = NULL;
        }
    }
    for (i = 0; i < 8; i++)
    {
        if (PartyMarkers[i])
        {
            RemoveChildWindow(PartyMarkers[i]->Entry);
            delete PartyMarkers[i];
            PartyMarkers[i] = NULL;
        }
    }
    TownButtons.Empty();
    TownPos.Empty();
    MapTextures.Empty();
    MarkPos.Empty();
    ToolTips.Empty();
    MapScales.Empty();
    MapOrigins.Empty();
    Targets.Empty();
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100bd170, NCMinimapWnd, OnDestroy)

// 0x100bd3d0 (anonymous in the db) - the console's mark and the marks emptied
void NCMinimapWnd::ClearTargets()
{
    MarkPos.Empty();
#if NW_HOOK_MODE
    ::nw::StockCallAt<void>(0x100BCD30, &ToolTips, 0);   // ToolTips.Empty(0): the template's out-of-line copy
#else
    ToolTips.Empty(0);
#endif
    Targets.Empty();
}
NW_HOOK_METHOD(0x100bd3d0, NCMinimapWnd, ClearTargets)

// 0x100b94f0 (anonymous in the db, name ours) - which part of the world map's piece (Col, Row) the 328 x 328 view at
// (ScrollX, ScrollY) shows: where it lands in the view (DstX, DstY), how big (W, H) and where it starts in the piece (SrcX,
// SrcY). The pieces are 1024 x 1024; a piece that only starts inside the view is drawn at its right / lower end
void NCMinimapWnd::GetWorldPieceRect(INT Col, INT Row, INT ViewX, INT ViewY, INT* DstX, INT* DstY, INT* W, INT* H,
                                     INT* SrcX, INT* SrcY)
{
    *DstX = 0;
    *DstY = 0;
    *W = 0;
    *H = 0;
    INT Base = Col * 0x400;
    *SrcX = 0;
    *SrcY = 0;
    if (ViewX < Base || ViewX >= Base + 0x400)
    {
        if (ViewX >= Base - 0x400 && ViewX < Base)
        {
            INT Part = ViewX - Col * 0x400 + 0x148;
            *W = Part;
            *DstX = 0x148 - Part;
        }
    }
    else
    {
        INT Off = ViewX - Col * 0x400;
        INT Part = 0x400 - Off;
        *SrcX = Off;
        if (Part > 0x147)
            Part = 0x148;
        *W = Part;
    }
    Base = Row * 0x400;
    if (Base <= ViewY && ViewY < Base + 0x400)
    {
        INT Off = ViewY - Row * 0x400;
        INT Part = 0x400 - Off;
        *SrcY = Off;
        if (Part > 0x147)
            Part = 0x148;
        *H = Part;
        return;
    }
    if (Base - 0x400 <= ViewY && ViewY < Base)
    {
        INT Part = ViewY - Row * 0x400 + 0x148;
        *H = Part;
        *DstY = 0x148 - Part;
    }
}
NW_HOOK_METHOD(0x100b94f0, NCMinimapWnd, GetWorldPieceRect)

// 0x100ba0e0 (anonymous in the db, name ours) - the window when it is a NCMapCoverWnd (class at 0x102E8DD0), else NULL
NCWnd* NMinimapAsMapCover(NCWnd* Wnd)
{
    if (Wnd)
    {
#if NW_HOOK_MODE
        UClass* CoverClass = (UClass*)::nw::Addr(0x102E8DD0);
#else
        UClass* CoverClass = NCMapCoverWnd::StaticClass();
#endif
        for (UField* TempClass = reinterpret_cast<UField*>(Wnd->GetClass()); TempClass; TempClass = TempClass->SuperField)
        {
            if (TempClass == reinterpret_cast<UField*>(CoverClass))
                return Wnd;
        }
    }
    return NULL;
}
NW_HOOK_FUNC(0x100ba0e0, NMinimapAsMapCover)

// 0x100ba530 (vslot 63, the db's OnPostPaint) - the tool tip over the player's mark (his name) and over the console's
// mark (its name, under it the tracked quest's), a box from the window's skin kept inside the window
INT NCMinimapWnd::OnDrawToolTip(NCGDevice* Device)
{
    guard(NCMinimapWnd::OnDrawToolTip);
    SIZE Size;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (bMyPosHover)
    {
        NCGDevice::GetTextExtent(&Size, GConsole->GetMyUser()->GetName(), 0, 0, 0, 0, 0);
        INT X = MyX;
        INT Y = MyY + 10;
        if (Size.cx + X + 16 > (INT)Width - 5)
            X = (INT)((DOUBLE)Width - 5.0 - 16.0 - (DOUBLE)Size.cx);
        if (Size.cy + Y + 16 > (INT)Height - 5)
            Y = (INT)((DOUBLE)Height - 5.0 - 16.0 - (DOUBLE)Size.cy);
        DrawToolTipSkin(Device, ToolTipSkin, X, Y, Size.cx, Size.cy);
        Device->DrawText(X + 8, Y + 8, 0xFFDCDCDC, GConsole->GetMyUser()->GetName(), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    if (bMarkHover && MarkPos.Num() != 0)
    {
        FMinimapTarget T = MarkPos(0);
        INT OutX;
        INT OutY;
        WorldToMap(T.X, T.Y, &OutX, &OutY, MapIndex);
        INT PX = OutX - ScrollX;
        INT PY = OutY - ScrollY;
        SIZE Quest;
        Quest.cx = 0;
        Quest.cy = 0;
        {
            // the mark's name by the console's virtual 377 (a FString by value, built by the callee in our buffer)
            BYTE NameBuffer[sizeof(FString)];
            FString& MarkName = *(FString*)NameBuffer;
            ::nw::VirtualCall<FString*>(GConsole, 377, &MarkName);
            NCGDevice::GetTextExtent(&Size, *MarkName, 0, 0, 0, 0, 0);
            MarkName.~FString();
        }
        // the tracked quest of the desktop's first window's list; an uninitialized local in the stock code, which
        // the compiler gave the slot of OutX
        INT QuestID = OutX;
        if (GConsole->Desktop)
        {
            NCWnd* Window = ::nw::Field<NCWnd*>(GConsole->Desktop, 0xE8);
            if (Window)
            {
                NCQuestGaraTreeWnd* List = ::nw::Field<NCQuestGaraTreeWnd*>(Window, 0xF8);
                if (List)
                    QuestID = List->GetTrackedQuestID();   // 0x100EA170
            }
        }
        if (QuestID > 0)
            NCGDevice::GetTextExtent(&Quest, GetQuestName(CurrentQuestID()), 0, 0, 0, 0, 0);
        if (Size.cx > Quest.cx)
            Quest.cx = Size.cx;
        INT BoxW = Quest.cx;
        INT BoxH = Quest.cy + Size.cy + 2;
        PX += -5;
        if (PX < 5)
            PX = 5;
        if (PY - 5 < 5)
            PY = 5;
        else
            PY += 5;
        INT WindowW = (INT)Width;
        if (BoxW + PX + 0x10 > WindowW - 5)
            PX = WindowW - BoxW - 0x15;
        INT WindowH = (INT)Height;
        if (BoxH + PY + 0x10 > WindowH - 5)
            PY = WindowH - BoxH - 0x15;
        DrawToolTipSkin(Device, ToolTipSkin, PX, PY, BoxW, BoxH);
        {
            BYTE NameBuffer[sizeof(FString)];
            FString& MarkName = *(FString*)NameBuffer;
            ::nw::VirtualCall<FString*>(GConsole, 377, &MarkName);
            Device->DrawText(PX + 8, PY + 8, 0xFFDCDCDC, *MarkName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
            MarkName.~FString();
        }
        INT QuestID2 = OutX;
        if (GConsole->Desktop)
        {
            NCWnd* Window = ::nw::Field<NCWnd*>(GConsole->Desktop, 0xE8);
            if (Window)
            {
                NCQuestGaraTreeWnd* List = ::nw::Field<NCQuestGaraTreeWnd*>(Window, 0xF8);
                if (List)
                    QuestID2 = List->GetTrackedQuestID();   // 0x100EA170
            }
        }
        Device->DrawText(PX + 8, PY + Size.cy + 10, 0xFFDCDCDC, GetQuestName(QuestID2), 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100ba530, NCMinimapWnd, OnDrawToolTip)

// the party the server keeps (UL2NetHandler::GetPartyInfoMap fills it: pairs of 0x18 bytes, a hash chain index, the key
// and the member's 16 bytes: id, x, y, z) as the stock holds it on the stack - the engine's TMap<INT, PartyInfo> - and its
// two helpers (0x100BA050 rebuilds the hash, 0x100BB860 empties the pairs)
struct NMinimapPartyMap
{
    BYTE* Pairs;
    INT Num;
    INT Max;
    INT* Hash;
    INT HashCount;
};

// 0x100bbea0 (vslot 62) - the piece of the map (the world map in its six pieces with the grid: the column and row numbers from
// the map's origin, every cell of the scale's width; a town's whole), the party members in view (a marker button each), the
// shadow band, the player's mark and the targets, the place's name, the mode text and the game's clock (the sun from 6
// to 24 o'clock, else the moon)
INT NCMinimapWnd::OnPaint(NCGDevice* Device)
{
    guard(NCMinimapWnd::OnPaint);
    TCHAR Buf[256];
    SIZE Size;
    Size.cx = 0;
    Size.cy = 0;
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Map = MapIndex;
    if (Map != 0 && !MapTextures(Map))
        return 0;
    // (the stock multiplies the scale by a float that is 0.0)
    INT OffX = (INT)(MapScales(Map).X * 0.0f);
    INT OffY = (INT)(MapScales(Map).Y * 0.0f);
    if (Map == 0)
    {
        for (INT Col = 0; Col < 2; Col++)
        {
            for (INT Row = 0; Row < 3; Row++)
            {
                INT DstX;
                INT DstY;
                INT W;
                INT H;
                INT SrcX;
                INT SrcY;
                GetWorldPieceRect(Col, Row, ScrollX, ScrollY, &DstX, &DstY, &W, &H, &SrcX, &SrcY);
                if (W > 0 && H > 0)
                    Device->DrawTexture(DstX, DstY, W, H, SrcX, SrcY, W, H, WorldTex[Col + Row * 2], 0xFF, 1);
            }
        }
    }
    else
        Device->DrawTexture(0, 0, 0x148, 0x148, ScrollX, ScrollY, 0x148, 0x148, MapTextures(Map), 0xFF, 1);
    if (MapIndex == 0)
    {
        INT CellW = (INT)(MapScales(0).X * 12288.0f);
        INT CellH = (INT)(MapScales(0).Y * 12288.0f);
        INT Label = (ScrollX + OffX) / CellW;
        INT X = CellW - (ScrollX + OffX) % CellW;
        INT Row = (ScrollY + OffY) / CellH;
        INT Y = CellH - (ScrollY + OffY) % CellH;
        Label += 20;
        INT RowLabel = Row + 20;
        for (INT k = 0; k <= 0x148 / CellW; k++, X += CellW, Label++)
        {
            if (Label >= 0)
            {
                appSprintf(Buf, TEXT("%d"), Label);
                NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
                if (X > 0xF)
                    Device->DrawText(X - Size.cx / 2, 2, 0xFFDCDCDC, Buf, 0, 0, 0, (L2FontType)1, 0, 0, 0, 0, 0, 0, 0);
            }
            Device->DrawLine(X, Size.cy, X, 0x148, 0x40000000, 1);
        }
        for (INT j = 0; j <= 0x148 / CellH; j++, Y += CellH, RowLabel++)
        {
            if (RowLabel >= 0)
            {
                appSprintf(Buf, TEXT("%d"), RowLabel + 1);
                NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
                if (Y > 0x14)
                    Device->DrawText(0, Y - Size.cy / 2, 0xFFDCDCDC, Buf, 0, 0, 0, (L2FontType)1, 0, 0, 0, 0, 0, 0, 0);
            }
            Device->DrawLine(Size.cx, Y, 0x148, Y, 0x40000000, 1);
        }
    }

    // the party members in view: a marker button each (the ones left over are hidden)
    NMinimapPartyMap Party;
    Party.Pairs = NULL;
    Party.Num = 0;
    Party.Max = 0;
    Party.Hash = NULL;
    Party.HashCount = 8;
    reinterpret_cast<NMapBase24*>(&Party)->Rehash();   // 0x100BA050
    ::nw::VirtualCall<void>(::nw::Field<BYTE*>(GConsole->Engine, 0x64), 52, &Party);
    INT Shown = 0;
    INT Index = 0;
    for (INT n = 0; n < Party.Num; n++)
    {
        INT* Pair = (INT*)(Party.Pairs + n * 0x18 + 8);
        INT MemberID = Pair[0];
        INT MemberX = Pair[1];
        INT MemberY = Pair[2];
        if (MemberX != -1 || MemberY != -1)
        {
            BYTE* Member = NULL;
            for (INT k = 0; k < 8; k++)
            {
                BYTE* Candidate = ::nw::Field<BYTE*>(::nw::Field<BYTE*>(GConsole->Desktop, 0x114), 0x100 + k * 4);
                if (::nw::Field<INT>(Candidate, 0x364) == MemberID)
                {
                    Member = Candidate;
                    break;
                }
            }
            if (Member)
            {
                INT MX;
                INT MY;
                WorldToMap(MemberX, MemberY, &MX, &MY, MapIndex);
                MX -= ScrollX;
                MY -= ScrollY;
                if (Shown <= 8 && MX >= 0xC && MX <= (INT)Width - 0xC && MY >= 0xC && MY < (INT)Height - 0xC)
                {
                    NCButton* Marker = PartyMarkers[Shown];
                    Marker->MoveWindow((MX - Marker->ScreenX) + ScreenX, (ScreenY - Marker->ScreenY) + MY);
                    PartyMarkers[Shown]->ShowWindow();
                    PartyMarkers[Shown]->SetToolTipText((const TCHAR*)(Member + 0x164), 1);
                    NCWnd* Cover = NMinimapAsMapCover(Entry->Parent->Wnd);
                    if (Cover && ::nw::Field<INT>(Cover, 0xF0) == Index)
                    {
                        // the member picked in the cover: his marker draws its tool tip now, outside the clip
                        Device->ReleaseClip();
                        Device->EndPaint();
                        NCButton* Picked = PartyMarkers[Shown];
                        INT WasOver = Picked->bMouseOver;
                        Picked->bMouseOver = 1;
                        PartyMarkers[Shown]->WindowProc(NWM_DRAWTOOLTIP, (WPARAM)Device, 0);
                        PartyMarkers[Shown]->bMouseOver = WasOver;
                        Device->BeginPaint(ScreenX, ScreenY);
                        Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
                    }
                    Shown++;
                }
            }
        }
        Index++;
    }
    for (INT m = Shown; m < 8; m++)
        PartyMarkers[m]->HideWindow();

    // the player
    GetMyMapPos(&MyX, &MyY, MapIndex);
    MyX -= ScrollX;
    MyY -= ScrollY;
    if (MyPosTex && MyX >= 0xC && MyX <= (INT)Width - 0xC && MyY >= 0xC && MyY < (INT)Height - 0xC)
        Device->DrawMinimapLoc(MyX, MyY, (UMaterial*)MyPosTex);

    // the targets the console marked
    for (INT i = 0; i < Targets.Num(); i++)
    {
        FMinimapTarget T = Targets(i);
        INT TX;
        INT TY;
        WorldToMap(T.X, T.Y, &TX, &TY, MapIndex);
        TX -= ScrollX;
        TY -= ScrollY;
        if (TargetPosTex && TX >= 0xC && TX <= (INT)Width - 0xC && TY >= 0xC && TY < (INT)Height - 0xC)
            Device->DrawTexture(TX, TY, 0x20, 0x20, 0, 0, 0x20, 0x20, TargetPosTex, 0xFF, 1);
    }

    // the console's mark (its virtual 368 gives a name and a position): asked for once, drawn as a pin
    if (bShowMark)
    {
        if (MarkPos.Num() == 0)
        {
            FString MarkName;
            FVector MarkLoc;
            ::nw::VirtualCall<void>(GConsole, 368, &MarkName, &MarkLoc);
            MarkPos.Empty();
            INT MX = (INT)MarkLoc.X;
            INT MY = (INT)MarkLoc.Y;
            INT MZ = (INT)MarkLoc.Z;
            INT At = MarkPos.Add();
            MarkPos(At).X = MX;
            MarkPos(At).Y = MY;
            MarkPos(At).Z = MZ;
        }
        for (INT i = 0; i < MarkPos.Num(); i++)
        {
            FMinimapTarget T = MarkPos(i);
            INT MX;
            INT MY;
            WorldToMap(T.X, T.Y, &MX, &MY, MapIndex);
            MX -= ScrollX;
            MY -= ScrollY;
            Device->DrawTexture(MX, MY - 0x14, 0x20, 0x20, 0, 0, 0x20, 0x20, MarkTex, 0xFF, 1);
        }
    }
    else
        MarkPos.Empty();

    // the band at the bottom with the place and the mode
    Device->DrawTexture(0, 0x136, 0x149, 0x17, 0, 0, 0x149, 0x17, ShadowTex, 0xFF, 1);
    appSprintf(Buf, TEXT("%s : %s"), *GL2GameData.SysString[471], GConsole->GetCurrentZoneName());
    Device->DrawText(0x16, (INT)(Height - 15.0f), 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    switch (Mode)
    {
    case 0:
        appSprintf(Buf, TEXT("%s : %s"), *GL2GameData.SysString[833], *GL2GameData.SysString[973]);
        Device->DrawText(0x16, (INT)(Height - 31.0f), 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        break;
    case 1:
        appSprintf(Buf, TEXT("%s : %s"), *GL2GameData.SysString[833], *GL2GameData.SysString[974]);
        Device->DrawText(0x16, (INT)(Height - 31.0f), 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        break;
    case 2:
        appSprintf(Buf, TEXT("%s : %s"), *GL2GameData.SysString[833], *GL2GameData.SysString[975]);
        Device->DrawText(0x16, (INT)(Height - 31.0f), 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        break;
    case 3:
        appSprintf(Buf, TEXT("%s : %s"), *GL2GameData.SysString[833], *GL2GameData.SysString[976]);
        Device->DrawText(0x16, (INT)(Height - 31.0f), 0xFFDCDCDC, Buf, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        break;
    }

    // the game's clock: hours (and the fraction of the hour as minutes) of the day, the sun or the moon, "AM" / "PM"
    TCHAR Suffix[3] = TEXT("AM");
    DOUBLE GameTime = ::nw::Field<DOUBLE>(::nw::Field<BYTE*>(GConsole->Engine, 0x70), 0x134);
    INT Hour = (INT)GameTime;
    INT Minute = (INT)((GameTime - (DOUBLE)Hour) * 60.0);
    if (Minute >= 60)
    {
        Minute -= 60;
        Hour++;
    }
    if (Hour >= 6 && Hour <= 24)
        Device->DrawTexture(4, 6, 0x19, 0x19, 0, 0, 0x19, 0x19, SunTex, 0xFF, 1);
    else
        Device->DrawTexture(4, 6, 0x19, 0x19, 0, 0, 0x19, 0x19, MoonTex, 0xFF, 1);
    if (Hour >= 12)
    {
        appStrcpy(Suffix, TEXT("PM"));
        Hour -= 12;
    }
    if (Hour == 0 || Hour >= 10)
    {
        if (Hour == 0)
            Hour = 12;
        if (Minute < 10)
            appSprintf(Buf, TEXT("%s %d : %d%d"), Suffix, Hour, 0, Minute);
        else
            appSprintf(Buf, TEXT("%s %d : %d"), Suffix, Hour, Minute);
    }
    else
    {
        if (Minute < 10)
            appSprintf(Buf, TEXT("%s %d%d : %d%d"), Suffix, 0, Hour, 0, Minute);
        else
            appSprintf(Buf, TEXT("%s %d%d : %d"), Suffix, 0, Hour, Minute);
    }
    Device->DrawText(0x1D, 0x13, 0xFFDCDCDC, Buf, 0, 0, 0, (L2FontType)1, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    if (Party.Hash)
        appFree(Party.Hash);
    Party.Hash = NULL;
    Party.HashCount = 0;
    DestroyPartyPairs(&Party);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100bbea0, NCMinimapWnd, OnPaint)

// 0x100ef930 - the radar's mark (the stock body at this address serves the radar only: the console's ShowRadar calls
// it on the radar window and 0x100BB250 on the map; the db gave it the map's name)
NW_HOOK_METHOD(0x100ef930, NCRadarWnd, AddTarget)

// 0x100347a0 is the shared KillFocus (NCWnd's): the hook build keeps the stub that jumps to it, the standalone does not override
// the slot (the header declares the override in the hook build only), so it points at NCWnd::KillFocus as the stock vtable does
#if NW_HOOK_MODE
NW_NAKED void NCMinimapWnd::KillFocus() NW_STOCK(0x100347a0)
#endif

// the destructor instances of the minimap's arrays (the stock compiler emitted each out of line; 0x100bb670 is the
// radar window's too)
typedef TArray<FMinimapTarget> TMinimapTargetArray;
typedef TArray<FMinimapScale> TMinimapScaleArray;
typedef TArray<NCButton*> TMinimapButtonArray;
typedef TArray<NToolTipInfo> TMinimapToolTipArray;
// 0x100bb670 - 12 byte elements (the town, mark and target positions)
NW_HOOK_DTOR(0x100bb670, TMinimapTargetArray)
// 0x100bb700 - 4 byte elements (the textures, the buttons)
NW_HOOK_DTOR(0x100bb700, TMinimapButtonArray)
// 0x100bb790 - 8 byte elements (the scales)
NW_HOOK_DTOR(0x100bb790, TMinimapScaleArray)
// 0x100bbce0 (ret 8) - Remove(Index, Count) of the tool tips: each element's destructor, then the engine's Remove
NW_HOOK_METHOD(0x100bbce0, TMinimapToolTipArray, Remove)
// 0x100bcce0 - the tool tips' array: all of them removed
NW_HOOK_DTOR(0x100bcce0, TMinimapToolTipArray)

// 0x100bd0a0 - the arrays go with the members
NW_HOOK_DTOR(0x100bd0a0, NCMinimapWnd)

NW_CHECK_VTABLE(NCMinimapWnd, 0x101bdeb0, 76)
