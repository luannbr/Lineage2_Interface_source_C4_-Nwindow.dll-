/*=============================================================================
    NCGaraDialogBox.cpp - the desktop's message box (C2 only). Stock: 0x100065f0 .. 0x100067cf,
    0x10006860 .. 0x10006bcf (Cancel), 0x10006c50 .. 0x10006d0d, 0x10006f90 .. 0x10007b1f,
    0x10007b20 .. 0x10008e77 (OK), 0x10008fa0 .. 0x10009091 and 0x10009500 (ShowWindow);
    NDialogInfo keeps 0x100064b0 .. 0x100065c0, 0x10006e60 and 0x100067d0.
=============================================================================*/
#include "nwindow/NCGaraDialogBox.h"
#include "nwindow/NDialogInfo.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCDialogBox.h"
#include "nwindow/NCEditBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCItemWnd.h"
#include "nwindow/NCMapCoverWnd.h"
#include "nwindow/NCMinimapWnd.h"
#include "nwindow/NCProgressCtrl.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCrt.h"
#include "nwindow/MsnUiStock.h"

enum { SYSMSG_ItemFlying = 179 };           // "cannot be dropped here" (name ours): SystemMsg[179]
enum { COLOR_Warning = 0xFFB09B79 };        // the system message color
enum { SOUND_TrashBasket = 4 };             // the table's 4 (name ours)

/*-----------------------------------------------------------------------------
    Construction.
-----------------------------------------------------------------------------*/

// 0x1000b1a0 - NControl's fields inline (the stock calls NCWnd's constructor), the strings of C3's two sounds
NCGaraDialogBox::NCGaraDialogBox()
{
    BackTex = NULL;
    OKButton = NULL;
    CancelButton = NULL;
    Progress = NULL;
    Owner = NULL;
    bProgress = 0;
    bInput = 0;
    Param1 = 0;
    Param2 = 0;
    DialogID = -1;
}
NW_HOOK_CTOR(0x1000b1a0, NCGaraDialogBox)


/*-----------------------------------------------------------------------------
    The small virtuals.
-----------------------------------------------------------------------------*/

// 0x10007b70 (vslot 25)
INT NCGaraDialogBox::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NControl::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND)
    {
        if ((INT)wParam == NID_DIALOG_OK)
            return OnOK() != 0;
        if ((INT)wParam == NID_DIALOG_CANCEL)
            return OnCancel() != 0;
        if ((INT)wParam == NID_DIALOG_KEYPAD)
            return OnKeypad(lParam) != 0;
        if ((INT)wParam == NID_DIALOG_EDIT)
            return OnEditNotify(lParam) != 0;
        if ((INT)wParam == NID_DIALOG_PROGRESS)
            return OnProgressEnd() != 0;
    }
    return 0;
}
NW_HOOK_METHOD(0x10007b70, NCGaraDialogBox, WindowProc)

// 0x10007c50 (vslot 29)
void NCGaraDialogBox::SetFocus()
{
    NCWnd::SetFocus();
    if (bInput)
        EditBox->SetFocus();
}
NW_HOOK_METHOD(0x10007c50, NCGaraDialogBox, SetFocus)

// 0x10007c70 (vslot 65) - Enter answers yes only where something was typed or the box is a notice
INT NCGaraDialogBox::OnKeyDown(WPARAM wParam, LPARAM lParam)
{
    if (wParam == VK_RETURN)
    {
        if (bInput == 0 && DialogID != 0x1D && DialogID != 0x1F && DialogID != 0x10)
        {
            OnCancel();
            return 1;
        }
        OnOK();
    }
    return 1;
}
NW_HOOK_METHOD(0x10007c70, NCGaraDialogBox, OnKeyDown)

// 0x10007d10 (vslot 49)
void NCGaraDialogBox::HideWindow()
{
    Reset();
    NCWnd::HideWindow();
}
NW_HOOK_METHOD(0x10007d10, NCGaraDialogBox, HideWindow)

// 0x100095c0 (vslot 48) - C3: the two sounds the info brought (names; "None" and empty ones are skipped by the console)
void NCGaraDialogBox::ShowWindow()
{
    NCWnd::ShowWindow();
    GConsole->PlaySoundName(*ShowSoundA);
    GConsole->PlayVoiceName(*ShowSoundB);   // NConsoleWnd::PlayVoiceName 0x1006c350 (C3 0x10060b00), the second sound's twin
    UpdateLayout();
}
NW_HOOK_METHOD(0x100095c0, NCGaraDialogBox, ShowWindow)

// 0x10007d20 (vslot 77) - Enter in the edit line (1), or on an empty one (0)
INT NCGaraDialogBox::OnEditNotify(INT Code)
{
    if (Code == 1)
    {
        OnOK();
        return 1;
    }
    if (Code == 0)
        OnCancel();
    return 1;
}
NW_HOOK_METHOD(0x10007d20, NCGaraDialogBox, OnEditNotify)

// 0x10008330 (vslot 79) - time is up
INT NCGaraDialogBox::OnProgressEnd()
{
    OnCancel();
    return 1;
}
NW_HOOK_METHOD(0x10008330, NCGaraDialogBox, OnProgressEnd)

// 0x10008d40 (vslot 78) - a key of the keypad: the digits are typed into the edit line, key 10 is the
// backspace, key 11 types the number of Unknown238 (the line emptied first), key 12 a zero
INT NCGaraDialogBox::OnKeypad(INT Key)
{
    TCHAR Text[128];
    if (Key >= 0 && Key < 9)
    {
        appSprintf(Text, TEXT("%d"), Key + 1);
        EditBox->InsertText(Text, 1);
        return 1;
    }
    if (Key == 9)
    {
        appSprintf(Text, TEXT("%d"), 0);
        EditBox->InsertText(Text, 1);
        return 1;
    }
    if (Key == 10)
    {
        SendTo(EditBox->Entry, WM_KEYDOWN, VK_BACK, 0);
        return 1;
    }
    if (Key == 11)
    {
        appSprintf(Text, TEXT("%d"), Unknown238);
        EditBox->ClearText();
        EditBox->InsertText(Text, 1);
        return 1;
    }
    if (Key == 12)
    {
        EditBox->ClearText();
        EditBox->InsertText(TEXT("0"), 1);
    }
    return 1;
}
NW_HOOK_METHOD(0x10008d40, NCGaraDialogBox, OnKeypad)

