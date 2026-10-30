/*=============================================================================
    NCPartyRoomMakingWnd.cpp - the window that opens or edits a party room. C3 stock: 0x100a4ec0 .. 0x100a5040,
    0x100a6000 .. 0x100a6440, 0x100a7130, 0x100a8a60 .. 0x100a8ab0.
=============================================================================*/
#include "nwindow/NCPartyRoomMakingWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NWinBStock.h"

// one child of OnDestroy (repeated inline in the stock code)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// a label right-aligned at RightX (spelled out at each use in the stock paint)
static FORCEINLINE void DrawLabel(NCGDevice* Device, INT RightX, INT Y, const TCHAR* Text)
{
    SIZE Size;
    NCGDevice::GetTextExtent(&Size, Text, 0, 0, 0, 0, 0);
    Device->DrawText(RightX - Size.cx, Y, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
}

// NCComboBox::AddItem(Text) (0x100068B0)
static FORCEINLINE void AddComboItem(NCComboBox* Combo, const TCHAR* Text)
{
    Combo->AddItem(Text);
}

// 0x100b53b0 - the room number is 0 (a new room)
NCPartyRoomMakingWnd::NCPartyRoomMakingWnd()
{
    RoomID = 0;
}
NW_HOOK_CTOR(0x100b53b0, NCPartyRoomMakingWnd)

// 0x100b16b0 (vslot 25) - OK and Cancel
INT NCPartyRoomMakingWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == (INT)NID_ROOMMAKING_OK)
            return OnOkBtnClick() != 0;
        if ((INT)wParam == (INT)NID_ROOMMAKING_CANCEL)
            return OnCancelBtnClick() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100b16b0, NCPartyRoomMakingWnd, WindowProc)

// 0x100b39c0 (vslot 37) - the title box (300 wide, 0xEC pixels on the screen), the members box (2 .. 12 in C4), the two
// level boxes (numbers), the buttons (texts 140 and 141) and the background
INT NCPartyRoomMakingWnd::OnCreate()
{
    NCWnd::OnCreate();
    TitleEdit = new NCEditBox(0, NULL, 1, 300, 0);
    TitleEdit->ID = NID_ROOMMAKING_TITLE;
    TitleEdit->SetPosition(0x67, 0x11, 0xEC, 0x11);
    CreateChild(TitleEdit, NWS_VISIBLE);
    MembersCombo = new NCComboBox();
    MembersCombo->ID = NID_ROOMMAKING_MEMBERS;
    MembersCombo->SetPosition(0x67, 0x25, 0x28, 0x11);
    CreateChild(MembersCombo, NWS_VISIBLE | 0x10);   // 0x12: 0x10's meaning is not known
    MinLevelEdit = new NCEditBox(3, NULL, 1, 0, 0);
    MinLevelEdit->ID = NID_ROOMMAKING_MINLEVEL;
    MinLevelEdit->SetPosition(0x83, 0x39, 0x1A, 0x11);
    CreateChild(MinLevelEdit, NWS_VISIBLE);
    MaxLevelEdit = new NCEditBox(3, NULL, 1, 0, 0);
    MaxLevelEdit->ID = NID_ROOMMAKING_MAXLEVEL;
    MaxLevelEdit->SetPosition(0xC9, 0x39, 0x1A, 0x11);
    CreateChild(MaxLevelEdit, NWS_VISIBLE);
    OKButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    OKButton->ID = NID_ROOMMAKING_OK;
    OKButton->SetPosition(0x65, 0x6E, 0x4C, 0x17);
    CreateChild(OKButton, NWS_VISIBLE);
    OKButton->SetTextID(0x8C);
    CancelButton = new NCButton(TEXT("L2UI_CH3.Button.Btn1_normal"), TEXT("L2UI_CH3.Button.Btn1_normalOn"), 0);
    CancelButton->ID = NID_ROOMMAKING_CANCEL;
    CancelButton->SetPosition(0xB5, 0x6E, 0x4C, 0x17);
    CreateChild(CancelButton, NWS_VISIBLE);
    CancelButton->SetTextID(0x8D);
    AddComboItem(MembersCombo, TEXT("2"));
    AddComboItem(MembersCombo, TEXT("3"));
    AddComboItem(MembersCombo, TEXT("4"));
    AddComboItem(MembersCombo, TEXT("5"));
    AddComboItem(MembersCombo, TEXT("6"));
    AddComboItem(MembersCombo, TEXT("7"));
    AddComboItem(MembersCombo, TEXT("8"));
    AddComboItem(MembersCombo, TEXT("9"));
    AddComboItem(MembersCombo, TEXT("10"));
    AddComboItem(MembersCombo, TEXT("11"));
    AddComboItem(MembersCombo, TEXT("12"));
    BackTex = FindTexture(TEXT("L2UI_CH3.PartyMatchWnd.PartyMatch3_Back"));
    return 1;
}
NW_HOOK_METHOD(0x100b39c0, NCPartyRoomMakingWnd, OnCreate)

