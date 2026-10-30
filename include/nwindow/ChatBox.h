#pragma once
// ChatBox - the conversation's text (UClass "ChatBox", 0x220; C3 only, the messenger): a list box of 200 lines (16
// shown) where ChatWnd::ReceiveInstantMessage / the local echo add the messages. A line is one wrapped piece of the
// message (the box wraps it itself, Add), drawn in its own color, with a small icon in front when the line has one
// (NListItem.Unknown8); the text starts at (4, 4) and the lines are 18 pixels apart.

#include "nwindow/NCListBox.h"
#include "nwindow/NChatTypes.h"
#pragma pack(push, 4)

class ChatBox : public NCListBox
{
    NW_DECLARE_CLASS(ChatBox, NCListBox, 0, 0x1031CEE0)

    ChatBox();                                                       // 0x101425f0
    ChatBox(INT InMaxLines, INT InVisibleLines, INT Unused);         // 0x10142690 (anonymous in the db) (ret 0xC)
    // ~ChatBox                                                      // 0x10142640

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10020890 (NCDebugWnd's, shared)
    virtual INT OnCreate();                                          // 37 0x1013f170 (shared with L2ChatBox)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013f1a0

    // names ours
    // 0x1011BB00 (ret 0xC): Text wrapped to the box's width and added line by line in Color; with bIndent the
    // continuation lines begin with three spaces
    void Add(const TCHAR* Text, DWORD Color, INT bIndent);
    // 0x1011B870 (ret 0xC): Text wrapped to the width less 35 for its first line (which gets Icon, a texture) and less 22
    // for the others; each line is added in Color
    void AddWithIcon(INT Icon, const TCHAR* Text, DWORD Color);
    // 0x1011C6B0 (ret 0xC): a message of the talk: with a person, a line "<SystemMsg 1326 with $s1 = the person's decoded
    // name>" in grey (0xFF929292) and the text indented under it; without, just the text in Color
    void AddMessage(const TCHAR* Text, NChatUser* From, DWORD Color);
};
NW_ASSERT_SIZE(ChatBox, 0x240);

#pragma pack(pop)
