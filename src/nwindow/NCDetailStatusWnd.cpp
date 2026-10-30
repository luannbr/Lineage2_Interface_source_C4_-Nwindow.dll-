/*=============================================================================
    NCDetailStatusWnd.cpp - the character tab of the status window (Alt+T) and the GM's view
    of another player's status.
    Stock: 0x1005be50 .. 0x1005be67, 0x1005c5b0 .. 0x1005c5ff, 0x1005cdb0 .. 0x1005cdfd,
    0x1005ce30 .. 0x1005e30b, 0x100aea90 .. 0x100aeb64, 0x100b1be0 .. 0x100b32fa.
=============================================================================*/
#include "nwindow/NCDetailStatusWnd.h"
#include "nwindow/NCFrameCtrl.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NConsoleWnd.h"

#include <stdio.h>
#include <wchar.h>

// exp at the start of each level (index = level), a table of the stock image, and the GM
// window's own copy of it, from level 1 (index = level - 1); same values
#if NW_HOOK_MODE
#define GExpTable (reinterpret_cast<const INT*>(::nw::Addr(0x101CC91C)))  // C3: read from NCDetailStatusWnd::OnPaint (0x100fdd59)
#define GGMExpTable (reinterpret_cast<const INT*>(::nw::Addr(0x101B39C0)))  // C3: the GM window's own copy, from level 1
                                                                            //   (NCGMDetailStatusWnd::OnPaint reads 0x101873CC + 4 * level)
#else
extern const INT GExpTable[];
extern const INT GGMExpTable[];
#endif

// every text of the window goes out with the same trailing arguments
static FORCEINLINE void DrawString(NCGDevice* Device, INT X, INT Y, DWORD Color, const TCHAR* Text)
{
    Device->DrawText(X, Y, Color, Text, 0, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0);
}

// a number centered on CenterX in the values' color (spelled out at each use in the stock code)
static FORCEINLINE void DrawNumber(NCGDevice* Device, TCHAR* Buf, INT CenterX, INT Y, INT Value)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%d"), Value);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, CenterX - Size.cx / 2, Y, 0xFFB09B79, Buf);
}

// 0x10117850 (no C1 match) - the textures other than the first are left as allocated
NCDetailStatusWnd::NCDetailStatusWnd()
{
    BackTex = NULL;
    for (INT i = 0; i < 17; i++)
        UnknownCC[i] = 0;
}
NW_HOOK_CTOR(0x10117850, NCDetailStatusWnd)

// 0x1008f490
NW_HOOK_DTOR(0x1008f490, NCDetailStatusWnd)

// stock body shared with NControl's (0x10004e90)
INT NCDetailStatusWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1010def0 (vslot 37) - the bars of the player window (HP, MP, exp, CP), five weight bars and, new in C4, the hero and
// the noble icons
INT NCDetailStatusWnd::OnCreate()
{
    NCWnd::OnCreate();
    BackTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.myinfo_back"));
    HPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_hpbar"));
    MPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_mpbar"));
    EXPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_expbar"));
    CPTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_cpbar"));
    WeightTex[0] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightBar1"));
    WeightTex[1] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightBar2"));
    WeightTex[2] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightBar3"));
    WeightTex[3] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightBar4"));
    WeightTex[4] = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.ps_weightBar5"));
    HeroTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.myinfo_heroicon"));
    NobleTex = FindTexture(TEXT("L2UI_CH3.PlayerStatusWnd.myinfo_nobleicon"));
    return 1;
}
NW_HOOK_METHOD(0x1010def0, NCDetailStatusWnd, OnCreate)

// 0x1010dff0 (vslot 38)
INT NCDetailStatusWnd::OnDestroy()
{
    guard(NCDetailStatusWnd::OnDestroy);
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1010dff0, NCDetailStatusWnd, OnDestroy)

// stock body shared with NCEditBox's (0x100a5d70)
void NCDetailStatusWnd::ShowWindow()
{
    NCWnd::ShowWindow();
}

// a stat and its bonus (the window's, not the player's) centered on CenterX: "N" without a bonus, "N(-B)" with a
// negative one and the format the caller gives ("%d(+%d)", "%d (+%d)") with a positive one
static FORCEINLINE void DrawStat(NCGDevice* Device, TCHAR* Buf, INT CenterX, INT Y, INT Value, INT Bonus,
                                 const TCHAR* PlusFormat)
{
    SIZE Size;
    if (Bonus > 0)
        appSprintf(Buf, PlusFormat, Value, Bonus);
    else if (Bonus < 0)
        appSprintf(Buf, TEXT("%d(%d)"), Value, Bonus);
    else
        appSprintf(Buf, TEXT("%d"), Value);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, CenterX - Size.cx / 2, Y, 0xFFB09B79, Buf);
}

// a value right-aligned on RightX in the values' color (spelled out at each use in the stock code)
static FORCEINLINE void DrawRightNumber(NCGDevice* Device, TCHAR* Buf, INT RightX, INT Y, INT Value)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%d"), Value);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, RightX - Size.cx, Y, 0xFFB09B79, Buf);
}

