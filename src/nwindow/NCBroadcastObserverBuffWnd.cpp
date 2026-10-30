/*=============================================================================
    NCBroadcastObserverBuffWnd.cpp - the effects on the followed player (C3 only; C3 addresses). Stock: OnCreate
    0x10112A60, Refresh 0x10112B70, OnPaint 0x10112CD0, the constructor 0x101135D0 and the destructor 0x10113650.
=============================================================================*/
#include "nwindow/NCBroadcastObserverBuffWnd.h"
#include "nwindow/NCBroadcastObserverPlayerWnd.h"
#include "nwindow/NCBroadcastObserverWnd.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NItemEntryC4.h"   // EntryID / EntryLevel: the skill id and level of an entry at their C4 offsets (+0x1B20 / +0x1B24)

// The record NCAbnormalStatusWnd::AddStatus takes, as this window builds it: C3's has one more int after the level
// (set to 1 by the stock Refresh; nothing in AddStatus reads it)
struct FAbnormalStatusExt : public FAbnormalStatus
{
    INT Unknown2310;
};
NW_ASSERT_SIZE(FAbnormalStatusExt, 0x2314);

// 0x101293b0 (ret 4) - the base window with 24 pixel icons, the player window it belongs to
NCBroadcastObserverBuffWnd::NCBroadcastObserverBuffWnd(NCBroadcastObserverPlayerWnd* InPlayerWnd)
    : NCAbnormalStatusWnd(24)
{
    PlayerWnd = InPlayerWnd;
}
NW_HOOK_CTOR_ARGS(0x101293b0, NCBroadcastObserverBuffWnd, (NCBroadcastObserverPlayerWnd* InPlayerWnd), (InPlayerWnd))

// 0x10129430
NW_HOOK_DTOR(0x10129430, NCBroadcastObserverBuffWnd)

// 0x10001b00 (vslot 68) - a body shared by many classes: does nothing (the base class resizes the window here)
void NCBroadcastObserverBuffWnd::UpdateLayout()
{
}

// 0x10128830 (vslot 37, guard: NCBroadcastObserverBuffWnd::OnCreate) - the one second timer and the blink timer (every
// 100 ms), the tool tip and icon textures; NCWnd's OnCreate is not called
INT NCBroadcastObserverBuffWnd::OnCreate()
{
    guard(NCBroadcastObserverBuffWnd::OnCreate);
    TimerID = SetTimer(TIMER_AbnormalStatus, 1000, 0);
    FadeTimerID = SetTimer(TIMER_AbnormalFade, 100, 0);
    ToolTipTex = FindTexture(TEXT("L2UI.tooltip.mini-tool01"));
    TipTopTex = FindTexture(TEXT("L2UI.Tooltip.Tool02"));
    TipMidTex = FindTexture(TEXT("L2UI.Tooltip.Tool01"));
    TipBottomTex = FindTexture(TEXT("L2UI.Tooltip.Tool03"));
    BackTex = FindTexture(TEXT("L2UI.EtcWndBack.AbnormalBack"));
    DebuffTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.debuff"));
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10128830, NCBroadcastObserverBuffWnd, OnCreate)

// 0x10128950 (guard: NCBroadcastObserverBuffWnd::Refresh) - the buffs cleared, then one per spell on the shown player (his
// roster line's lists): level 1, the icon, name and description of the skill from the tables
void NCBroadcastObserverBuffWnd::Refresh()
{
    guard(NCBroadcastObserverBuffWnd::Refresh);
    ClearStatus();
    NBroadcastUser* User = PlayerWnd->GetUser();
    if (User)
    {
        for (INT i = 0; i < User->SpellIDs.Num(); i++)
        {
            FAbnormalStatusExt Status;
            Status.Unknown2310 = 1;
            Status.Level = 1;
            Status.SkillID = User->SpellIDs(i);
            Status.RemainTime = User->SpellTimes(i);
            const TCHAR* IconName = GConsole->GetSkillIconName(Status.SkillID, Status.Level);
            if (IconName)
                appStrcpy(Status.IconName, IconName);
            TCHAR* Name = GConsole->GetSkillName(Status.SkillID, Status.Level);
            if (Name)
                appStrcpy(Status.Name, Name);
            TCHAR* Desc = GConsole->GetSkillDesc(Status.SkillID, Status.Level);
            if (Desc)
                appStrcpy(Status.Desc, Desc);
            AddStatus(&Status);
        }
    }
    unguard;
}
NW_HOOK_METHOD(0x10128950, NCBroadcastObserverBuffWnd, Refresh)

// 0x1006a9b0 (anonymous, name ours; ret 8, `this` is not used; the struct is declared in NCAbnormalStatusWnd.h) - whether a skill is a
// debuff: its record in the skill table has a value at +0x4C (FL2MagicSkillData::bDebuff; the buff windows draw the "debuff" frame over
// the icon)
INT NConsoleWndSkillExt::IsDebuffSkill(INT SkillID, INT Level)
{
    FL2MagicSkillData* Data = GL2GameData.GetMSData(SkillID, Level);
    if (Data && Data->bDebuff)
        return 1;
    return 0;
}
NW_HOOK_METHOD(0x1006a9b0, NConsoleWndSkillExt, IsDebuffSkill)

// An icon of the first list: the icon stretched over its 40 x 40 cell at (X, Y) with the entry's alpha, the place the
// tool tip reads left in the record, and the debuff frame over it when the skill is a debuff
static FORCEINLINE void DrawBuffIcon(NCGDevice* Device, UTexture* DebuffTex, NAbnormalEntry* E, INT X, INT Y, INT IconX, INT IconY)
{
    if (E->bVisible)
        Device->DrawStretchedTex(X, Y, 0x28, 0x28, E->Icon, 1, (BYTE)E->Alpha);
    E->IconX = IconX;
    E->IconY = IconY;
    if (((NConsoleWndSkillExt*)GConsole)->IsDebuffSkill(EntryID(E), EntryLevel(E)) && E->bVisible)
        Device->DrawTexture(X, Y, 0x28, 0x28, 0, 0, 0x1A, 0x1A, DebuffTex, (BYTE)E->Alpha, 1);
}

