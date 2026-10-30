#pragma once
// L2TaskBox - TaskBox's Lineage II skin (UClass "L2TaskBox", 0x22C; C3 only, the messenger): the list of L2Task entries.
// The same 140 x 21 buttons (the three textures of TaskBox), without the line icons, the name at +3; a line's Param is
// its L2Task window. Its AddString takes the Param before the color.

#include "nwindow/NCListBox.h"
#pragma pack(push, 4)

class NCWnd;

class L2TaskBox : public NCListBox
{
    NW_DECLARE_CLASS(L2TaskBox, NCListBox, 0, 0x10321CD0)

    UTexture* NormalTex;             // +0x240 (name ours) "L2UI_CH3.Msn.msnbutton" (OnCreate, TaskBox's body)
    UTexture* SelectedTex;           // +0x244 "L2UI_CH3.Msn.msnbutton_down"
    UTexture* AlertTex;              // +0x248 "L2UI_CH3.Msn.msnbutton_light"

    L2TaskBox();                                                     // 0x1014aa70
    L2TaskBox(INT InMaxLines, INT InVisibleLines, INT InUnknown20C); // 0x1014ab30 (anonymous in the db) (ret 0xC)
    // ~L2TaskBox                                                    // 0x1014aae0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148eb0 (TaskBox's, shared)
    virtual INT OnCreate();                                          // 37 0x1013f6e0 (TaskBox's, shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10149250
    virtual INT OnLButtonUp(WPARAM wParam, LPARAM lParam);           // 69 0x10149460 (the db calls it OnLButtonDown)
    virtual INT HitTest(INT X, INT Y);                               // 75 0x1013e740 (TaskBox's, shared)
    // 70 0x10124EB0 ret 0xC: the second argument is the line's Param and the third its color (the opposite of the base)
    virtual void AddString(const TCHAR* Text, DWORD Param, INT Color);
};
NW_ASSERT_OFFSET(L2TaskBox, NormalTex, 0x240);
NW_ASSERT_OFFSET(L2TaskBox, AlertTex, 0x248);
NW_ASSERT_SIZE(L2TaskBox, 0x24C);

#pragma pack(pop)
