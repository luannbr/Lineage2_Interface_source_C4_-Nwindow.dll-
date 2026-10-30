/*=============================================================================
    NCPartyRoomWnd.cpp - the window of the party room the player is in. C3 stock: 0x100a5100 .. 0x100a5380,
    0x100a5380 .. 0x100a5720 (the room's helpers), 0x100a66d0 .. 0x100a6e20 (WindowProc, OnDestroy, OnPaint,
    OnCloseBtnClick), 0x100a6ea0, 0x100a75b0, 0x100a8bd0 .. 0x100a8c70, OnCreate 0x100a8c70.
=============================================================================*/
#include "nwindow/NCPartyRoomWnd.h"
#include "nwindow/NCPartyRoomMakingWnd.h"
#include "nwindow/NCPartyRoomFrameCtrl.h"
#include "nwindow/NCMatchPartyListCtrl.h"
#include "nwindow/NCMatchWnd.h"
#include "nwindow/NCIcon.h"
#include "nwindow/NCListBox.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCommandParser.h"
#include "nwindow/NColumnInfoC4.h"
#include "nwindow/NWinBStock.h"
#include "nwindow/NConsoleFns.h"   // NCIconFlashView (also through NWinBStock.h)
#include "nwindow/NCTextListBoxC4.h"
#include "nwindow/NConsoleWnd.h"

// the chat type of the room's own chat (SendChatMsgToServer's first argument)
enum { CHAT_PartyRoom = 14 };

// the table sizes of the console's lists the window asks (the stock code takes the member's role text from three
// sysstring lines)
enum { STR_RoleMember = 1061, STR_RoleOwner = 1062, STR_RoleOther = 1063 };

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

// a value at X (the colors of the stock paint)
static FORCEINLINE void DrawValue(NCGDevice* Device, INT X, INT Y, const TCHAR* Text)
{
    Device->DrawText(X, Y, 0xFFB09B79, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
}

// a column of the member list (the stock code spells out each with its own record, 0x4018 bytes in C4)
static FORCEINLINE void AddMemberColumn(NCListCtrl* List, NColumnInfoC4& Info, INT Width, INT TitleID, UBOOL bNumeric)
{
    Info.Width = Width;
    Info.Height = 19;
    Info.bNumeric = bNumeric;
    Info.SetTitleID(TitleID);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Info.SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Info.SetTabDownTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
    List->AddColumn(0, (NListColumnInfo*)&Info);
}

// NCButton::SetTextID for the room's buttons
static FORCEINLINE NCButton* NewRoomButton(NCWnd* Parent, INT ID, INT X, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("L2UI_CH3.Button.SmallButton2"), TEXT("L2UI_CH3.Button.SmallButton2_down"), 0);
    Button->ID = ID;
    Button->SetPosition(X, 0x192, 0x42, 0x15);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}

// the level range text of a member's level (repeated inline in the stock code): "1 ~ 9", "10 ~ 19" .. "60 ~ 69",
// "70 ~ 78"
static FORCEINLINE void FormatLevelRange(TCHAR* Out, INT Level)
{
    if (Level < 10)
        appSprintf(Out, TEXT("1 ~ 9"));
    else if (Level < 70)
        appSprintf(Out, TEXT("%d ~ %d"), Level / 10 * 10, Level / 10 * 10 + 9);
    else
        appSprintf(Out, TEXT("70 ~ %d"), 78);
}

// The icon of the minimized window starts to flash (sub_1001DDB0, name ours; the stock code calls it with the window's
// Icon): when the icon is shown, a flag at +0x220 and a repeating timer (500 ms, id 0xFFF001, kept at +0x228) are set. NCIcon
// is not this lane's: the method is declared on a view of it (NCIconFlashView, NConsoleFns.h: the one definition of the struct)
void NCIconFlashView::StartFlash()
{
    if (IsShowWindow())
    {
        ::nw::Field<INT>(this, 0x220) = 1;
        ::nw::Field<INT>(this, 0x228) = SetTimer(0xFFF001, 500, NULL);
    }
}
NW_HOOK_METHOD(0x1001DDB0, NCIconFlashView, StartFlash)