/*-----------------------------------------------------------------------------
    Creation, destruction and paint.
-----------------------------------------------------------------------------*/

// 0x100090f0 (vslot 37) - the CH3 skin: the back, the keypad's panel (right of the box, 108 wide) and the warning icon;
// the edit line at (50, 48); the 13 keys of the keypad drawn at X + Width (25 px steps from 7, the three keys of the
// right column 47 high); the progress bar at (17, 61) 222 x 6; the buttons 76 x 23 at y 75. C4: the same flow as C3
// (checked instruction by instruction against the C4 listing: the key table is at 0x10219d30, the ids are 0x1504,
// 0xF5 (NotifyID = the key), 0xF00DE, 0x1004 and 0x1005), only the offsets moved
INT NCGaraDialogBox::OnCreate()
{
    BackTex = FindTexture(TEXT("L2UI_ch3.dialog.system_back"));
    KeypadBackTex = FindTexture(TEXT("L2UI_ch3.Calculate.Calculate1_back"));
    IconTex = FindTexture(TEXT("L2UI_ch3.dialog.warningicon"));

    EditBox = new NCEditBox(3, NULL, 1, 150, 0);
    EditBox->ID = NID_DIALOG_EDIT;
    EditBox->SetPosition(50, 48, 156, 17);
    CreateChild(EditBox, NWS_VISIBLE);

    INT X = 7;
    INT Y = 7;
    INT W = 23;
    INT H = 23;
    for (INT i = 0; i < 13; i++)
    {
        if (i >= 0 && i < 9)
        {
            if (i % 3 == 0)
            {
                X = 7;
                if (i != 0)
                    Y += 25;
            }
            else
                X += 25;
        }
        else if (i == 9)
        {
            X = 7;
            Y += 25;
            W = 23;
        }
        else if (i == 10)
        {
            Y = 7;
            X = 82;
            H = 47;
            W = 23;
        }
        else if (i == 11)
        {
            X = 32;
            Y = 82;
            W = 47;
            H = 23;
        }
        else if (i == 12)
        {
            Y = 57;
            X = 82;
            H = 47;
            W = 23;
        }
        Keypad[i] = new NCButton(GKeypadTextures[i][0], GKeypadTextures[i][1], 0);
        Keypad[i]->ID = NID_DIALOG_KEYPAD;
        Keypad[i]->NotifyID = i;
        Keypad[i]->SetPosition((INT)((FLOAT)X + Width), Y, W, H);
        CreateChild(Keypad[i], NWS_VISIBLE);
    }

    Progress = new NCProgressCtrl(TEXT("L2UI_ch3.dialog.minibar_back"), TEXT("L2UI_ch3.dialog.minibar_time1"), 1);
    Progress->ID = NID_DIALOG_PROGRESS;
    Progress->SetPosition(17, 61, 222, 6);
    CreateChild(Progress, NWS_VISIBLE);
    Progress->SetMax(10000);
    Progress->Reset();

    OKButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    OKButton->ID = NID_DIALOG_OK;
    OKButton->SetPosition(51, 75, 76, 23);
    CreateChild(OKButton, NWS_VISIBLE);

    CancelButton = new NCButton(TEXT("L2UI_ch3.button.btn1_normal"), TEXT("L2UI_ch3.button.btn1_normalon"), 0);
    CancelButton->ID = NID_DIALOG_CANCEL;
    CancelButton->SetPosition(131, 75, 76, 23);
    CreateChild(CancelButton, NWS_VISIBLE);
    return 1;
}
NW_HOOK_METHOD(0x100090f0, NCGaraDialogBox, OnCreate)

// 0x10008860 (vslot 38) - guarded in C2; NCWnd's OnDestroy at the end (NControl's is not called)
INT NCGaraDialogBox::OnDestroy()
{
    guard(NCGaraDialogBox::OnDestroy);
    for (INT i = 0; i < 13; i++)
    {
        if (Keypad[i])
        {
            RemoveChildWindow(Keypad[i]->Entry);
            delete Keypad[i];
            Keypad[i] = NULL;
        }
    }
    if (OKButton)
    {
        RemoveChildWindow(OKButton->Entry);
        delete OKButton;
        OKButton = NULL;
    }
    if (CancelButton)
    {
        RemoveChildWindow(CancelButton->Entry);
        delete CancelButton;
        CancelButton = NULL;
    }
    if (EditBox)
    {
        RemoveChildWindow(EditBox->Entry);
        delete EditBox;
        EditBox = NULL;
    }
    if (Progress)
    {
        RemoveChildWindow(Progress->Entry);
        delete Progress;
        Progress = NULL;
    }
    NCWnd::OnDestroy();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x10008860, NCGaraDialogBox, OnDestroy)

// 0x100086a0 (vslot 62) - C3: the box stretched over the window, the warning icon at (11, 15), with an edit line the
// keypad's panel at x 256 (108 wide), and the message at (41, 16) wrapped to 200 px in 0xFFDCDCDC, its escaped
// newlines made real; the clip goes on before the origin
INT NCGaraDialogBox::OnPaint(NCGDevice* Device)
{
    guard(NCGaraDialogBox::OnPaint);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->BeginPaint(ScreenX, ScreenY);
    if (BackTex)
        Device->DrawTexture(0, 0, (INT)Width, (INT)Height, 0, 0, (INT)Width, (INT)Height, BackTex);
    if (IconTex)
        Device->DrawTexture(11, 15, 24, 24, 0, 0, 24, 24, IconTex);
    if (bInput != 0 && KeypadBackTex)
        Device->DrawTexture(256, 0, 108, (INT)Height, 0, 0, 108, (INT)Height, KeypadBackTex);
    FString Text;
    GConsole->ConvertNewLines((TCHAR*)*Message, Text);
    Device->DrawText(41, 16, 0xFFDCDCDC, *Text, 200, 0, 0, L2FONT_Normal, 0, 0, 0, 0, 0, 0, 0);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
    unguard;
}
NW_HOOK_METHOD(0x100086a0, NCGaraDialogBox, OnPaint)

/*-----------------------------------------------------------------------------
    The state: Reset, SetInfo, the buttons' texts, the layout.
-----------------------------------------------------------------------------*/

