/*=============================================================================
    AlarmWnd.cpp - the messenger's alarm box (C3 only; C3 addresses). Stock: 0x10130AA0 (OnCreate), 0x10130AD0 (OnDestroy),
    0x10130B20 (OnTimer), 0x10130BE0 / 0x10130C40 (ShowWindow, HideWindow), 0x10131D20 (OnPaint), 0x101397E0 / 0x101398C0
    (constructor, destructor), 0x10139950 (OnLButtonDown).
=============================================================================*/
#include "nwindow/AlarmWnd.h"
#include "nwindow/Msn.h"
#include "nwindow/Msn1Pending.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NGameData.h"

enum
{
    ALARM_SlideTimerID = 0xA50,
    ALARM_HideTimerID = 0xA51,
    ALARM_SlideStep = 3,
    ALARM_HideDelay = 8000,
    ALARM_Online = 0,           // AlarmType: a person came in (the click opens the chat)
    ALARM_Message = 1,          // a message arrived (the click shows Target)
    ALARM_Mail = 2,             // a mail arrived
    ALARM_Type3 = 3,
    ALARM_Type4 = 4,
    SYSMSG_AlarmOnline = 1359,
    SYSMSG_AlarmMessage = 1326,
    SYSMSG_AlarmMail = 1360,
    SYSMSG_AlarmType3 = 1325,
    SYSMSG_AlarmType4 = 1361,
};

// 0x1015e150 - the owner and the protocol object are the arguments; the person, the subject and the message are built, the
// rest (but the type) is cleared
AlarmWnd::AlarmWnd(MsnWnd* InOwner, Msn* InMsn)
{
    Target = NULL;
    SlideTimer = 0;
    HideTimer = 0;
    SlideOffset = 0;
    BackTex = NULL;
    MsnObj = InMsn;
    Owner = InOwner;
}
NW_HOOK_CTOR_ARGS(0x1015e150, AlarmWnd, (MsnWnd* A, Msn* B), (A, B))

// 0x1015e230 - the destructor: the vtable, ConditionalDestroy, then the message (msn2's ~NMsnMessage 0x1014efe0), the subject and the person (msn2's
// ~NMsnFriend 0x10150ff0) in reverse. The hook waits for that lane (Msn1Pending.h; proved in the merged tree)
NW_HOOK_DTOR(0x1015e230, AlarmWnd)


// 0x10119f60 (vslot 25) - NControl's body, shared (hooked there)
#if NW_HOOK_MODE
NW_NAKED INT AlarmWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam) NW_STOCK(0x10148f10)
#else
INT AlarmWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)   // the standalone: NControl::WindowProc's body (0x10148f10)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}
#endif

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10155710 (vslot 37) - NCWnd's, then the picture
INT AlarmWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.Msn.msn_alarm"));
    return 1;
}
NW_HOOK_METHOD(0x10155710, AlarmWnd, OnCreate)

// 0x10155740 (vslot 38) - both timers stopped, then NCWnd's
INT AlarmWnd::OnDestroy()
{
    if (SlideTimer)
    {
        KillTimer(SlideTimer);
        SlideTimer = 0;
    }
    if (HideTimer)
    {
        KillTimer(HideTimer);
        HideTimer = 0;
    }
    NCWnd::OnDestroy();
    return 1;
}
NW_HOOK_METHOD(0x10155740, AlarmWnd, OnDestroy)

// 0x10155790 (vslot 36) - the slide timer moves the box up 3 pixels until it has risen by its height and 44; then it stops
// and the 8 second timer starts. When that one rings, the box goes
INT AlarmWnd::OnTimer(INT TimerID)
{
    if (TimerID == ALARM_SlideTimerID)
    {
        if (SlideOffset < (INT)Height + 0x2C)
        {
            MoveWindow(0, -ALARM_SlideStep);
            SlideOffset += ALARM_SlideStep;
            return 1;
        }
        if (SlideTimer)
        {
            KillTimer(SlideTimer);
            SlideTimer = 0;
        }
        if (!HideTimer)
        {
            HideTimer = SetTimer(ALARM_HideTimerID, ALARM_HideDelay, NULL);
            return 1;
        }
    }
    else if (TimerID == ALARM_HideTimerID)
    {
        if (HideTimer)
        {
            KillTimer(HideTimer);
            HideTimer = 0;
        }
        HideWindow();
    }
    return 1;
}
NW_HOOK_METHOD(0x10155790, AlarmWnd, OnTimer)

// 0x10155850 (vslot 48) - NCWnd's, the slide starts, and the protocol object plays the sound of types 0 and 2
void AlarmWnd::ShowWindow()
{
    NCWnd::ShowWindow();
    SlideOffset = 0;
    SlideTimer = SetTimer(ALARM_SlideTimerID, 1, NULL);
    if (AlarmType == ALARM_Online)
        MsnObj->PlayAlarmSound(0);                                       // Msn::PlayAlarmSound 0x1014fca0
    else if (AlarmType == ALARM_Mail)
        MsnObj->PlayAlarmSound(2);
}
NW_HOOK_METHOD(0x10155850, AlarmWnd, ShowWindow)

