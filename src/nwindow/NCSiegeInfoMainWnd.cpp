/*=============================================================================
    NCSiegeInfoMainWnd.cpp - the castle siege window, its tabs and its clan list.
    Stock: 0x100ab3a0 .. 0x100adf1c (the TArray<NSiegeClanInfo*> code at 0x100ad570 is the
    template's).
=============================================================================*/
#include "nwindow/NCSiegeInfoMainWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCTabControl.h"
#include "nwindow/NColumnInfoC3.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NDialogInfo.h"

// NWM_COMMAND ids (names ours)
enum
{
    NID_SIEGE_FRAME = 0xF22035,
    NID_SIEGE_TABS = 0xF22036,
    NID_SIEGE_APPLY = 0xF023A0,
    NID_SIEGE_CANCELAPPLY = 0xF023A1,
    NID_SIEGE_APPROVE = 0xF023A2,
    NID_SIEGE_REFUSE = 0xF023A3,
    NID_SIEGE_TIMECOMBO = 0xA0F311,
};

// what the date's dialog sends its owner on OK (NCDialogBox::OnOK, name ours)
enum { NWM_SIEGE_DATE_OK = (INT)0xF0000011 };

// NDialogInfo::ID (names ours): 0x3D answers the date tab, the others the server (NCDialogBox).
// C2: every id is 4 above C1's (0x39 .. 0x3F)
enum
{
    DIALOG_SIEGE_DATE = 0x3D,
    DIALOG_SIEGE_ATTACK = 0x3E,
    DIALOG_SIEGE_CANCEL_ATTACK = 0x3F,
    DIALOG_SIEGE_DEFEND = 0x40,
    DIALOG_SIEGE_CANCEL_DEFEND = 0x41,
    DIALOG_SIEGE_APPROVE = 0x42,
    DIALOG_SIEGE_REFUSE = 0x43,
};

// systemmsg lines
enum
{
    MSG_ConfirmSiegeTime = 663,      // "The siege time has been declared for $s. ..."
    MSG_ApplyAttack = 667,           // "You are registering on the attacking side of the $s1 siege. ..."
    MSG_ApplyDefense = 668,          // "... on the defending side of the $s1 siege. ..."
    MSG_CancelApplication = 669,
    MSG_RefuseDefender = 670,
    MSG_ApproveDefender = 671,
};

// sysstring lines
enum
{
    STR_Leader = 342,
    STR_Approved = 567,
    STR_Waiting = 568,
    STR_AllianceLeader = 569,
    STR_ApplyTime = 570,
    STR_AllianceName = 571,
    STR_Apply = 572,
    STR_CancelApply = 573,
    STR_Approve = 574,
    STR_Cancel = 575,                // the refuse button's
    STR_SiegeInfo_TabDate = 0x247,   // the three tabs' titles (C3)
    STR_AttackingClans_Tab = 0x24A,
    STR_DefendingClans_Tab = 0x24B,
    STR_Refused = 579,               // a refused defender's status (C1 had a literal; "Refuse")
    STR_AttackingClans = 576,
    STR_DefendingClans = 577,
    STR_WaitingClans = 578,
    STR_ClanName = 580,
    STR_Status = 581,
    STR_CurrentTime = 582,
    STR_SiegeDate = 583,
    STR_Undecided = 584,
    STR_AutoSetting = 585,
    STR_SiegeInfo = 586,
    STR_DefenseInfo = 587,
    STR_Lord = 588,
    STR_Clan = 589,
    STR_Alliance = 590,
    STR_NoAlliance = 591,
    STR_NoCastleOwner = 595,
};

enum { SOUND_WindowOpen = 5 };  // interface sound (FL2NetNotify::OnInterfacePlaySound)

enum { SIEGE_TITLE_COLOR = 0xFFB09B79, SIEGE_LABEL_COLOR = 0xFFA3A3A3 };

// the stock code destroys its dialog record through one out-of-line destructor (0x10006e20); ours inlines the
// strings' destructors, so the function gets one of its own here
struct NDlg : public NDialogInfo
{
    __declspec(noinline) ~NDlg() {}
};

// NCFrameCtrl(Decorated, Vertical, Small): 0x100126E0 (ret 0xc), run on the object that `new` allocates
static NCFrameCtrl* NewFrameCtrl(INT A, INT B, INT C)
{
    return new NCFrameCtrl(A, B, C);
}

// the list is made through the siege list's own constructor (0x1010C4D0, ret 0x10)
static NCSiegeInfoListCtrl* NewSiegeList(INT MaxRows, INT VisibleRows, INT MaxColumns, INT Unused)
{
    return new NCSiegeInfoListCtrl(MaxRows, VisibleRows, MaxColumns, Unused);
}

// a child window goes (repeated inline in the stock OnDestroys)
template <class T> static FORCEINLINE void DestroyChild(NCWnd* Parent, T*& Child)
{
    if (Child)
    {
        Parent->RemoveChildWindow(Child->Entry);
        delete Child;
        Child = NULL;
    }
}

// the siege window a tab belongs to
static FORCEINLINE NCSiegeInfoMainWnd* MainOf(NCWnd* Tab)
{
    return (NCSiegeInfoMainWnd*)Tab->Entry->Parent->Wnd;
}

// a cell's text, NULL when empty (inlined in the stock code)
static FORCEINLINE const TCHAR* CellText(NListCell& Cell)
{
    return (Cell.Text && Cell.Text[0]) ? Cell.Text : NULL;
}

/*-----------------------------------------------------------------------------
    NSiegeCastleInfo, NSiegeClanInfo.
-----------------------------------------------------------------------------*/

