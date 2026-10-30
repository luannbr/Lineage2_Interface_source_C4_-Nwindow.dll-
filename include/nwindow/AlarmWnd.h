#pragma once
// AlarmWnd - the messenger's alarm box (UClass "CAlarmWnd", 0x190; C3 only, no C2 source): a small window that slides up from
// the bottom right corner of the screen (3 pixels per timer tick), stays a few seconds and goes, with one or two lines about
// what happened (a person signed in, a message or a mail arrived...). The messenger window makes it as a child of the
// desktop (0x10139A40); the protocol object fills it and shows it. A click opens the chat (type 0) or the window of the
// event (type 1).
//
// Layout and virtuals from the stock constructor 0x101397E0, OnCreate 0x10130AA0, OnDestroy 0x10130AD0, OnTimer 0x10130B20,
// ShowWindow 0x10130BE0, HideWindow 0x10130C40, OnPaint 0x10131D20 and OnLButtonDown 0x10139950.

#include "nwindow/NCWnd.h"
#include "nwindow/MsnWnd.h"
#include "nwindow/NChatMsg.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class Msn;

// the message of type 1 is the chat lane's NMsnMessage (NChatMsg.h: Body is the text the alarm shows after the name)

class AlarmWnd : public NCWnd
{
    NW_DECLARE_CLASS(AlarmWnd, NCWnd, 0, 0x10325600)

    MsnWnd* Owner;                   // +0xE8 the messenger window that made it (the constructor's first argument) (name ours)
    Msn* MsnObj;                     // +0xEC the protocol object (the second; 0 from the messenger window, set later) (name ours)
    INT AlarmType;                   // +0xF0 what the alarm is about: 0 a person came in, 1 a message, 2 a mail, 3, 4 (name ours)
                                     //   (not touched by the constructor)
    NMsnPerson Person;               // +0xF4 who it is about (name ours)
    FString Subject;                 // +0x138 a second text (the mail's subject going by type 2) (name ours)
    NMsnMessage Message;             // +0x144 the message of type 1 (name ours)
    NCWnd* Target;                   // +0x19C the window a click of type 1 shows (name ours)
    INT SlideTimer;                  // +0x1A0 the timer 0xA50 that moves the box up (name ours)
    INT HideTimer;                   // +0x1A4 the timer 0xA51 that hides it after 8 s (name ours)
    INT SlideOffset;                 // +0x1A8 how far up it has moved (name ours)
    UTexture* BackTex;               // +0x1AC "L2UI_CH3.Msn.msn_alarm"

    AlarmWnd(MsnWnd* InOwner = NULL, Msn* InMsn = NULL);                           // 0x1015e150 (ret 8); defaults = what the stock InternalConstructor pushes
    // ~AlarmWnd                                                     // 0x1015e230

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10148f10 (NControl's body, shared)
    virtual INT OnTimer(INT TimerID);                                // 36 0x10155790
    virtual INT OnCreate();                                          // 37 0x10155710
    virtual INT OnDestroy();                                         // 38 0x10155740
    virtual void ShowWindow();                                       // 48 0x10155850
    virtual void HideWindow();                                       // 49 0x101558b0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x101569b0
    virtual INT OnLButtonDown(WPARAM wParam, LPARAM lParam);         // 68 0x1015e2c0

    // names ours (anonymous in the db; the record and the string are given by value and freed by the callee)
    __declspec(noinline) void SetOnline(NMsnPersonRec Who);          // 0x10155910 (ret 0x44) the alarm of type 0 about this person (hidden first)
    __declspec(noinline) void SetMail(NMsnStringArg MailSubject);    // 0x10155990 (ret 0xC) the alarm of type 2 with this subject
};
NW_ASSERT_OFFSET(AlarmWnd, Owner, 0xE8);
NW_ASSERT_OFFSET(AlarmWnd, MsnObj, 0xEC);
NW_ASSERT_OFFSET(AlarmWnd, AlarmType, 0xF0);
NW_ASSERT_OFFSET(AlarmWnd, Person, 0xF4);
NW_ASSERT_OFFSET(AlarmWnd, Subject, 0x138);
NW_ASSERT_OFFSET(AlarmWnd, Message, 0x144);
NW_ASSERT_OFFSET(AlarmWnd, Target, 0x19C);
NW_ASSERT_OFFSET(AlarmWnd, SlideTimer, 0x1A0);
NW_ASSERT_OFFSET(AlarmWnd, BackTex, 0x1AC);
NW_ASSERT_SIZE(AlarmWnd, 0x1B0);

#pragma pack(pop)