// An icon of the second list or the extra icon: a 26 x 26 back at (BackX, BackY) and the icon (24 x 24, one pixel in) over
// it at full alpha (the extra icon takes the entry's alpha)
static FORCEINLINE void DrawSmallIcon(NCGDevice* Device, UTexture* BackTex, NAbnormalEntry* E, INT BackX, INT BackY, DWORD Alpha)
{
    Device->DrawTexture(BackX, BackY, 0x1A, 0x1A, 0, 0, 0x1A, 0x1A, BackTex, 0xFF, 1);
    if (E->bVisible)
        Device->DrawStretchedTex(BackX + 1, BackY + 1, 0x18, 0x18, E->Icon, 1, Alpha);
}

// 0x10128ab0 (vslot 62, guard: NCBroadcastObserverBuffWnd::OnPaint) - the window hides itself when it holds nothing. The
// first list in 40 x 40 icons, ten to a row (rows at y 0, 40, 80), then the extra icon in the row after the list, then
// the second list in 26 x 26 cells under the first (a column to the right when there is an extra icon)
INT NCBroadcastObserverBuffWnd::OnPaint(NCGDevice* Device)
{
    guard(NCBroadcastObserverBuffWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (Statuses.Num() == 0 && Statuses2.Num() == 0 && ExtraStatus == NULL)
        HideWindow();
    for (INT i = 0; i < Statuses.Num(); i++)
    {
        NAbnormalEntry* E = Statuses(i);
        if (!E || !E->Icon)
            continue;
        if (i < 10)
            DrawBuffIcon(Device, DebuffTex, E, i * 40, 0, i * 26 + 1, 1);
        else if (i < 20)
            DrawBuffIcon(Device, DebuffTex, E, i * 40 - 400, 0x28, i * 26 - 0x103, 0x1A);
        else if (i < 30)
            DrawBuffIcon(Device, DebuffTex, E, i * 40 - 800, 0x50, i * 26 - 0x207, 0x33);
    }
    if (ExtraStatus)
    {
        INT Count = Statuses.Num();
        if (Count == 0)
        {
            Device->DrawTexture(0, 0, 0x1A, 0x1A, 0, 0, 0x1A, 0x1A, BackTex, 0xFF, 1);
            if (ExtraStatus->bVisible)
                Device->DrawStretchedTex(1, 1, 0x18, 0x18, ExtraStatus->Icon, 1, (BYTE)ExtraStatus->Alpha);
            ExtraStatus->IconX = 0x1B;
            ExtraStatus->IconY = 1;
        }
        else if (Count <= 10)
        {
            Device->DrawTexture(0, 0x19, 0x1A, 0x1A, 0, 0, 0x1A, 0x1A, BackTex, 0xFF, 1);
            if (ExtraStatus->bVisible)
                Device->DrawStretchedTex(1, 0x1A, 0x18, 0x18, ExtraStatus->Icon, 1, (BYTE)ExtraStatus->Alpha);
            ExtraStatus->IconX = 0x1B;
            ExtraStatus->IconY = 0x1A;
        }
        else if (Count <= 20)
        {
            Device->DrawTexture(0, 0x32, 0x1A, 0x1A, 0, 0, 0x1A, 0x1A, BackTex, 0xFF, 1);
            if (ExtraStatus->bVisible)
                Device->DrawStretchedTex(1, 0x33, 0x18, 0x18, ExtraStatus->Icon, 1, (BYTE)ExtraStatus->Alpha);
            ExtraStatus->IconX = 0x1B;
            ExtraStatus->IconY = 0x33;
        }
        else if (Count <= 30)
        {
            Device->DrawTexture(0, 0x4B, 0x1A, 0x1A, 0, 0, 0x1A, 0x1A, BackTex, 0xFF, 1);
            if (ExtraStatus->bVisible)
                Device->DrawStretchedTex(1, 0x4C, 0x18, 0x18, ExtraStatus->Icon, 1, (BYTE)ExtraStatus->Alpha);
            ExtraStatus->IconX = 0x1B;
            ExtraStatus->IconY = 0x4C;
        }
    }
    for (INT j = 0; j < Statuses2.Num(); j++)
    {
        NAbnormalEntry* E = Statuses2(j);
        if (!E || !E->Icon)
            continue;
        INT Count = Statuses.Num();
        INT BackX = j * 26;
        if (ExtraStatus)
            BackX += 0x1A;
        if (Count == 0)
        {
            DrawSmallIcon(Device, BackTex, E, BackX, 0, 0xFF);
            E->IconY = 1;
        }
        else if (Count <= 10)
        {
            DrawSmallIcon(Device, BackTex, E, BackX, 0x19, 0xFF);
            E->IconY = 0x1A;
        }
        else if (Count <= 20)
        {
            DrawSmallIcon(Device, BackTex, E, BackX, 0x32, 0xFF);
            E->IconY = 0x33;
        }
        else if (Count <= 30)
        {
            DrawSmallIcon(Device, BackTex, E, BackX, 0x4B, 0xFF);
            E->IconY = 0x4C;
        }
        else
            continue;
        E->IconX = BackX + 1;
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10128ab0, NCBroadcastObserverBuffWnd, OnPaint)

NW_CHECK_VTABLE(NCBroadcastObserverBuffWnd, 0x101d2a68, 75)
