#pragma once
// The fields of an NItemInfo at their C4 offsets (when this file was written, wave A, the header's NItemInfo had the C3 layout
// with the C4 growth appended at its end, db/struct_sizes.csv; stage 4 (rest1) gave it the layout below, see NItemInfo.h and the
// asserts of NItemC4.h; these accessors are raw views with literal offsets).
//
// Evidence: NCActionItemListWnd::LoadActions (0x10050100) builds one on its stack and fills it with the constructor
// 0x1004CD30 and the stores below. The C4 string part is 0x200 bytes longer than C3's (two strings of 128 characters after
// the three icon names, as the item entry has them at +0x714 and +0x814) and two more dwords follow the three icons:
//   +0x000 Name[256], +0x200 AdditionalName[256], +0x400 IconName[128], +0x500 and +0x600 (the second and third icons'
//   names), +0x700 and +0x800 (new strings of 128 characters), +0x900 Description[2048],
//   +0x1900 Icon, +0x1904 and +0x1908 (the second and third icons), +0x190C and +0x1910 (new dwords),
//   +0x1914 Param, +0x1918 (C3 +0x1710), +0x191C ObjectID, +0x1920 ID, +0x1924 Count ... the size 0x19B4.

#include "nwindow/NItemInfo.h"

inline TCHAR* InfoName(NItemInfo* I) { return (TCHAR*)I; }
inline TCHAR* InfoIconName(NItemInfo* I) { return (TCHAR*)((BYTE*)I + 0x400); }
inline TCHAR* InfoDescription(NItemInfo* I) { return (TCHAR*)((BYTE*)I + 0x900); }
inline UTexture*& InfoIcon(NItemInfo* I) { return *(UTexture**)((BYTE*)I + 0x1900); }
inline INT& InfoParam(NItemInfo* I) { return *(INT*)((BYTE*)I + 0x1914); }
inline INT& InfoID(NItemInfo* I) { return *(INT*)((BYTE*)I + 0x1920); }
inline INT& InfoWorn(NItemInfo* I) { return *(INT*)((BYTE*)I + 0x19A0); }   // C3 +0x1798: worn, goes to the paperdoll
// the five icon names (+0x400, +0x500, +0x600, +0x700, +0x800: 128 characters each) and the five textures they are looked
// up into (+0x1900, +0x1904, +0x1908, +0x190C, +0x1910)
inline TCHAR* InfoIconNameN(NItemInfo* I, INT N) { return (TCHAR*)((BYTE*)I + 0x400 + 0x100 * N); }
inline UTexture*& InfoIconN(NItemInfo* I, INT N) { return *(UTexture**)((BYTE*)I + 0x1900 + 4 * N); }
