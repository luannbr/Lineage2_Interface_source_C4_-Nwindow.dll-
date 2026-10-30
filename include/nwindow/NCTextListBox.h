#pragma once
// NCTextListBox - the text list of the C4 chat windows (UClass "CTextListBox", 0x244, super NCListBox): lines wrapped to the
// width (continuation lines indented 12 px), 17 px apart, with scroll controls on the right unless the owner draws its own
// (bScrollControls). The chat windows' lists (NCChatListBox and its system message variant), the command channel's chat and the
// petition's conversation derive from or use it. NCTextListBoxC4.h has the helper that builds one the way the stock code does.
//
// Layout and virtuals from the stock constructors 0x10021A20 / 0x10021AC0, OnCreate 0x100213C0, ShowWindow 0x100208B0, OnPaint
// 0x10020BD0, AddString 0x10021510 (the db says NCChatListBox's) and OnDestroy 0x10021490.

#include "nwindow/NCListBox.h"
#pragma pack(push, 4)

class NCTextListBox : public NCListBox
{
    NW_DECLARE_CLASS(NCTextListBox, NCListBox, 0, 0x102C2518)

    BYTE bScrollControls;      // +0x240 the list keeps the scroll buttons and the bar at its right edge (1; the 4-argument constructor's last
                               //   argument); when 0 ShowWindow hides them and the owner moves the list by other means (name ours)

    NCTextListBox();                                                 // 0x10021A20 (scroll controls on)
    NCTextListBox(INT InMaxLines, INT InVisibleLines, INT Unused, INT bInScrollControls);  // 0x10021AC0 (ret 0x10; the third is never read)
    // ~NCTextListBox                                                // 0x10021A70

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10020890
    virtual INT OnCreate();                                          // 37 0x100213C0
    virtual INT OnDestroy();                                         // 38 0x10021490
    virtual void ShowWindow();                                       // 48 0x100208B0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10020BD0
    virtual void AddString(const TCHAR* Text, DWORD Color, INT Param);  // 76 0x10021510
};
NW_ASSERT_OFFSET(NCTextListBox, bScrollControls, 0x240);
NW_ASSERT_SIZE(NCTextListBox, 0x244);

#pragma pack(pop)