// 0x100b2af0 (vslot 38) - the six controls go; NCWnd's OnDestroy is not called (the guard's name has the typo of the
// stock code: "OnDestroy()")
INT NCPartyRoomMakingWnd::OnDestroy()
{
    guard(NCPartyRoomMakingWnd::OnDestroy());
    DestroyChild(this, TitleEdit);
    DestroyChild(this, MembersCombo);
    DestroyChild(this, MinLevelEdit);
    DestroyChild(this, MaxLevelEdit);
    DestroyChild(this, OKButton);
    DestroyChild(this, CancelButton);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b2af0, NCPartyRoomMakingWnd, OnDestroy)

// 0x100b2c70 (vslot 62) - the background (356 x 141) and the five labels, right-aligned in front of the boxes
INT NCPartyRoomMakingWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPartyRoomMakingWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, 356, 141, 0, 0, 356, 141, BackTex, 0xFF, 1);
    DrawLabel(Device, 0x5E, 0x14, *GL2GameData.SysString[413]);
    DrawLabel(Device, 0x5E, 0x28, *GL2GameData.SysString[1042]);
    DrawLabel(Device, 0x5E, 0x3C, *GL2GameData.SysString[1030]);
    DrawLabel(Device, 0x7F, 0x3C, *GL2GameData.SysString[1043]);
    DrawLabel(Device, 0xC5, 0x3C, *GL2GameData.SysString[1044]);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b2c70, NCPartyRoomMakingWnd, OnPaint)

// 0x100b2f30 (vslot 74) - the request to open (or change) a room: the room number, the members, the level range
// (each from 1 to 78, the second not below the first; the defaults 1 and 78 when a box is empty), the party's loot
// rule (the console's +0x3C6C in C4) and the title (the default title of system message 1398 when empty; checked
// for bad words, which it takes out); then the window closes
INT NCPartyRoomMakingWnd::OnOkBtnClick()
{
    guard(NCPartyRoomMakingWnd::OnOkBtnClick);
    L2ParamStack Params(10);
    Params.PushBack((void*)RoomID);
    Params.PushBack((void*)(MembersCombo->Selected + 2));
    INT Min = 1;
    INT Max = 78;
    if (MinLevelEdit->CommitText())
    {
        Min = appAtoi(MinLevelEdit->CommitText());
        if (Min <= 0)
            Min = 1;
        else if (Min > 78)
            Min = 78;
    }
    if (MaxLevelEdit->CommitText())
    {
        Max = appAtoi(MaxLevelEdit->CommitText());
        if (Max > 78)
            Max = 78;
        else if (Max <= 0)
            Max = 1;
    }
    if (Max < Min)
        Max = Min;
    Params.PushBack((void*)Min);
    Params.PushBack((void*)Max);
    Params.PushBack((void*)GConsole->PartyItemRule);   // the party item rule (NConsoleWnd +0x3C6C)
    TCHAR Title[1024];
    const TCHAR* TitleText;
    if (TitleEdit->CommitText())
    {
        appStrcpy(Title, TitleEdit->CommitText());
        GConsole->HasBadWord(Title);
        TitleText = Title;
    }
    else
        TitleText = *GL2GameData.SystemMsg[1398].Message;
    Params.PushBack((void*)TitleText);
    ((NConsoleForwardFns*)GConsole)->RequestManagePartyRoom(Params);   // 0x1006DF50
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b2f30, NCPartyRoomMakingWnd, OnOkBtnClick)

// 0x100b1830 (vslot 75) - closes
INT NCPartyRoomMakingWnd::OnCancelBtnClick()
{
    guard(NCPartyRoomMakingWnd::OnCancelBtnClick);
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b1830, NCPartyRoomMakingWnd, OnCancelBtnClick)

// 0x100b1730 (ret 0x18) - the room's data into the boxes: the title and the level range as the text of the boxes
// (they are emptied first), the members box on its index
void NCPartyRoomMakingWnd::SetData(INT InRoomID, const TCHAR* Title, INT MembersIndex, INT MinLevel, INT MaxLevel,
                                   INT Unused)
{
    guard(NCPartyRoomMakingWnd::SetData);
    RoomID = InRoomID;
    TitleEdit->ClearText();
    TitleEdit->InsertText(Title, 1);
    MembersCombo->SetSelected(MembersIndex);
    MinLevelEdit->ClearText();
    MinLevelEdit->InsertText(appItoa(MinLevel), 1);
    MaxLevelEdit->ClearText();
    MaxLevelEdit->InsertText(appItoa(MaxLevel), 1);
    unguard;
}
NW_HOOK_METHOD(0x100b1730, NCPartyRoomMakingWnd, SetData)

// 0x100b5400 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100b5400, NCPartyRoomMakingWnd)

NW_CHECK_VTABLE(NCPartyRoomMakingWnd, 0x101bc098, 76)
