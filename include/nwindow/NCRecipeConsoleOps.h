#pragma once
// NRecipeConsoleOps - two NConsoleWnd methods only the recipe windows call (names ours; thiscall on the console, the way the
// stock callers reach them: `mov ecx, GConsole`). NConsoleWnd.h is not this group's, so they sit on a struct derived from it
// (a view: the hook needs a class, `this` is the console):
//   0x100570d0 (ret 8)  the item's name with its additional name, "Name-Additional" (just the name when the additional name is
//                       missing or the table's "NAME_None"): the recipe information window's product name
//   NConsoleWnd::MakeItemInfo (the guard's name; 0x1007D760 here): an item entry's name, additional name, icon name and
//                       description from the item table by the entry's id (the recipe windows' ingredients): the console's own
// If the console's group ports FormatItemName under NConsoleWnd, drop the hook here and call theirs.

#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCItemWnd.h"
#pragma pack(push, 4)

struct NRecipeConsoleOps : public NConsoleWnd
{
    void FormatItemName(TCHAR* Out, INT ItemID);       // 0x100570d0
};

// the desktop console's, as the stock callers do
inline void NFormatItemName(TCHAR* Out, INT ItemID)
{
    static_cast<NRecipeConsoleOps*>(GConsole)->FormatItemName(Out, ItemID);
}
inline void NMakeItemInfo(NItemEntry* Entry)
{
    GConsole->MakeItemInfo(Entry);   // NConsoleWnd::MakeItemInfo, 0x1007D760
}

#pragma pack(pop)