// 0x10007cc0 - ParamText and the params are kept. C3 clears the two new flags as well
void NCGaraDialogBox::Reset()
{
    DialogID = -1;
    bInput = 0;
    Owner = NULL;
    Message.Empty();
    Unknown238 = 0;
    bProgress = 0;
    Flag238 = 0;
    bLargeAmount = 0;
    EditBox->ClearText();
}
NW_HOOK_METHOD(0x10007cc0, NCGaraDialogBox, Reset)

// 0x10008280
void NCGaraDialogBox::SetButtonTexts(INT OKTextID, INT CancelTextID)
{
    if (OKButton)
    {
        if (*SysString(OKTextID) != NULL)
            OKButton->SetTextID(OKTextID);
    }
    if (CancelButton)
    {
        if (*SysString(CancelTextID) != NULL)
            CancelButton->SetTextID(CancelTextID);
    }
}
NW_HOOK_METHOD(0x10008280, NCGaraDialogBox, SetButtonTexts)

// 0x10008320
NWndEntry* NCGaraDialogBox::GetOwner()
{
    return Owner;
}
NW_HOOK_METHOD(0x10008320, NCGaraDialogBox, GetOwner)

// 0x100082f0 (ret 4)
void NCGaraDialogBox::SetEditText(const TCHAR* Text)
{
    if (bInput && Text)
        EditBox->InsertText(Text, 1);
}
NW_HOOK_METHOD(0x100082f0, NCGaraDialogBox, SetEditText)

// 0x100089d0 - C3: 256 x 108 (87 was C2's: the buttons at y 75 + 23 ended below the box; the box with a digits line: 364
// wide, with the keypad); the buttons at (51, 75) and
// (131, 75), a notice's OK alone and centered (half the width less 38); the FLOAT sizes
void NCGaraDialogBox::UpdateLayout()
{
    SetWindowSize(256.0f, 108.0f);
    // the stock code fills { X, Y, W, H } with GetLocalRect; SetScreenPos reads x and y
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0;
    Local.H = 0;
    GetLocalRect(&Local);
    OKButton->SetScreenPos(51, 75, (const POINT*)&Local);
    CancelButton->SetScreenPos(131, 75, (const POINT*)&Local);
    if (bInput == 0)
    {
        SetWindowSize(256.0f, 108.0f);
        EditBox->HideWindow();
        for (INT i = 0; i < 13; i++)
            Keypad[i]->HideWindow();
    }
    else if (Type == 3)
        SetWindowSize(364.0f, 108.0f);
    else
    {
        SetWindowSize(256.0f, 108.0f);
        for (INT i = 0; i < 13; i++)
            Keypad[i]->HideWindow();
    }
    if (DialogID == 0x1D || DialogID == 0x1F || DialogID == 0x10 || DialogID == 0x50)
    {
        CancelButton->HideWindow();
        OKButton->SetScreenPos((INT)(Width * 0.5f - 38.0f), 75, (const POINT*)&Local);
    }
    if (bProgress == 0)
        Progress->HideWindow();
    else
    {
        Progress->ShowWindow();
        if (DialogID != 0x13 && DialogID != 0x34)
            Progress->SetMax(10000);
        else
            Progress->SetMax(900000);
    }
}
NW_HOOK_METHOD(0x100089d0, NCGaraDialogBox, UpdateLayout)

// 0x10008b90 - Text2 wins over Text; an empty ParamText or a zero param keeps the box's. An open box is cancelled first (C4: not
// the confirmation 0x46 of Param1 0x5E6, which stays), the progress flag is at +0x1C of the info (+0x18 is a flag the box
// keeps at +0x258), the place and the two sounds come along (+0x78 .. +0x94). The stock asks the box for its local rectangle
// in between and ignores it
void NCGaraDialogBox::SetInfo(NDialogInfo* Info)
{
    if (DialogID != -1 && (DialogID != 0x46 || Param1 != 0x5E6))
        OnCancel();
    Reset();
    bLargeAmount = Info->bLargeAmount;
    DialogID = Info->ID;
    Type = Info->Type;
    EditBox->SetType(Info->Type);
    bInput = Info->bInput;
    Flag238 = Info->Flag18;
    Owner = Info->Owner;
    bProgress = Info->bProgress;
    if (bProgress == 0)
        Progress->HideWindow();
    else
    {
        Progress->ShowWindow();
        Progress->Reset();
        Progress->Start();
    }
    if (Info->Text2.Num() == 0)
    {
        if (Info->Text.Num() != 0)
            Message = Info->Text;
    }
    else
        Message = Info->Text2;
    if (Info->ParamText.Num() != 0)
        ParamText = Info->ParamText;
    if (Info->Param1)
        Param1 = Info->Param1;
    if (Info->Param2)
        Param2 = Info->Param2;
    NRectF Local;
    Local.X = 0;
    Local.Y = 0;
    Local.W = 0;
    Local.H = 0;
    GetLocalRect(&Local);
    Unknown238 = Info->Unknown74;
    Unknown22C[0] = ((INT*)&Info->Location)[0];
    Unknown22C[1] = ((INT*)&Info->Location)[1];
    Unknown22C[2] = ((INT*)&Info->Location)[2];
    ShowSoundA = Info->Text88;
    ShowSoundB = Info->Text94;
    if (bInput && Info->Text60.Num() != 0)
        SetEditText(*Info->Text60);
}
NW_HOOK_METHOD(0x10008b90, NCGaraDialogBox, SetInfo)

/*-----------------------------------------------------------------------------
    The answers. OnOK: the amount typed is read first (0 .. 1,000,000, or 100,000,000 with
    bLargeAmount; 1 without an edit line or a text), then DialogID picks what the box does. The ids
    are C2's (C1's from 0x24 on moved up by four, with new ones in between).
-----------------------------------------------------------------------------*/

// a request with one number
#define VALUE_REQUEST(Request, Value)                                                          \
    {                                                                                          \
        L2ParamStack Params(10);                                                               \
        Params.PushBack((void*)(INT_PTR)(Value));                                              \
        GConsole->Request(Params);                                                             \
    }
// a request with ParamText and a number
#define NAMED_REQUEST(Request, Value)                                                          \
    {                                                                                          \
        L2ParamStack Params(10);                                                               \
        Params.PushBack((void*)*ParamText);                                                    \
        Params.PushBack((void*)(INT_PTR)(Value));                                              \
        GConsole->Request(Params);                                                             \
    }