// 0x1010a210
__declspec(noinline) NSiegeCastleInfo& NSiegeCastleInfo::operator=(const NSiegeCastleInfo& Other)
{
    appStrcpy(AllianceName, Other.AllianceName);
    appStrcpy(LordName, Other.LordName);
    appStrcpy(ClanName, Other.ClanName);
    CastleID = Other.CastleID;
    bIsOwner = Other.bIsOwner;
    PledgeID = Other.PledgeID;
    AllianceID = Other.AllianceID;
    CurrentTime = Other.CurrentTime;
    SiegeTime = Other.SiegeTime;
    return *this;
}
NW_HOOK_METHOD(0x1010a210, NSiegeCastleInfo, operator=)

// 0x1010a2a0
__declspec(noinline) NSiegeClanInfo::NSiegeClanInfo(const NSiegeClanInfo& Other)
{
    appStrcpy(ClanName, Other.ClanName);
    appStrcpy(LeaderName, Other.LeaderName);
    appStrcpy(AllianceName, Other.AllianceName);
    appStrcpy(AllianceLeader, Other.AllianceLeader);
    PledgeID = Other.PledgeID;
    CrestID = Other.CrestID;
    ApplyTime = Other.ApplyTime;
    Type = Other.Type;
    AllianceID = Other.AllianceID;
}
NW_HOOK_CTOR_ARGS(0x1010a2a0, NSiegeClanInfo, (const NSiegeClanInfo& A), (A))

/*-----------------------------------------------------------------------------
    NCSiegeInfoListCtrl.
-----------------------------------------------------------------------------*/

// 0x1010c430 - NCListCtrl's constructor with the scroll bar (0), the siege list's vtable
NCSiegeInfoListCtrl::NCSiegeInfoListCtrl() : NCListCtrl(0)
{
}
NW_HOOK_CTOR(0x1010c430, NCSiegeInfoListCtrl)

// 0x1010c4d0 (ret 0x10) - the sized one: NCListCtrl's with the scroll bar (0)
NCSiegeInfoListCtrl::NCSiegeInfoListCtrl(INT InMaxRows, INT InVisibleRows, INT InMaxColumns, INT Unused)
    : NCListCtrl(InMaxRows, InVisibleRows, InMaxColumns, Unused, 0)
{
}
NW_HOOK_CTOR_ARGS(0x1010c4d0, NCSiegeInfoListCtrl, (INT A, INT B, INT C, INT D), (A, B, C, D))

// 0x100f7310 (vslot 70) - the slot of NCListCtrl's hit test: the override below runs at this address
#if NW_HOOK_MODE
NW_NAKED void NCSiegeInfoListCtrl::NewVirtual76() NW_STOCK(0x10109f80)
#else
// the slot takes (X, Y) and gives the row (ret 8: HitTestRow below); the header declares it without arguments (NCListCtrl's
// placeholder), so the slot IS HitTestRow (the stock slot holds 0x10109f80 itself): the arguments, `this` and the `ret 8` are its own
NW_SLOT_IS("?NewVirtual76@NCSiegeInfoListCtrl@@UAEXXZ", "?HitTestRow@NCSiegeInfoListCtrl@@QAEHHH@Z")   // NCSiegeInfoListCtrl::NewVirtual76 = NCSiegeInfoListCtrl::HitTestRow (0x10109f80)
#endif

// 0x10109f80 (name ours, ret 8) - the row under a screen point (-1 outside the list and below the last row): the
// rows start under the header and the text offset, 13 pixels plus the line gap high
INT NCSiegeInfoListCtrl::HitTestRow(INT X, INT Y)
{
    INT RX = X - ScreenX;
    INT RY = Y - ScreenY;
    INT Result = -1;
    if (RX < 0)
        return -1;
    INT W = (INT)Width;
    if (RX > W)
        return -1;
    INT Top = HeaderHeight;
    if (RY < Top)
        return -1;
    if (RY > (INT)Height)
        return -1;
    INT Row = TopRow;
    INT Last = VisibleRows + TopRow;
    for (INT i = 0; Row < Last; i++, Row++)
    {
        if (Row >= NumRows)
            break;
        if (RX < W)
        {
            INT RowHeight = LineGap + 13;
            if (RY >= RowHeight * i + TextY + Top && RY < (i + 1) * RowHeight + TextY + Top)
            {
                Result = Row;
                break;
            }
        }
    }
    return Result;
}
NW_HOOK_METHOD(0x10109f80, NCSiegeInfoListCtrl, HitTestRow)

// 0x1010c480
NW_HOOK_DTOR(0x1010c480, NCSiegeInfoListCtrl)

// 0x100a4c10  the stock body is NCMatchListCtrl's (0x100b1460): NCListCtrl's WindowProc as a flag
#if NW_HOOK_MODE
NW_NAKED INT NCSiegeInfoListCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x100b1460)
#else
INT NCSiegeInfoListCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCListCtrl::WindowProc(Msg, wParam, lParam) != 0;
}
#endif

// (the console's crests by the clan's / alliance's id, 0x1006A900 / 0x1006A910, are NConsoleWnd::GetPledgeCrestTex /
// GetAllyCrestTex in NConsoleWndNet.cpp)

