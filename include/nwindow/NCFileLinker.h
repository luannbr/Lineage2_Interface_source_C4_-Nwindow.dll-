#pragma once
// NCFileLinker - an <a link> of the HTML viewer (UClass "CFileLinker", 0x5D8 in C3): an invisible
// control over the text run whose click hands the page to load to the viewer (NWM_COMMAND,
// lParam = File), or, in C3, a chat line the page gave (Message: "/command", run by the chat window).

#include "nwindow/NControl.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCFileLinker : public NControl
{
    NW_DECLARE_CLASS(NCFileLinker, NControl, 0, 0x102CBDE8)

    TCHAR* File;                     // +0x1F4 (name ours) the page, an item's string
    TCHAR Message[512];              // +0x1F8 (name ours) C3: a chat line the click runs instead (NCDynamicLinker copies the
                                     //   viewer's message into it); empty: the page is loaded

    NCFileLinker(TCHAR* InFile = NULL);                                     // 0x1004a460
    // ~NCFileLinker                                                 // 0x1004a4d0

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10021e60 (shared)
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x100472c0
};
NW_ASSERT_OFFSET(NCFileLinker, File, 0x1F4);
NW_ASSERT_OFFSET(NCFileLinker, Message, 0x1F8);
NW_ASSERT_SIZE(NCFileLinker, 0x5F8);

#pragma pack(pop)