// the name typed as the stock code measures it: bytes in the ANSI code page (C3: a 800 byte buffer, C2's was 512)
static FORCEINLINE INT AnsiLength(const TCHAR* Text)
{
    ANSICHAR Ansi[800];
    WideCharToMultiByte(CP_ACP, 0, Text, -1, Ansi, 800, NULL, NULL);
    return (INT)strlen(Ansi);
}

// the stock's own C runtime (the DLL links it statically): _wtoi64
#if NW_HOOK_MODE
#define NWtoi64(Text)   (((INT64(__cdecl*)(const TCHAR*))::nw::Addr(0x1017143E))(Text))
#else
#define NWtoi64(Text)   _wtoi64(Text)
#endif

// 0x10075990 (name ours; NConsoleWnd's, ret 4) - a notice box (dialog 0x1F, the desktop's main box) with one system
// message: the answer of a refused amount, the owner not taking it. The stock ignores this and reads GConsole
static void __fastcall ShowNoticeDialog(NConsoleWnd*, void*, INT MessageID)
{
    NDialogInfo Info;
    Info.bInput = 0;
    Info.ID = 0x1F;
    Info.bMainBox = 1;
    Info.SetSystemMessage(MessageID);
    GConsole->ShowDialog(&Info);
}
NW_HOOK_FUNC(0x10075990, ShowNoticeDialog)

// 0x10009090 (NCMinimapWnd's, ret 4; name ours) - the map shows the mark of the console's report; asked for, it starts with
// the list empty (OnPaint fills it)
struct NMinimapFns : public NCMinimapWnd   // stand-in: the stock reads ecx, so the hook is a method's
{
    __declspec(noinline) void SetShowMark(UBOOL bShow);
};
void NMinimapFns::SetShowMark(UBOOL bShow)
{
    bShowMark = bShow;
    if (bShow)
        MarkPos.Empty();
}
NW_HOOK_METHOD(0x10009090, NMinimapFns, SetShowMark)

// 0x100090C0 (NConsoleWnd's, ret 4; name ours) - the desktop's map cover, its map's mark; the stock tests the three links in
// a row and jumps to 0x10009090
struct NGaraConsoleFns : public NConsoleWnd   // stand-in for the three functions of the console below
{
    __declspec(noinline) void SetMapShowMark(UBOOL bShow);
    __declspec(noinline) void RemovePledgeCrestLarge();
    __declspec(noinline) void RegisterPledgeCrestLarge(L2ParamStack& Params);
};
void NGaraConsoleFns::SetMapShowMark(UBOOL bShow)
{
    NCConsole* Desktop = GetDesktop();
    if (Desktop)
    {
        NCMapCoverWnd* Cover = (NCMapCoverWnd*)Desktop->MapCoverWnd;
        if (Cover)
        {
            if (Cover->Map)
                ((NMinimapFns*)Cover->Map)->SetShowMark(bShow);
        }
    }
}
NW_HOOK_METHOD(0x100090C0, NGaraConsoleFns, SetMapShowMark)

// the large clan crest (new in C4): FFileManager slots the header does not declare (Delete 5, MakeDirectory 10) and the network
// handler's request (+0x1C0, next to the small crest's +0x1BC); the stock's own C runtime (the buffers never leave the function)
enum { FILEMANAGER_Delete = 5, FILEMANAGER_MakeDirectory = 10 };
enum { NETHANDLER_SetPledgeCrestLarge = 0x1C0 / 4 };
#if NW_HOOK_MODE
#define LargeCrestMalloc(Size)  (((void*(__cdecl*)(size_t))::nw::Addr(0x10171E01))(Size))
#define LargeCrestFree(Ptr)     (((void(__cdecl*)(void*))::nw::Addr(0x101722E3))(Ptr))
#else
#define LargeCrestMalloc(Size)  malloc(Size)   // the buffers never leave the function: the module's own C runtime
#define LargeCrestFree(Ptr)     free(Ptr)
#endif

// 0x100684D0 (NConsoleWnd's; name ours) - the large clan crest removed: the request with nothing
void NGaraConsoleFns::RemovePledgeCrestLarge()
{
    ::nw::VirtualCall<void>(Engine->NetHandler, NETHANDLER_SetPledgeCrestLarge, 0, 0);
}
NW_HOOK_METHOD(0x100684D0, NGaraConsoleFns, RemovePledgeCrestLarge)

// 0x100809D0 (NConsoleWnd's, ret 4; name ours) - the large clan crest from a .bmp file (the path given; the packet's second
// value goes to the DXT1 conversion): a 64 x 64 picture of 8 bits per pixel or less (the string 1478 when it does not fit,
// 528 when the file cannot be read), saved as ..\crest\bmp_crest.bmp, converted (the fourth and sixth arguments are 1), the
// dds read back, sent and deleted. As in RegisterPledgeCrest (0x10080BF0) a file that does not fit is not freed, nor its reader
void NGaraConsoleFns::RegisterPledgeCrestLarge(L2ParamStack& Params)
{
    TCHAR* Path = (TCHAR*)Params.Top();
    INT Extra = (INT)Params.Top();
    FArchive* Reader = GFileManager->CreateFileReader(Path, 0, GNull);
    if (!Reader)
    {
        ShowSystemMessage(0x210, COLOR_Warning);
        return;
    }
    INT Size = Reader->TotalSize();
    BYTE* File = (BYTE*)LargeCrestMalloc(Size);
    Reader->Serialize(File, Size);
    if (File && *(WORD*)File != 0x4D42)
    {
        ShowSystemMessage(0x5C6, COLOR_Warning);
        return;
    }
    BITMAPINFOHEADER* Info = (BITMAPINFOHEADER*)(File + 0xE);
    if (Info && (Info->biWidth != 0x40 || Info->biHeight != 0x40 || Info->biBitCount > 8))
    {
        ShowSystemMessage(0x5C6, COLOR_Warning);
        return;
    }
    Reader->Flush();
    delete Reader;
    if (::nw::VirtualCall<UBOOL>(GFileManager, FILEMANAGER_MakeDirectory, TEXT("..\\crest"), 0))
    {
        TCHAR BmpPath[32];
        TCHAR DdsPath[32];
        appSprintf(BmpPath, TEXT("..\\crest\\bmp_crest.bmp"));
        appSprintf(DdsPath, TEXT("..\\crest\\dxt1_crest.dds"));
        FArchive* Writer = GFileManager->CreateFileWriter(BmpPath, 0x1C, GNull);
        if (Writer)
        {
            Writer->Serialize(File, Size);
            Writer->Flush();
            delete Writer;
            INT DdsSize = 0;
            GCanvas->MakeBMPToDXT1(BmpPath, DdsPath, DdsSize, 1, Extra, 1);
            BYTE* Dds = (BYTE*)LargeCrestMalloc(DdsSize);
            Reader = GFileManager->CreateFileReader(DdsPath, 0, GNull);
            Reader->Serialize(Dds, DdsSize);
            Reader->Flush();
            delete Reader;
            ::nw::VirtualCall<void>(Engine->NetHandler, NETHANDLER_SetPledgeCrestLarge, DdsSize, (ANSICHAR*)Dds);
            LargeCrestFree(Dds);
            ::nw::VirtualCall<UBOOL>(GFileManager, FILEMANAGER_Delete, DdsPath, 0, 0);
        }
    }
    LargeCrestFree(File);
}
NW_HOOK_METHOD(0x100809D0, NGaraConsoleFns, RegisterPledgeCrestLarge)

