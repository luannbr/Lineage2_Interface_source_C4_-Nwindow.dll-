#pragma once
// The fields of an item entry at their C4 offsets, for the item entry's subclasses of the status lane
// (NAbnormalEntry, NActionItem, NHennaEntry).
//
// NItemEntry (NCItemWnd.h) had the C3 layout with the C4 growth appended at its end when this file was written (wave A,
// db/struct_sizes.csv); stage 4 (rest1) gave it the layout below too, and NItemC4.h asserts that every member it shares with
// NItemEntryC4 is at the same offset. The constructor 0x1001F7F0 and the readers in the C4 NWindow show the C4 layout (every
// offset from DisplayName on is the C3 one + 0x208):
//   +0x000 Icon, +0x004, +0x008 (the second and third icons), +0x00C and +0x010 (two new dwords, cleared by the constructor)
//   +0x014 Name[256], +0x214 AdditionalName[256], +0x414 IconName[128], +0x514 and +0x614 (the textures of the second and
//   third icons), +0x714 and +0x814 (two new strings of 128 characters), +0x914 Description[2048], +0x1914 DisplayName[256]
//   +0x1B14 Type, +0x1B18 (the kind of an etc item), +0x1B1C ObjectID, +0x1B20 ID, +0x1B24 Level (a skill's: -1), +0x1B28 Count,
//   +0x1B40 the number the tool tip shows (C3 UnknownC34: the wanted amount, a buff's remaining seconds; -1 = none)
//   +0x1BC0 ToolTip (NToolTipInfo, 0x80 bytes), the size of the entry 0x1C40.
// These accessors are raw views (the offsets are literals): NItemC4.h's asserts, not these functions, tie them to the struct.

#include "nwindow/NCItemWnd.h"

// NToolTipSkin::DrawFull (0x10032560, ret 0x98; NToolTip.cpp) takes the tool tip BY VALUE and destroys its parameter itself (its
// end calls ~NToolTipInfo, 0x10002E40, as every callee of this compiler does), so the copy has to be the copy constructor's
// (0x10002EB0: the strings and the line array are copied deep); a bit copy would have the callee free the original's buffers.
// DrawEntryToolTip below is the typed call (the compiler makes the copy); NToolTipBits is only the 0x80 bytes with the same copy
// constructor, for a caller that goes through a DrawFullFn pointer; it has no destructor (the callee's runs)
struct NToolTipBits
{
    BYTE Data[0x80];
    NToolTipBits() {}
    NToolTipBits(const NToolTipBits& Src)
    {
        new ((void*)this) NToolTipInfo(*(const NToolTipInfo*)&Src);   // 0x10002EB0
    }
};
typedef void(__fastcall * DrawFullFn)(void* This, void* Edx, NCGDevice* Device, NToolTipBits Info, INT X, INT Y,
                                      NToolTipRect Wnd);

inline UTexture*& EntryIcon(NItemEntry* E) { return *(UTexture**)((BYTE*)E + 0x0); }
inline TCHAR* EntryName(NItemEntry* E) { return (TCHAR*)((BYTE*)E + 0x14); }
inline TCHAR* EntryIconName(NItemEntry* E) { return (TCHAR*)((BYTE*)E + 0x414); }
inline INT& EntryObjectID(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B1C); }
inline INT& EntryCount(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B28); }
inline INT& EntryPrice(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B2C); }   // C3's UnknownC20: a shop price
inline TCHAR* EntryAdditionalName(NItemEntry* E) { return (TCHAR*)((BYTE*)E + 0x214); }
inline TCHAR* EntryDescription(NItemEntry* E) { return (TCHAR*)((BYTE*)E + 0x914); }
inline INT& EntryID(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B20); }
inline INT& EntryLevel(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B24); }
inline INT& EntryNumber(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B40); }
inline NToolTipInfo* EntryToolTip(NItemEntry* E) { return (NToolTipInfo*)((BYTE*)E + 0x1BC0); }
// the entry's tool tip drawn by the skin at (X, Y) of the window Wnd
FORCEINLINE void DrawEntryToolTip(void* Skin, NCGDevice* Device, NItemEntry* E, INT X, INT Y, NToolTipRect Wnd)
{
    ((NToolTipSkin*)Skin)->DrawFull(Device, *EntryToolTip(E), X, Y, Wnd);   // 0x10032560, the tip copied by value
}
inline INT& EntryBodyPart(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B30); }   // the slot mask (C3 +0x1928)
inline INT& EntryFlag1BA0(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1BA0); }   // C3 Unknown1C98: the paperdoll's bow rule
// the five icons of an entry (+0x00 .. +0x10: the first is the item's own; the paperdoll's two- and four-slot items draw
// the others) and the names of icons 2 .. 5 (+0x514, +0x614, +0x714, +0x814: 128 characters each)
inline UTexture*& EntryIconN(NItemEntry* E, INT N) { return *(UTexture**)((BYTE*)E + 4 * N); }
inline TCHAR* EntryIconNameN(NItemEntry* E, INT N) { return (TCHAR*)((BYTE*)E + 0x514 + 0x100 * (N - 1)); }
inline INT& EntryInt1B3C(NItemEntry* E) { return *(INT*)((BYTE*)E + 0x1B3C); }     // C3 UnknownC30: 6 = the bow
