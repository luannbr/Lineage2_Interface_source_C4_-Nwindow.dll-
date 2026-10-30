/*=============================================================================
    NCOlympiadPlayerWnd.cpp - the status window of one olympiad competitor (C4 only). Stock: 0x1005fc40 (constructor), 0x1005fce0
    (destructor), 0x1013ceb0 .. 0x1013d400+ (the methods), 0x1013d590 (OnPaint), 0x1013d920 (OnCreate, stock).
=============================================================================*/
#include "nwindow/NCOlympiadPlayerWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

// 0x10011740 (an anonymous function in the db, called only by this window's OnPaint) - the frame bar's expand state: +0x258, the
// bExpanded of NCFrameCtrl::Expand. A stand-in class so that it is hooked as a method (thiscall)
struct NFrameExpandFns : public NCFrameCtrl
{
    __declspec(noinline) INT IsExpanded();
};
INT NFrameExpandFns::IsExpanded()
{
    return Expand.bExpanded;
}
NW_HOOK_METHOD(0x10011740, NFrameExpandFns, IsExpanded)

// NWM_COMMAND id of the frame bar (its expand button reports to the window; names ours)
enum { NID_OLYMPIAD_FRAME = 0xAFDDCEA0 };

// 0x1005fc40 - NCWnd's constructor, then the name and the five messages empty
NCOlympiadPlayerWnd::NCOlympiadPlayerWnd()
{
}
NW_HOOK_CTOR(0x1005fc40, NCOlympiadPlayerWnd)

// 0x1005fce0 - ConditionalDestroy, the messages and the name, NCWnd's
NW_HOOK_DTOR(0x1005fce0, NCOlympiadPlayerWnd)

// 0x1013d920 (vslot 37, guard: the stock's string is NCManorInfoSettingWnd's, copied) - the frame bar (12 px wide, as high as the
// window) with its expand button (46 px collapsed, 126 expanded), the pieces of the skin, the four bar pictures; nobody yet
INT NCOlympiadPlayerWnd::OnCreate()
{
    guard(NCManorInfoSettingWnd::OnCreate);
    NCWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 1, 1);
    FrameID = NID_OLYMPIAD_FRAME;
    FrameCtrl->ID = NID_OLYMPIAD_FRAME;
    FrameCtrl->SetPosition(0, 0, 12, (INT)Height);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    FrameCtrl->CreateExpandBtn(0x2E, 0x7E);
    for (INT i = 0; i < 3; i++)
    {
        BackTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.SmallWnd.SmallWindow_Back%d"), i + 1));
        TallTex[i] = FindTexture(*FString::Printf(TEXT("L2UI_CH3.SmallWnd.SmallWindow3_Back%d"), i + 1));
    }
    CpBarTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.CpBar"));
    HpBarTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.HpBar"));
    CpBackTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.CpBar_back"));
    HpBackTex = FindTexture(TEXT("L2UI_CH3.SmallWnd.HpBar_back"));
    Name = TEXT("");
    MaxHP = 0;
    HP = 0;
    MaxCP = 0;
    CP = 0;
    ObjectID = -1;
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013d920, NCOlympiadPlayerWnd, OnCreate)

// 0x1013ceb0 (guard) - nobody yet: the player id -1, the numbers 0, the name and the messages empty
void NCOlympiadPlayerWnd::Initialize()
{
    guard(NCOlympiadPlayerWnd::Initialize);
    ObjectID = -1;
    Name = TEXT("");
    HP = 0;
    MaxHP = 0;
    CP = 0;
    MaxCP = 0;
    MsgNext = 0;
    for (INT i = 0; i < 5; i++)
        Msgs[i] = TEXT("");
    unguard;
}
NW_HOOK_METHOD(0x1013ceb0, NCOlympiadPlayerWnd, Initialize)

// 0x1013cf70 (guard) - ExOlympiadUserInfo's values in the order they come: the object id, the name, the class (dropped), HP, max HP,
// CP, max CP
void NCOlympiadPlayerWnd::OnOlympiadUserInfo(L2ParamStack& Params)
{
    guard(NCOlympiadPlayerWnd::OnOlympiadUserInfo);
    ObjectID = (INT)Params.Top();
    Name = (const TCHAR*)Params.Top();
    Params.Top();
    HP = (INT)Params.Top();
    MaxHP = (INT)Params.Top();
    CP = (INT)Params.Top();
    MaxCP = (INT)Params.Top();
    unguard;
}
NW_HOOK_METHOD(0x1013cf70, NCOlympiadPlayerWnd, OnOlympiadUserInfo)

