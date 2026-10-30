#pragma once
// NDropDialogInfo - an NDialogInfo with the vector the questions of the item windows' drops fill (name ours).
// C2: NDialogInfo is really 0x84 bytes: the three floats at +0x78 are not set by its constructor (0x10006E60) and
// NCDialogBox::SetInfo (0x10007A00) copies them to the box's +0x22C (the ground point the answer drops at); the
// callers (NCEquipItemWnd / NCInvenItemWnd::OnLButtonUp, ...) write them after SetParams. NDialogInfo.h still says
// 0x78 (it is not ours to change).

#include "nwindow/NDialogInfo.h"

struct NDropDialogInfo : public NDialogInfo
{
    // Location: NDialogInfo has it in C3 (+0x7C)
};
NW_ASSERT_SIZE(NDropDialogInfo, 0xA0);