// 0x1010a400 (vslot 56) - NCListCtrl's paint (the rows are 13 high plus the line gap here, the band texture under
// the odd rows only), but the first column starts with the clan's crest and, on the attacking side, the second one
// with the alliance's; the cell icons are not drawn
INT NCSiegeInfoListCtrl::OnPaint(NCGDevice* Device)
{
    guard(NCSiegeInfoListCtrl::OnPaint);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);
    INT Line = 0;
    for (INT i = TopRow; i < TopRow + VisibleRows && i <= NumRows && i < Rows.Num(); i++, Line++)
    {
        Device->SetClip(ScreenX, ScreenY + HeaderHeight, (INT)Width, (INT)(Height - (FLOAT)HeaderHeight));
        if (i % 2 == 1)
            Device->DrawTexture(0, (LineGap + 13) * Line + TextY + HeaderHeight - 1, (INT)Width, 15, 0, 0, 8, 15,
                                LineTex, 0xFF, 1);
        if (i == Selected)
        {
            Device->DrawStretchedTex(0, (LineGap + 13) * Line + TextY + HeaderHeight, (INT)(Width - 47.0f - 2.0f), 13,
                                     0, 0, 16, 13, SelectTex, 0xFF);
            Device->DrawStretchedTex((INT)(Width - 47.0f - 2.0f), (LineGap + 13) * Line + TextY + HeaderHeight, 32, 13,
                                     0, 0, 32, 13, SelectTex2, 0xFF);
        }
        Device->ReleaseClip();
        INT X = TextX;
        NListRow* Row = Rows(i);
        for (INT c = 0; c < Columns.Num(); c++)
        {
            NCHeaderCtrl* Column = Columns(c);
            INT Y = (LineGap + 13) * Line + TextY + HeaderHeight + 1;
            Device->SetClip(ScreenX + X, ScreenY + Y - 1, Column->ColumnWidth, 15);
            if (c == 0)
            {
                UTexture* Crest = GConsole->GetPledgeCrestTex(Row->Param1);   // C4: slot 45 (0x1006A900)
                if (Crest)
                    Device->DrawTexture(0, Y, 16, 12, 0, 4, 16, 12, Crest, 0xFF, 1);
                Device->DrawText(17, Y, Row->Cells[0].Color, Row->Cells[0].Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                                 0, 0);
            }
            else if (Row->Cells[0].Param2 == 0)
            {
                UTexture* Crest = GConsole->GetAllyCrestTex(Row->Param1);   // C4: slot 46 (0x1006A910)
                if (Crest)
                    Device->DrawTexture(Row->Cells[c].IconW + X, Y, 8, 12, 0, 4, 8, 12, Crest, 0xFF, 1);
                Device->DrawText(X + 9, Y, Row->Cells[c].Color, Row->Cells[c].Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                                 0, 0, 0);
            }
            else
            {
                Device->DrawText(X, Y, Row->Cells[c].Color, Row->Cells[c].Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0,
                                 0, 0);
            }
            X += Column->ColumnWidth;
            Device->ReleaseClip();
        }
    }
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1010a400, NCSiegeInfoListCtrl, OnPaint)

// (C2's DrawRowToolTip, 0x100ab530, is gone from this client: no slot, no caller)

// 0x1010a330 - the clan's id in the selected row's first parameter, 0 without a row
INT NCSiegeInfoListCtrl::GetSelectedPledgeID()
{
    if (Selected >= 0 && Selected < Rows.Num() && Rows(Selected))
        return Rows(Selected)->Param1;
    return 0;
}
NW_HOOK_METHOD(0x1010a330, NCSiegeInfoListCtrl, GetSelectedPledgeID)

// 0x1010a360 - the selected row's status text (its second cell): 3 when it is the system string 567, 2 for 568, else -1
INT NCSiegeInfoListCtrl::GetSelectedStatus()
{
    if (Selected >= 0 && Selected < Rows.Num() && Rows(Selected))
    {
        NListRow* Row = Rows(Selected);
        const TCHAR* Text = Row->Cells[1].Text;
        if (!Text || !*Text)
            Text = NULL;
        if (appStrcmp(*NCObject::SysString(567), Text) == 0)
            return 3;
        Text = Row->Cells[1].Text;
        if (!Text || !*Text)
            Text = NULL;
        if (appStrcmp(*NCObject::SysString(568), Text) == 0)
            return 2;
    }
    return -1;
}
NW_HOOK_METHOD(0x1010a360, NCSiegeInfoListCtrl, GetSelectedStatus)

NW_CHECK_VTABLE(NCSiegeInfoListCtrl, 0x101cc1f8, 82)

/*-----------------------------------------------------------------------------
    NCSiegePartInfoWnd.
-----------------------------------------------------------------------------*/

// 0x1010c500
NCSiegePartInfoWnd::NCSiegePartInfoWnd()
{
    List = NULL;
    Side = SIEGESIDE_Attacker;
    BackTex = FindTexture(TEXT("L2UI_ch3.SiegeWnd.siege_back32"));
}
NW_HOOK_CTOR(0x1010c500, NCSiegePartInfoWnd)

// 0x1010c5b0
NW_HOOK_DTOR(0x1010c5b0, NCSiegePartInfoWnd)

// 0x1010a080 (vslot 25) - the four buttons; C2 does not test IsEnabled first
INT NCSiegePartInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_SIEGE_APPLY)
            return OnApply() != 0;
        if ((INT)wParam == NID_SIEGE_CANCELAPPLY)
            return OnCancelApply() != 0;
        if ((INT)wParam == NID_SIEGE_APPROVE)
            return OnApprove() != 0;
        if ((INT)wParam == NID_SIEGE_REFUSE)
            return OnRefuse() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1010a080, NCSiegePartInfoWnd, WindowProc)

// 0x1010c610 (vslot 37) - the clan list (256 rows kept, 12 seen, 2 columns) and the four buttons; C3: the
// buttons are 76 x 23 and the apply / cancel / refuse ones share a place
INT NCSiegePartInfoWnd::OnCreate()
{
    List = NewSiegeList(256, 12, 2, 15);
    List->SetPosition(7, 18, 242, 200);
    CreateChild(List, NWS_VISIBLE | NWS_UNKNOWN20);   // 0x22
    List->SetTextOffset(0, 3);

    ApplyButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    ApplyButton->ID = NID_SIEGE_APPLY;
    ApplyButton->SetPosition(158, 224, 76, 23);
    CreateChild(ApplyButton, NWS_VISIBLE);
    ApplyButton->SetTextID(STR_Apply);

    CancelApplyButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelApplyButton->ID = NID_SIEGE_CANCELAPPLY;
    CancelApplyButton->SetPosition(158, 224, 76, 23);
    CreateChild(CancelApplyButton, NWS_VISIBLE);
    CancelApplyButton->SetTextID(STR_CancelApply);

    ApproveButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    ApproveButton->ID = NID_SIEGE_APPROVE;
    ApproveButton->SetPosition(80, 224, 76, 23);
    CreateChild(ApproveButton, NWS_VISIBLE);
    ApproveButton->SetTextID(STR_Approve);

    RefuseButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    RefuseButton->ID = NID_SIEGE_REFUSE;
    RefuseButton->SetPosition(158, 224, 76, 23);
    CreateChild(RefuseButton, NWS_VISIBLE);
    RefuseButton->SetTextID(STR_Cancel);
    return 1;
}
NW_HOOK_METHOD(0x1010c610, NCSiegePartInfoWnd, OnCreate)

