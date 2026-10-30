/*=============================================================================
    NConsoleWndText.cpp - the console's text services: the keyboard IME's kind, the checks
    of names and HTML / dialog input (bad words, characters the font lacks, '&'), the escaped
    newlines of the tool tips, the "$s1" and Korean particle ("%이") formatting of the system
    messages and the dialogs, and the clan and alliance crest uploads.
=============================================================================*/
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NConsoleWndExt.h"
#include "nwindow/NCIME.h"
#include "nwindow/NViewerData.h"

#include <imm.h>
#include <malloc.h>
#include <string.h>
#include <wchar.h>

// system message colors and lines
enum
{
    STR_CrestPledgeFormat = 211,        // "Can only register 16*12 sized bmp files of 256 colors."
    STR_FileNotFound = 528,
    STR_CrestAllyFormat = 529,          // "You may only register 8x12 *.bmp files with 256 colors."
};
static const DWORD SYSMSG_COLOR = 0xFFB09B79;

// The particles of the system messages in this client are Hangul syllables: "%" and one of  0xC774 (i / ga), 0xC744 (eul / reul),
// 0xC740 (eun / neun) or 0xC640 (gwa / wa) puts the form that fits the end of the word before (bNoFinal: it ends without a final
// consonant); with no word before the forms of a word without a final consonant, except that the first stays i. 0 for another code
static FORCEINLINE TCHAR NParticle(TCHAR Particle, UBOOL bNoFinal)
{
    if (Particle == 0xC774)
        return bNoFinal ? 0xAC00 : 0xC774;
    if (Particle == 0xC744)
        return bNoFinal ? 0xB97C : 0xC744;
    if (Particle == 0xC740)
        return bNoFinal ? 0xB294 : 0xC740;
    if (Particle == 0xC640)
        return bNoFinal ? 0xC640 : 0xACFC;
    return 0;
}
static FORCEINLINE TCHAR NParticleAlone(TCHAR Particle)
{
    if (Particle == 0xC774)
        return 0xC774;
    if (Particle == 0xC744)
        return 0xB97C;
    if (Particle == 0xC740)
        return 0xB294;
    if (Particle == 0xC640)
        return 0xC640;
    return 0;
}

// MakeSystemMessage's parameter types (names ours)
enum
{
    SYSMSGPARAM_Text,
    SYSMSGPARAM_Number,
    SYSMSGPARAM_Npc,                    // npc id + 1000000
    SYSMSGPARAM_Item,
    SYSMSGPARAM_Skill,                  // skill id, level
    SYSMSGPARAM_Castle,
};


/*-----------------------------------------------------------------------------
    The keyboard.
-----------------------------------------------------------------------------*/

// 0x10064b10 - the kind of the keyboard layout's IME: -1 for a language NWindow does not handle, English 1; Korean 1 in
// alphanumeric mode and 0 in hangul (or without a conversion status); Japanese 2 and Chinese 4 .. 12 by input method (the
// traditional ones by GLanguageType), 1 while their IME is closed; Thai 13. C4 (C3 0x1005a4b0 differs): the open status is
// asked of the context this function gets (C3: of GActiveIMC), and the context is released before every return.
INT NConsoleWnd::GetIMEType()
{
    char FileName[0x104];
    DWORD Conversion, Sentence;
    INT Type = 1;
    HKL Layout = GetKeyboardLayout(0);
    ImmGetIMEFileNameA(Layout, FileName, 0x103);
    HIMC Context = ImmGetContext(GViewportHWnd);
    switch (PRIMARYLANGID(LOWORD((DWORD_PTR)Layout)))
    {
    case LANG_KOREAN:
        Type = 0;
        if (ImmGetConversionStatus(Context, &Conversion, &Sentence) &&
            !(Conversion & IME_CMODE_NATIVE))
            Type = 1;
        break;
    case LANG_ENGLISH:
        Type = 1;
        break;
    case LANG_THAI:
        Type = 13;
        break;
    case LANG_JAPANESE:
        Type = 2;
        if (!ImmGetOpenStatus(Context))
            Type = 1;
        break;
    case LANG_CHINESE:
    {
        WORD Device = HIWORD((DWORD_PTR)Layout);
        if (Device == 0xE002)
            Type = 4;
        else if (Device == 0xE006)
            Type = 5;
        else if (Device == 0xE008)
            Type = 6;
        else if (!lstrcmpiA(FileName, "CINTLGNT.IME"))
            Type = 4;
        else if (GLanguageType == 3 && Device == 0xE020)
            Type = 7;
        else if (Device == 0xE00E)
            Type = 8;
        else if (Device == 0xE001)
            Type = 9;
        else if (Device == 0xE004)
            Type = 10;
        else if (GLanguageType == 4 && (Device == 0xE020 || Device == 0xE025))
            Type = 12;
        if (!ImmGetOpenStatus(Context))
            Type = 1;
        break;
    }
    default:
        Type = -1;
        break;
    }
    ImmReleaseContext(GViewportHWnd, Context);
    return Type;
}
NW_HOOK_METHOD(0x10064b10, NConsoleWnd, GetIMEType)