// the icon a class is shown with (C4: the name from the console, sub_10074C70, then the texture)
static FORCEINLINE UTexture* ClassIcon(NCPartyRoomWnd* Wnd, INT ClassID)
{
#if NW_HOOK_MODE
    return Wnd->FindTexture(::nw::StockCallAt<const TCHAR*>(0x10074C70, GConsole, ClassID), 1);
#else
    return Wnd->FindTexture(NPartyStyleIconName(ClassID), 1);   // 0x10074C70
#endif
}

// the sysstring of a role (nothing is set for another value)
static FORCEINLINE void SetRoleText(NListCell* Cell, INT Role)
{
    if (Role == 0)
        Cell->SetText(*GL2GameData.SysString[STR_RoleMember]);
    else if (Role == 1)
        Cell->SetText(*GL2GameData.SysString[STR_RoleOwner]);
    else if (Role == 2)
        Cell->SetText(*GL2GameData.SysString[STR_RoleOther]);
}

// 0x100b5520 - NCWnd's and the vtable
NCPartyRoomWnd::NCPartyRoomWnd()
{
}
NW_HOOK_CTOR(0x100b5520, NCPartyRoomWnd)

// 0x100b31c0 (vslot 25) - the four buttons and the chat line
INT NCPartyRoomWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == (INT)NID_PARTYROOM_MANAGE)
            return OnManageBtnClick() != 0;
        if ((INT)wParam == (INT)NID_PARTYROOM_BAN)
            return OnBanBtnClick() != 0;
        if ((INT)wParam == (INT)NID_PARTYROOM_INVITE)
            return OnInviteBtnClick() != 0;
        if ((INT)wParam == (INT)NID_PARTYROOM_CLOSE)
            return OnCloseBtnClick() != 0;
        if ((INT)wParam == (INT)NID_PARTYROOM_EDIT)
            return OnChatEnter(lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x100b31c0, NCPartyRoomWnd, WindowProc)

// 0x100b55c0 (vslot 37) - the title bar (its own class: closing it leaves the room; its title is a text id), the member list
// (9 rows shown of 20, 5 columns: name, class, level, place, role), the room's chat (a text list of 40 lines, 4 visible,
// and the line to type in) and the four buttons under it
INT NCPartyRoomWnd::OnCreate()
{
    guard(NCPartyRoomWnd::OnCreate);
    NColumnInfoC4 NameColumn, ClassColumn, LevelColumn, PlaceColumn, RoleColumn;   // five records, as in the stock frame
    NCWnd::OnCreate();
    RoomFrame = new NCPartyRoomFrameCtrl();
    RoomFrame->ID = NID_PARTYROOM_FRAME;
    RoomFrame->SetPosition(0, 0, 600, 20);
    CreateChild(RoomFrame, NWS_VISIBLE);
    RoomFrame->SetTitleID(389);   // 0x10011810: the title is a text id
    BackTex = FindTexture(TEXT("L2UI_CH3.PartyMatchWnd.PartyMatch2_Back"), 1);
    MemberList = new NCMatchPartyListCtrl(20, 9, 5, 16);
    MemberList->SetPosition(7, 0x7B, 0x24A, 0xAC);
    CreateChild(MemberList, NWS_VISIBLE | NWS_UNKNOWN20);   // 0x22
    MemberList->SetTextOffset(0, 3);
    AddMemberColumn(MemberList, NameColumn, 0xCC, 0x189, 0);
    AddMemberColumn(MemberList, ClassColumn, 0x36, 0x187, 1);
    AddMemberColumn(MemberList, LevelColumn, 0x4A, 0x188, 1);
    AddMemberColumn(MemberList, PlaceColumn, 0x84, 0x405, 0);
    AddMemberColumn(MemberList, RoleColumn, 0x79, 0x407, 0);

    ChatList = NewTextListBox(40, 4, 16, 1);   // the room's chat: an NCTextListBox (NCTextListBoxC4.h)
    ChatList->ID = NID_PARTYROOM_CHAT;
    ChatList->SetPosition(7, 0x12E, 0x24A, 0x48);
    CreateChild(ChatList, NWS_VISIBLE);
    ChatEdit = new NCEditBox(1, NULL, 1, 0x83, 0);
    ChatEdit->ID = NID_PARTYROOM_EDIT;
    ChatEdit->SetPosition(7, 0x179, 0x1B4, 0x11);
    CreateChild(ChatEdit, NWS_VISIBLE);

    ManageButton = NewRoomButton(this, NID_PARTYROOM_MANAGE, 0x12A, 1032);
    BanButton = NewRoomButton(this, NID_PARTYROOM_BAN, 0x175, 1033);
    InviteButton = NewRoomButton(this, NID_PARTYROOM_INVITE, 0x1C0, 396);
    CloseButton = NewRoomButton(this, NID_PARTYROOM_CLOSE, 0x20B, 908);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b55c0, NCPartyRoomWnd, OnCreate)

