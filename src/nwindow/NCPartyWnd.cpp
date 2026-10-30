/*=============================================================================
    NCPartyWnd.cpp - the party window. Stock (C4 addresses): 0x1010de50 .. 0x1010dec7, 0x1010ded0 .. 0x1010dee8,
    0x101105a0 .. 0x10110674, 0x101106b0 .. 0x101107bf, 0x10110820 .. 0x101109b8, 0x101109c0 .. 0x10110c80,
    0x10115630 .. 0x101158c0, 0x10117480 .. 0x101174e0, 0x10117530 .. 0x10117848.
=============================================================================*/
#include "nwindow/NCPartyWnd.h"
#include "nwindow/NCStatusWnd.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NWndCreate.h"

// NWM_COMMAND ids: the button that switches the members' buff icons, the right edge that widens the window
enum { NID_PARTY_TOGGLE = 0xFF0001F2, NID_PARTY_RESIZE = 0xFF0001F3 };

// title bar id
enum { NID_PARTY_FRAME = 0xFFA0B0 };

// the height of a member's window (SetPosition) and the step the members below move by
enum { PARTY_MEMBER_HEIGHT = 46 };

// 0x10117480 - C4: the count and the shown flag cleared, and ExtraWidth
NCPartyWnd::NCPartyWnd()
{
    bShown = 0;
    NumMembers = 0;
    ExtraWidth = 0;
}
NW_HOOK_CTOR(0x10117480, NCPartyWnd)

// 0x101174e0
NW_HOOK_DTOR(0x101174e0, NCPartyWnd)

// 0x1010de50 (vslot 25) - the buff button and (C4) the right edge: slot 75 is the toggle, slot 74 the resize
INT NCPartyWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_PARTY_TOGGLE)
            return OnToggle() != 0;
        if ((INT)wParam == NID_PARTY_RESIZE)
            return ::nw::VirtualCall<INT>(this, 74, (INT)lParam) != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x1010de50, NCPartyWnd, WindowProc)

// 0x10117530 (vslot 37) - C4: the title bar, the buff button, the right edge and eight member windows
INT NCPartyWnd::OnCreate()
{
    NCWnd::OnCreate();
    FrameTopTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar1"));
    FrameCenterTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar2"));
    FrameBottomTex = FindTexture(TEXT("L2UI_CH3.FrameCtrl.smallbar3"));

    BuffButton = NewButton(TEXT("L2UI_CH3.PartyWnd.party_buffbutton"), TEXT("L2UI_CH3.PartyWnd.party_buffbutton_down"), 0);
    BuffButton->SetPosition(1, 7, 11, 13);
    BuffButton->ID = (INT)NID_PARTY_TOGGLE;
    CreateChild(BuffButton, NWS_VISIBLE | 0x20);   // 0x20: meaning unknown
    BuffButton->SetToolTip(0x371, 1);

    FrameCtrl = NewFrameCtrl(0, 0, 0);
    FrameCtrl->SetPosition(0, 18, 12, (DWORD)(INT)Height);
    FrameCtrl->ID = NID_PARTY_FRAME;
    CreateChild(FrameCtrl, NWS_VISIBLE);

    ResizeFrame = NewResizeFrame(1);
    ResizeFrame->ID = (INT)NID_PARTY_RESIZE;
    ResizeFrame->SetPosition((INT)(Width - 10.0f), 0, 10, (DWORD)(INT)Height);
    CreateChild(ResizeFrame, NWS_VISIBLE | 0x20);

    for (INT i = 0; i < NUM_PARTY_SLOTS; i++)
    {
        Members[i] = new NCPartyStatusWnd(144);
        Members[i]->SetPosition(12, 0, 160, PARTY_MEMBER_HEIGHT);
        CreateChild(Members[i], NWS_VISIBLE);
    }
    bToggle = 0;
    return 1;
}
NW_HOOK_METHOD(0x10117530, NCPartyWnd, OnCreate)

// 0x101105a0 (vslot 38) - the base's first, then the members, the buff button and the right edge
INT NCPartyWnd::OnDestroy()
{
    guard(NCPartyWnd::OnDestroy);
    NCWnd::OnDestroy();
    for (INT i = 0; i < NUM_PARTY_SLOTS; i++)
        DestroyChild(this, Members[i]);
    DestroyChild(this, BuffButton);
    DestroyChild(this, ResizeFrame);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x101105a0, NCPartyWnd, OnDestroy)

