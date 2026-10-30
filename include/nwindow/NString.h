#pragma once
// NString - NWindow's growable text buffer (name ours; 0x14 bytes, plain C++, allocated with
// the global new). List boxes keep one per line (Color is the line color); the combo box and
// the character lists use it too. The buffer grows by at least 256 characters.

#include "nwindow/NWindow.h"

class NString
{
public:
    UINT Capacity;             // +0x00 characters allocated
    TCHAR* Data;               // +0x04
    UINT Length;               // +0x08
    DWORD Color;               // +0x0C used by the list boxes
    INT Indent;                // +0x10 X offset of the line (chat continuation lines: 12)

    NString();                                           // 0x10021cf0
    NString(const TCHAR* Text);                          // 0x1002a120 (ret 4)
    ~NString();                                          // 0x10021ef0 (jmp Empty)

    __declspec(noinline) UINT GetLength() const { return Length; }   // 0x1012c6a0 (a folded getter, called out of line by Append)
    TCHAR* GetAt(UINT Index);                            // 0x10021d10 NULL past the end
    __declspec(noinline) TCHAR* AllocBuffer(INT Count);  // 0x10029f80 (out of line in the stock code)
    __declspec(noinline) INT Insert(const TCHAR* Text, UINT Pos);   // 0x10029fb0 characters inserted (Append calls it)
    INT Append(const TCHAR* Text);                       // 0x10021e80
    void Empty();                                        // 0x10021ea0
};
NW_ASSERT_SIZE(NString, 0x14);