// 0x10009600 (slot 75) - OK. The box closes afterwards unless noted ("return 1" before the end keeps it open or
// has closed it already). The amount typed is read as a 64 bit number at the top and only once (it goes to the cases in
// Amount); an item that is not adena or a stackable one (the console's 0x1007C360 asks the item table: 3 / 2) has no limit
// but the box's own, a refused amount shows the notice (0x53A / 0x559) and the box stays closed; the answers that need the
// owner's yes show the notice 0x559 when it refuses; the confirmations of the war (0x17, 0x19) and the alliance (0x32) ask
// a second question (0x4E, 0x4F, 0x4D) that makes the request itself. C4 is C3's with the console's requests at their C4
// addresses and slots, and new ids: 0x52 (a refusal through the console's request 0x10067F50), 0x53 / 0x54 (the large
// clan crest: the file name asked, then 0x100809D0), 0x55 (the large crest removed, 0x100684D0), 0x56 / 0x57 (the owner told,
// 0xF000001C / 0xF000001D)
INT NCGaraDialogBox::OnOK()
{
    if (bProgress)
        Progress->Stop();
    INT Amount = 1;
    if (bInput && EditBox->CommitText())
    {
        INT64 Typed = NWtoi64(EditBox->CommitText());
        if (((NConsoleLeftFns*)GConsole)->GetEtcItemStackKind(Flag238) == 3 || bLargeAmount != 0)    // 0x1007C360
        {
            if (Typed >= 0)
            {
                if (Typed > 2000000000)
                {
                    Reset();
                    ShowNoticeDialog(GConsole, NULL, 0x559);
                    return 1;
                }
                Amount = (INT)Typed;
            }
            else
                Amount = 0;
        }
        else
        {
            if (((NConsoleLeftFns*)GConsole)->GetEtcItemStackKind(Flag238) == 2)
            {
                if (Typed < 0)
                    Amount = 0;
                else if (Typed > 1000000)
                {
                    Reset();
                    ShowNoticeDialog(GConsole, NULL, 0x53A);
                    return 1;
                }
                else
                    Amount = (INT)Typed;
            }
            else if (Typed < 0)
                Amount = 0;
            else
                Amount = (INT)Typed;
        }
    }

    switch (DialogID)
    {
    case 0: // yes to the owner
    case 1:
        if (Owner)
            SendTo(Owner, 0xF0000003, 1, 0);
        break;
    case 2: // amounts for the owner (items, adena...); without an edit line, yes; the id comes along
    case 3:
    case 8:
    case 0x23:
        if (Owner)
        {
            if (bInput == 0)
                SendTo(Owner, 0xF0000003, 1, DialogID);
            else if (EditBox->CommitText())
                SendTo(Owner, 0xF0000003, Amount, DialogID);
        }
        break;
    case 4: // drop an item where the pawn stands (unless that is forbidden)
    {
        NItemRequest Request;
        Request.ObjectID = Param1;
        Request.Count = Amount;
        if ((GConsole->GetMyUser()->Pawn->Unknown690 & 1) == 0)
            GConsole->RequestDropItem(Request, *(FVector*)Unknown22C);
        else
            GConsole->GetDesktop()->AddSystemText(*GL2GameData.SystemMsg[SYSMSG_ItemFlying].Message, 0, 0xFFDCDCDC);
        break;
    }
    case 5: // destroy an item
    {
        NItemRequest Request;
        Request.ObjectID = Param1;
        Request.Count = Amount;
        GConsole->RequestDestroyItem(Request);
        GConsole->PlayInterfaceSound(SOUND_TrashBasket);
        break;
    }
    case 6: // crystallize an item
        GConsole->RequestCrystallizeItem(Param1, Amount);
        GConsole->PlayInterfaceSound(SOUND_TrashBasket);
        break;
    case 7: // trade request
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)1);
        GConsole->AnswerTradeRequest(Params);
        break;
    }
    case 0xA: // party invitation
        VALUE_REQUEST(RequestAnswerJoinParty, 1)
        break;
    case 0xB:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)EditBox->CommitText());
        GConsole->RequestCreatePledge(Params);
        break;
    }
    case 0xC:
        GConsole->RequestDismissPledge();
        break;
    case 0xE: // clan invitation
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)1);
        GConsole->RequestAnswerJoinPledge(Params);
        break;
    }
    case 0xF: // title for the selected clan member
    {
        User* Target = GConsole->GetSelectedActor();
        if (!Target || Target->Unknown8)
            break;
        INT Len = AnsiLength(EditBox->CommitText());
        if (!GConsole->HasInvalidChars((TCHAR*)EditBox->CommitText()) &&
            GConsole->IsValidName((TCHAR*)EditBox->CommitText()))
        {
            if (Len > 16)
            {
                GConsole->ShowSystemMessage(80, COLOR_Warning);
                EditBox->SetFocus();
                return 1;
            }
            L2ParamStack Params(10);
            Params.PushBack(Target->GetName());
            Params.PushBack((void*)EditBox->CommitText());
            GConsole->RequestGiveNickName(Params);
            break;
        }
        GConsole->ShowSystemMessage(204, COLOR_Warning);
        EditBox->ClearText();
        EditBox->SetFocus();
        return 1;
    }
    case 0x10: // clan crest notice: then the file name (dialog 0x11)
    {
        NDialogInfo Info;
        Info.Owner = Entry;
        Info.ID = 0x11;
        Info.bMainBox = 1;
        Info.bInput = 1;
        Info.Type = 0;
        Info.SetSystemMessage(208);
        GConsole->ShowDialog(&Info);
        return 1;
    }
    case 0x11: // the crest's file (C3: a second value, 0)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)EditBox->CommitText());
        Params.PushBack((void*)0);
        GConsole->RegisterPledgeCrest(Params);
        break;
    }
    case 0x12: // clan war: declare, answer, stop, answer
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)EditBox->CommitText());
        GConsole->RequestStartPledgeWar(Params);
        break;
    }
    case 0x13:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        Params.PushBack((void*)1);
        GConsole->RequestReplyStartPledgeWar(Params);
        break;
    }
    case 0x14:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)EditBox->CommitText());
        GConsole->RequestStopPledgeWar(Params);
        break;
    }
    case 0x15:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        Params.PushBack((void*)1);
        GConsole->RequestReplyStopPledgeWar(Params);
        break;
    }
    case 0x17: // surrender the war: the clan name typed, then the question (0x4E) with it
        if (EditBox->CommitText())
        {
            NDialogInfo Info;
            Info.ID = 0x4E;
            Info.Owner = Entry;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.SetParamText(EditBox->CommitText());
            Info.SetSystemMessage(0x4E4);
            GConsole->ShowDialog(&Info);
            return 1;
        }
        break;
    case 0x18: // expel a member
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        GConsole->RequestOustPledgeMember(Params);
        break;
    }
    case 0x19: // surrender personally: the question (0x4F) with the name typed
        if (EditBox->CommitText())
        {
            NDialogInfo Info;
            Info.ID = 0x4F;
            Info.Owner = Entry;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.SetParamText(EditBox->CommitText());
            Info.SetSystemMessage(0x4E5);
            GConsole->ShowDialog(&Info);
            return 1;
        }
        break;
    case 0x1A: // leave the clan
        GConsole->RequestWithdrawalPledge();
        break;
    case 0x1B: // restart / quit (the system menu)
        if (Owner)
            SendTo(Owner, 0xF0000003, 0x1B, 0);
        break;
    case 0x1C:
        if (Owner)
            SendTo(Owner, 0xF0000003, 0x1C, 0);
        break;
    case 0x1D: // disconnected: the game ends
        GConsole->QuitGame();
        break;
    case 0x1E: // delete a quest, the desktop's quest window forgets the selection
        GConsole->RequestDestroyQuest(Param1);
        ((NGaraConsoleFns*)GConsole)->SetMapShowMark(0);
        break;
    case 0x20: // revival
        GConsole->AnswerReviveRequest(1, Param1, Param2);
        break;
    case 0x21: // use an item
        GConsole->RequestUseItem(Param1);
        break;
    case 0x22: // delete a recipe
        GConsole->RequestRecipeItemDelete(Param1);
        break;
    case 0x24: // the recipe shop's message
        if (EditBox->CommitText())
        {
            const TCHAR* Text = EditBox->CommitText();
            if (Text && *Text)
                GConsole->RequestRecipeShopMessageSet((TCHAR*)Text);
        }
        break;
    case 0x25: // private store (sell) amounts; the box stays when the owner refuses
        if (Owner)
        {
            if (bInput == 0)
                SendTo(Owner, 0xF0000006, 1, 0);
            else if (EditBox->CommitText())
            {
                if (SendTo(Owner, 0xF0000006, Amount, 0))
                {
                    Reset();
                    HideWindow();
                    return 1;
                }
                if (DialogID != 0x25)
                    return 1;
                ShowNoticeDialog(GConsole, NULL, 0x559);
                return 1;
            }
        }
        break;
    case 0x26:
        if (Owner)
        {
            if (bInput != 0 && !EditBox->CommitText())
                break;
            if (!SendTo(Owner, 0xF0000007, bInput == 0 ? 1 : Amount, 0))
            {
                ShowNoticeDialog(GConsole, NULL, 0x559);
                return 1;
            }
        }
        break;
    case 0x27: // private store message
        if (EditBox->CommitText())
        {
            const TCHAR* Text = EditBox->CommitText();
            if (*Text)
                GConsole->SetPrivateStoreMsg((TCHAR*)Text);
        }
        break;
    case 0x28:
        if (Owner)
        {
            if (SendTo(Owner, 0xF0000008, 0, 0))
            {
                Reset();
                HideWindow();
                return 1;
            }
            if (DialogID != 0x28)
                return 1;
            ShowNoticeDialog(GConsole, NULL, 0x559);
            return 1;
        }
        break;
    case 0x29:
        if (Owner)
        {
            if (!SendTo(Owner, 0xF000000A, 0, 0))
            {
                ShowNoticeDialog(GConsole, NULL, 0x559);
                return 1;
            }
        }
        break;
    case 0x2A: // private store (buy) amounts
        if (Owner)
        {
            if (bInput == 0)
                SendTo(Owner, 0xF000000B, 1, 0);
            else if (EditBox->CommitText())
            {
                if (!SendTo(Owner, 0xF000000B, Amount, 0))
                    return 1;
                Reset();
                HideWindow();
                return 1;
            }
        }
        break;
    case 0x2B:
        if (Owner)
        {
            if (bInput == 0)
                SendTo(Owner, 0xF000000C, 1, 0);
            else if (EditBox->CommitText())
            {
                if (!SendTo(Owner, 0xF000000C, Amount, 0))
                {
                    ShowNoticeDialog(GConsole, NULL, 0x559);
                    return 1;
                }
            }
        }
        break;
    case 0x2C: // private store (buy) message
        if (EditBox->CommitText())
        {
            const TCHAR* Text = EditBox->CommitText();
            if (*Text)
                GConsole->SetPrivateStoreBuyMsg((TCHAR*)Text);
        }
        break;
    case 0x2D:
        if (Owner)
        {
            if (!SendTo(Owner, 0xF000000D, 0, 0))
                return 1;
            Reset();
            HideWindow();
            return 1;
        }
        break;
    case 0x2E:
        if (Owner)
        {
            if (!SendTo(Owner, 0xF000000F, 0, 0))
            {
                ShowNoticeDialog(GConsole, NULL, 0x559);
                return 1;
            }
        }
        break;
    case 0x2F: // alliance: crest (C3: a second value, 0)
        if (EditBox->CommitText())
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)EditBox->CommitText());
            Params.PushBack((void*)0);
            GConsole->RegisterAllyCrest(Params);
        }
        break;
    case 0x30: // dismiss
        GConsole->RequestDismissAlly();
        break;
    case 0x31: // leave
        GConsole->RequestWithdrawAlly();
        break;
    case 0x32: // expel a clan: the question (0x4D) with the name typed
        if (EditBox->CommitText())
        {
            NDialogInfo Info;
            Info.ID = 0x4D;
            Info.Owner = Entry;
            Info.bMainBox = 1;
            Info.bInput = 0;
            Info.SetParamText(EditBox->CommitText());
            Info.SetSystemMessage(0x4E3);
            GConsole->ShowDialog(&Info);
            return 1;
        }
        break;
    case 0x33: // alliance invitation
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)1);
        GConsole->RequestAnswerJoinAlly(Params);
        break;
    }
    case 0x34: // alliance war: start / stop asked by the other side
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        Params.PushBack((void*)1);
        GConsole->RequestReplyStartAllianceWar(Params);
        break;
    }
    case 0x35:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        Params.PushBack((void*)1);
        GConsole->RequestReplyStopAllianceWar(Params);
        break;
    }
    case 0x36: // give an item to the pet / take one back
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)(INT_PTR)Amount);
        GConsole->RequestGiveItemToPet(Params);
        break;
    }
    case 0x37:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)(INT_PTR)Amount);
        Params.PushBack((void*)0);
        GConsole->RequestGetItemFromPet(Params);
        break;
    }
    case 0x38: // drop an item where the cursor points (unless that is forbidden)
    {
        NItemRequest Request;
        Request.ObjectID = Param1;
        Request.Count = Amount;
        if ((GConsole->GetMyUser()->Pawn->Unknown690 & 1) == 0)
            GConsole->RequestDropItem(Request, ((UGameEngine*)GConsole->Engine)->MouseHitLocation);
        else
            GConsole->GetDesktop()->AddSystemText(*GL2GameData.SystemMsg[SYSMSG_ItemFlying].Message, 0, 0xFFDCDCDC);
        break;
    }
    case 0x39: // pet name
    {
        INT Len = AnsiLength(EditBox->CommitText());
        if (!GConsole->HasInvalidChars((TCHAR*)EditBox->CommitText()) &&
            GConsole->IsValidName((TCHAR*)EditBox->CommitText()))
        {
            if (Len > 16)
            {
                GConsole->ShowSystemMessage(80, COLOR_Warning);
                EditBox->SetFocus();
                return 1;
            }
            GConsole->RequestChangePetName((TCHAR*)EditBox->CommitText());
            break;
        }
        GConsole->ShowSystemMessage(591, COLOR_Warning);
        EditBox->ClearText();
        EditBox->SetFocus();
        return 1;
    }
    case 0x3A: // action 20, with the modifier keys held (C3: vslot 297, the attack key, in place of Ctrl)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)20);
        Params.PushBack((void*)(INT_PTR)::nw::VirtualCall<UBOOL>(GConsole, 319));
        Params.PushBack((void*)(INT_PTR)GConsole->Engine->Client->Viewports(0)->Input->KeyDown(IK_Shift));
        GConsole->RequestActionUse(Params);
        break;
    }
    case 0x3B: // friend request
        VALUE_REQUEST(RequestAnswerFriendInvite, 1)
        break;
    case 0x3C:
        if (Owner)
            SendTo(Owner, 0xF0000010, 0, 0);
        break;
    case 0x3D:
        if (Owner)
            SendTo(Owner, 0xF0000011, 0, 0);
        break;
    case 0x3E: // siege: register / cancel as attacker / defender (castle in Param1)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)1);
        Params.PushBack((void*)1);
        GConsole->RequestJoinSiege(Params);
        break;
    }
    case 0x3F:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)1);
        Params.PushBack((void*)0);
        GConsole->RequestJoinSiege(Params);
        break;
    }
    case 0x40:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)0);
        Params.PushBack((void*)1);
        GConsole->RequestJoinSiege(Params);
        break;
    }
    case 0x41:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)0);
        Params.PushBack((void*)0);
        GConsole->RequestJoinSiege(Params);
        break;
    }
    case 0x42: // siege: approve / refuse a defending clan
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)(INT_PTR)Param2);
        Params.PushBack((void*)1);
        GConsole->RequestConfirmSiegeWaitingList(Params);
        break;
    }
    case 0x43:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)(INT_PTR)Param2);
        Params.PushBack((void*)0);
        GConsole->RequestConfirmSiegeWaitingList(Params);
        break;
    }
    case 0x44:
    case 0x47:
        if (Owner)
            SendTo(Owner, 0xF0000003, 0, 0);
        break;
    case 0x45:
        GConsole->RequestUseItem(Param1);
        break;
    case 0x46: // a confirmation: Param1 and 1
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)1);
        GConsole->SendConfirmDlg(Params);
        break;
    }
    case 0x48: // take the title of the selected clan member away
    {
        User* Target = GConsole->GetSelectedActor();
        if (!Target || Target->Unknown8)
            break;
        L2ParamStack Params(10);
        Params.PushBack(Target->GetName());
        TCHAR Empty[2];
        Empty[0] = 0;
        Params.PushBack(Empty);
        GConsole->RequestGiveNickName(Params);
        break;
    }
    case 0x49:
        ((NConsoleLeftFns*)GConsole)->RemovePledgeCrest();          // 0x10068490
        break;
    case 0x4A: // GM: send a player home
    {
        FString Command(TEXT("sendhome "));
        Command += ParamText;
        L2ParamStack Params(10);
        Params.PushBack((void*)*Command);
        ::nw::VirtualCall<void>(GConsole, 194, &Params);        // NConsoleWnd::BuilderCommand
        break;
    }
    case 0x4B: // GM: recall a player
    {
        FString Command(TEXT("recall "));
        Command += ParamText;
        L2ParamStack Params(10);
        Params.PushBack((void*)*Command);
        ::nw::VirtualCall<void>(GConsole, 194, &Params);
        break;
    }
    case 0x4C:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        GConsole->RequestSurrenderAllianceWar(Params);
        break;
    }
    case 0x4D:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        GConsole->RequestOustAlly(Params);
        break;
    }
    case 0x4E:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        GConsole->RequestSurrenderPledgeWar(Params);
        break;
    }
    case 0x4F:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)*ParamText);
        GConsole->RequestSurrenderPersonally(Params);
        break;
    }
    case 0x50:
        appRequestExit(0);
        break;
    case 0x51: // the VIP item: the object, the shop, and the number typed in ParamText
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)(INT_PTR)Param2);
        Params.PushBack((void*)(INT_PTR)appAtoi(*ParamText));
        ::nw::VirtualCall<void>(GConsole, 232, &Params);        // NConsoleWnd::RequestBuyVIPItem
        break;
    }
    case 0x52:
        VALUE_REQUEST(RequestExAcceptJoinMPCC, 1)
        break;
    case 0x53: // the large clan crest: then the file name (dialog 0x54)
    {
        NDialogInfo Info;
        Info.Owner = Entry;
        Info.ID = 0x54;
        Info.bMainBox = 1;
        Info.bInput = 1;
        Info.Type = 0;
        Info.SetSystemMessage(208);
        GConsole->ShowDialog(&Info);
        return 1;
    }
    case 0x54: // the large crest's file (0x100809D0)
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)EditBox->CommitText());
        Params.PushBack((void*)0);
        ((NGaraConsoleFns*)GConsole)->RegisterPledgeCrestLarge(Params);
        break;
    }
    case 0x55:
        ((NGaraConsoleFns*)GConsole)->RemovePledgeCrestLarge();
        break;
    case 0x56:
        if (Owner)
            SendTo(Owner, 0xF000001C, 0, 0);
        break;
    case 0x57:
        if (Owner)
            SendTo(Owner, 0xF000001D, 0, 0);
        break;
    }
    Reset();
    HideWindow();
    if (EditBox)
        EditBox->KillFocus();
    KillFocus();
    return 1;
}
NW_HOOK_METHOD(0x10009600, NCGaraDialogBox, OnOK)