// 0x1013d0f0 (an anonymous function in the db) - the competitor's object id. Hooked in NConsoleWndC4.cpp (the console lane's
// NOlympiadPlayerWndExt::GetPlayerID); this is the copy OnAttack calls, as the stock does
INT NCOlympiadPlayerWnd::GetPlayerID()
{
    return ObjectID;
}

// 0x1013d030 (guard) - the message goes in the ring and the ring turns (the string comes by value: this function frees it)
void NCOlympiadPlayerWnd::AddMsg(FString Msg)
{
    guard(NCOlympiadPlayerWnd::AddMsg);
    Msgs[MsgNext] = Msg;
    MsgNext = (MsgNext + 1) % 5;
    unguard;
}
NW_HOOK_METHOD(0x1013d030, NCOlympiadPlayerWnd, AddMsg)

// 0x1013d100 (vslot 38, guard) - NCWnd's OnDestroy
INT NCOlympiadPlayerWnd::OnDestroy()
{
    guard(NCOlympiadPlayerWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013d100, NCOlympiadPlayerWnd, OnDestroy)

// 0x1013d180 (guard) - the frame's expand button: when the bar opens (0xFFFF) or closes (0xFFFE) the target window of this side
// (the desktop's +0x21C for the first competitor, +0x220 for the second) moves 80 pixels down or up
INT NCOlympiadPlayerWnd::OnFrame(INT State)
{
    guard(NCOlympiadPlayerWnd::OnFrame);
    NCWnd* Desk = GConsole->Desktop;
    NCWnd* Target;
    INT Delta;
    if (State == 0xFFFF)
        Delta = 0x50;
    else if (State == 0xFFFE)
        Delta = -0x50;
    else
        return 1;
    if (::nw::Field<NCOlympiadPlayerWnd*>(Desk, 0x214) == this)
        Target = ::nw::Field<NCWnd*>(Desk, 0x21C);
    else if (::nw::Field<NCOlympiadPlayerWnd*>(Desk, 0x218) == this)
        Target = ::nw::Field<NCWnd*>(Desk, 0x220);
    else
        return 1;
    Target->MoveWindow(0, Delta);
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013d180, NCOlympiadPlayerWnd, OnFrame)

// 0x1013d270 (vslot 25)
INT NCOlympiadPlayerWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && wParam == NID_OLYMPIAD_FRAME)
        return OnFrame((INT)lParam) != 0;
    return 0;
}
NW_HOOK_METHOD(0x1013d270, NCOlympiadPlayerWnd, WindowProc)

// 0x1013d2d0 (guard) - a skill used (0 .. 1999): the console formats system message 46 with the skill as its parameter and the text
// goes in the ring
void NCOlympiadPlayerWnd::NotifyMagicSkillUse(INT SkillID)
{
    guard(NCOlympiadPlayerWnd::NotifyMagicSkillUse);
    if (SkillID >= 0 && SkillID <= 1999)
    {
        L2ParamStack Skill(10);
        Skill.PushBack((void*)4);
        Skill.PushBack((void*)(INT_PTR)SkillID);
        Skill.PushBack((void*)1);
        GConsole->AddSystemMessageParam(Skill);
        L2ParamStack Message(10);
        Message.PushBack((void*)0x2E);
        TCHAR Text[1024];
        ::nw::VirtualCall<void>(GConsole, 191, &Message, 1, Text, 0);
        AddMsg(FString(Text));
    }
    unguard;
}
NW_HOOK_METHOD(0x1013d2d0, NCOlympiadPlayerWnd, NotifyMagicSkillUse)