// 0x100b32a0 (vslot 38) - the seven controls and the title bar go; NCWnd's OnDestroy is not called
INT NCPartyRoomWnd::OnDestroy()
{
    guard(NCPartyRoomWnd::OnDestroy);
    DestroyChild(this, ChatList);
    DestroyChild(this, ChatEdit);
    DestroyChild(this, MemberList);
    DestroyChild(this, ManageButton);
    DestroyChild(this, BanButton);
    DestroyChild(this, InviteButton);
    DestroyChild(this, CloseButton);
    DestroyChild(this, RoomFrame);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b32a0, NCPartyRoomWnd, OnDestroy)

// 0x100b3480 (vslot 62) - the background (600 x 411 under the title bar), then a label and its value: the room's
// number, its title, its place, the members now / most, the loot rule and the level range
INT NCPartyRoomWnd::OnPaint(NCGDevice* Device)
{
    guard(NCPartyRoomWnd::OnPaint);
    TCHAR Buf[260];
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, 600, 411, 0, 0, 600, 411, BackTex, 0xFF, 1);
    DrawLabel(Device, 0x3B, 0x29, *GL2GameData.SysString[416]);
    appSprintf(Buf, TEXT("%d"), RoomID);
    DrawValue(Device, 0x44, 0x29, Buf);
    DrawLabel(Device, 0x3B, 0x38, *GL2GameData.SysString[413]);
    appSprintf(Buf, TEXT("%s"), Title);
    DrawValue(Device, 0x44, 0x38, Buf);
    DrawLabel(Device, 0x3B, 0x47, *GL2GameData.SysString[1029]);
    appSprintf(Buf, TEXT("%s"), ((NConsoleLeftFns*)GConsole)->GetPartyRoomAreaName(Location));   // the place's name
    DrawValue(Device, 0x44, 0x47, Buf);
    DrawLabel(Device, 0xFE, 0x47, *GL2GameData.SysString[1036]);
    appSprintf(Buf, TEXT("%d/%d"), MemberList->GetRowCount(), MaxMembers);
    DrawValue(Device, 0x107, 0x47, Buf);
    DrawLabel(Device, 0x3B, 0x56, *GL2GameData.SysString[489]);
    appSprintf(Buf, TEXT("%s"), ((NConsoleLeftFns*)GConsole)->GetLootRuleName(LootRule));   // the loot rule's name
    DrawValue(Device, 0x44, 0x56, Buf);
    DrawLabel(Device, 0xFE, 0x56, *GL2GameData.SysString[1030]);
    appSprintf(Buf, TEXT("%d-%d"), MinLevel, MaxLevel);
    DrawValue(Device, 0x107, 0x56, Buf);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b3480, NCPartyRoomWnd, OnPaint)

