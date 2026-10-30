#pragma once
// NCHennaListWnd - the tattoo lists of the symbol maker (UClass "CHennaListWnd", 0xAC): the tattoos that can be drawn
// (Mode 0, the title "Select the symbol to add") or taken off (Mode 1), in an NCHennaListBox, and the player's adena
// at the bottom. A click on a row asks the server for its information (RequestHennaItemInfo / RequestHennaUnequipInfo):
// the answer fills NCHennaInfoWnd. The console fills it (NConsoleWnd's slots 135..144: it keeps this window at
// NCConsole +0x14C and the information window at +0x150).

#include "nwindow/NCHennaListBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// NWM_COMMAND ids (names ours)
enum
{
    NID_HENNALIST_FRAME = 0xF01,
    NID_HENNALIST_LIST = 0x1A06,
};

class NCHennaListWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCHennaListWnd, NCWnd, 0, 0x102D9E98)

    NCHennaListBox* ListBox;         // +0xE8 100 rows kept, 5 seen
    UTexture* BackTex;               // +0xEC "L2UI.HennaWnd.HennaListBack"
    UTexture* AdenaTex;              // +0xF0 "L2UI.WindowIcon.WindowIconAdena"   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    INT Mode;                        // +0xF4 0: to add, 1: to take off (set by the console; not by the constructor)
    INT Adena;                       // +0xF8 the player's adena, shown at the bottom (the console sets it)

    NCHennaListWnd();                                                // 0x10093800
    // ~NCHennaListWnd                                               // 0x10079900 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100917a0
    virtual INT OnCreate();                                          // 37 0x100938b0
    virtual INT OnDestroy();                                         // 38 0x10091fd0
    virtual void ShowWindow();                                       // 48 0x10091820
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10092080 (the db names it NCRecipeListWnd's: copied code)
    // names ours
    virtual INT OnSelect(INT Index);                                 // 74 0x10092230 a row clicked

    // names ours (the console calls them)
    void Clear();                                                    // 0x10091800 the rows and the adena
    void AddEntry(const NHennaListEntry* Src);                       // 0x10093410 (a tail jump into the list box's)
};
NW_ASSERT_OFFSET(NCHennaListWnd, BackTex, 0xEC);
NW_ASSERT_OFFSET(NCHennaListWnd, Mode, 0xF4);
NW_ASSERT_OFFSET(NCHennaListWnd, Adena, 0xF8);
NW_ASSERT_SIZE(NCHennaListWnd, 0xFC);

#pragma pack(pop)
