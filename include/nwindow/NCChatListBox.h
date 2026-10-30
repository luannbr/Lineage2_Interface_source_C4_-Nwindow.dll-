#pragma once
// NCChatListBox - the chat message list (UClass "CChatListBox", 0x284, super NCTextListBox in C4). Long messages are wrapped to the
// width (continuation lines indented 12 px); the scroll controls sit on the left and the visible line count follows the height
// (15 px lines). Each chat tab has one; its Filter says which message types it shows (IsChannelShown, NCChatWnd::AddChatMessage).
// The system message window (NCChatSystemMsgWnd) derives from it.

#include "nwindow/NCTextListBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

// The channel switches of one chat tab as C4 has them (names ours, after their chatfilter.ini keys; the message types are those of
// L2's Say2 packet): 0x34 bytes at NCChatListBox +0x244 (C3: 0x2C at +0x240; the hero channel and one more flag are new).
// Passed by value to NCChatFilterWnd::SetFilter (ret 0x34, NCChatFilterWnd.h has its own copy of this struct, NChatFilterC4).
struct NChatFilter
{
    UBOOL bSystem;             // +0x00 "system" type 5 (system / GM messages)
    UBOOL bChat;               // +0x04 "chat"   master switch of the chat types below
    UBOOL bDamage;             // +0x08 "damage" type 5 subtype 3, needs bSystem too
    UBOOL bNormal;             // +0x0C "normal" type 0
    UBOOL bShout;              // +0x10 "shout"  type 1 (!)
    UBOOL bPledge;             // +0x14 "pledge" type 4 (@)
    UBOOL bParty;              // +0x18 "party"  type 3 (#)
    UBOOL bMarket;             // +0x1C "market" type 8 (+)
    UBOOL bTell;               // +0x20 "tell"   type 2 (")
    UBOOL bAlly;               // +0x24 "ally"   type 9 ($)
    UBOOL bUseItems;           // +0x28 "useitems" (the ini key): the messages of items used, type 5 subtype 7, a sub-switch of bSystem like bDamage
    UBOOL bHero;               // +0x2C "hero" new in C4: the hero voice, type 17
    UBOOL Unknown30;           // +0x30 new in C4: copied with the rest, nobody reads or writes it in the code we know
};
NW_ASSERT_SIZE(NChatFilter, 0x34);

class NCChatListBox : public NCTextListBox
{
    NW_DECLARE_CLASS(NCChatListBox, NCTextListBox, 0, 0x102D0CD8)

    NChatFilter Filter;        // +0x244
    UTexture* BackTex2;        // +0x278 "L2UI_CH3.ChatWnd.Chatting_Back2" the top piece of a split-off chat page, 348 x 18 (names ours)
    UTexture* BackTex3;        // +0x27C "L2UI_CH3.ChatWnd.Chatting_Back3" the middle piece, one 15 px row each
    UTexture* BackTex4;        // +0x280 "L2UI_CH3.ChatWnd.Chatting_Back4" the bottom piece

    NCChatListBox();                                                 // 0x1005A4D0
    NCChatListBox(INT InMaxLines, INT InVisibleLines, INT Unused);   // 0x1005A520 (ret 0xC; anonymous in the db: the db's 0x1005A620 is the
                                                                     //   system message window's)
    // ~NCChatListBox                                                // 0x1005A450

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100bf280 (shared with NCDebugWnd's)
    virtual INT OnCreate();                                          // 37 0x10053EF0
    // 38, 47 and 48 are stock stubs of the generated NNewVirtuals.gen.cpp (shared file: the three could be ported here the day it drops them)
    virtual INT OnDestroy();                                         // 38 0x10052910 (stub there)
    virtual void SetWindowSize(FLOAT W, FLOAT H);                    // 47 0x10052AD0 (stub there)
#if NW_HOOK_MODE
    virtual void ShowWindow();                                       // 48 0x100208B0 (NCTextListBox's, hooked in NCTextListBox.cpp)
#endif   // the standalone inherits NCTextListBox::ShowWindow: the stock slot points straight at it
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10056530 (stays stock)
    virtual UBOOL IsChannelShown(INT Type, INT SubType);             // 82 0x10052990 (name ours; the stock
                                                                     //   NCChatWnd::CaptureMessage calls +0x148)
};
NW_ASSERT_OFFSET(NCChatListBox, Filter, 0x244);
NW_ASSERT_OFFSET(NCChatListBox, BackTex2, 0x278);
NW_ASSERT_OFFSET(NCChatListBox, BackTex4, 0x280);
NW_ASSERT_SIZE(NCChatListBox, 0x284);

#pragma pack(pop)