// 0x101558b0 (vslot 49) - NCWnd's, the timers stopped, and the box moved back down to where it started
void AlarmWnd::HideWindow()
{
    NCWnd::HideWindow();
    if (SlideTimer)
    {
        KillTimer(SlideTimer);
        SlideTimer = 0;
    }
    if (HideTimer)
    {
        KillTimer(HideTimer);
        HideTimer = 0;
    }
    MoveWindow(0, SlideOffset);
    SlideOffset = 0;
}
NW_HOOK_METHOD(0x101558b0, AlarmWnd, HideWindow)

// the person's display name, by value (the stock builds it into the argument slot)
static FString PersonName(NMsnPersonRec& P)
{
    FString Result;
    P.GetDisplayName(&Result);
    return Result;
}

// 0x101569b0 (vslot 56) - the picture over the whole window, then the text of the alarm fitted in the box: its first line
// at (11, 16), 22 less than the width, 32 less than the height
INT AlarmWnd::OnPaint(NCGDevice* Device)
{
    TCHAR Text[2048];
    TCHAR Out[2048];
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    if (BackTex)
    {
        INT H = (INT)Height;
        INT W = (INT)Width;
        Device->DrawTexture(0, 0, W, H, 0, 0, W, H, BackTex, 0xFF, 1);
    }
    Out[0] = 0;
    switch (AlarmType)
    {
    case ALARM_Online:
    {
        // "$s1 has come in": the system message with the person's name put in
        FString Msg(*MSN_SYSMSG(SYSMSG_AlarmOnline));
        Msg = Msg.Substitute(FString(TEXT("$s1")), PersonName(Person));
        appSprintf(Text, TEXT("%s"), *Msg);
        break;
    }
    case ALARM_Message:
    {
        // the message with the name put in, then the text of the message; a message without the "$s1" place gets the name in front
        FString Msg(*MSN_SYSMSG(SYSMSG_AlarmMessage));
        FString Name = PersonName(Person);
        FString Shown = Msg.Substitute(FString(TEXT("$s1")), Name);
        if (Msg == Shown)
            Shown = (Name += Msg);
        appSprintf(Text, TEXT("%s %s"), *Shown, *Message.Body);
        break;
    }
    case ALARM_Mail:
    {
        FString Msg(*MSN_SYSMSG(SYSMSG_AlarmMail));
        Msg = Msg.Substitute(FString(TEXT("$s1")), Subject);
        appSprintf(Text, TEXT("%s"), *Msg);
        break;
    }
    case ALARM_Type3:
        appSprintf(Text, *MSN_SYSMSG(SYSMSG_AlarmType3));
        break;
    case ALARM_Type4:
        appSprintf(Text, *MSN_SYSMSG(SYSMSG_AlarmType4));
        break;
    }
    MsnFitText(Text, Out, (INT)(Width - 22.0f), (INT)(Height - 32.0f));
    Device->DrawText(0xB, 0x10, 0xFFDCDCDC, Out, (DWORD)(INT)(Width - 22.0f), 0, 0, (L2FontType)0, 0, 0, 0, 0, 0, 0, 0);
    Device->ReleaseClip();
    Device->EndPaint();
    return 1;
}
NW_HOOK_METHOD(0x101569b0, AlarmWnd, OnPaint)

// 0x1015e2c0 (vslot 68) - type 0: the owner opens a chat with the person; type 1: the window of the event is shown and
// focused
INT AlarmWnd::OnLButtonDown(WPARAM wParam, LPARAM lParam)
{
    if (AlarmType == ALARM_Online)
    {
        Owner->OnAlarmClick(MsnPersonCopy(Person));
        return 1;
    }
    if (AlarmType == ALARM_Message && Target)
    {
        Target->ShowWindow();
        Target->SetFocus();
    }
    return 1;
}
NW_HOOK_METHOD(0x1015e2c0, AlarmWnd, OnLButtonDown)

// 0x10155910 (ret 0x44) - the box (when it is up) goes, and is made the alarm of type 0 about this person
void AlarmWnd::SetOnline(NMsnPersonRec Who)
{
    if (IsShowWindow())
        HideWindow();
    AlarmType = ALARM_Online;
    Person.Assign(&Who);
    MsnPersonFree(&Who);
}
NW_HOOK_VARARGS(0x10155910, AlarmWnd, SetOnline, void (AlarmWnd::*)(NMsnPersonRec))

// 0x10155990 (ret 0xC) - the box (when it is up) goes, and is made the alarm of type 2 with this subject
void AlarmWnd::SetMail(NMsnStringArg MailSubject)
{
    if (IsShowWindow())
        HideWindow();
    AlarmType = ALARM_Mail;
    Subject = MailSubject.Str();
    MailSubject.Str().~FString();
}
NW_HOOK_VARARGS(0x10155990, AlarmWnd, SetMail, void (AlarmWnd::*)(NMsnStringArg))

NW_CHECK_VTABLE(AlarmWnd, 0x101dd0f8, 74)
