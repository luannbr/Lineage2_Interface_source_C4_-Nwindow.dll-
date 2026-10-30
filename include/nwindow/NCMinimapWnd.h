#pragma once
// NCMinimapWnd - the map window (UClass "CMinimapWnd"). C4 rebuilt it again: seventeen maps (0 the world map, in six
// 1024 x 1024 pieces; 1..16 the towns', whose buttons sit over it: ids 0xB01000 + n, the map of a town is n + 1; the
// textures, the sizes and the scales are tables of the image), a 328 x 328 piece of one of them under a numbered grid
// (the world map only), the player's mark, the marks the console sets (AddTarget), eight markers for the party members
// and a button that goes back to the world map from a town. The two continents of C3 and their combo box are gone.
//
// Everything below is read from the C4 code. The window is a child of NCMapCoverWnd (the desktop's +0x14C).

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;

// A position the console marks on the map and the radar (name ours; world coordinates). The map also keeps its fixed
// button positions in this shape (X, Y in map pixels; Z is never written: the stock stores a stack slot there).
struct FMinimapTarget
{
    INT X;
    INT Y;
    INT Z;
};
NW_ASSERT_SIZE(FMinimapTarget, 0xC);

// the scale of a map: world units to map pixels (name ours; the tables at 0x1025CBC0 / 0x1025CC08)
struct FMinimapScale
{
    FLOAT X;
    FLOAT Y;
};
NW_ASSERT_SIZE(FMinimapScale, 0x8);

// the maps (names ours)
enum
{
    MINIMAP_World = 0,      // the world map (six pieces)
    MINIMAP_FirstTown = 1,  // 1..16 the towns (button ids 0xB01000 .. 0xB0100F)
    MINIMAP_NumMaps = 17,
    MINIMAP_NumTowns = 16,
};

// NWM_COMMAND ids of the window's buttons (names ours)
enum
{
    NID_MINIMAP_ZOOMOUT = 0xB000F6,    // from a town back to the world map
    NID_MINIMAP_TOWN = 0xB01000,       // + the town's number: the buttons over the world map (16)
};

class NCMinimapWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCMinimapWnd, NCWnd, 0, 0x102E98F0)

    TArray<UTexture*> MapTextures;        // +0xE8 the maps' textures by map number (17: the world map's is NULL, its pieces are
                                          //   WorldTex; then the towns', the strings at 0x102E9300)
    UTexture* WorldTex[6];                // +0xF4 the world map's pieces ("<localization>.ch4_worldmap1" .. "6"), two columns by
                                          //   three rows of 1024 x 1024
    TArray<NCButton*> TownButtons;        // +0x10C the 26 x 26 buttons over the world map (ids 0xB01000 + n)
    TArray<FMinimapTarget> TownPos;       // +0x118 where they sit on the map (0x1025CB40 / 0x1025CB80)
    TArray<FMinimapTarget> MarkPos;       // +0x124 the mark the console reports (filled by OnPaint while bShowMark)
    TArray<NToolTipInfo> ToolTips;        // +0x130 only destroyed / emptied (0x80 bytes each)
    TArray<FMinimapScale> MapScales;      // +0x13C world to map pixels, by map (0x1025CBC0 / 0x1025CC08)
    TArray<FMinimapTarget> MapOrigins;    // +0x148 the world position of each map's corner (0x1025CC50 / 0x1025CC98)
    TArray<FMinimapTarget> Targets;       // +0x154 the marks the console sets (ShowRadar: AddTarget / RemoveTarget)
    BYTE _Unknown160[0x164 - 0x160];      // +0x160 never touched
    UTexture* MyPosTex;                   // +0x164 "L2UI.MinimapWnd.MinimapPos" (the player)
    UTexture* TargetPosTex;               // +0x168 "L2UI.MinimapWnd.TargetPos" (a target)
    UTexture* MarkTex;                    // +0x16C "L2UI_CH3.Minimap.MapIcon_Mark"
    UTexture* ShadowTex;                  // +0x170 "L2UI_CH3.Minimap.MapShadow" (the band under the map)
    UTexture* BarShadowTex;               // +0x174 "L2UI_CH3.FrameCtrl.Bar_Shadow" (only loaded)
    UTexture* MoonTex;                    // +0x178 "L2UI_CH3.Minimap.Map_Moon"
    UTexture* SunTex;                     // +0x17C "L2UI_CH3.Minimap.Map_Sun"
    BYTE _Unknown180[0x184 - 0x180];      // +0x180 never touched
    NCButton* ZoomOutButton;              // +0x184 id 0xB000F6, shown on a town's map only
    NCButton* PartyMarkers[8];            // +0x188 (new in C4) "L2UI_CH3.Minimap.minimap_party": one for each party member in view
    INT Unknown1A8;                       // +0x1A8 always 0 (C3: the continent shown; the stock still clears it when a map is chosen)
    INT Mode;                             // +0x1AC what opened the map (NCConsole::ToggleMinimap's argument, 0..3: the
                                          //   text under the map is SysString[973 + Mode])
    UBOOL bDragging;                      // +0x1B0 the left button is down on the map
    UBOOL bShowMark;                      // +0x1B4 (name ours) nothing in the image sets it: the mark of MarkPos is
                                          //   never shown
    UBOOL bMyPosHover;                    // +0x1B8 the mouse is over the player's mark (OnMouseMove)
    UBOOL bMarkHover;                     // +0x1BC the mouse is over the mark of MarkPos
    INT MapIndex;                         // +0x1C0 the map shown (0..16)
    INT MyX;                              // +0x1C4 the player on the map, relative to its piece (each paint)
    INT MyY;                              // +0x1C8
    INT ScrollX;                          // +0x1CC the map pixel at the piece's corner
    INT ScrollY;                          // +0x1D0
    INT SavedScrollX;                     // +0x1D4 the world map's scroll, kept while a town is shown
    INT SavedScrollY;                     // +0x1D8
    INT ClickX;                           // +0x1DC the last mouse position in the window (a drag moves the map by the
    INT ClickY;                           // +0x1E0   difference)
    BYTE _Unknown1E4[0x1EC - 0x1E4];      // +0x1E4 never touched

    NCMinimapWnd();                                                  // 0x100bcf70
    // ~NCMinimapWnd                                                 // 0x100bd0a0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100b8980
#if NW_HOOK_MODE
    virtual void KillFocus();                                        // 30 0x100347a0 (NCWnd's own: the stock vtable points straight at it)