// 0x101109c0 (vslot 48)
void NCPartyWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    UpdateLayout();
}
NW_HOOK_METHOD(0x101109c0, NCPartyWnd, ShowWindow)

// 0x101106b0 (vslot 62) - the left edge (top, center stretched, bottom) while there are members
INT NCPartyWnd::OnPaint(NCGDevice* Device)
{
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (NumMembers > 0)
    {
        if (FrameTopTex)
            Device->DrawTexture(0, 0, 12, 8, 0, 0, 12, 8, FrameTopTex, 0xFF, 1);
        if (FrameCenterTex)
            Device->DrawTexture(0, 8, 12, (INT)(Height - 16.0f), 0, 0, 12, 8, FrameCenterTex, 0xFF, 1);
        if (FrameBottomTex)
            Device->DrawTexture(0, (INT)(Height - 8.0f), 12, 8, 0, 0, 12, 8, FrameBottomTex, 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x101106b0, NCPartyWnd, OnPaint)

// 0x1010ded0 (vslot 75; name ours) - the members' windows read it (buff icons shown)
INT NCPartyWnd::OnToggle()
{
    bToggle = !bToggle;
    return 1;
}
NW_HOOK_METHOD(0x1010ded0, NCPartyWnd, OnToggle)

// 0x101109d0 (slot 74 of the stock vtable; see NCTargetStatusWnd::OnResizeImpl) - the right edge was dragged by Delta: the
// window grows or shrinks within 0 .. 202 pixels, the edge follows, the members are as wide as the window less 12 and
// their names grow with Delta
INT NCPartyWnd::OnResizeImpl(INT Delta)
{
    NRectF Rect = {0, 0, 0.0f, 0.0f};
    GetLocalRect(&Rect);
    MaxExtraWidth = 202;
    if (Delta > 0)
    {
        if (ExtraWidth >= MaxExtraWidth)
            return 1;
        INT NewExtra = ExtraWidth + Delta;
        ExtraWidth = NewExtra;
        if (NewExtra > MaxExtraWidth)
        {
            Delta += MaxExtraWidth - NewExtra;
            ExtraWidth = MaxExtraWidth;
        }
    }
    else
    {
        if (Delta >= 0)
            return 1;
        if (ExtraWidth <= 0)
            return 1;
        INT NewExtra = ExtraWidth + Delta;
        ExtraWidth = NewExtra;
        if (NewExtra < 0)
        {
            Delta -= NewExtra;
            ExtraWidth = 0;
        }
    }
    SetWindowSize((FLOAT)Delta + Rect.W, Rect.H);
    NRectF EdgeRect = {0, 0, 0.0f, 0.0f};
    ResizeFrame->GetLocalRect(&EdgeRect);
    ResizeFrame->SetPosition((INT)(Width - EdgeRect.W), 0, (DWORD)(INT)EdgeRect.W, (DWORD)(INT)EdgeRect.H);
    for (INT i = 0; i < NUM_PARTY_SLOTS; i++)
    {
        Members[i]->SetWindowSize(Width - 12.0f, 46.0f);
        Members[i]->UpdateStyle();
        NRectF NameRect = {0, 0, 0.0f, 0.0f};
        Members[i]->NameCtrl->GetLocalRect(&NameRect);
        Members[i]->NameCtrl->SetWindowSize((FLOAT)Delta + NameRect.W, NameRect.H);
    }
    return 1;
}
NW_HOOK_METHOD(0x101109d0, NCPartyWnd, OnResizeImpl)

// 0x10110820 - C4: the width stays (it can be widened); each member shown is 46 high and the ones below it move down
void NCPartyWnd::UpdateLayout()
{
    NRectF Rect = {0, 0, 0.0f, 0.0f};
    SetWindowSize(Width, 0.0f);
    GetLocalRect(&Rect);
    for (INT i = 0; i < NUM_PARTY_SLOTS; i++)
        Members[i]->SetScreenPos(12, 0, (const POINT*)&Rect);
    UBOOL bSized = 0;
    for (INT i = 0; i < NUM_PARTY_SLOTS; i++)
    {
        if (!Members[i]->IsEmpty())
        {
            Members[i]->ShowWindow();
            Members[i]->UpdateStyle();
            NRectF Cur = {0, 0, 0.0f, 0.0f};
            GetLocalRect(&Cur);
            if (!bSized)
            {
                SetWindowSize(Width, Cur.H + 46.0f);
                bSized = 1;
            }
            else
                SetWindowSize(Width, Cur.H + 46.0f);
            for (INT j = i + 1; j < NUM_PARTY_SLOTS; j++)
                Members[j]->OffsetWindow(0, PARTY_MEMBER_HEIGHT);
        }
        else
            Members[i]->HideWindow();
    }
    FrameCtrl->SetPosition(0, 18, 12, (DWORD)(INT)Height);
    NRectF EdgeRect = {0, 0, 0.0f, 0.0f};
    ResizeFrame->GetLocalRect(&EdgeRect);
    ResizeFrame->SetFrameSize((DWORD)(INT)EdgeRect.W, (DWORD)(INT)Height);
}
NW_HOOK_METHOD(0x10110820, NCPartyWnd, UpdateLayout)

// 0x10115630 (ret 4) - C4: the packet's parameters go to the first free member (C3: AddMember popped them)
void NCPartyWnd::AddMember(L2ParamStack& Params)
{
    ShowWindow();
    INT i;
    for (i = 0; i < NUM_PARTY_SLOTS; i++)
        if (Members[i]->IsEmpty())
            break;
    if (i >= NUM_PARTY_SLOTS)
        return;
    Members[i]->SetInfo(Params);
    NumMembers++;
    bShown = 1;
    UpdateLayout();
}
NW_HOOK_METHOD(0x10115630, NCPartyWnd, AddMember)

// 0x101156a0 - by name; the members below move up one place, buffs included: C4 hands each one's record over as a packet
// of parameters (the order SetInfo pops them in)
void NCPartyWnd::DeleteMember(INT MemberID, const TCHAR* Name)
{
    if (Name)
    {
        INT i;
        for (i = 0; i < NUM_PARTY_SLOTS; i++)
            if (Members[i]->IsName(Name))
                break;
        if (i >= NUM_PARTY_SLOTS)
            return;
        Members[i]->Clear();
        for (INT j = i + 1; j < NUM_PARTY_SLOTS; j++)
        {
            NCPartyStatusWnd* Src = Members[j];
            if (!Src->IsEmpty())
            {
                L2ParamStack Params(20);
                Params.PushBack((void*)Src->LeaderID);
                Params.PushBack((void*)Src->MemberID);
                Params.PushBack((void*)Src->LootRule);
                Params.PushBack(Src->Name);
                Params.PushBack((void*)Src->CP);
                Params.PushBack((void*)Src->MaxCP);
                Params.PushBack((void*)Src->HP);
                Params.PushBack((void*)Src->MaxHP);
                Params.PushBack((void*)Src->MP);
                Params.PushBack((void*)Src->MaxMP);
                Params.PushBack((void*)0);
                Params.PushBack((void*)Src->ClassID);
                Params.PushBack((void*)0);
                Params.PushBack((void*)0);
                Members[j - 1]->SetInfo(Params);
                for (INT k = 0; k < Members[j]->BuffSkillIDs.Num(); k++)
                    Members[j - 1]->AddBuff(Members[j]->BuffSkillIDs(k), 0);
                Members[j]->Clear();
            }
        }
        NumMembers--;
        if (NumMembers == 0)
        {
            bShown = 0;
            HideWindow();
        }
        UpdateLayout();
    }
}
NW_HOOK_METHOD(0x101156a0, NCPartyWnd, DeleteMember)

// 0x101158c0
void NCPartyWnd::DeleteAll()
{
    for (INT i = 0; i < NumMembers; i++)
        Members[i]->Clear();
    NumMembers = 0;
    bShown = 0;
    UpdateLayout();
    HideWindow();
}
NW_HOOK_METHOD(0x101158c0, NCPartyWnd, DeleteAll)

// 0x10115920 - the member with that id gets a buff icon
void NCPartyWnd::AddMemberBuff(INT MemberID, INT SkillID, INT Unused)
{
    for (INT i = 0; i < NumMembers; i++)
    {
        if (Members[i]->MemberID == MemberID)
        {
            Members[i]->AddBuff(SkillID, Unused);
            return;
        }
    }
}
NW_HOOK_METHOD(0x10115920, NCPartyWnd, AddMemberBuff)

// 0x10115980 - the member with that id loses its buff icons
void NCPartyWnd::ClearMemberBuffs(INT MemberID)
{
    for (INT i = 0; i < NumMembers; i++)
    {
        if (Members[i]->MemberID == MemberID)
        {
            Members[i]->ClearBuffs();
            return;
        }
    }
}
NW_HOOK_METHOD(0x10115980, NCPartyWnd, ClearMemberBuffs)

NW_CHECK_VTABLE(NCPartyWnd, 0x101ce700, 76)
