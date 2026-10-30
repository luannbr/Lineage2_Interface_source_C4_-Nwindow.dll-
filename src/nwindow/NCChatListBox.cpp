/*=============================================================================
    NCChatListBox.cpp - the chat message list (C4: derives from NCTextListBox, which has the text, the wrapping and the scroll
    controls). Stock: 0x1005a4d0 / 0x1005a520 (constructors), 0x1005a450 (destructor), 0x100bf280 (WindowProc, shared), 0x10053ef0
    (OnCreate), 0x10052990 (IsChannelShown), 0x10056530 (OnPaint), 0x100562f0 (the cast of the parent, anonymous in the db).
    OnDestroy, SetWindowSize and ShowWindow are stubs in NNewVirtuals.gen.cpp.
=============================================================================*/
#include "nwindow/NCChatListBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCChatWnd.h"
#include "nwindow/NCSliderBar.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NString.h"

// 0x1005a4d0 - NCTextListBox's constructor, the three pieces of the page cleared
NCChatListBox::NCChatListBox()
{
    BackTex2 = NULL;
    BackTex3 = NULL;
    BackTex4 = NULL;
}
NW_HOOK_CTOR(0x1005a4d0, NCChatListBox)

// 0x1005a520 (ret 0xC; anonymous in the db) - the line limit, the visible lines and the third argument the list box never reads, with
// the scroll controls on
NCChatListBox::NCChatListBox(INT InMaxLines, INT InVisibleLines, INT Unused)
    : NCTextListBox(InMaxLines, InVisibleLines, Unused, 1)
{
    BackTex2 = NULL;
    BackTex3 = NULL;
    BackTex4 = NULL;
}
NW_HOOK_CTOR_ARGS(0x1005a520, NCChatListBox, (INT A, INT B, INT C), (A, B, C))

// 0x1005a450
NW_HOOK_DTOR(0x1005a450, NCChatListBox)

// 0x100bf280 (vslot 25) - the stock body is shared with NCDebugWnd's: a plain forward to the list box (no enabled check, unlike the
// other WindowProcs)
INT NCChatListBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListBox::WindowProc(Msg, wParam, lParam) != 0;
}
NW_HOOK_METHOD(0x100bf280, NCChatListBox, WindowProc)

// 0x10053ef0 (vslot 37) - NCListBox's OnCreate (not NCTextListBox's), the three pieces of the page, the scroll controls at the left
// (5 px in: the up button at 10, the down button at the foot, the bar between) and the text 23 px in, 14 down
INT NCChatListBox::OnCreate()
{
    NCListBox::OnCreate();
    BackTex2 = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back2"));
    BackTex3 = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back3"));
    BackTex4 = FindTexture(TEXT("L2UI_CH3.ChatWnd.Chatting_Back4"));
    ScrollUpButton->SetPosition(5, 10, 15, 15);
    ScrollDownButton->SetPosition(5, (INT)(Height - 15.0f), 15, 15);
    ScrollBar->SetPosition(5, 25, 15, (INT)(Height - 40.0f));
    TextX = 23;
    TextY = 14;
    return 1;
}
NW_HOOK_METHOD(0x10053ef0, NCChatListBox, OnCreate)

// Members of a stand-in derived class (the list is `this`) for the two slots below: their own definitions are the generated NW_STOCK_AT stubs of
// NNewVirtuals.gen.cpp (another lane's file), which jump to these addresses and so reach the hooks
struct NChatListBoxFns : public NCChatListBox
{
    __declspec(noinline) INT OnDestroyBody();
    __declspec(noinline) void SetWindowSizeBody(FLOAT W, FLOAT H);
};

// 0x10052910 (vslot 38, guard; 67 bytes) - NCTextListBox's OnDestroy and nothing else; 1
INT NChatListBoxFns::OnDestroyBody()
{
    guard(NCChatListBox::OnDestroy);
    NCTextListBox::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10052910, NChatListBoxFns, OnDestroyBody)

// 0x10052ad0 (vslot 47, guard, ret 8; 72 bytes) - NCWnd's SetWindowSize and nothing else (the same stub in NNewVirtuals.gen.cpp)
void NChatListBoxFns::SetWindowSizeBody(FLOAT W, FLOAT H)
{
    guard(NCChatListBox::SetWindowSize);
    NCWnd::SetWindowSize(W, H);
    unguard;
}
NW_HOOK_METHOD(0x10052ad0, NChatListBoxFns, SetWindowSizeBody)