#endif
    virtual INT OnCreate();                                          // 37 0x100bbac0
    virtual INT OnDestroy();                                         // 38 0x100bd170
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100bbea0
    virtual INT OnDrawToolTip(NCGDevice* Device);                    // 63 0x100ba530 (the db names it OnPostPaint)
    virtual INT OnMouseMove(WPARAM wParam, LPARAM lParam);           // 67 0x100bac60
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100b8bd0
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x100b8c50

    // the maps' tables (names ours); the stock calls them with its own `this`, which they never read
    __declspec(noinline) INT GetMapWidth(INT Map);                                        // 0x100b8c80 0 out of 0..16
    __declspec(noinline) INT GetMapHeight(INT Map);                                       // 0x100b8ca0
    // the town buttons are shown on the world map only (and put where the scroll says), hidden on a town's
    __declspec(noinline) void RearrangeMapDisplay(INT Map);                               // 0x100b92f0 (the db's name, from its guard)
    // a world position to map pixels of Map (the first map adds its origin, the others subtract theirs)
    __declspec(noinline) void WorldToMapPixels(INT* X, INT* Y, INT Map);                  // 0x100b9430
    __declspec(noinline) void RefreshButtonLoc();                                         // 0x100b9600 (the db's name) the buttons after a scroll
    __declspec(noinline) INT GetMapIndex(INT X, INT Y, INT Z);                            // 0x100b96d0 the map of the zone a position is in
    __declspec(noinline) void GetMyMapPos(INT* OutX, INT* OutY, INT Map);                 // 0x100b99d0 the player on the map
    __declspec(noinline) void WorldToMap(INT WX, INT WY, INT* OutX, INT* OutY, INT Map);  // 0x100b9b30 a world position on the map
    // which part of a world map piece the view shows (anonymous in the db, name ours): where it lands, its size, where it starts
    __declspec(noinline) void GetWorldPieceRect(INT Col, INT Row, INT ViewX, INT ViewY, INT* DstX, INT* DstY, INT* W, INT* H,
                                                INT* SrcX, INT* SrcY);          // 0x100b94f0
    __declspec(noinline) void LoadMapData();                                              // 0x100ba110 OnCreate's tables and buttons
    __declspec(noinline) void CenterOnPlayer();                                           // 0x100baee0 the cover calls it when it is shown
    __declspec(noinline) void ShowPosition(INT X, INT Y, INT Z, UBOOL bFollow);           // 0x100bb0b0 the console's "show this place"
    __declspec(noinline) void AddTarget(INT X, INT Y, INT Z);                             // 0x100bb250
    __declspec(noinline) void RemoveTarget(INT X, INT Y, INT Z);                          // 0x100bb290
    __declspec(noinline) void ClearTargets();                                             // 0x100bd3d0 (anonymous in the db) the marks and the console's mark emptied
    // the handlers of the two kinds of button (WindowProc calls them through the slots)
    __declspec(noinline) INT OnZoomOutButton();                                           // 0x100bb2f0 slot 74
    __declspec(noinline) INT OnTownButton(UINT ID);                                       // 0x100b9b90 slot 75
    virtual void NewVirtual74();  // 74 0x100bb2f0 (only this client has it; defined in NNewVirtuals.gen.cpp)
    virtual void NewVirtual75();  // 75 0x100b9b90 (only this client has it; defined in NNewVirtuals.gen.cpp)
};
NW_ASSERT_OFFSET(NCMinimapWnd, MapTextures, 0xE8);
NW_ASSERT_OFFSET(NCMinimapWnd, WorldTex, 0xF4);
NW_ASSERT_OFFSET(NCMinimapWnd, TownButtons, 0x10C);
NW_ASSERT_OFFSET(NCMinimapWnd, TownPos, 0x118);
NW_ASSERT_OFFSET(NCMinimapWnd, MarkPos, 0x124);
NW_ASSERT_OFFSET(NCMinimapWnd, ToolTips, 0x130);
NW_ASSERT_OFFSET(NCMinimapWnd, MapScales, 0x13C);
NW_ASSERT_OFFSET(NCMinimapWnd, MapOrigins, 0x148);
NW_ASSERT_OFFSET(NCMinimapWnd, Targets, 0x154);
NW_ASSERT_OFFSET(NCMinimapWnd, MyPosTex, 0x164);
NW_ASSERT_OFFSET(NCMinimapWnd, SunTex, 0x17C);
NW_ASSERT_OFFSET(NCMinimapWnd, ZoomOutButton, 0x184);
NW_ASSERT_OFFSET(NCMinimapWnd, PartyMarkers, 0x188);
NW_ASSERT_OFFSET(NCMinimapWnd, Unknown1A8, 0x1A8);
NW_ASSERT_OFFSET(NCMinimapWnd, Mode, 0x1AC);
NW_ASSERT_OFFSET(NCMinimapWnd, bDragging, 0x1B0);
NW_ASSERT_OFFSET(NCMinimapWnd, MapIndex, 0x1C0);
NW_ASSERT_OFFSET(NCMinimapWnd, ScrollX, 0x1CC);
NW_ASSERT_OFFSET(NCMinimapWnd, ClickX, 0x1DC);
NW_ASSERT_SIZE(NCMinimapWnd, 0x1EC);

#pragma pack(pop)