// a stat with its bonus centered on CenterX (the C3 window gives every bonus the same format)
static FORCEINLINE void DrawStatC3(NCGDevice* Device, TCHAR* Buf, INT CenterX, INT Y, INT Value, INT Bonus)
{
    DrawStat(Device, Buf, CenterX, Y, Value, Bonus, TEXT("%d (+%d)"));
}

// a bar of the player window: 85 pixels at most (the clamped one is drawn YClamp pixels high: the stock code has
// 10 for HP, MP and CP and 12 for the exp bar)
static FORCEINLINE void DrawStatusBar(NCGDevice* Device, INT X, INT Y, INT Width, INT YClamp, UTexture* Tex)
{
    if (Width > 0 && Width <= 85)
        Device->DrawTexture(X, Y, Width, 12, 0, 0, 8, 12, Tex, 0xFF, 1);
    else if (Width > 85)
        Device->DrawTexture(X, Y, 85, YClamp, 0, 0, 8, 12, Tex, 0xFF, 1);
}

// NCGDevice::DrawSpecialDigit(bLarge, X, Y, Color, Text, Alpha) (0x10013350, ret 0x18): the alpha is a C4 argument (0xFF here)
static FORCEINLINE void DrawDigits(NCGDevice* Device, INT X, INT Y, DWORD Color, const TCHAR* Text)
{
    Device->DrawSpecialDigit(1, X, Y, Color, (TCHAR*)Text, 0xFF);
}

// "now / max" around a slash centered on CenterX at row Y, in the large digits: the slash, the value ending 10 pixels
// left of the center and the maximum starting 6 pixels right of it
static FORCEINLINE void DrawBarNumbers(NCGDevice* Device, TCHAR* Buf, INT CenterX, INT Y, INT Value, INT Max)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%s"), TEXT("/"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigits(Device, CenterX - Size.cx / 2, Y, 0xFFDCDCDC, Buf);
    appSprintf(Buf, TEXT("%d"), Value);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigits(Device, CenterX - 10 - Size.cx, Y, 0xFFDCDCDC, Buf);
    appSprintf(Buf, TEXT("%d"), Max);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigits(Device, CenterX + 6, Y, 0xFFDCDCDC, Buf);
}