// 0x1010bcc0 (vslot 38) - NCWnd's first; the clans' copies are freed without a destructor, then the list and the
// four buttons go
INT NCSiegePartInfoWnd::OnDestroy()
{
    NCWnd::OnDestroy();
    for (INT i = 0; i < Clans.Num(); i++)
    {
        if (Clans(i))
            appFree(Clans(i));
    }
    Clans.Empty();
    DestroyChild(this, List);
    DestroyChild(this, ApplyButton);
    DestroyChild(this, CancelApplyButton);
    DestroyChild(this, ApproveButton);
    DestroyChild(this, RefuseButton);
    return 1;
}
NW_HOOK_METHOD(0x1010bcc0, NCSiegePartInfoWnd, OnDestroy)

// 0x1010a850 (vslot 62) - the background and, on top, the number of clans: "name : n" for the attackers,
// "name/name : approved/waiting" for the defenders
INT NCSiegePartInfoWnd::OnPaint(NCGDevice* Device)
{
    TCHAR Text[128];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    INT H = (INT)Height;
    INT W = (INT)Width;
    Device->DrawTexture(0, 0, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    if (Side == SIEGESIDE_Attacker)
        appSprintf(Text, TEXT("%s : %d"), *GL2GameData.SysString[STR_AttackingClans], Clans.Num());
    else
        appSprintf(Text, TEXT("%s/%s : %d/%d"), *GL2GameData.SysString[STR_DefendingClans],
                   *GL2GameData.SysString[STR_WaitingClans], NumApproved, NumWaiting);
    Device->DrawText(14, 2, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x1010a850, NCSiegePartInfoWnd, OnPaint)

// 0x1010ac40 (vslot 74) - asks before applying: the dialog depends on the side (the message ids 0x29C / 0x29B, the
// dialog ids 0x40 / 0x3E), with the castle's name and id; the owner is not set
INT NCSiegePartInfoWnd::OnApply()
{
    NDlg Info;
    INT MsgID;
    if (Side == SIEGESIDE_Defender)
    {
        Info.ID = 0x40;
        MsgID = 0x29C;
    }
    else
    {
        Info.ID = 0x3E;
        MsgID = 0x29B;
    }
    Info.SetSystemMessage(MsgID);
    Info.bMainBox = 1;
    Info.bInput = 0;
    Info.SetButtons(GConsole->GetCastleName(CastleID), NULL);
    Info.SetParams(CastleID, -1);
    GConsole->ShowDialog(&Info);
    return 1;
}
NW_HOOK_METHOD(0x1010ac40, NCSiegePartInfoWnd, OnApply)

// 0x1010ad20 (vslot 75) - asks before cancelling the application (dialog id 0x41 for the defenders, else 0x3F)
INT NCSiegePartInfoWnd::OnCancelApply()
{
    NDlg Info;
    Info.bMainBox = 1;
    Info.bInput = 0;
    Info.ID = (Side == SIEGESIDE_Defender) * 2 + 0x3F;
    Info.SetSystemMessage(0x29D);
    Info.SetButtons(GConsole->GetCastleName(CastleID), NULL);
    Info.SetParams(CastleID, -1);
    GConsole->ShowDialog(&Info);
    return 1;
}
NW_HOOK_METHOD(0x1010ad20, NCSiegePartInfoWnd, OnCancelApply)

// 0x1010ae00 (vslot 76) - the owner approves the selected waiting clan (status 2): asks first (dialog id 0x42,
// message 0x29F), with the clan's name and the castle id with the clan id
INT NCSiegePartInfoWnd::OnApprove()
{
    if (List->GetSelectedPledgeID() > 0 && List->GetSelectedStatus() == 2)
    {
        PledgeInfo* Pledge = GConsole->GetPledgeInfo(List->GetSelectedPledgeID());
        if (Pledge)
        {
            NDlg Info;
            Info.ID = 0x42;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.SetSystemMessage(0x29F);
            const TCHAR* PledgeName = Pledge->Name;
            if (PledgeName && *PledgeName)
                Info.SetButtons(PledgeName, NULL);
            Info.SetParams(CastleID, List->GetSelectedPledgeID());
            GConsole->ShowDialog(&Info);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1010ae00, NCSiegePartInfoWnd, OnApprove)

// 0x1010af20 (vslot 77) - the same to refuse a waiting or approved clan (status 2 or 3: dialog id 0x43, message 0x29E)
INT NCSiegePartInfoWnd::OnRefuse()
{
    if (List->GetSelectedPledgeID() > 0)
    {
        if (List->GetSelectedStatus() != 2 && List->GetSelectedStatus() != 3)
            return 1;
        PledgeInfo* Pledge = GConsole->GetPledgeInfo(List->GetSelectedPledgeID());
        if (Pledge)
        {
            NDlg Info;
            Info.ID = 0x43;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.SetSystemMessage(0x29E);
            const TCHAR* PledgeName = Pledge->Name;
            if (PledgeName && *PledgeName)
                Info.SetButtons(PledgeName, NULL);
            Info.SetParams(CastleID, List->GetSelectedPledgeID());
            GConsole->ShowDialog(&Info);
        }
    }
    return 1;
}
NW_HOOK_METHOD(0x1010af20, NCSiegePartInfoWnd, OnRefuse)

// the three caps of a header, normal and pushed (the stock code spells them out at each column)
static FORCEINLINE void SetColumnCaps(NListColumnInfo* Column)
{
    Column->SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab1"), 0);
    Column->SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab1_down"), 0);
    Column->SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab2"), 1);
    Column->SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab2_down"), 1);
    Column->SetNormalTex(TEXT("L2UI_CH3.ListCtrl.tab3"), 2);
    Column->SetPushedTex(TEXT("L2UI_CH3.ListCtrl.tab3_down"), 2);
}

// 0x1010a9c0 - the list's columns: the clan's name, then the alliance's name (attackers) or the status (defenders).
// The second column sorts as numbers on both sides. C4: both columns are built inline (the out-of-line constructor of
// C3 is gone) and the titles are the system strings' ids
void NCSiegePartInfoWnd::SetSide(INT InSide)
{
    Side = InSide;

    NListColumnInfo NameColumn;
    NameColumn.Width = 130;
    NameColumn.Height = 19;
    NameColumn.SetTitleID(STR_ClanName);
    SetColumnCaps(&NameColumn);
    List->AddColumn(0, &NameColumn);

    NListColumnInfo SecondColumn;
    SecondColumn.Width = 112;
    SecondColumn.Height = 19;
    SecondColumn.bNumeric = 1;
    if (Side == SIEGESIDE_Attacker)
        SecondColumn.SetTitleID(STR_AllianceName);
    else
        SecondColumn.SetTitleID(STR_Status);
    SetColumnCaps(&SecondColumn);
    List->AddColumn(0, &SecondColumn);
}
NW_HOOK_METHOD(0x1010a9c0, NCSiegePartInfoWnd, SetSide)

// 0x1010b050 - apply or cancel by whether the player's clan is listed; the owner gets
// neither, and approves or refuses the defenders instead
__declspec(noinline) void NCSiegePartInfoWnd::UpdateButtons()
{
    bListed = 1;
    if (Side == SIEGESIDE_Attacker)
    {
        if (bRegistered)
        {
            ApplyButton->HideWindow();
            CancelApplyButton->ShowWindow();
        }
        else
        {
            ApplyButton->ShowWindow();
            CancelApplyButton->HideWindow();
        }
        if (MainOf(this)->Castle.bIsOwner)
        {
            ApplyButton->HideWindow();
            CancelApplyButton->HideWindow();
        }
        ApproveButton->HideWindow();
        RefuseButton->HideWindow();
    }
    else
    {
        if (bRegistered)
        {
            ApplyButton->HideWindow();
            CancelApplyButton->ShowWindow();
        }
        else
        {
            ApplyButton->ShowWindow();
            CancelApplyButton->HideWindow();
        }
        if (MainOf(this)->Castle.bIsOwner)
        {
            ApplyButton->HideWindow();
            CancelApplyButton->HideWindow();
            ApproveButton->ShowWindow();
            RefuseButton->ShowWindow();
        }
        else
        {
            ApproveButton->HideWindow();
            RefuseButton->HideWindow();
        }
    }
}
NW_HOOK_METHOD(0x1010b050, NCSiegePartInfoWnd, UpdateButtons)

// 0x1010be60 - the window closing or showing another castle
__declspec(noinline) void NCSiegePartInfoWnd::Reset()
{
    for (INT i = 0; i < Clans.Num(); i++)
    {
        if (Clans(i))
            appFree(Clans(i));
    }
    Clans.Empty();
    List->ClearRows();
    bRegistered = 0;
    NumApproved = 0;
    NumWaiting = 0;
    bListed = 0;
    CastleID = 0;
}
NW_HOOK_METHOD(0x1010be60, NCSiegePartInfoWnd, Reset)

// 0x1010bee0 - the console, before a new list: the copies of the clans are dropped without
// being freed (they leak, as in the stock code)
void NCSiegePartInfoWnd::ClearList()
{
    Clans.Empty();
    List->ClearRows();
    bRegistered = 0;
    NumApproved = 0;
    NumWaiting = 0;
}
NW_HOOK_METHOD(0x1010bee0, NCSiegePartInfoWnd, ClearList)

// 0x1010bf20 - the clan's row (its data in the header) and a copy of the record. On the defending side the owner
// counts as waiting, and "refused" is the system string 579. The player's own clan turns apply into cancel
void NCSiegePartInfoWnd::AddClan(NSiegeClanInfo* Info)
{
    NListRow* Row = new NListRow(2);
    if (Side == SIEGESIDE_Attacker)
    {
        Row->Cells[0].SetText(Info->ClanName);
        Row->Cells[0].SetParams(Info->ApplyTime, 0, -1);
        Row->Cells[1].SetText(Info->AllianceName);
    }
    else
    {
        Row->Cells[0].SetText(Info->ClanName);
        Row->Cells[0].SetParams(Info->ApplyTime, 1, -1);
        switch (Info->Type)
        {
        case SIEGECLAN_Owner:
            Row->Cells[1].SetText(*GL2GameData.SysString[STR_Lord]);
            NumWaiting++;
            break;
        case SIEGECLAN_Waiting:
            Row->Cells[1].SetText(*GL2GameData.SysString[STR_Waiting]);
            NumWaiting++;
            break;
        case SIEGECLAN_Approved:
            Row->Cells[1].SetText(*GL2GameData.SysString[STR_Approved]);
            NumApproved++;
            break;
        case SIEGECLAN_Refused:
            Row->Cells[1].SetText(*GL2GameData.SysString[STR_Refused]);
            break;
        }
    }
    Row->Cells[0].SetText2(Info->LeaderName);
    Row->Cells[1].SetText2(Info->AllianceLeader);
    Row->SetText(Info->AllianceName);
    Row->SetParams(Info->PledgeID, Info->CrestID, -1);
    List->AddRow(Row);
    Clans.AddItem(new NSiegeClanInfo(*Info));
    if (Info->PledgeID == GConsole->GetMyUser()->PledgeID)
        bRegistered = 1;
}
NW_HOOK_METHOD(0x1010bf20, NCSiegePartInfoWnd, AddClan)

NW_CHECK_VTABLE(NCSiegePartInfoWnd, 0x101cc380, 78)

/*-----------------------------------------------------------------------------
    NCSiegeDateInfoWnd.
-----------------------------------------------------------------------------*/

// 0x1010c9d0
NCSiegeDateInfoWnd::NCSiegeDateInfoWnd()
{
    BackTex = FindTexture(TEXT("L2UI_ch3.SiegeWnd.siege_back31"));
}
NW_HOOK_CTOR(0x1010c9d0, NCSiegeDateInfoWnd)

// 0x1010ca70 - the dates' array goes (TIntArray's destructor, 0x100231d0), then NCWnd's
NW_HOOK_DTOR(0x1010ca70, NCSiegeDateInfoWnd)


// 0x1010a130 (vslot 25) - the combo box picks a date (lParam = the item); the confirmation box answers with
// NWM_SIEGE_DATE_OK. C2 does not test IsEnabled first
INT NCSiegeDateInfoWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_SIEGE_TIMECOMBO)
            return OnSelectTime(lParam) != 0;
    }
    else if (Msg == NWM_SIEGE_DATE_OK)
        return OnConfirmTime(wParam, lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1010a130, NCSiegeDateInfoWnd, WindowProc)

// 0x1010c160 (vslot 37) - the combo box of the dates
INT NCSiegeDateInfoWnd::OnCreate()
{
    TimeCombo = new NCComboBox();
    TimeCombo->ID = NID_SIEGE_TIMECOMBO;
    TimeCombo->SetPosition(80, 60, 126, 15);
    CreateChild(TimeCombo, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1010c160, NCSiegeDateInfoWnd, OnCreate)

// 0x1010b190 (vslot 38) - guarded in C2; NCWnd's first, then the combo box goes
INT NCSiegeDateInfoWnd::OnDestroy()
{
    guard(NCSiegeDateInfoWnd::OnDestroy);
    NCWnd::OnDestroy();
    DestroyChild(this, TimeCombo);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1010b190, NCSiegeDateInfoWnd, OnDestroy)

// 0x1010b240 (vslot 62) - the current time and the siege's: undecided while none is set and the player's clan
// does not own the castle
INT NCSiegeDateInfoWnd::OnPaint(NCGDevice* Device)
{
    guard(NCSiegeDateInfoWnd::OnPaint);
    TCHAR Current[132];
    TCHAR Siege[128];
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    INT H = (INT)Height;
    INT W = (INT)Width;
    Device->DrawTexture(0, 0, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    GConsole->FormatDateTime(CurrentTime, Current);
    Device->DrawText(10, 30, 0xFFDCDCDC, *GL2GameData.SysString[STR_CurrentTime], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(80, 30, 0xFFDCDCDC, Current, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->DrawText(10, 50, 0xFFDCDCDC, *GL2GameData.SysString[STR_SiegeDate], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    const TCHAR* SiegeText = NULL;
    if (SiegeTime > 0)
    {
        GConsole->FormatDateTime(SiegeTime, Siege);
        SiegeText = Siege;
    }
    else if (SiegeTime == 0 && !MainOf(this)->Castle.bIsOwner)
        SiegeText = *GL2GameData.SysString[STR_Undecided];
    if (SiegeText)
        Device->DrawText(80, 50, 0xFFDCDCDC, SiegeText, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1010b240, NCSiegeDateInfoWnd, OnPaint)

// 0x1010b490 (vslot 74) - a date picked (not the automatic setting) is confirmed first; OK comes back as
// NWM_SIEGE_DATE_OK
INT NCSiegeDateInfoWnd::OnSelectTime(INT Index)
{
    if (Index > 0)
    {
        SelectedIndex = Index;
        NDlg Info;
        Info.ID = DIALOG_SIEGE_DATE;
        Info.Owner = Entry;
        Info.bMainBox = 1;
        Info.bInput = 0;
        Info.SetSystemMessage(MSG_ConfirmSiegeTime);
        Info.SetButtons(TimeCombo->GetItemText(Index), NULL);
        GConsole->ShowDialog(&Info);
    }
    return 1;
}
NW_HOOK_METHOD(0x1010b490, NCSiegeDateInfoWnd, OnSelectTime)

// 0x1010b560 (vslot 75) - the castle and the date chosen (0: the automatic setting)
INT NCSiegeDateInfoWnd::OnConfirmTime(WPARAM wParam, LPARAM lParam)
{
    L2ParamStack Params(10);
    Params.PushBack((void*)(INT_PTR)CastleID);
    Params.PushBack((void*)(INT_PTR)(SelectedIndex > 0 ? Times(SelectedIndex - 1) : 0));
    GConsole->RequestSetSiegeTime(Params);
    return 1;
}
NW_HOOK_METHOD(0x1010b560, NCSiegeDateInfoWnd, OnConfirmTime)

// 0x1010b430 - the combo box back to its one "Automatic setting" item
__declspec(noinline) void NCSiegeDateInfoWnd::Reset()
{
    Times.Empty();
    TimeCombo->ClearItems();
    TimeCombo->AddItem((INT)STR_AutoSetting);   // C4: the combo takes the system string id (AddItemID 0x10006800)
    CastleID = 0;
    SelectedIndex = 0;
    CurrentTime = 0;
    SiegeTime = 0;
    TimeCombo->SetSelected(0);
}
NW_HOOK_METHOD(0x1010b430, NCSiegeDateInfoWnd, Reset)

// 0x1010b600 - the combo box shows to the owner only, while no date is set
__declspec(noinline) void NCSiegeDateInfoWnd::SetTimes(INT InCurrentTime, INT InSiegeTime)
{
    CurrentTime = InCurrentTime;
    SiegeTime = InSiegeTime;
    if (InSiegeTime == 0)
    {
        if (MainOf(this)->Castle.bIsOwner)
            TimeCombo->ShowWindow();
        else
            TimeCombo->HideWindow();
    }
    else
        TimeCombo->HideWindow();
}
NW_HOOK_METHOD(0x1010b600, NCSiegeDateInfoWnd, SetTimes)

// 0x1010c210 - one more date the owner can choose (the console, from the server's offer)
void NCSiegeDateInfoWnd::AddTime(INT Time)
{
    TCHAR Text[128];
    GConsole->FormatDateTime(Time, Text);
    TimeCombo->AddItem(Text);
    Times.AddItem(Time);
}
NW_HOOK_METHOD(0x1010c210, NCSiegeDateInfoWnd, AddTime)

NW_CHECK_VTABLE(NCSiegeDateInfoWnd, 0x101cc4f8, 76)

/*-----------------------------------------------------------------------------
    NCSiegeInfoMainWnd.
-----------------------------------------------------------------------------*/

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x1010cad0 - the castle record empty (NSiegeCastleInfo's constructor, inline) and the two textures
NCSiegeInfoMainWnd::NCSiegeInfoMainWnd()
{
    BackTex = FindTexture(TEXT("L2UI_ch3.SiegeWnd.Siege_Back1"));
    SecondBackTex = FindTexture(TEXT("L2UI_CH3.SiegeWnd.Siege_Back2"));
}
NW_HOOK_CTOR(0x1010cad0, NCSiegeInfoMainWnd)

// 0x1010cbb0 - nothing of its own to free (the castle record is plain data)
NW_HOOK_DTOR(0x1010cbb0, NCSiegeInfoMainWnd)


// 0x1010a1b0 (vslot 25) - C2 has no IsEnabled check of its own
INT NCSiegeInfoMainWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_SIEGE_TABS)
        return OnTab((INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1010a1b0, NCSiegeInfoMainWnd, WindowProc)

// 0x1010cc00 (vslot 37) - NCWnd::OnCreate is not called; the title bar, the three tabs' windows (the date tab and
// the two sides) and the tab control that shows one of them
INT NCSiegeInfoMainWnd::OnCreate()
{
    FrameCtrl = NewFrameCtrl(1, 0, 0);
    FrameID = NID_SIEGE_FRAME;
    FrameCtrl->ID = NID_SIEGE_FRAME;
    FrameCtrl->SetPosition(0, 0, (DWORD)(INT)Width, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);

    DateInfo = new NCSiegeDateInfoWnd();
    DateInfo->SetPosition(0, 148, (DWORD)(INT)Width, 253);
    CreateChild(DateInfo, NWS_VISIBLE);

    AttackerInfo = new NCSiegePartInfoWnd();
    AttackerInfo->SetPosition(0, 148, (DWORD)(INT)Width, 253);
    CreateChild(AttackerInfo, NWS_VISIBLE);
    AttackerInfo->SetSide(SIEGESIDE_Attacker);
    AttackerInfo->HideWindow();

    DefenderInfo = new NCSiegePartInfoWnd();
    DefenderInfo->SetPosition(0, 148, (DWORD)(INT)Width, 253);
    CreateChild(DefenderInfo, NWS_VISIBLE);
    DefenderInfo->SetSide(SIEGESIDE_Defender);
    DefenderInfo->HideWindow();

    TabCtrl = new NCTabControl();
    TabCtrl->ID = NID_SIEGE_TABS;
    TabCtrl->SetPosition(12, 125, 224, 23);
    CreateChild(TabCtrl, NWS_VISIBLE);
    TabCtrl->AddTab(STR_SiegeInfo_TabDate, DateInfo, 74, 23, TEXT("L2UI_ch3.SiegeWnd.siege_Tab3"),
                    TEXT("L2UI_ch3.SiegeWnd.siege_Tab1"), 1, -1);
    TabCtrl->AddTab(STR_AttackingClans_Tab, AttackerInfo, 74, 23, TEXT("L2UI_ch3.SiegeWnd.siege_Tab3"),
                    TEXT("L2UI_ch3.SiegeWnd.siege_Tab2"), 1, -1);
    TabCtrl->AddTab(STR_DefendingClans_Tab, DefenderInfo, 74, 23, TEXT("L2UI_ch3.SiegeWnd.siege_Tab3"),
                    TEXT("L2UI_ch3.SiegeWnd.siege_Tab2"), 0, -1);
    return 1;
}
NW_HOOK_METHOD(0x1010cc00, NCSiegeInfoMainWnd, OnCreate)

// 0x1010b660 (vslot 38) - C2 guards it
INT NCSiegeInfoMainWnd::OnDestroy()
{
    guard(NCSiegeInfoMainWnd::OnDestroy);
    NCWnd::OnDestroy();
    DestroyChild(this, TabCtrl);
    DestroyChild(this, DateInfo);
    DestroyChild(this, AttackerInfo);
    DestroyChild(this, DefenderInfo);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1010b660, NCSiegeInfoMainWnd, OnDestroy)

// 0x1010c370 (vslot 48) - opens emptied on the date tab (the console's SetCastleInfo follows); C3: the tab
// control's SetTab takes a second argument (1)
void NCSiegeInfoMainWnd::ShowWindow()
{
    PlayInterfaceSound(SOUND_WindowOpen);
    NCWnd::ShowWindow();
    Reset();
    TabCtrl->SetTab(0, 1);   // 0x1002dd70
}
NW_HOOK_METHOD(0x1010c370, NCSiegeInfoMainWnd, ShowWindow)

// 0x1010c360 (vslot 49)
void NCSiegeInfoMainWnd::HideWindow()
{
    NCWnd::HideWindow();
    Reset();
}
NW_HOOK_METHOD(0x1010c360, NCSiegeInfoMainWnd, HideWindow)

// 0x1010b790 (vslot 62) - the castle's name and its owner: the lord, the clan (with its crest) and the alliance
INT NCSiegeInfoMainWnd::OnPaint(NCGDevice* Device)
{
    guard(NCSiegeInfoMainWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    Device->DrawTexture(0, 20, (INT)Width, 105, 0, 0, (INT)Width, 105, BackTex, 0xFF, 1);
    Device->DrawTexture(0, 125, (INT)Width, 23, 0, 0, (INT)Width, 23, SecondBackTex, 0xFF, 1);
    const TCHAR* CastleName = GConsole->GetCastleName(Castle.CastleID);
    if (CastleName)
    {
        SIZE Size;
        NCGDevice::GetTextExtent(&Size, CastleName, 0, 0, 0, 0, 0);
        Device->DrawText(120 - Size.cx / 2, 41, SIEGE_TITLE_COLOR, CastleName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0,
                         0, 0);
    }
    Device->DrawText(20, 70, SIEGE_LABEL_COLOR, *GL2GameData.SysString[STR_Lord], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(20, 86, SIEGE_LABEL_COLOR, *GL2GameData.SysString[STR_Clan], 0, 0, 0, L2FONT_Normal, 0, 0, 0,
                     0, 0, 0);
    Device->DrawText(20, 102, SIEGE_LABEL_COLOR, *GL2GameData.SysString[STR_Alliance], 0, 0, 0, L2FONT_Normal, 0, 0,
                     0, 0, 0, 0);
    const TCHAR* Text;
    INT X;
    INT Y;
    if (!Castle.LordName || !Castle.LordName[0])
    {
        Text = *GL2GameData.SysString[STR_NoCastleOwner];
        Y = 70;
        X = 80;
    }
    else
    {
        Device->DrawText(80, 70, 0xFFDCDCDC, Castle.LordName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        X = Device->DrawPledgeCrest(80, 86, Castle.PledgeID) ? 100 : 80;
        Device->DrawText(X, 86, 0xFFDCDCDC, Castle.ClanName, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
        Y = 102;
        Text = Castle.AllianceName;
        if (!Text || !Text[0])
        {
            Text = *GL2GameData.SysString[STR_NoAlliance];
            X = 80;
        }
        else
            X = Device->DrawAllyCrest(88, 102, Castle.PledgeID) ? 100 : 80;
    }
    Device->DrawText(X, Y, 0xFFDCDCDC, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1010b790, NCSiegeInfoMainWnd, OnPaint)

// 0x1010bb00 (vslot 74) - a side's list is asked for once (until Reset)
INT NCSiegeInfoMainWnd::OnTab(INT Tab)
{
    switch (Tab)
    {
    case 0:
        DateInfo->ShowWindow();
        AttackerInfo->HideWindow();
        DefenderInfo->HideWindow();
        DateInfo->SetTimes(Castle.CurrentTime, Castle.SiegeTime);
        break;
    case 1:
        if (!AttackerInfo->bListed)
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)(INT_PTR)Castle.CastleID);
            GConsole->RequestSiegeAttackerList(Params);
        }
        DateInfo->HideWindow();
        AttackerInfo->ShowWindow();
        DefenderInfo->HideWindow();
        AttackerInfo->UpdateButtons();
        break;
    case 2:
        if (!DefenderInfo->bListed)
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)(INT_PTR)Castle.CastleID);
            GConsole->RequestSiegeDefenderList(Params);
        }
        DateInfo->HideWindow();
        AttackerInfo->HideWindow();
        DefenderInfo->ShowWindow();
        DefenderInfo->UpdateButtons();
        break;
    }
    return 1;
}
NW_HOOK_METHOD(0x1010bb00, NCSiegeInfoMainWnd, OnTab)

// 0x1010c270 - the castle's data and the three tabs emptied
__declspec(noinline) void NCSiegeInfoMainWnd::Reset()
{
    Castle.Clear();
    AttackerInfo->Reset();
    DefenderInfo->Reset();
    DateInfo->Reset();
}
NW_HOOK_METHOD(0x1010c270, NCSiegeInfoMainWnd, Reset)

// 0x1010c2d0 - the console, with the server's siege info (after ShowWindow)
void NCSiegeInfoMainWnd::SetCastleInfo(NSiegeCastleInfo* Info)
{
    Reset();
    Castle = *Info;
    DateInfo->CastleID = Info->CastleID;
    AttackerInfo->CastleID = Info->CastleID;
    DefenderInfo->CastleID = Info->CastleID;
    DateInfo->SetTimes(Info->CurrentTime, Info->SiegeTime);
}
NW_HOOK_METHOD(0x1010c2d0, NCSiegeInfoMainWnd, SetCastleInfo)

// 0x1010c340
void NCSiegeInfoMainWnd::AddAttacker(NSiegeClanInfo* Info)
{
    AttackerInfo->AddClan(Info);
}
NW_HOOK_METHOD(0x1010c340, NCSiegeInfoMainWnd, AddAttacker)

// 0x1010c350
void NCSiegeInfoMainWnd::AddDefender(NSiegeClanInfo* Info)
{
    DefenderInfo->AddClan(Info);
}
NW_HOOK_METHOD(0x1010c350, NCSiegeInfoMainWnd, AddDefender)

NW_CHECK_VTABLE(NCSiegeInfoMainWnd, 0x101cc6a0, 75)
