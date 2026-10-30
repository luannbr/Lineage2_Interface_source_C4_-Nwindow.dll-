#pragma once
// FriendBox - the roster list of a conversation window (UClass "FriendBox", 0x220; C3 only, the messenger): the people
// in the conversation, one per line, each with its small status icon (NListItem.Unknown8) and its name cut to 201
// pixels. The text starts at (4, 2).

#include "nwindow/NCListBox.h"
#include "nwindow/NChatTypes.h"
#pragma pack(push, 4)

class FriendBox : public NCListBox
{
    NW_DECLARE_CLASS(FriendBox, NCListBox, 0, 0x1031D410)

    FriendBox();                                                     // 0x10142520
    FriendBox(INT InMaxLines, INT InVisibleLines, INT Unused);       // 0x101425c0 (anonymous in the db) (ret 0xC)
    // ~FriendBox                                                    // 0x10142570

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10020890 (NCDebugWnd's, shared)
    virtual INT OnCreate();                                          // 37 0x1013ef50
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x1013ef80

    // 0x1011B610 (ret 0x48, name ours): a person's line: the decoded name in Color and the status icon (NLN online, BRB / IDL /
    // AWY / LUN away, BSY / PHN busy, FLN offline = the fourth, anything else none); the person comes by value
    void AddUser(NChatUser User, DWORD Color);
};
NW_ASSERT_SIZE(FriendBox, 0x240);

#pragma pack(pop)