// 0x10110c90 (vslot 62) - the player window of Scions of Destiny: the C3 window (bars 85 pixels wide for HP, MP, CP,
// exp and weight, the weight bar's texture follows the load: up to 50%, 66.6%, 80%, 100% and above; the title and the
// name left-aligned at 15, each cut to 9 characters and ".." when both are wider than 220 pixels together, the name
// reddened by the karma; the clan's crest and name after a label; the class and its level; the combat values
// right-aligned at 114 and 235; the six stats centered with their bonuses; karma, PvP / PK and two more counters at
// the bottom) with the large digits' alpha and, at the top right, the hero or the noble icon. The User of the C4
// Engine (+0x10 on the C3 offsets from the CP on: the CP maximum +0x214, the CP +0x218, the class +0x210, the PK
// and PvP counts +0x208 / +0x204, the two counters +0x22E / +0x22C, the noble flag +0x23C, the hero flag +0x240)
INT NCDetailStatusWnd::OnPaint(NCGDevice* Device)
{
    guard(NCDetailStatusWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex, 0xFF, 1);

    DrawString(Device, 15, 25, 0xFFA3A3A3, *SysString(430));    // "Clan"
    DrawString(Device, 15, 46, 0xFFA3A3A3, *SysString(88));     // "Lv"
    DrawString(Device, 22, 110, 0xFFDCDCDC, *SysString(93));    // "Combat"
    DrawString(Device, 16, 132, 0xFFA3A3A3, *SysString(94));    // "P.Atk."
    DrawString(Device, 16, 147, 0xFFA3A3A3, *SysString(95));    // "P.Def."
    DrawString(Device, 16, 162, 0xFFA3A3A3, *SysString(96));    // "Accuracy"
    DrawString(Device, 16, 177, 0xFFA3A3A3, *SysString(113));   // "Critical"
    DrawString(Device, 16, 192, 0xFFA3A3A3, *SysString(111));   // "Atk. Spd."
    DrawString(Device, 22, 214, 0xFFDCDCDC, *SysString(103));   // "Basic"
    DrawString(Device, 137, 132, 0xFFA3A3A3, *SysString(98));   // "M.Atk."
    DrawString(Device, 137, 148, 0xFFA3A3A3, *SysString(99));   // "M.Def."
    DrawString(Device, 137, 163, 0xFFA3A3A3, *SysString(97));   // "Evasion"
    DrawString(Device, 137, 178, 0xFFA3A3A3, *SysString(432));  // "Speed"
    DrawString(Device, 137, 193, 0xFFA3A3A3, *SysString(112));  // "Casting Spd."
    DrawString(Device, 16, 236, 0xFFA3A3A3, *SysString(104));   // "STR"
    DrawString(Device, 96, 236, 0xFFA3A3A3, *SysString(105));   // "DEX"
    DrawString(Device, 177, 236, 0xFFA3A3A3, *SysString(106));  // "CON"
    DrawString(Device, 16, 252, 0xFFA3A3A3, *SysString(107));   // "INT"
    DrawString(Device, 96, 252, 0xFFA3A3A3, *SysString(108));   // "WIT"
    DrawString(Device, 177, 252, 0xFFA3A3A3, *SysString(109));  // "MEN"
    DrawString(Device, 22, 273, 0xFFDCDCDC, *SysString(100));   // "Social"
    DrawString(Device, 16, 294, 0xFFA3A3A3, *SysString(101));   // "Karma"
    DrawString(Device, 137, 294, 0xFFA3A3A3, *SysString(102));  // "PvP"
    DrawString(Device, 16, 310, 0xFFA3A3A3, *SysString(709));
    DrawString(Device, 137, 310, 0xFFA3A3A3, *SysString(710));

    Player = GConsole->GetMyUser();
    if (Player)
    {
        INT HPW = 0;
        INT MPW = 0;
        INT CPW = 0;
        INT WeightW = 0;
        INT EXPW = 0;
        INT MaxCP = nw::Field<INT>(Player, 0x214);   // the CP and its maximum (names ours)
        INT CP = nw::Field<INT>(Player, 0x218);
        if (Player->MaxHP)
            HPW = (INT)(Player->HP * 85.0 / Player->MaxHP);
        if (Player->MaxMP)
            MPW = (INT)(Player->MP * 85.0 / Player->MaxMP);
        if (MaxCP)
            CPW = (INT)(CP * 85.0 / MaxCP);
        if (Player->MaxLoad)
            WeightW = (INT)(Player->Load * 85.0 / Player->MaxLoad);
        if (GExpTable[Player->Level + 1])
            EXPW = (INT)((DOUBLE)(Player->Exp - GExpTable[Player->Level]) * 85.0 /
                         (GExpTable[Player->Level + 1] - GExpTable[Player->Level]));

        DrawStatusBar(Device, 35, 59, HPW, 10, HPTex);
        DrawStatusBar(Device, 35, 73, MPW, 10, MPTex);
        DrawStatusBar(Device, 156, 59, CPW, 10, CPTex);
        DrawStatusBar(Device, 35, 87, EXPW, 12, EXPTex);

        FLOAT WeightPct = 0.f;
        if (WeightW > 0)
        {
            // the first test uses the unrounded quotient, the others the stored float
            DOUBLE Pct = (DOUBLE)Player->Load / Player->MaxLoad * 100.0;
            WeightPct = (FLOAT)Pct;
            if (Pct <= 50.0)
                Device->DrawTexture(156, 73, WeightW, 12, 0, 0, WeightW, 12, WeightTex[0], 0xFF, 1);
            else if (WeightPct <= 66.6)
                Device->DrawTexture(156, 73, WeightW, 12, 0, 0, WeightW, 12, WeightTex[1], 0xFF, 1);
            else if (WeightPct > 66.6 && WeightPct <= 80.f)
                Device->DrawTexture(156, 73, WeightW, 12, 0, 0, WeightW, 12, WeightTex[2], 0xFF, 1);
            else if (WeightPct > 80.f && WeightPct <= 100.f)
                Device->DrawTexture(156, 73, WeightW, 12, 0, 0, WeightW, 12, WeightTex[3], 0xFF, 1);
            else if (WeightPct > 100.f)
                Device->DrawTexture(156, 73, WeightW, 10, 0, 0, WeightW, 12, WeightTex[4], 0xFF, 1);
        }

        TCHAR Buf[64];
        SIZE Size;
        INT LevelExp = GExpTable[Player->Level];
        _swprintf(Buf, TEXT("%.2f%s"),
                  (DOUBLE)(Player->Exp - LevelExp) * 100.0 / (GExpTable[Player->Level + 1] - LevelExp), TEXT("%"));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
        DrawDigits(Device, 77 - Size.cx / 2, 89, 0xFFB4B4B4, Buf);

        // "now / max" around a slash centered at 77 (HP, MP) and 198 (CP), in the large digits
        DrawBarNumbers(Device, Buf, 77, 61, Player->HP, Player->MaxHP);
        DrawBarNumbers(Device, Buf, 77, 75, Player->MP, Player->MaxMP);
        DrawBarNumbers(Device, Buf, 198, 61, CP, MaxCP);

        DrawNumber(Device, Buf, 198, 88, Player->SP);
        _swprintf(Buf, TEXT("%.2f%s"), (DOUBLE)WeightPct, TEXT("%"));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
        DrawDigits(Device, 198 - Size.cx / 2, 75, 0xFFDCDCDC, Buf);

        // the name is white without karma; with it, green and blue go down by the karma (at least by 100 + karma / 16,
        // at most by 255: red)
        INT Red = Player->Karma;
        if (Red > 255)
            Red = 255;
        if (Red > 0)
        {
            INT MinRed = Red / 16 + 100;
            if (Red < MinRed)
                Red = MinRed;
            else if (Red >= 255)
                Red = 255;
        }
        DWORD NameColor = ((-1 - Red) << 8) | (255 - Red) | 0xFFFF0000;

        // the title first, when there is one; the name after it
        TCHAR Name[64];
        TCHAR Title[64];
        SIZE NameSize;
        SIZE TitleSize;
        if (Player->GetNickName() && *Player->GetNickName())
        {
            appSprintf(Title, TEXT("%s"), Player->GetNickName());
            NCGDevice::GetTextExtent(&TitleSize, Title, 0, 0, 0, 0, 0);
            appSprintf(Name, TEXT("%s"), Player->GetName());
            NCGDevice::GetTextExtent(&NameSize, Name, 0, 0, 0, 0, 0);
            if (TitleSize.cx + NameSize.cx > 220)
            {
                if (TitleSize.cx > 109)
                {
                    Title[9] = '.';
                    Title[10] = '.';
                    Title[11] = 0;
                    NCGDevice::GetTextExtent(&TitleSize, Title, 0, 0, 0, 0, 0);
                }
                if (NameSize.cx > 109)
                {
                    Name[9] = '.';
                    Name[10] = '.';
                    Name[11] = 0;
                    NCGDevice::GetTextExtent(&NameSize, Name, 0, 0, 0, 0, 0);
                }
            }
            DrawString(Device, 15, 9, 0xFFA2F9EC, Title);
            DrawString(Device, TitleSize.cx + 21, 9, NameColor, Name);
        }
        else
        {
            appSprintf(Name, TEXT("%s"), Player->GetName());
            NCGDevice::GetTextExtent(&NameSize, Name, 0, 0, 0, 0, 0);
            DrawString(Device, 15, 9, NameColor, Name);
        }

        // the clan: its label, then the crest and name after it (the name further right when the clan has a crest id),
        // else SysString 431 ("No Clan")
        SIZE ClanLabelSize;
        NCGDevice::GetTextExtent(&ClanLabelSize, *SysString(873), 0, 0, 0, 0, 0);
        PledgeInfo* Pledge = GConsole->GetPledgeInfo(Player->PledgeID);
        if (Pledge)
        {
            // the crest texture of the record is at +0xE0 in C4 (the header's PledgeInfo has it at +0xD0)
            UTexture* CrestTex = nw::Field<UTexture*>(Pledge, 0xE0);
            if (CrestTex)
                Device->DrawTexture(ClanLabelSize.cx + 20, 25, 16, 12, 0, 4, 16, 12, CrestTex, 0xFF, 1);
            const TCHAR* PledgeName = Pledge->Name;
            if (PledgeName && *PledgeName)
            {
                NCGDevice::GetTextExtent(&Size, PledgeName, 0, 0, 0, 0, 0);
                if (Pledge->CrestID > 0)
                    DrawString(Device, ClanLabelSize.cx + 40, 25, 0xFFB09B79, PledgeName);
                else
                    DrawString(Device, ClanLabelSize.cx + 20, 25, 0xFFB09B79, PledgeName);
            }
        }
        else
            DrawString(Device, ClanLabelSize.cx + 20, 25, 0xFFDCDCDC, *SysString(431));

        appSprintf(Buf, TEXT("%d %s"), Player->Level, GConsole->GetClassNameString(nw::Field<INT>(Player, 0x210)));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
        DrawString(Device, 35, 46, 0xFFB09B79, Buf);

        DrawRightNumber(Device, Buf, 114, 132, Player->PAtk);
        DrawRightNumber(Device, Buf, 114, 147, Player->PDef);
        DrawRightNumber(Device, Buf, 114, 162, Player->Accuracy);
        DrawRightNumber(Device, Buf, 114, 177, Player->Critical);
        DrawRightNumber(Device, Buf, 114, 192, Player->PAtkSpeed);
        DrawRightNumber(Device, Buf, 235, 132, Player->MAtk);
        DrawRightNumber(Device, Buf, 235, 147, Player->MDef);
        DrawRightNumber(Device, Buf, 235, 162, Player->Evasion);
        // the speed as the player moves: running or walking (anything else: the text of the line before is drawn
        // again); the pawn's +0x6B0 is the medium (1 swimming, 2 flying: the two more speeds at +0x100 / +0x104),
        // times the speed rate (a double)
        APawn* Pawn = Player->Pawn;
        BYTE MoveMode = *(BYTE*)(nw::Field<BYTE*>(Pawn, 0x13F0) + 0x41C);   // the controller's move mode
        INT Medium = nw::Field<INT>(Pawn, 0x6B0);
        if (MoveMode == 1)
        {
            INT Speed;
            if (Medium == 1)
                Speed = Player->RunSpeedWater;
            else if (Medium == 2)
                Speed = nw::Field<INT>(Player, 0x100);
            else
                Speed = Player->RunSpeed;
            appSprintf(Buf, TEXT("%d"), (INT)(Speed * Player->MoveSpeedRate));
        }
        else if (MoveMode == 0)
        {
            INT Speed;
            if (Medium == 1)
                Speed = Player->WalkSpeedWater;
            else if (Medium == 2)
                Speed = nw::Field<INT>(Player, 0x104);
            else
                Speed = Player->WalkSpeed;
            appSprintf(Buf, TEXT("%d"), (INT)(Speed * Player->MoveSpeedRate));
        }
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
        DrawString(Device, 235 - Size.cx, 177, 0xFFB09B79, Buf);
        DrawRightNumber(Device, Buf, 235, 192, Player->CastSpeed);

        // the six stats: the bonuses are the window's (set by the console)
        DrawStatC3(Device, Buf, 65, 236, Player->Str, UnknownCC[BONUS_Str]);
        DrawStatC3(Device, Buf, 148, 236, Player->Dex, UnknownCC[BONUS_Dex]);
        DrawStatC3(Device, Buf, 229, 236, Player->Con, UnknownCC[BONUS_Con]);
        DrawStatC3(Device, Buf, 65, 252, Player->Int, UnknownCC[BONUS_Int]);
        DrawStatC3(Device, Buf, 148, 252, Player->Wit, UnknownCC[BONUS_Wit]);
        DrawStatC3(Device, Buf, 229, 252, Player->Men, UnknownCC[BONUS_Men]);

        // from 999999 on the karma gets a "+" (the value itself is still printed)
        if (Player->Karma >= 999999)
            appSprintf(Buf, TEXT("%d+"), Player->Karma);
        else
            appSprintf(Buf, TEXT("%d"), Player->Karma);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
        DrawString(Device, 114 - Size.cx, 294, 0xFFB09B79, Buf);
        appSprintf(Buf, TEXT("%d / %d"), nw::Field<INT>(Player, 0x208), nw::Field<INT>(Player, 0x204));
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
        DrawString(Device, 235 - Size.cx, 294, 0xFFB09B79, Buf);
        DrawRightNumber(Device, Buf, 114, 310, (INT)nw::Field<SWORD>(Player, 0x22E));
        DrawRightNumber(Device, Buf, 235, 310, (INT)nw::Field<SWORD>(Player, 0x22C));

        // new in C4: the hero's icon, else the noble's, at the top right
        UTexture* StatusIcon = NULL;
        if (nw::Field<INT>(Player, 0x240) != 0)
            StatusIcon = HeroTex;
        else if (nw::Field<INT>(Player, 0x23C) != 0)
            StatusIcon = NobleTex;
        if (StatusIcon)
            Device->DrawTexture(192, 40, 13, 15, 0, 0, 13, 15, StatusIcon, 0xFF, 1);
    }
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10110c90, NCDetailStatusWnd, OnPaint)

