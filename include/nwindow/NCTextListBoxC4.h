#pragma once
// NCTextListBox - the text list the C4 chat windows of the status lane use (the room's chat of the party matching, the
// petition conversation): UClass at 0x102C2518, constructor 0x10021AC0 (NCTextListBox.h). It is a list box in every use here, so
// the windows keep it as an NCListBox, whose slots it shares (AddString is slot 76, Clear slot 77, in both).
//
// The constructor takes the lines kept, the lines shown, a third argument the list box's constructor ignores (16 or 15)
// and a flag (1). Built as the stock code builds it: allocated by StaticAllocateObject, then constructed in place (the OnCreate
// bodies inline both: the class's `new` does the same).

#include "nwindow/NCListBox.h"
#include "nwindow/NCTextListBox.h"

FORCEINLINE NCListBox* NewTextListBox(INT MaxLines, INT VisibleLines, INT Unused, INT bFlag)
{
    return new NCTextListBox(MaxLines, VisibleLines, Unused, bFlag);   // 0x10021AC0
}