// 0x100562f0 (cdecl; anonymous in the db) - UObject's checked cast to NCChatWnd, out of line in the stock image: the object comes back
// whatever the class is, and a wrong one is logged through GError ("Cast of %s to %s failed")
static __declspec(noinline) NCChatWnd* __cdecl CastToChatWnd(NCWnd* Obj)
{
    return CastChecked<NCChatWnd>(Obj);
}
NW_HOOK_FUNC(0x100562f0, CastToChatWnd)

// 0x10056530 (vslot 62, guard) - while the tab is torn off the chat window (a split page) the list paints its own frame: the top piece,
// a middle piece for each row of 15 px that fits and the bottom piece; the number of lines shown follows the height (one for each
// 15 px less 10, the scroll bar told); then the lines from the top index, each at the text origin plus its indent, one row of
// 11 + the line gap under the other, wrapped to the width less the indent
INT NCChatListBox::OnPaint(NCGDevice* Device)
{
    guard(NCChatListBox::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT Row = 0;
    if (CastToChatWnd(Entry->Parent->Wnd)->TabCtrl->IsPageSplit(this))
    {
        if (BackTex2)
            Device->DrawTexture(0, 0, 0x15C, 0x12, 0, 0, 0x15C, 0x12, BackTex2, 0xFF, 1);
        for (INT i = 0; i < (INT)((Height - 36.0f) * (1.0f / 15.0f)); i++)
        {
            if (BackTex3)
                Device->DrawTexture(0, i * 15 + 0x12, 0x15C, 0xF, 0, 0, 0x15C, 0xF, BackTex3, 0xFF, 1);
        }
        if (BackTex4)
            Device->DrawTexture(0, (INT)(Height - 18.0f), 0x15C, 0x12, 0, 0, 0x15C, 0x12, BackTex4, 0xFF, 1);
    }
    FLOAT Inner = Height - 10.0f;
    if ((INT)Inner / 15 != VisibleLines)
    {
        VisibleLines = (INT)(Inner * (1.0f / 15.0f));
        if (Items.Num() >= VisibleLines)
            TopIndex = Items.Num() - VisibleLines;
        ScrollBar->SetRange(NumLines, VisibleLines);
        ScrollBar->SetValue(TopIndex);
    }
    ScrollBar->SetRange(NumLines, VisibleLines);
    if (Items.Num() < VisibleLines)
        TopIndex = 0;
    INT i = TopIndex;
    for (; i < TopIndex + VisibleLines && i <= NumLines && i < Items.Num(); i++, Row++)
    {
        NListItem Item = Items(i);
        NString* Line = Item.Text;
        TCHAR* Text = Line->GetAt(0);
        INT Indent = Line->Indent;
        DWORD Color = Line->Color;
        Device->DrawText(TextX + Indent, TextY + (LineGap + 11) * Row, Color, Text,
                         (DWORD)(INT)(Width - 16.0f - 6.0f - (FLOAT)(UINT)Indent), 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10056530, NCChatListBox, OnPaint)

// 0x10052990 (vslot 82) - does this tab show a message of the type (and the subtype, for the system messages)? The chat types need
// their own switch and the master switch of the chat; the system messages bSystem (the damage ones and the items-used ones also their
// own switch; subtype 6 is always shown); types 6, 7, 10 and 18 are never filtered, the others (11 .. 16, above 18) never shown
UBOOL NCChatListBox::IsChannelShown(INT Type, INT SubType)
{
    switch (Type)
    {
    case 0:
        return Filter.bNormal && Filter.bChat;
    case 1:
        return Filter.bShout && Filter.bChat;
    case 2:
        return Filter.bTell && Filter.bChat;
    case 3:
        return Filter.bParty && Filter.bChat;
    case 4:
        return Filter.bPledge && Filter.bChat;
    case 5:
        if (SubType == 6)
            return 1;
        if (SubType == 3)
            return Filter.bDamage && Filter.bSystem;
        if (SubType == 7)
            return Filter.bUseItems && Filter.bSystem;
        return Filter.bSystem;
    case 6:
    case 7:
    case 10:
    case 18:
        return 1;
    case 8:
        return Filter.bMarket && Filter.bChat;
    case 9:
        return Filter.bAlly && Filter.bChat;
    case 17:
        return Filter.bHero && Filter.bChat;
    }
    return 0;
}
NW_HOOK_METHOD(0x10052990, NCChatListBox, IsChannelShown)

NW_CHECK_VTABLE(NCChatListBox, 0x101ac928, 83)