/*-----------------------------------------------------------------------------
    Checks of what the player types.
-----------------------------------------------------------------------------*/

// 0x1006a570 - 1 for an empty text or one with a character the default font lacks
UBOOL NConsoleWnd::HasInvalidChars(TCHAR* Text)
{
    if (!Text || !*Text)
        return 1;
    UL2Font** Fonts = GCanvas->L2Fonts;
    if (Fonts)
    {
        UL2Font* Font = Fonts[L2FONT_Normal];
        for (; *Text; Text++)
            if (!Font->GetChar(*Text))
                return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x1006a570, NConsoleWnd, HasInvalidChars)

// 0x1006a5d0 - 0 when the text holds a '&'
UBOOL NConsoleWnd::HasNoAmpersand(TCHAR* Text)
{
    if (Text)
        for (; *Text; Text++)
            if (*Text == '&')
                return 0;
    return 1;
}
NW_HOOK_METHOD(0x1006a5d0, NConsoleWnd, HasNoAmpersand)

// 0x1006a610 - the characters the default font lacks taken out of the text (the line breaks stay; the rest moves
// up over each; the length is not updated, the moves stop at the old end)
void NConsoleWnd::RemoveInvalidChars(TCHAR* Text)
{
    if (!Text)
        return;
    UL2Font** Fonts = GCanvas->L2Fonts;
    UL2Font* Font = *Fonts;
    if (!Fonts)
        return;
    INT Len = wcslen(Text);
    INT Next = 1;
    for (TCHAR* P = Text; *P;)
    {
        if (Font->GetChar(*P) || *P == 0xD || *P == 0xA)
        {
            Next++;
            P++;
            continue;
        }
        *P = 0;
        INT i = Next - 1;
        for (INT j = Next; j < Len; j++, i++)
            Text[i] = Text[i + 1];
        Text[i] = 0;
    }
}
NW_HOOK_METHOD(0x1006a610, NConsoleWnd, RemoveInvalidChars)

// 0x10072f40 - the word filter: the characters the font lacks are taken out of the text (C3: first; C2 did it after
// the copy), a lower case copy of the text (A-Z only) is made, then each bad word found in the copy is replaced, in
// the text and in the copy, by the sysstring line 740 (C1 had a fixed "-_-" and no copy: the match is
// case-sensitive on the words, so the data has them in lower case). The text grows (the callers' buffers are
// large); 1 when a word was found.
UBOOL NConsoleWnd::HasBadWord(TCHAR* Text)
{
    UBOOL bFound = 0;
    FString Before;
    FString Lowered;
    if (!Text)
        return 0;
    RemoveInvalidChars(Text);   // C3: before the lower case copy is made (C2: after it)
    for (INT c = 0; c < appStrlen(Text); c++)
    {
        TCHAR Char = Text[c];
        if (Char >= 0x41 && Char <= 0x5A)
            Char += 0x20;
        TCHAR One[20];
        _swprintf(One, TEXT("%c"), Char);
        Lowered += One;
    }
    for (INT i = 0; i < GL2GameData.ObsceneData.Num(); i++)
    {
        FL2ObsceneData* Bad = GL2GameData.ObsceneData(i);
        if (!Bad)
            continue;
        FString& Word = Bad->Word;
        if (!*Word)
            continue;
        TCHAR* Found = appStrstr(*Lowered, *Word);
        if (!Found)
            continue;
        bFound = 1;
        do
        {
            Before = Text;
            INT Pos = Found - *Lowered;
            INT WordEnd = appStrlen(*Word) + Pos;
            appSprintf(Text, TEXT("%s%s%s"), *Before.Left(Pos), *GL2GameData.SysString[740],
                       *Before.Right(Before.Len() - WordEnd));
            Lowered = Lowered.Left(Pos) + *GL2GameData.SysString[740] +
                      Lowered.Right(Lowered.Len() - WordEnd);
            (void)*Lowered;
            Found = appStrstr(*Lowered, *Word);
        } while (Found);
    }
    return bFound;
}
NW_HOOK_METHOD(0x10072f40, NConsoleWnd, HasBadWord)

// 0x10081c50 - a character or pet name: not empty, no bad word (which also takes the
// characters the font lacks out of it) and no NPC's name, whatever the case
UBOOL NConsoleWnd::IsValidName(TCHAR* Name)
{
    if (!Name || !*Name || HasBadWord(Name))
        return 0;
    NDataMap<NViewNpcData>* Npcs = GL2GameData.NpcTable;
    for (INT i = 0; i < Npcs->Pairs.Num(); i++)
    {
        TCHAR* NpcName = GL2GameData.GetNpcName(Npcs->Pairs(i).Key);
        if (NpcName && !_wcsicmp(Name, NpcName))
            return 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x10081c50, NConsoleWnd, IsValidName)

/*-----------------------------------------------------------------------------
    Formatting.
-----------------------------------------------------------------------------*/

// 0x1006b130 - the text with its escaped newlines ("\n", two characters) turned into CR LF
void NConsoleWnd::ConvertNewLines(TCHAR* Text, FString& Result)
{
    Result = Text;
    INT Len = Result.Len();
    INT Pos = Result.InStr(TEXT("\\n"));
    while (Pos != -1)
    {
        FString Line(Result);
        Result = Line.Left(Pos);
        Result += TEXT("\r\n");
        Result += Line.Mid(Pos + 2, Len - Pos - 2);
        Pos = Result.InStr(TEXT("\\n"));
    }
}
NW_HOOK_METHOD(0x1006b130, NConsoleWnd, ConvertNewLines)

// 0x1006a3d0 - a system message's text with its parameters: the first "$..." (three
// characters) gets Param1, the second Param2 (a "$" left over is dropped); "%" and a particle
// puts the particle's form for the character before (anything else after "%" is dropped with
// it). Out may not be Format.
void NConsoleWnd::FormatSystemMessage(TCHAR* Out, const TCHAR* Format, const TCHAR* Param1,
                                      const TCHAR* Param2)
{
    INT Len = 0;
    if (Format)
        while (Format[Len])
            Len++;
    INT OutLen = 0;
    UBOOL bParam1 = 0;
    UBOOL bParam2 = 0;
    for (INT i = 0; i < Len; i++)
    {
        TCHAR C = Format[i];
        if (C == '$')
        {
            if (!bParam1 && Param1)
            {
                for (const TCHAR* P = Param1; *P; P++)
                    Out[OutLen++] = *P;
                i += 2;
                bParam1 = 1;
            }
            else if (!bParam2 && Param2)
            {
                for (const TCHAR* P = Param2; *P; P++)
                    Out[OutLen++] = *P;
                i += 2;
                bParam2 = 1;
            }
        }
        else if (C == '%')
        {
            TCHAR Form = NParticle(Format[i + 1], HasNoFinalConsonant(Out[OutLen - 1]));
            if (Form)
                Out[OutLen++] = Form;
            i++;
        }
        else
            Out[OutLen++] = C;
    }
    Out[OutLen] = 0;
}
NW_HOOK_METHOD(0x1006a3d0, NConsoleWnd, FormatSystemMessage)

// 0x1006ea50 - the parameter's text: a copy of InText and its length; nothing for NULL
__declspec(noinline) void NSysMsgParam::Set(const TCHAR* InText)
{
    if (!InText)
        return;
    Text = new TCHAR[appStrlen(InText) + 1];
    appStrcpy(Text, InText);
    Len = appStrlen(Text);
}
NW_HOOK_METHOD(0x1006ea50, NSysMsgParam, Set)

// 0x1006eaa0 - the parameter's text: the number
__declspec(noinline) void NSysMsgParam::SetNumber(INT Value)
{
    TCHAR Number[1024];
    appSprintf(Number, TEXT("%d"), Value);
    Text = new TCHAR[appStrlen(Number) + 1];
    appStrcpy(Text, Number);
    Len = appStrlen(Text);
}
NW_HOOK_METHOD(0x1006eaa0, NSysMsgParam, SetNumber)

// 0x10081d20 - a system message with its parameters into Out (FormatDateTime): the message
// id, the count of parameters, then each as its type and value(s) (SYSMSGPARAM_*), gathered
// in SysMsgParams as the engine's parameter packets are (AddSystemMessageParam). "$sN" puts
// the N-th parameter (no check below 1); "%" and a particle puts the particle's form for the
// last parameter put - the first "%" before any "$" reads the parameter before the array.
// A parameter without text gets the forms of a word without a final consonant (NParticleAlone). Out is left alone for a
// message the table does not have; the parameters are freed either way.
void NConsoleWnd::MakeSystemMessage(L2ParamStack& Params, TCHAR* Out)
{
    INT MsgID = (INT)Params.Top();
    INT Count = (INT)Params.Top();
    for (INT n = Count; n > 0; n--)
    {
        NSysMsgParam Param;
        Param.Text = NULL;
        Param.Len = 0;
        switch ((INT)Params.Top())
        {
        case SYSMSGPARAM_Text:
            Param.Set((const TCHAR*)Params.Top());
            break;
        case SYSMSGPARAM_Number:
            Param.SetNumber((INT)Params.Top());
            break;
        case SYSMSGPARAM_Npc:
            Param.Set(GL2GameData.GetNpcName((INT)Params.Top() - 1000000));
            break;
        case SYSMSGPARAM_Item:
            Param.Set(GetItemName((INT)Params.Top()));
            break;
        case SYSMSGPARAM_Skill:
        {
            INT SkillID = (INT)Params.Top();
            INT Level = (INT)Params.Top();
            Param.Set(GetSkillName(SkillID, Level));
            break;
        }
        case SYSMSGPARAM_Castle:
            Param.Set(GetCastleName((INT)Params.Top()));
            break;
        }
        SysMsgParams.AddItem(Param);
    }

    TCHAR Buffer[1024] = {0};
    INT NumParams = SysMsgParams.Num();
    FL2SystemMsgData& Line = GL2GameData.SystemMsg[MsgID];
    INT OutLen = 0;
    INT LastParam = -1;
    if (Line.Unknown0 && MsgID >= 0 && MsgID < 3000)
    {
        BYTE MsgMem[sizeof(FL2SystemMsgData)];
        FL2SystemMsgData& Msg = *(FL2SystemMsgData*)MsgMem;
        ((NSystemMsgFns*)MsgMem)->CopyFrom(&Line);
        const TCHAR* Format = *Msg.Message;
        for (INT i = 0; i < Msg.Message.Len(); )
        {
            TCHAR C = Format[i];
            if (C == '$')
            {
                INT Index = Format[i + 2] - '1';
                LastParam = Index;
                if (Index < NumParams && ((NSysMsgParam*)SysMsgParams.GetData())[Index].Text)
                {
                    appStrcpy(&Buffer[OutLen], ((NSysMsgParam*)SysMsgParams.GetData())[Index].Text);
                    OutLen += ((NSysMsgParam*)SysMsgParams.GetData())[Index].Len;
                }
                i += 3;
            }
            else if (C == '%')
            {
                NSysMsgParam& Last = ((NSysMsgParam*)SysMsgParams.GetData())[LastParam];
                TCHAR Particle = Format[i + 1];
                TCHAR Form;
                if (Last.Text)
                    Form = NParticle(Particle, HasNoFinalConsonant(Last.Text[Last.Len - 1]));
                else
                    Form = NParticleAlone(Particle);
                if (Form)
                    Buffer[OutLen++] = Form;
                i += 2;
            }
            else
            {
                Buffer[OutLen++] = C;
                i++;
            }
        }
        Buffer[OutLen] = 0;
        appStrcpy(Out, Buffer);
        Msg.Message3.~FString();
        Msg.Message2.~FString();
        Msg.Message.~FString();
    }
    for (INT j = 0; j < NumParams; j++)
    {
        NSysMsgParam& Param = ((NSysMsgParam*)SysMsgParams.GetData())[j];
        if (Param.Text)
        {
            appFree(Param.Text);
            Param.Text = NULL;
        }
        Param.Len = 0;
    }
    SysMsgParams.Empty();
}
NW_HOOK_METHOD(0x10081d20, NConsoleWnd, MakeSystemMessage)

/*-----------------------------------------------------------------------------
    Crests.
-----------------------------------------------------------------------------*/

// FFileManager slots the header does not declare (Delete 5, MakeDirectory 10)
enum { FILEMANAGER_Delete = 5, FILEMANAGER_MakeDirectory = 10 };

// C4 slots of the network handler's crest requests (RequestSetPledgeCrest +0x1BC, RequestSetAllyCrest +0x208)
enum { NETHANDLER_SetPledgeCrest = 0x1BC / 4, NETHANDLER_SetAllyCrest = 0x208 / 4 };

// the stock's own C runtime (the buffers never leave these functions, the calls are the stock's): malloc, free and the memory copy that
// the engine's appMemcpy is not (cdecl)
#if NW_HOOK_MODE
#define CrestMalloc(Size)       (((void*(__cdecl*)(size_t))::nw::Addr(0x10171E01))(Size))
#define CrestFree(Ptr)          (((void(__cdecl*)(void*))::nw::Addr(0x101722E3))(Ptr))
#define CrestCopy(Dst, Src, N)  (((void(__cdecl*)(void*, const void*, size_t))::nw::Addr(0x10001CB0))(Dst, Src, N))
#else
#define CrestMalloc(Size)       malloc(Size)   // the module's own C runtime: the buffers never leave these functions
#define CrestFree(Ptr)          free(Ptr)
#define CrestCopy(Dst, Src, N)  memcpy(Dst, Src, N)
#endif

// The crest is not sent as the bmp: it is saved as ..\crest\bmp_crest.bmp, the canvas converts it to DXT1
// (..\crest\dxt1_crest.dds; the fourth argument of MakeBMPToDXT1 is 1 for the clan's and 0 for the alliance's, the
// fifth is the second value of the packet, the sixth - new in C4 - is 0), that file is read back and its bytes sent
// (RequestSetPledgeCrest / RequestSetAllyCrest) and the dds is deleted (the bmp stays). Nothing is sent when the folder
// cannot be made or the bmp cannot be written; the dds read is not checked. (Name ours.)
static FORCEINLINE void SendCrest(NConsoleWnd* Console, BYTE* Crest, INT CrestSize, INT Dxt1, UBOOL bAlly, INT Extra)
{
    if (!nw::VirtualCall<UBOOL>(GFileManager, FILEMANAGER_MakeDirectory, TEXT("..\\crest"), 0))
        return;
    TCHAR DdsPath[32];
    TCHAR BmpPath[32];
    appSprintf(BmpPath, TEXT("..\\crest\\bmp_crest.bmp"));
    appSprintf(DdsPath, TEXT("..\\crest\\dxt1_crest.dds"));
    FArchive* Writer = GFileManager->CreateFileWriter(BmpPath, 0x1C, GNull);
    if (!Writer)
        return;
    Writer->Serialize(Crest, CrestSize);
    Writer->Flush();
    delete Writer;
    INT DdsSize = 0;
    GCanvas->MakeBMPToDXT1(BmpPath, DdsPath, DdsSize, Dxt1, Extra, 0);
    BYTE* Dds = (BYTE*)CrestMalloc(DdsSize);
    FArchive* Reader = GFileManager->CreateFileReader(DdsPath, 0, GNull);
    Reader->Serialize(Dds, DdsSize);
    Reader->Flush();
    delete Reader;
    nw::VirtualCall<void>(Console->Engine->NetHandler, bAlly ? NETHANDLER_SetAllyCrest : NETHANDLER_SetPledgeCrest,
                          DdsSize, (ANSICHAR*)Dds);
    CrestFree(Dds);
    nw::VirtualCall<UBOOL>(GFileManager, FILEMANAGER_Delete, DdsPath, 0, 0);
}

// the file of a crest checked and rebuilt as the clan's or the alliance's picture (name ours): a .bmp of at most 0xC00
// bytes, 8 bits per pixel or less, CrestWidth x 12; the result is the 16 rows of CrestSize bytes - the first 12 copied
// from the file, the rest 0xFF - with the header's size and dimensions set (CrestWidth x 16, ImageSize bytes of pixels).
// A file that does not fit is not freed, nor its reader (the stock leaks them)
static FORCEINLINE void RegisterCrest(NConsoleWnd* Console, L2ParamStack& Params, INT CrestWidth, INT FormatMsg,
                                      INT CrestSize, INT ImageSize, UBOOL bAlly)
{
    TCHAR* Path = (TCHAR*)Params.Top();
    INT Extra = (INT)Params.Top();
    FArchive* Reader = GFileManager->CreateFileReader(Path, 0, GNull);
    if (!Reader)
    {
        Console->ShowSystemMessage(STR_FileNotFound, SYSMSG_COLOR);
        return;
    }
    INT Size = Reader->TotalSize();
    BYTE* File = (BYTE*)CrestMalloc(Size);
    Reader->Serialize(File, Size);
    if (File && (*(WORD*)File != 0x4D42 || ((BITMAPFILEHEADER*)File)->bfSize > 0xC00))
    {
        Console->ShowSystemMessage(FormatMsg, SYSMSG_COLOR);
        return;
    }
    BITMAPINFOHEADER* Info = (BITMAPINFOHEADER*)(File + 0xE);
    if (Info && (Info->biWidth != CrestWidth || Info->biHeight != 12 || Info->biBitCount > 8))
    {
        Console->ShowSystemMessage(FormatMsg, SYSMSG_COLOR);
        return;
    }
    BYTE* Crest = (BYTE*)CrestMalloc(CrestSize);
    appMemset(Crest, 0xFF, CrestSize);
    ((BITMAPFILEHEADER*)File)->bfSize = CrestSize;
    Info->biWidth = CrestWidth;
    Info->biHeight = 16;
    Info->biSizeImage = ImageSize;
    CrestCopy(Crest, File, 0xE);
    CrestCopy(Crest + 0xE, Info, 0x28);
    CrestCopy(Crest + 0x36, File + 0x36, 0x400);
    for (INT Row = 0; Row < 12; Row++)
        CrestCopy(Crest + 0x436 + Row * CrestWidth, File + 0x436 + Row * CrestWidth, CrestWidth);
    Reader->Flush();
    delete Reader;
    SendCrest(Console, Crest, CrestSize, bAlly ? 0 : 1, bAlly, Extra);
    CrestFree(File);
    CrestFree(Crest);
}

// 0x10080bf0 (ret 4) - the clan crest from a .bmp file (the path given; the packet's second value goes to the DXT1 conversion): 16 x 12,
// made into a 256-color 16 x 16 bmp of 0x536 bytes (the string 211 when the file does not fit, 528 when it cannot be read)
void NConsoleWnd::RegisterPledgeCrest(L2ParamStack& Params)
{
    RegisterCrest(this, Params, 16, STR_CrestPledgeFormat, 0x536, 0x100, 0);
}
NW_HOOK_METHOD(0x10080bf0, NConsoleWnd, RegisterPledgeCrest)

// 0x10080ec0 (ret 4) - the alliance crest: 8 x 12 made into 8 x 16 (0x4B6 bytes, the string 529 when the file does not fit)
void NConsoleWnd::RegisterAllyCrest(L2ParamStack& Params)
{
    RegisterCrest(this, Params, 8, STR_CrestAllyFormat, 0x4B6, 0x80, 1);
}
NW_HOOK_METHOD(0x10080ec0, NConsoleWnd, RegisterAllyCrest)
