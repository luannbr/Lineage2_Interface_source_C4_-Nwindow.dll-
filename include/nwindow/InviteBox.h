#pragma once
// InviteBox - the list of friends the invite window (InviteWnd) offers (UClass "InviteBox", 0x224; C3 only, the
// messenger's conversation windows). A list box of 16 visible lines (256 kept) whose lines are drawn with a small
// icon (the friend's status icon, NListItem.Unknown8), a highlight bar under the selected one (two textures) and the
// name cut to 250 pixels; a double click is reported to the parent as WM_LBUTTONDBLCLK (0x203) and invites at once.

#include "nwindow/NCListBox.h"
#include "nwindow/NChatTypes.h"
#pragma pack(push, 4)

class InviteBox : public NCListBox
{
    NW_DECLARE_CLASS(InviteBox, NCListBox, 0, 0x1031BF48)

    UTexture* SelectTex2;            // +0x240 "L2UI_CH3.ListCtrl.TextSelect2": the right end of the highlight bar
                                     //   (the left part is NCListBox::HighlightTex, "L2UI_CH3.ListCtrl.TextSelect")

    InviteBox();                                                     // 0x10142070
    InviteBox(INT InMaxLines, INT InVisibleLines, INT InUnknown20C); // 0x10142120 (anonymous in the db) (ret 0xC)
    // ~InviteBox                                                    // 0x101420d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x1013e2b0
    virtual INT OnCreate();                                          // 37 0x1013eac0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013eb10
    virtual INT OnLButtonDblClick(WPARAM wParam, LPARAM lParam);     // 78 0x1013e310 the parent hears it (0x203)

    // 0x1011B450 (ret 0x10, name ours): a friend's line: Text (cut to fit with "..."), the person as the line's Param, the
    // status Icon
    void AddFriend(UTexture* Icon, const TCHAR* Text, NChatUser* User, DWORD Color);
};
NW_ASSERT_OFFSET(InviteBox, SelectTex2, 0x240);
NW_ASSERT_SIZE(InviteBox, 0x244);

#pragma pack(pop)