// a request to a console function, by the method that holds the C4 address (the console header's names of 0x1005d4c0 .. 0x1005d530 were
// shifted by one against the C2 source's: the addresses are the evidence, they are named beside each call)
#define VALUE_REQUEST_AT(Method, Value)                                                        \
    {                                                                                          \
        L2ParamStack Params(10);                                                               \
        Params.PushBack((void*)(INT_PTR)(Value));                                              \
        GConsole->Method(Params);                                                              \
    }
#define NAMED_REQUEST_AT(Method, Value)                                                        \
    {                                                                                          \
        L2ParamStack Params(10);                                                               \
        Params.PushBack((void*)*ParamText);                                                    \
        Params.PushBack((void*)(INT_PTR)(Value));                                              \
        GConsole->Method(Params);                                                              \
    }

// 0x10007e00 (slot 76) - Cancel: the refusals; the box closes. Dialog 0 answers the owner like 1, 0x46 (a confirmation sent
// with Param1 and 0) and, new in C4, 0x52 (a refusal through the console's request 0x10067F50). The console's requests are
// called by their C4 address (all take the parameter stack, ret 4), or by slot (7: 431, 0x20: 427, 0x34: 251, 0x35: 253)
INT NCGaraDialogBox::OnCancel()
{
    if (bProgress)
        Progress->Stop();
    if (EditBox)
        EditBox->KillFocus();
    switch (DialogID)
    {
    case 0:
    case 1:
        if (Owner)
            SendTo(Owner, NWM_DIALOG_RESULT, 0, 0);
        break;
    case 7:
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)0);
            ::nw::VirtualCall<void>(GConsole, 431, &Params);       // AnswerTradeRequest
        }
        break;
    case 0xA:
        VALUE_REQUEST_AT(RequestAnswerJoinParty, 0)          // 0x10068A90
        break;
    case 0xE:
        VALUE_REQUEST_AT(RequestAnswerJoinPledge, 0)         // 0x10068500
        break;
    case 0x13:
        NAMED_REQUEST_AT(RequestReplyStartPledgeWar, 0)      // 0x10068540
        break;
    case 0x15:
        VALUE_REQUEST_AT(RequestReplyStopPledgeWar, 0)       // 0x10068560; no clan name, unlike OK
        break;
    case 0x20:
        ::nw::VirtualCall<void>(GConsole, 427, 0, Param1, Param2);   // AnswerReviveRequest
        break;
    case 0x29:
        if (Owner)
            Owner->Wnd->HideWindow();
        break;
    case 0x33:
        VALUE_REQUEST_AT(RequestAnswerJoinAlly, 0)           // 0x100687A0
        break;
    case 0x34:
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)*ParamText);
            Params.PushBack((void*)0);
            ::nw::VirtualCall<void>(GConsole, 251, &Params);       // RequestReplyStartAllianceWar
        }
        break;
    case 0x35:
        {
            L2ParamStack Params(10);
            Params.PushBack((void*)*ParamText);
            Params.PushBack((void*)0);
            ::nw::VirtualCall<void>(GConsole, 253, &Params);       // RequestReplyStopAllianceWar
        }
        break;
    case 0x3B:
        VALUE_REQUEST_AT(RequestAnswerFriendInvite, 0)       // 0x100689B0
        // falls through: the owner is told too
    case 0x28:
        if (Owner)
            SendTo(Owner, 0xF0000009, 0, 0);
        break;
    case 0x46:
    {
        L2ParamStack Params(10);
        Params.PushBack((void*)(INT_PTR)Param1);
        Params.PushBack((void*)0);
        GConsole->SendConfirmDlg(Params);                         // NConsoleWnd::SendConfirmDlg 0x1006C410
        break;
    }
    case 0x52:
        VALUE_REQUEST_AT(RequestExAcceptJoinMPCC, 0)         // 0x10067F50
        break;
    }
    Reset();
    HideWindow();
    KillFocus();
    return 1;
}
NW_HOOK_METHOD(0x10007e00, NCGaraDialogBox, OnCancel)

#undef GARA_AMOUNT
#undef TEXT_REQUEST
#undef VALUE_REQUEST
#undef NAMED_REQUEST
#undef SIEGE_REQUEST

// 0x1000b2b0 - the four strings (the two sounds, the param text, the message) go, then NControl
NW_HOOK_DTOR(0x1000b2b0, NCGaraDialogBox)

NW_CHECK_VTABLE(NCGaraDialogBox, 0x101a11a0, 80)