// 0x1013d400 (guard, ret 0x14) - an attack (the console tells both Olympiad windows): this competitor is the attacker (P5 set: system
// message 44), or the victim (P3 set: system message 42 with the attacker's name as its parameter; else P4 set: system message 111)
void NCOlympiadPlayerWnd::OnAttack(User* Attacker, User* Victim, INT P3, INT P4, INT P5)
{
    guard(NCOlympiadPlayerWnd::OnAttack);
    INT Me = GetPlayerID();
    if (Me == Attacker->ObjectID)
    {
        if (P5)
            AddMsg(FString(*GL2GameData.SystemMsg[44].Message));
        return;
    }
    if (Me != Victim->ObjectID)
        return;
    if (P3)
    {
        L2ParamStack Who(10);
        Who.PushBack((void*)0);
        Who.PushBack((void*)((BYTE*)Attacker + 0x1C));   // User::GetName's buffer
        GConsole->AddSystemMessageParam(Who);
        L2ParamStack Message(10);
        Message.PushBack((void*)0x2A);
        TCHAR Text[1024];
        ::nw::VirtualCall<void>(GConsole, 191, &Message, 1, Text, 0);
        AddMsg(FString(Text));
        return;
    }
    if (P4)
        AddMsg(FString(*GL2GameData.SystemMsg[111].Message));
    unguard;
}
NW_HOOK_METHOD(0x1013d400, NCOlympiadPlayerWnd, OnAttack)

// 0x1013d590 (vslot 62, guard) - the skin (collapsed: the three short pieces; expanded: the tall pieces, as high as the window),
// the empty bars and the filled part of each (326 pixels at most), the name and, expanded, the ring of messages from the oldest
INT NCOlympiadPlayerWnd::OnPaint(NCGDevice* Device)
{
    guard(NCOlympiadPlayerWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (!((NFrameExpandFns*)FrameCtrl)->IsExpanded())
    {
        if (BackTex[0])
            Device->DrawTexture(0xC, 0, 0x10, (INT)Height, 0, 0, 0x10, 0x2E, BackTex[0], 0xFF, 1);
        if (BackTex[1])
            Device->DrawTexture(0x1C, 0, (INT)(Width - 32.0f - 12.0f), (INT)Height, 0, 0, 0x10, 0x2E, BackTex[1], 0xFF, 1);
        if (BackTex[2])
            Device->DrawTexture((INT)(Width - 16.0f), 0, 0x10, (INT)Height, 0, 0, 0x10, 0x2E, BackTex[2], 0xFF, 1);
    }
    else
    {
        if (TallTex[0])
            Device->DrawTexture(0xC, 0, 0x10, (INT)Height, 0, 0, 0x10, (INT)Height, TallTex[0], 0xFF, 1);
        if (TallTex[1])
            Device->DrawTexture(0x1C, 0, (INT)(Width - 44.0f), (INT)Height, 0, 0, 0x10, (INT)Height, TallTex[1], 0xFF, 1);
        if (TallTex[2])
            Device->DrawTexture((INT)(Width - 16.0f), 0, 0x10, (INT)Height, 0, 0, 0x10, (INT)Height, TallTex[2], 0xFF, 1);
    }
    Device->DrawTexture(0x10, 0x1A, 0x146, 6, 0, 0, 8, 6, CpBackTex, 0xFF, 1);
    Device->DrawTexture(0x10, 0x21, 0x146, 6, 0, 0, 8, 6, HpBackTex, 0xFF, 1);
    if (MaxCP != 0)
        Device->DrawTexture(0x10, 0x1A, (CP * 0x146) / MaxCP, 6, 0, 0, 8, 6, CpBarTex, 0xFF, 1);
    if (MaxHP != 0)
        Device->DrawTexture(0x10, 0x21, (HP * 0x146) / MaxHP, 6, 0, 0, 8, 6, HpBarTex, 0xFF, 1);
    Device->DrawText(0x19, 6, 0xFFDCDCDC, *Name, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    if (((NFrameExpandFns*)FrameCtrl)->IsExpanded())
    {
        for (INT i = 0; i < 5; i++)
        {
            FString& Line = Msgs[(MsgNext + i) % 5];
            if (!(Line == TEXT("")))
                Device->DrawText(0x12, (i + 3) * 0xF, 0xFFDCDCDC, *Line, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
        }
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1013d590, NCOlympiadPlayerWnd, OnPaint)

NW_CHECK_VTABLE(NCOlympiadPlayerWnd, 0x101ae9c0, 74)