NW_CHECK_VTABLE(NCDetailStatusWnd, 0x101b4590, 74)

/*-----------------------------------------------------------------------------
    NCGMDetailStatusWnd.
-----------------------------------------------------------------------------*/

// a number of the GM window: black one pixel lower right, then white
static FORCEINLINE void DrawShadowText(NCGDevice* Device, INT X, INT Y, const TCHAR* Text)
{
    DrawString(Device, X + 1, Y + 1, 0xFF000000, Text);
    DrawString(Device, X, Y, 0xFFFFFFFF, Text);
}

// the slash between two numbers of the GM window, in the large digits, with the same shadow
static FORCEINLINE void DrawShadowSlash(NCGDevice* Device, TCHAR* Buf, INT CenterX, INT Y)
{
    SIZE Size;
    appSprintf(Buf, TEXT("%s"), TEXT("/"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    Device->DrawSpecialDigit(1, CenterX + 1 - Size.cx / 2, Y + 1, 0xFF000000, Buf);
    Device->DrawSpecialDigit(1, CenterX - Size.cx / 2, Y, 0xFFFFFFFF, Buf);
}

// 0x10090230 - the User is built by Engine's constructor
NCGMDetailStatusWnd::NCGMDetailStatusWnd()
{
}
NW_HOOK_CTOR(0x10090230, NCGMDetailStatusWnd)

// 0x100902b0
NW_HOOK_DTOR(0x100902b0, NCGMDetailStatusWnd)

// stock body shared with NCButton's (0x100051f0)
INT NCGMDetailStatusWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCDetailStatusWnd::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1008d9a0 (vslot 37) - the character tab's textures, and a title bar (256 x 20)
INT NCGMDetailStatusWnd::OnCreate()
{
    NCDetailStatusWnd::OnCreate();
    FrameCtrl = new NCFrameCtrl(1, 0, 0);
    FrameCtrl->ID = NID_GMDETAIL_FRAME;
    FrameCtrl->SetPosition(0, 0, 256, 20);
    CreateChild(FrameCtrl, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x1008d9a0, NCGMDetailStatusWnd, OnCreate)

// 0x1008abc0 (vslot 62) - the character tab of the player the GM asked about (Target): the same paint as the player's, one
// title bar (20 pixels) lower, with the speeds (water, air, ground) in one line, HP and MP in the large digits, the clan from
// its info, the stats' bonuses (STR as "N(+B)", the others "N (+B)"). The GM window has its own exp table (from level 1).
// The User is C4's (the CP and the counters 0x10 on from the C3 offsets); no hero or noble icon here
INT NCGMDetailStatusWnd::OnPaint(NCGDevice* Device)
{
    guard(NCGMDetailStatusWnd::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 20, (INT)Width, (INT)(Height - 20.0f), 0, 0, (INT)Width, (INT)(Height - 20.0f), BackTex,
                            0xFF, 1);

    DrawString(Device, 15, 45, 0xFFA3A3A3, *SysString(430));    // "Clan"
    DrawString(Device, 15, 66, 0xFFA3A3A3, *SysString(88));     // "Lv"
    DrawString(Device, 22, 130, 0xFFDCDCDC, *SysString(93));    // "Combat"
    DrawString(Device, 16, 152, 0xFFA3A3A3, *SysString(94));    // "P.Atk."
    DrawString(Device, 16, 167, 0xFFA3A3A3, *SysString(95));    // "P.Def."
    DrawString(Device, 16, 182, 0xFFA3A3A3, *SysString(96));    // "Accuracy"
    DrawString(Device, 16, 197, 0xFFA3A3A3, *SysString(113));   // "Critical"
    DrawString(Device, 16, 212, 0xFFA3A3A3, *SysString(111));   // "Atk. Spd."
    DrawString(Device, 22, 234, 0xFFDCDCDC, *SysString(103));   // "Basic"
    DrawString(Device, 137, 152, 0xFFA3A3A3, *SysString(98));   // "M.Atk."
    DrawString(Device, 137, 168, 0xFFA3A3A3, *SysString(99));   // "M.Def."
    DrawString(Device, 137, 183, 0xFFA3A3A3, *SysString(97));   // "Evasion"
    DrawString(Device, 137, 198, 0xFFA3A3A3, *SysString(112));  // "Casting Spd."
    DrawString(Device, 16, 256, 0xFFA3A3A3, *SysString(104));   // "STR"
    DrawString(Device, 96, 256, 0xFFA3A3A3, *SysString(105));   // "DEX"
    DrawString(Device, 177, 256, 0xFFA3A3A3, *SysString(106));  // "CON"
    DrawString(Device, 16, 272, 0xFFA3A3A3, *SysString(107));   // "INT"
    DrawString(Device, 96, 272, 0xFFA3A3A3, *SysString(108));   // "WIT"
    DrawString(Device, 177, 272, 0xFFA3A3A3, *SysString(109));  // "MEN"
    DrawString(Device, 22, 293, 0xFFDCDCDC, *SysString(100));   // "Social"
    DrawString(Device, 16, 314, 0xFFA3A3A3, *SysString(101));   // "Karma"
    DrawString(Device, 137, 314, 0xFFA3A3A3, *SysString(102));  // "PvP"
    DrawString(Device, 16, 330, 0xFFA3A3A3, *SysString(709));
    DrawString(Device, 137, 330, 0xFFA3A3A3, *SysString(710));  

    // the GM's exp table starts at level 1 (index = level - 1)
    INT HPW = 0;
    INT MPW = 0;
    INT CPW = 0;
    INT WeightW = 0;
    INT EXPW = 0;
    INT MaxCP = nw::Field<INT>(&Target, 0x214);   // the CP and its maximum (names ours)
    INT CP = nw::Field<INT>(&Target, 0x218);
    if (Target.MaxHP)
        HPW = (INT)(Target.HP * 85.0 / Target.MaxHP);
    if (Target.MaxMP)
        MPW = (INT)(Target.MP * 85.0 / Target.MaxMP);
    if (MaxCP)
        CPW = (INT)(CP * 85.0 / MaxCP);
    if (Target.MaxLoad)
        WeightW = (INT)(Target.Load * 85.0 / Target.MaxLoad);
    if (GGMExpTable[Target.Level])
        EXPW = (INT)((DOUBLE)(Target.Exp - GGMExpTable[Target.Level - 1]) * 85.0 /
                     (GGMExpTable[Target.Level] - GGMExpTable[Target.Level - 1]));

    DrawStatusBar(Device, 35, 79, HPW, 10, HPTex);
    DrawStatusBar(Device, 35, 93, MPW, 10, MPTex);
    DrawStatusBar(Device, 156, 79, CPW, 10, CPTex);
    DrawStatusBar(Device, 35, 107, EXPW, 12, EXPTex);

    FLOAT WeightPct = 0.f;
    if (WeightW > 0)
    {
        DOUBLE Pct = (DOUBLE)Target.Load / Target.MaxLoad * 100.0;
        WeightPct = (FLOAT)Pct;
        if (Pct <= 50.0)
            Device->DrawTexture(156, 93, WeightW, 12, 0, 0, WeightW, 12, WeightTex[0], 0xFF, 1);
        else if (WeightPct <= 66.66)
            Device->DrawTexture(156, 93, WeightW, 12, 0, 0, WeightW, 12, WeightTex[1], 0xFF, 1);
        else if (WeightPct > 66.66 && WeightPct <= 80.f)
            Device->DrawTexture(156, 93, WeightW, 12, 0, 0, WeightW, 12, WeightTex[2], 0xFF, 1);
        else if (WeightPct > 80.f)
            Device->DrawTexture(156, 93, WeightW, 10, 0, 0, WeightW, 12, WeightTex[3], 0xFF, 1);
    }

    TCHAR Buf[64];
    SIZE Size;
    INT LevelExp = GGMExpTable[Target.Level - 1];
    _swprintf(Buf, TEXT("%.2f%s"),
              (DOUBLE)(Target.Exp - LevelExp) * 100.0 / (GGMExpTable[Target.Level] - LevelExp), TEXT("%"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigits(Device, 77 - Size.cx / 2, 109, 0xFFB4B4B4, Buf);

    // "now / max" around a slash centered at 77 (HP, MP) and 198 (CP), in the large digits
    DrawBarNumbers(Device, Buf, 77, 81, Target.HP, Target.MaxHP);
    DrawBarNumbers(Device, Buf, 77, 95, Target.MP, Target.MaxMP);
    DrawBarNumbers(Device, Buf, 198, 81, CP, MaxCP);

    DrawNumber(Device, Buf, 198, 108, Target.SP);
    _swprintf(Buf, TEXT("%.2f%s"), (DOUBLE)WeightPct, TEXT("%"));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawDigits(Device, 198 - Size.cx / 2, 95, 0xFFDCDCDC, Buf);

    // the name is white without karma; with it, green and blue go down by the karma (at least by 100 + karma / 16,
    // at most by 255: red)
    INT Red = Target.Karma;
    if (Red > 255)
        Red = 255;
    if (Red > 0)
    {
        INT MinRed = Red / 16 + 100;
        if (Red < MinRed)
            Red = MinRed;
        else if (Red >= 255)
            Red = 255;
    }
    DWORD NameColor = ((-1 - Red) << 8) | (255 - Red) | 0xFFFF0000;

    // the title first, when there is one; the name after it
    TCHAR Name[64];
    TCHAR Title[64];
    SIZE NameSize;
    SIZE TitleSize;
    if (Target.GetNickName() && *Target.GetNickName())
    {
        appSprintf(Title, TEXT("%s"), Target.GetNickName());
        NCGDevice::GetTextExtent(&TitleSize, Title, 0, 0, 0, 0, 0);
        appSprintf(Name, TEXT("%s"), Target.GetName());
        NCGDevice::GetTextExtent(&NameSize, Name, 0, 0, 0, 0, 0);
        if (TitleSize.cx + NameSize.cx > 220)
        {
            if (TitleSize.cx > 109)
            {
                Title[9] = '.';
                Title[10] = '.';
                Title[11] = 0;
                NCGDevice::GetTextExtent(&TitleSize, Title, 0, 0, 0, 0, 0);
            }
            if (NameSize.cx > 109)
            {
                Name[9] = '.';
                Name[10] = '.';
                Name[11] = 0;
                NCGDevice::GetTextExtent(&NameSize, Name, 0, 0, 0, 0, 0);
            }
        }
        DrawString(Device, 15, 29, 0xFFA2F9EC, Title);
        DrawString(Device, TitleSize.cx + 21, 29, NameColor, Name);
    }
    else
    {
        appSprintf(Name, TEXT("%s"), Target.GetName());
        NCGDevice::GetTextExtent(&NameSize, Name, 0, 0, 0, 0, 0);
        DrawString(Device, 15, 29, NameColor, Name);
    }

    // the clan: its label, then the crest and name after it (the name further right when the clan has a crest id),
    // else SysString 431 ("No Clan")
    SIZE ClanLabelSize;
    NCGDevice::GetTextExtent(&ClanLabelSize, *SysString(873), 0, 0, 0, 0, 0);
    PledgeInfo* Pledge = GConsole->GetPledgeInfo(Target.PledgeID);
    if (Pledge)
    {
        UTexture* CrestTex = nw::Field<UTexture*>(Pledge, 0xE0);   // C4: the header's PledgeInfo has it at +0xD0
        if (CrestTex)
            Device->DrawTexture(ClanLabelSize.cx + 20, 45, 16, 12, 0, 4, 16, 12, CrestTex, 0xFF, 1);
        const TCHAR* PledgeName = Pledge->Name;
        if (PledgeName && *PledgeName)
        {
            NCGDevice::GetTextExtent(&Size, PledgeName, 0, 0, 0, 0, 0);
            if (Pledge->CrestID > 0)
                DrawString(Device, ClanLabelSize.cx + 40, 45, 0xFFB09B79, PledgeName);
            else
                DrawString(Device, ClanLabelSize.cx + 20, 45, 0xFFB09B79, PledgeName);
        }
    }
    else
        DrawString(Device, ClanLabelSize.cx + 20, 45, 0xFFDCDCDC, *SysString(431));

    appSprintf(Buf, TEXT("%d %s"), Target.Level, GConsole->GetClassNameString(nw::Field<INT>(&Target, 0x210)));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, 35, 66, 0xFFB09B79, Buf);

    DrawRightNumber(Device, Buf, 114, 152, Target.PAtk);
    DrawRightNumber(Device, Buf, 114, 167, Target.PDef);
    DrawRightNumber(Device, Buf, 114, 182, Target.Accuracy);
    DrawRightNumber(Device, Buf, 114, 197, Target.Critical);
    DrawRightNumber(Device, Buf, 114, 212, Target.PAtkSpeed);
    DrawRightNumber(Device, Buf, 235, 152, Target.MAtk);
    DrawRightNumber(Device, Buf, 235, 167, Target.MDef);
    DrawRightNumber(Device, Buf, 235, 182, Target.Evasion);

    // the speeds, each times the speed rate: water, air and ground (run, walk) in one line under the label
    DrawString(Device, 137, 213, 0xFFA3A3A3, *SysString(432));   // "Speed"
    {
        INT RunWater = (INT)(Target.RunSpeedWater * Target.MoveSpeedRate);
        INT WalkWater = (INT)(Target.WalkSpeedWater * Target.MoveSpeedRate);
        INT RunAir = (INT)(nw::Field<INT>(&Target, 0x100) * Target.MoveSpeedRate);
        INT WalkAir = (INT)(nw::Field<INT>(&Target, 0x104) * Target.MoveSpeedRate);
        INT RunGround = (INT)(Target.RunSpeed * Target.MoveSpeedRate);
        INT WalkGround = (INT)(Target.WalkSpeed * Target.MoveSpeedRate);
        appSprintf(Buf, TEXT("%d,%d/%d,%d/%d,%d"), RunWater, WalkWater, RunAir, WalkAir, RunGround, WalkGround);
        NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
        DrawString(Device, 235 - Size.cx, 228, 0xFFB09B79, Buf);
        DrawString(Device, 235 - Size.cx, 240, 0xFFA3A3A3, TEXT("Water/Air/Ground"));
    }

    DrawRightNumber(Device, Buf, 235, 197, Target.CastSpeed);
    DrawStat(Device, Buf, 65, 256, Target.Str, UnknownCC[BONUS_Str], TEXT("%d(+%d)"));
    DrawStat(Device, Buf, 148, 256, Target.Dex, UnknownCC[BONUS_Dex], TEXT("%d (+%d)"));
    DrawStat(Device, Buf, 229, 256, Target.Con, UnknownCC[BONUS_Con], TEXT("%d (+%d)"));
    DrawStat(Device, Buf, 65, 272, Target.Int, UnknownCC[BONUS_Int], TEXT("%d (+%d)"));
    DrawStat(Device, Buf, 148, 272, Target.Wit, UnknownCC[BONUS_Wit], TEXT("%d (+%d)"));
    DrawStat(Device, Buf, 229, 272, Target.Men, UnknownCC[BONUS_Men], TEXT("%d (+%d)"));

    // from 999999 on the karma gets a "+" (the value itself is still printed)
    if (Target.Karma >= 999999)
        appSprintf(Buf, TEXT("%d+"), Target.Karma);
    else
        appSprintf(Buf, TEXT("%d"), Target.Karma);
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, 114 - Size.cx, 314, 0xFFB09B79, Buf);
    appSprintf(Buf, TEXT("%d / %d"), nw::Field<INT>(&Target, 0x208), nw::Field<INT>(&Target, 0x204));
    NCGDevice::GetTextExtent(&Size, Buf, 0, 0, 0, 0, 0);
    DrawString(Device, 235 - Size.cx, 314, 0xFFB09B79, Buf);
    DrawRightNumber(Device, Buf, 114, 330, (INT)nw::Field<SWORD>(&Target, 0x22E));
    DrawRightNumber(Device, Buf, 235, 330, (INT)nw::Field<SWORD>(&Target, 0x22C));

    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x1008abc0, NCGMDetailStatusWnd, OnPaint)

// 0x100891d0 - the server's answer about the player (NConsoleWnd's virtual), by value: the stock function copies 0x248
// bytes and pops them (ret 0x248)
void NCGMDetailStatusWnd::SetTarget(User InTarget)
{
    Target = InTarget;
}
NW_HOOK_METHOD(0x100891d0, NCGMDetailStatusWnd, SetTarget)

NW_CHECK_VTABLE(NCGMDetailStatusWnd, 0x101b5288, 74)