// 0x100b18c0 (vslot 74) - the room's data into the making window (the title, the members as an index, the levels,
// the loot rule), which is shown and takes the focus
INT NCPartyRoomWnd::OnManageBtnClick()
{
    guard(NCPartyRoomWnd::OnManageBtnClick);
    static_cast<NCPartyRoomMakingWnd*>(GDesktop->PartyRoomMakingWnd)->SetData(RoomID, Title, MaxMembers - 2, MinLevel,
                                                                              MaxLevel, LootRule);
    GDesktop->PartyRoomMakingWnd->ShowWindow();
    GDesktop->PartyRoomMakingWnd->SetFocus();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b18c0, NCPartyRoomWnd, OnManageBtnClick)

// 0x100b19b0 (vslot 75) - the selected member is banned (the request carries his object id)
INT NCPartyRoomWnd::OnBanBtnClick()
{
    guard(NCPartyRoomWnd::OnBanBtnClick);
    NListRow* Row = MemberList->GetSelectedRow();
    if (Row)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)MemberObjectID(Row));
        ((NConsoleForwardFns*)GConsole)->RequestOustFromPartyRoom(Params);   // 0x1006DF70
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b19b0, NCPartyRoomWnd, OnBanBtnClick)

// 0x100b1a70 (vslot 76) - the selected member (by name) is invited to the party, with the room's loot rule
INT NCPartyRoomWnd::OnInviteBtnClick()
{
    guard(NCPartyRoomWnd::OnInviteBtnClick);
    NListRow* Row = MemberList->GetSelectedRow();
    if (Row)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)Row->Cells->Text);
        Params.PushBack((void*)LootRule);
        GConsole->RequestPartyInvite(Params);
    }
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b1a70, NCPartyRoomWnd, OnInviteBtnClick)

// 0x100b3940 (vslot 77) - leaves the room, and the window closes
INT NCPartyRoomWnd::OnCloseBtnClick()
{
    guard(NCPartyRoomWnd::OnCloseBtnClick);
    RequestExit();
    HideWindow();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100b3940, NCPartyRoomWnd, OnCloseBtnClick)

// 0x100b1b40 (name ours) - the owner dismisses the room, a member (or another one) leaves it; the request is the
// room's number; nothing for another mode
void NCPartyRoomWnd::RequestExit()
{
    if (Mode == PARTYROOM_Owner)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)RoomID);
        ((NConsoleForwardFns*)GConsole)->RequestDismissPartyRoom(Params);   // 0x1006DF80
    }
    else if (Mode == PARTYROOM_Member || Mode == PARTYROOM_Other)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)RoomID);
        ((NConsoleForwardFns*)GConsole)->RequestWithdrawPartyRoom(Params);   // 0x1006DF90
    }
}
NW_HOOK_METHOD(0x100b1b40, NCPartyRoomWnd, RequestExit)

// 0x100b1c10 (the console's slot 376) - the room: a minimized window flashes its icon, else the matching window goes and
// this one is shown with the focus; then the numbers and the title are read (the number, the most members, the levels, the
// loot rule, the place, the title)
void NCPartyRoomWnd::SetRoomInfo(L2ParamStack& Params)
{
    if (Icon && Icon->IsShowWindow())
        static_cast<NCIconFlashView*>(Icon)->StartFlash();
    else
    {
        GDesktop->MatchWnd->HideWindow();
        SetFocus();
        ShowWindow();
    }
    RoomID = (INT)Params.Top();
    MaxMembers = (INT)Params.Top();
    MinLevel = (INT)Params.Top();
    MaxLevel = (INT)Params.Top();
    LootRule = (INT)Params.Top();
    Location = (INT)Params.Top();
    appStrcpy(Title, (TCHAR*)Params.Top());
}
NW_HOOK_METHOD(0x100b1c10, NCPartyRoomWnd, SetRoomInfo)

