#pragma once
// L2ChatBox - ChatBox's Lineage II skin (UClass "L2ChatBox", 0x220; C3 only, the messenger): the conversation's text list
// of L2ChatWnd. The same wrapped lines of ChatBox, drawn without icons.

#include "nwindow/NCListBox.h"
#pragma pack(push, 4)

class L2ChatBox : public NCListBox
{
    NW_DECLARE_CLASS(L2ChatBox, NCListBox, 0, 0x10322200)

    L2ChatBox();                                                     // 0x1014a280
    L2ChatBox(INT InMaxLines, INT InVisibleLines, INT Unused);       // 0x1014a320 (anonymous in the db) (ret 0xC)
    // ~L2ChatBox                                                    // 0x1014a2d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10020890 (NCDebugWnd's, shared)
    virtual INT OnCreate();                                          // 37 0x1013f170 (ChatBox's, shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10148f50

    // names ours
    // 0x10124B80 (ret 0xC): Text wrapped to the box's width (12 pixels less when bIndent: the three spaces put in front of each
    // line) and added line by line in Color
    void Add(const TCHAR* Text, DWORD Color, INT bIndent);
    // 0x101250C0 (ret 0xC): a message of the talk: with a name, a grey line "<SystemMsg 1326 with $s1 = the name>" and the text
    // indented under it; without a name, just the text in Color
    void AddMessage(const TCHAR* From, const TCHAR* Text, DWORD Color);
};
NW_ASSERT_SIZE(L2ChatBox, 0x240);

#pragma pack(pop)