// 0x100b1f70 (ret 8) - a line into the room's chat (NCConsole::Say's), the line's color, no parameter; the minimized
// window's icon flashes
void NCPartyRoomWnd::AddChatLine(const TCHAR* Text, DWORD Color)
{
    if (ChatList)
    {
        ChatList->AddString(Text, Color, -1);
        static_cast<NCIconFlashView*>(Icon)->StartFlash();
    }
}
NW_HOOK_METHOD(0x100b1f70, NCPartyRoomWnd, AddChatLine)

// 0x100b1fa0 (ret 4) - Enter in the chat line: a "//" line goes as a builder command, a plain one to the room's chat
// (type 14), another /command nowhere; the line is emptied
INT NCPartyRoomWnd::OnChatEnter(LPARAM lParam)
{
    if (lParam == 1)
    {
        const TCHAR* Text = ChatEdit->CommitText();
        if (Text)
        {
            TCHAR Line[1024];
            appStrcpy(Line, Text);
            NCommandParser Parser;
            INT Command = Parser.GetCommand(Line);
            if (Command == NCMD_Builder)
            {
                L2ParamStack Params(10);
                Params.PushBack(Line + 2);
                ::nw::VirtualCall<void>(GConsole, 194, &Params);   // BuilderCommand (slot 194 of this console)
            }
            else if (Command == NCMD_Chat)
                GConsole->SendChatMsgToServer(CHAT_PartyRoom, (TCHAR*)Text, NULL);
            ChatEdit->ClearText();
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x100b1fa0, NCPartyRoomWnd, OnChatEnter)

// a member's row (the stock code spells it out for the list's fill and for the add and the update of one member):
// the object id and the name, the class icon (no text), the level range, the place and the role, as the packet
// carries them; returns the role
static FORCEINLINE INT FillMemberRow(NCPartyRoomWnd* Wnd, NListRow* Row, INT ObjectID, L2ParamStack& Params)
{
    TCHAR Buf[260];
    Row->Cells[0].SetParams(ObjectID, -1, -1);
    Row->Cells[0].SetText((TCHAR*)Params.Top());
    INT ClassID = (INT)Params.Top();
    Row->Cells[1].SetIcon(ClassIcon(Wnd, ClassID), 11, 11);
    Row->Cells[1].SetNumber(ClassID);          // C3 kept the class in the row's Param2
    ::nw::Field<INT>(&Row->Cells[1], 0x1A60) = 0;   // the cell's bTextVisible (the header's NListCell has C3's +0x1820)
    FormatLevelRange(Buf, (INT)Params.Top());
    Row->Cells[2].SetText(Buf);
    Row->Cells[3].SetText(((NConsoleLeftFns*)GConsole)->GetPartyRoomAreaName((INT)Params.Top()));
    INT Role = (INT)Params.Top();
    SetRoleText(&Row->Cells[4], Role);
    return Role;
}

// 0x100b1cc0 (the console's slot 377) - the whole member list: the rows go, the mode (what the player is in the room)
// sets the buttons, then the members as the packet gives them; the minimized window's icon flashes
void NCPartyRoomWnd::SetMembers(L2ParamStack& Params)
{
    MemberList->ClearRows();
    Mode = (INT)Params.Top();
    UBOOL bEnable;
    if (Mode == PARTYROOM_Owner)
    {
        ManageButton->EnableWindow(1);
        BanButton->EnableWindow(1);
        bEnable = 1;
        InviteButton->EnableWindow(bEnable);
        CloseButton->EnableWindow(1);
    }
    else if (Mode == PARTYROOM_Member || Mode == PARTYROOM_Other)
    {
        ManageButton->EnableWindow(0);
        BanButton->EnableWindow(0);
        bEnable = 0;
        InviteButton->EnableWindow(bEnable);
        CloseButton->EnableWindow(1);
    }
    INT Count = (INT)Params.Top();
    while (Count > 0)
    {
        NListRow* Row = new NListRow(5);
        FillMemberRow(this, Row, (INT)Params.Top(), Params);
        MemberList->AddRow(Row);
        Row->ToolTip.FillMatchRow(Row, 0x800000);
        Count--;
    }
    static_cast<NCIconFlashView*>(Icon)->StartFlash();
}
NW_HOOK_METHOD(0x100b1cc0, NCPartyRoomWnd, SetMembers)

// 0x100b3eb0 (the console's slot 379) - one member: 0 adds, 1 changes and 2 removes the row of the member (by his
// object id); when the member is the player, his role sets the mode and the buttons; the minimized window's icon flashes
void NCPartyRoomWnd::UpdateMember(L2ParamStack& Params)
{
    INT Action = (INT)Params.Top();
    INT MemberID = (INT)Params.Top();
    INT Role = 0;
    if (Action == 0)
    {
        NListRow* Row = new NListRow(5);
        Role = FillMemberRow(this, Row, MemberID, Params);
        MemberList->AddRow(Row);
        Row->ToolTip.FillMatchRow(Row, 0x800000);
    }
    else if (Action == 1)
    {
        NListRow* Row = MemberList->FindMemberRow(MemberID);
        if (Row)
        {
            Role = FillMemberRow(this, Row, MemberID, Params);
            Row->ToolTip.FillMatchRow(Row, 0x800000);
        }
    }
    else if (Action == 2)
    {
        INT Index = MemberList->FindMemberIndex(MemberID);
        if (Index >= 0)
            MemberList->Rows.Remove(Index, 1);   // (the row itself is not freed: as the stock code)
    }
    User* Me = GConsole->GetMyUser();
    if (Me && Me->ObjectID == MemberID)
    {
        Mode = Role;
        UBOOL bEnable;
        if (Role == PARTYROOM_Owner)
        {
            ManageButton->EnableWindow(1);
            BanButton->EnableWindow(1);
            bEnable = 1;
            InviteButton->EnableWindow(bEnable);
            CloseButton->EnableWindow(1);
        }
        else if (Role == PARTYROOM_Member || Role == PARTYROOM_Other)
        {
            ManageButton->EnableWindow(0);
            BanButton->EnableWindow(0);
            bEnable = 0;
            InviteButton->EnableWindow(bEnable);
            CloseButton->EnableWindow(1);
        }
    }
    static_cast<NCIconFlashView*>(Icon)->StartFlash();
}
NW_HOOK_METHOD(0x100b3eb0, NCPartyRoomWnd, UpdateMember)

// 0x100a4d80 (vslot 49) - minimizing: stock body shared with NCMatchWnd's (the same icon)
void NCPartyRoomWnd::Minimize()
{
    if (Icon)
        Icon->SetIcon(TEXT("L2UI_CH3.PartyMatchWnd.PartyMatchIcon"), 0x185, 1);
    if (ScreenX < 0 && Icon)
        Icon->OffsetWindow(-ScreenX, 0);
    HideWindow();
    if (Icon)
    {
        Icon->ShowWindow();
        Icon->SetFocus();
    }
}

// stock body shared with NCMatchWnd's (0x100aa860): shown again from its minimized icon, the window takes the icon's
// place and the icon goes away
void NCPartyRoomWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    if (Icon)
    {
        SetPosition(Icon->ScreenX, Icon->ScreenY, (INT)Width, (INT)Height);
        Icon->SetPosition(0, 0, 0, 0);
        Icon->HideWindow();
    }
}

// 0x100b5570 - the vtable, ConditionalDestroy, NCWnd
NW_HOOK_DTOR(0x100b5570, NCPartyRoomWnd)

NW_CHECK_VTABLE(NCPartyRoomWnd, 0x101bc310, 78)
