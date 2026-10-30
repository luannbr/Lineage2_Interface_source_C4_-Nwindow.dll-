/*=============================================================================
    NHtmlParser.cpp - the HTML tokenizer of the page viewer: the tag table, NHtmlTag and
    NHtmlParser. Stock: 0x10025c40 .. 0x10025d32, 0x1002c6e0 .. 0x1002c917,
    0x1002ca80 .. 0x1002cbef, 0x1002d530 .. 0x1002e028, 0x1002e1b0 .. 0x1002e7a9,
    0x1002f170 .. 0x1002f469, 0x1002f550 .. 0x1002f5ae, 0x1002f640 .. 0x1002fced
    (the deleting destructors 0x1002c760, 0x1002d000 and 0x10025d40 are the compiler's).

    Parse reads the page into a list of NHtmlTag: a tag known to GHtmlTagInfo gets its
    HTMLTAG_* id and its known attributes (ids and values in two vectors; the last unknown
    one is kept as text), <extend> keeps its source up to </extend>, comments and unknown tags
    keep theirs up to the first '>' or '<', and everything else is text. A text run takes the
    <font color> tags inside it along: their colors and words go to NHtmlTag::Colors, the rest
    to Text, runs of blanks become one space and the entities and "&$n;" macros are replaced.
=============================================================================*/
#include "nwindow/NHtmlLayout.h"
#include "nwindow/NConsoleWnd.h"
#include "nwindow/NCrt.h"

#include <ctype.h>
#include <string.h>

// entities replaced in the text (stock tables, names ours): "&nbsp;", "&apos;", "&quot;",
// "&amp;", "&lt;", "&gt;" ("> ", sic), "&#160;", "&#10;". The value table has 7 entries: the
// eighth entity takes the next global, the "&$" mark (sic; the standalone table has it as an
// eighth value). Then the numbered macros "&$n;" (sysstring n), "&%n;" (castle n's name), "&^n;"
// (its location), "&*n;" (its description); C4 adds "&@n;" (NPC n's name) and "&#n;" (item n's name).
#if NW_HOOK_MODE
#define GHtmlEntityNames  (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x102369E4)))
#define GHtmlEntityValues (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x10236A04)))
#define GHtmlSysStringMark    NW_GLOBAL(const TCHAR*, 0x10236A20)
#define GHtmlCastleNameMark   NW_GLOBAL(const TCHAR*, 0x10236A24)
#define GHtmlCastleLocMark    NW_GLOBAL(const TCHAR*, 0x10236A28)
#define GHtmlCastleDescMark   NW_GLOBAL(const TCHAR*, 0x10236A2C)
#define GHtmlNpcNameMark      NW_GLOBAL(const TCHAR*, 0x10236A30)
#define GHtmlItemNameMark     NW_GLOBAL(const TCHAR*, 0x10236A34)
#else
extern const TCHAR* const GHtmlEntityNames[];
extern const TCHAR* const GHtmlEntityValues[];
extern const TCHAR* GHtmlSysStringMark;
extern const TCHAR* GHtmlCastleNameMark;
extern const TCHAR* GHtmlCastleLocMark;
extern const TCHAR* GHtmlCastleDescMark;
extern const TCHAR* GHtmlNpcNameMark;
extern const TCHAR* GHtmlItemNameMark;
#endif

enum { HTMLCOLOR_Sep = 0x1E };     // separator of NHtmlTag::Colors

static FORCEINLINE UBOOL IsBlank(TCHAR C)
{
    return C == ' ' || C == '\t' || C == '\r' || C == '\n';
}

/*-----------------------------------------------------------------------------
    NHtmlTagTable.
-----------------------------------------------------------------------------*/

// 0x1003aa80
NW_HOOK_CTOR(0x1003aa80, NHtmlTagTable)

// 0x1003aa90
NW_HOOK_DTOR(0x1003aa90, NHtmlTagTable)

// 0x1003aaa0 - binary search of the first 50 names
INT NHtmlTagTable::FindTag(const TCHAR* Name)
{
    INT Lo = 0;
    INT Hi = 51;   // C2: 51 tags (BR1 is new)
    do
    {
        INT Mid = Lo + (Hi - Lo) / 2;
        INT Cmp = wcscmp(GHtmlTagInfo[Mid].Name, Name);
        if (Cmp == 0)
            return Mid;
        if (Cmp < 0)
            Lo = Mid + 1;
        else
            Hi = Mid;
    } while (Hi > Lo);
    return -1;
}
NW_HOOK_METHOD(0x1003aaa0, NHtmlTagTable, FindTag)

// 0x1003ab00
INT NHtmlTagTable::FindAttr(INT Tag, const TCHAR* Name)
{
    if (Tag < 0)
        return -1;
    INT Hi = GHtmlTagInfo[Tag].NumAttrs;
    const TCHAR* const* Attrs = GHtmlTagInfo[Tag].Attrs;
    INT Lo = 0;
    while (Hi > Lo)
    {
        INT Mid = Lo + (Hi - Lo) / 2;
        INT Cmp = wcscmp(Attrs[Mid], Name);
        if (Cmp == 0)
            return Mid + 1;
        if (Cmp < 0)
            Lo = Mid + 1;
        else
            Hi = Mid;
    }
    return -1;
}
NW_HOOK_METHOD(0x1003ab00, NHtmlTagTable, FindAttr)

// C2: no vtable for NW_CHECK_VTABLE(NHtmlTagTable, 0x100de04c, 1)

/*-----------------------------------------------------------------------------
    NHtmlTag.
-----------------------------------------------------------------------------*/

// 0x10046600
NW_HOOK_CTOR(0x10046600, NHtmlTag)

// 0x100137b0 - std::uninitialized_copy over pointers (the Ucopy of the tag vector and of the point deque's map: a NULL
// destination is skipped); cdecl, returns the end of the destination
static void** UcopyPointers(void** First, void** Last, void** Dest)
{
    for (; First != Last; First++, Dest++)
    {
        if (Dest)
            *Dest = *First;
    }
    return Dest;
}
NW_HOOK_FUNC(0x100137b0, UcopyPointers)

// 0x10046510 - the members go in reverse order: the colors, the attribute values (each string, then the block), the
// attribute ids, the text
NW_HOOK_DTOR(0x10046510, NHtmlTag)


/*-----------------------------------------------------------------------------
    NHtmlParser.
-----------------------------------------------------------------------------*/

// 0x10043b90 (out-of-line copy for the stock unwind code)
NW_HOOK_DTOR(0x10043b90, NHtmlParserBase)

// 0x10043e90
NHtmlParser::NHtmlParser()
{
    File = NULL;
}
NW_HOOK_CTOR(0x10043e90, NHtmlParser)

// 0x10044390 - the file stays (FreeFile)
NHtmlParser::~NHtmlParser()
{
}
NW_HOOK_DTOR(0x10044390, NHtmlParser)

// 0x10043b10
bool NHtmlParser::IsAlnum(TCHAR C)
{
    return (NIswctype(C, _ALPHA) | NIswctype(C, _DIGIT)) != 0;   // VC7.0 table: 0xAA/B5/BA no letters
}
NW_HOOK_METHOD(0x10043b10, NHtmlParser, IsAlnum)

// 0x1003de70 - the ported callers use this body; the stock entry is hooked below
void NHtmlParser::ResetError()
{
    bOutOfMemory = 0;
}

// 0x10043b40 (the hook of the stock entry) - `mov [ecx+0xC], 0` and nothing else: the stock body leaves EVERY register alone, and the
// stock caller NHtmlParser::Parse (0x100410da, not ported) keeps a zero in ax across the call (`cmp word ptr [edi], ax` right after).
// So the hook is written as the stock's two instructions (a compiled function declared void would be refused by check_retval for
// that caller, and could touch eax); the declared INT only says that eax comes back as it went in. It is a member of a view of the
// parser (a member hook: `this` comes in ecx)
struct NHtmlParserStockView : public NHtmlParser
{
    INT ResetErrorStock();
};
NW_NAKED INT NHtmlParserStockView::ResetErrorStock()
{
    __asm
    {
        mov dword ptr [ecx + 0xC], 0
        ret
    }
}
NW_HOOK_METHOD(0x10043b40, NHtmlParserStockView, ResetErrorStock)

// 0x10043bd0
TCHAR* NHtmlParser::SkipSpaces(TCHAR* P)
{
    while (IsBlank(*P))
        P++;
    return P;
}
NW_HOOK_METHOD(0x10043bd0, NHtmlParser, SkipSpaces)

// 0x10043c00
TCHAR* NHtmlParser::SkipWord(TCHAR* P)
{
    while (*P && !IsBlank(*P) && *P != '>' && *P != '<')
        P++;
    return P;
}
NW_HOOK_METHOD(0x10043c00, NHtmlParser, SkipWord)

// 0x10043c50 - SkipWord's stop set and '=' too
TCHAR* NHtmlParser::SkipName(TCHAR* P)
{
    while (*P && !IsBlank(*P) && *P != '=' && *P != '>' && *P != '<')
        P++;
    return P;
}
NW_HOOK_METHOD(0x10043c50, NHtmlParser, SkipName)

// 0x10043ca0
void NHtmlParser::FreeFile()
{
    if (File)
    {
        appFree(File);
        File = NULL;
    }
}
NW_HOOK_METHOD(0x10043ca0, NHtmlParser, FreeFile)

// 0x10043cd0 - the whole file, two zero bytes after it
TCHAR* NHtmlParser::LoadFile(TCHAR* FileName)
{
    FreeFile();
    INT Size = GFileManager->FileSize(FileName);
    FArchive* Ar = GFileManager->CreateFileReader(FileName, 0, GNull);
    if (Ar)
    {
        TCHAR* Buf = (TCHAR*)appMalloc(Size + 2, TEXT("ParserTest3"));
        appMemset(Buf, 0, Size + 2);
        if (Buf)
        {
            Ar->Serialize(Buf, Size);
            File = Buf;
        }
        delete Ar;
    }
    return File;
}
NW_HOOK_METHOD(0x10043cd0, NHtmlParser, LoadFile)

// 0x100446d0 - after '<': the tag id in OutTag (index in GHtmlTagInfo; -1 a comment, -10 an
// unknown name, -100 none (a font tag, which stays in the text)) and where the name ends. The
// name, in upper case (the narrow toupper of the stock CRT, 'a'..'z' only), is left in Name.
TCHAR* NHtmlParser::ReadTagName(TCHAR* P, NHtmlTag* Tag, INT* OutTag)
{
    if (*P == '!')
    {
        if (P[1] == '-')
        {
            *OutTag = -1;
            P += 2;
            if (*P == '-')
                P++;
            return P;
        }
        P = SkipSpaces(P + 1);
    }
    if (*P == '/')
    {
        Tag->bOpen = 0;
        P = SkipSpaces(P + 1);
    }
    else
        Tag->bOpen = 1;
    if (P[0] == 'f' && P[1] == 'o' && P[2] == 'n' && P[3] == 't')
    {
        *OutTag = -100;
        return P;
    }
    if (!IsAlnum(*P))
    {
        *OutTag = -100;
        return P;
    }
    TCHAR* End = SkipWord(P);
    if (P == End)
    {
        *OutTag = -100;
        return P;
    }
    INT Len = End - P;
    TCHAR* Upper = (TCHAR*)appMalloc(Len * sizeof(TCHAR) + sizeof(TCHAR), TEXT("ParserTest0"));
    INT i;
    for (i = 0; i < Len; i++)
        Upper[i] = (TCHAR)toupper(P[i]);
    Upper[i] = 0;
    {
        NStlWString Tmp;
        Tmp.Assign(Upper, wcslen(Upper));
        Name.Assign(Tmp, 0, NStlWString::NPos);
    }
    appFree(Upper);
    *OutTag = Table.FindTag(Name.CStr());
    if (*OutTag == -1)
        *OutTag = -10;
    return End;
}
NW_HOOK_METHOD(0x100446d0, NHtmlParser, ReadTagName)

// 0x10044910 - the source of a comment or an unknown tag, from its '<' to the first '>'
// (included) or '<' (not)
TCHAR* NHtmlParser::ReadSource(TCHAR* P, NHtmlTag* Tag)
{
    TCHAR* Q = P + 1;
    while (*Q)
    {
        TCHAR C = *Q;
        if (C == '<')
            break;
        Q++;
        if (C == '>')
            break;
    }
    if (P != Q)
    {
        NStlWString Tmp;
        Tmp.Assign(P, Q - P);
        Tag->Text.Assign(Tmp, 0, NStlWString::NPos);
    }
    return Q;
}
NW_HOOK_METHOD(0x10044910, NHtmlParser, ReadSource)

// a numbered macro (inlined 4 times in the stock code): Mark, a number, ';'. A missing or zero
// number removes the macro but its ';' and ends the search; the search starts over after each
// replacement.
enum
{
    HTMLMACRO_SysString,
    HTMLMACRO_CastleName,
    HTMLMACRO_CastleLocation,
    HTMLMACRO_CastleDescription,
    HTMLMACRO_NpcName,
    HTMLMACRO_ItemName,
};
static FORCEINLINE void ReplaceMacro(NStlWString* Str, const TCHAR* Mark, INT Kind, TCHAR* Buf)
{
    UINT Pos = Str->Find(Mark, 0, wcslen(Mark));
    UINT End = Str->Find(TEXT(";"), Pos, wcslen(TEXT(";")));
    UINT Span = End - Pos;
    while (Pos < Str->Size && End < Str->Size)
    {
        if (Pos < Str->Size && End < Str->Size)
        {
            {
                NStlWString Num;
                Str->Substr(&Num, Pos + appStrlen(Mark), Span - appStrlen(Mark));
                appStrcpy(Buf, Num.CStr());
            }
            INT Id = appAtoi(Buf);
            if (!Buf[0] || !Id)
            {
                Str->Erase(Pos, Span);
                Pos = NStlWString::NPos;
                continue;
            }
            Str->Erase(Pos, Span + 1);
            const TCHAR* Text;
            switch (Kind)
            {
            case HTMLMACRO_SysString:
                appAtoi(Buf);          // read again, not used (sic)
                Text = *GL2GameData.SysString[Id];
                break;
            case HTMLMACRO_CastleName:
                Text = GConsole->GetCastleName(Id) ? GConsole->GetCastleName(Id) : TEXT("-");
                break;
            case HTMLMACRO_CastleLocation:
                Text = GConsole->GetCastleLocation(Id) ? GConsole->GetCastleLocation(Id)
                                                       : TEXT("-");
                break;
            case HTMLMACRO_CastleDescription:
                Text = GConsole->GetCastleDescription(Id) ? GConsole->GetCastleDescription(Id)
                                                          : TEXT("-");
                break;
            case HTMLMACRO_NpcName:
                Text = GL2GameData.GetNpcName(Id) ? GL2GameData.GetNpcName(Id) : TEXT("-");
                break;
            default:
                Text = GConsole->GetItemName(Id) ? GConsole->GetItemName(Id) : TEXT("-");
                break;
            }
            Str->Insert(Pos, Text, wcslen(Text));
        }
        Pos = Str->Find(Mark, 0, wcslen(Mark));
        End = Str->Find(TEXT(";"), Pos, wcslen(TEXT(";")));
        Span = End - Pos;
    }
}

// 0x100449e0 - an entity found in the text as it was before the first replacement is looked
// for again (stock limit). What lies between a macro's mark and its ';' is copied whole into
// one of two 512-character buffers of the stock frame: &$ &% &^ &* &@ into the first, which spills
// harmlessly into the second up to 1023 characters (C3 put &* into the second), and &# into the second, good up to 511;
// past that the stock overwrites its SEH record and return address. Here both have 4096 (the
// /GS check ends the process only past 4095, 8191 for the second): same output wherever the stock survives
void NHtmlParser::ReplaceEntities(NStlWString* Str)
{
    TCHAR Buf[4096];
    TCHAR DescBuf[4096];
    for (INT i = 0; i < 8; i++)
    {
        INT Len = appStrlen(GHtmlEntityNames[i]);
        UINT Size = Str->Size;
        UINT Pos = Str->Find(GHtmlEntityNames[i], 0, wcslen(GHtmlEntityNames[i]));
        while (Pos < Size)
        {
            Str->Erase(Pos, Len);
            Str->Insert(Pos, GHtmlEntityValues[i], wcslen(GHtmlEntityValues[i]));
            Pos = Str->Find(GHtmlEntityNames[i], 0, wcslen(GHtmlEntityNames[i]));
        }
    }
    ReplaceMacro(Str, GHtmlSysStringMark, HTMLMACRO_SysString, Buf);
    ReplaceMacro(Str, GHtmlCastleNameMark, HTMLMACRO_CastleName, Buf);
    ReplaceMacro(Str, GHtmlCastleLocMark, HTMLMACRO_CastleLocation, Buf);
    ReplaceMacro(Str, GHtmlCastleDescMark, HTMLMACRO_CastleDescription, Buf);
    ReplaceMacro(Str, GHtmlNpcNameMark, HTMLMACRO_NpcName, Buf);
    ReplaceMacro(Str, GHtmlItemNameMark, HTMLMACRO_ItemName, DescBuf);
}
NW_HOOK_METHOD(0x100449e0, NHtmlParser, ReplaceEntities)

// runs of blanks become one space, the leading ones go (inlined twice in ReadText; for the
// color runs a blank after a separator is dropped too)
static FORCEINLINE void CollapseBlanks(NStlWString& Str, UBOOL bColors)
{
    TCHAR* It = Str.CStr();
    UBOOL bSeen = 0;
    UBOOL bPending = 0;
    while (It != Str.CStr() + Str.Size)
    {
        if (IsBlank(*It))
        {
            Str.Erase(It - Str.CStr(), 1);
            if (bSeen && (!bColors || It[-1] != HTMLCOLOR_Sep))
                bPending = 1;
            continue;
        }
        if (!bSeen)
            bSeen = 1;
        if (bPending && It != Str.CStr() + Str.Size)
        {
            Str.Insert(It - Str.CStr(), 1, ' ');
            bPending = 0;
        }
        It++;
    }
}

// 0x10045550 - text up to a tag other than <font>/</font>
TCHAR* NHtmlParser::ReadText(TCHAR* P, NHtmlTag* Tag)
{
    TCHAR* End = P;
    if (*P == '<')
        End = P + 1;
    Unknown64.Assign(TEXT(""), wcslen(TEXT("")));
    UBOOL bFont = 0;
    if (*End)
    {
        for (;;)
        {
            if (*End == '<')
            {
                if (End[1] == '/' && End[2] == 'f' && End[3] == 'o' && End[4] == 'n' &&
                    End[5] == 't')
                    bFont = 1;
                else if (End[1] == 'f' && End[2] == 'o' && End[3] == 'n' && End[4] == 't')
                    bFont = 1;
                else
                    break;
            }
            End++;
            if (!*End)
                break;
        }
    }

    if (bFont)
    {
        // each <font color="c">word</font>: "c" (between separators) and the word go to
        // Colors, the tags leave the text
        NStlWString Run;
        Run.Assign(P, End - P);
        TCHAR Sep[2];
        appSprintf(Sep, TEXT("%c"), HTMLCOLOR_Sep);
        for (;;)
        {
            UINT Pos = Run.Find(TEXT("color="), 1, wcslen(TEXT("color=")));
            UINT Quote = Run.Find(TEXT("\""), Pos, wcslen(TEXT("\"")));
            if (Quote > 0 && Quote < Run.Size)
                Run.Replace(Quote, 1, Sep, wcslen(Sep));
            Quote = Run.Find(TEXT("\""), Quote, wcslen(TEXT("\"")));
            if (Quote > 0 && Quote < Run.Size)
                Run.Replace(Quote, 1, Sep, wcslen(Sep));
            UINT Gt = Run.Find(TEXT(">"), Pos, wcslen(TEXT(">")));
            UINT Close = Run.Find(TEXT("</font>"), Gt, wcslen(TEXT("</font>")));
            if (Pos > Run.Size)
                break;
            UINT Len = Gt - Pos;
            {
                NStlWString Color;
                Run.Substr(&Color, Pos + 6, Len - 6);
                Tag->Colors.Append(Color, 0, NStlWString::NPos);
            }
            {
                NStlWString Word;
                Run.Substr(&Word, Gt + 1, Close - Gt - 1);
                Tag->Colors.Append(Word, 0, NStlWString::NPos);
            }
            Run.Erase(Pos - 6, Len + 7);
            Run.Erase(Pos + (Close - Gt) - 7, 7);
            if (Pos > Run.Size)
                break;
        }
        NStlWString Copy(Run);
        Tag->Text.Assign(Copy, 0, NStlWString::NPos);
    }
    else
    {
        NStlWString Plain;
        Plain.Assign(P, End - P);
        Tag->Text.Assign(Plain, 0, NStlWString::NPos);
    }

    CollapseBlanks(Tag->Text, 0);
    if (bFont)
        CollapseBlanks(Tag->Colors, 1);
    ReplaceEntities(&Tag->Text);
    if (bFont)
        ReplaceEntities(&Tag->Colors);
    return End;
}
NW_HOOK_METHOD(0x10045550, NHtmlParser, ReadText)

// 0x10046660 - the source of <extend ...> up to its </extend> (included), or to the end
TCHAR* NHtmlParser::ReadExtend(TCHAR* P, NHtmlTag* Tag, INT TagIndex)
{
    NHtmlTag Inner;            // the names read on the way
    INT Id;
    TCHAR* Q;
    for (Q = P + 1; *Q; Q++)
    {
        if (*Q == '<')
        {
            TCHAR* N = Q + 1;
            while (IsBlank(*N))
                N++;
            Q = ReadTagName(N, &Inner, &Id);
            if (!Inner.bOpen && Id == TagIndex)
            {
                while (*Q && *Q != '>')
                    Q++;
                Q++;
                break;
            }
        }
    }
    if (P != Q)
    {
        NStlWString Src;
        Src.Assign(P, Q - P);
        Tag->Text.Assign(Src, 0, NStlWString::NPos);
    }
    return Q;
}
NW_HOOK_METHOD(0x10046660, NHtmlParser, ReadExtend)

// 0x100468f0
NHtmlTag* NHtmlParser::NewTag()
{
    NHtmlTag* Tag = (NHtmlTag*)appMalloc(sizeof(NHtmlTag), TEXT("ParserTest4"));
    if (Tag)
        Tag = ::new (static_cast<void*>(Tag)) NHtmlTag;
    if (!Tag)
    {
        bOutOfMemory = 1;
        return NULL;
    }
    return Tag;
}
NW_HOOK_METHOD(0x100468f0, NHtmlParser, NewTag)

// 0x100469b0
void NHtmlParser::DeleteTag(NHtmlTag* Tag)
{
    if (Tag)
    {
        Tag->~NHtmlTag();
        appFree(Tag);
    }
}
NW_HOOK_METHOD(0x100469b0, NHtmlParser, DeleteTag)

// 0x10046b10
void NHtmlParser::FreeTags(NHtmlTagList* Tags)
{
    if (Tags)
    {
        for (NHtmlTag** It = Tags->First; It != Tags->Last; It++)
            DeleteTag(*It);
        Tags->~NHtmlTagList();
        appFree(Tags);
    }
}
NW_HOOK_METHOD(0x10046b10, NHtmlParser, FreeTags)

// 0x10046b80 - the attributes up to '>' (skipped) or '<' (not): name[=value] with the value
// quoted or not; the names in upper case. Those the tag does not have go to Extra, each one
// replacing the previous (sic), and the last one to the tag's Colors.
TCHAR* NHtmlParser::ReadAttrs(TCHAR* P, NHtmlTag* Tag, INT TagIndex)
{
    Extra.Assign(TEXT(""), wcslen(TEXT("")));
    if (*P)
    {
        for (;;)
        {
            while (IsBlank(*P))
                P++;
            if (*P == '<')
                break;
            if (*P == '>')
            {
                P++;
                break;
            }
            Name.Erase(0, NStlWString::NPos);
            Value.Erase(0, NStlWString::NPos);
            UBOOL bQuoted = 0;
            if (*P != '=' && *P != '"')
            {
                TCHAR* End = SkipName(P);
                if (P != End)
                {
                    INT Len = End - P;
                    TCHAR* Upper =
                        (TCHAR*)appMalloc(Len * sizeof(TCHAR) + sizeof(TCHAR), TEXT("ParserTest1"));
                    INT i;
                    for (i = 0; i < Len; i++)
                        Upper[i] = (TCHAR)toupper(P[i]);
                    Upper[i] = 0;
                    {
                        NStlWString Tmp;
                        Tmp.Assign(Upper, wcslen(Upper));
                        Name.Assign(Tmp, 0, NStlWString::NPos);
                    }
                    appFree(Upper);
                    while (IsBlank(*End))
                        End++;
                    P = End;
                }
            }
            if (*P == '=' || *P == '"')
            {
                if (*P == '=')
                    P = SkipSpaces(P + 1);
                TCHAR* End;
                if (*P == '"')
                {
                    P++;
                    bQuoted = 1;
                    for (End = P; *End && *End != '"'; End++)
                        ;
                }
                else
                    End = SkipWord(P);
                if (P != End)
                {
                    NStlWString Tmp;
                    Tmp.Assign(P, End - P);
                    Value.Assign(Tmp, 0, NStlWString::NPos);
                }
                if (bQuoted)
                    End++;
                while (IsBlank(*End))
                    End++;
                P = End;
            }
            INT Attr;
            if (Name.Size && (Attr = Table.FindAttr(TagIndex, Name.CStr())) != -1)
            {
                _WORD Id = (_WORD)Attr;
                Tag->AttrIds.PushBack(Id);
                Tag->AttrValues.PushBack(Value);
            }
            else
            {
                if (Extra.Size)
                    Extra.Assign(TEXT(" "), wcslen(TEXT(" ")));
                if (Name.Size)
                    Extra.Assign(Name, 0, NStlWString::NPos);
                if (Value.Size)
                {
                    Extra.Append(TEXT("="), wcslen(TEXT("=")));
                    if (bQuoted)
                        Extra.Append(TEXT("\""), wcslen(TEXT("\"")));
                    Extra.Append(Value, 0, NStlWString::NPos);
                    if (bQuoted)
                        Extra.Append(TEXT("\""), wcslen(TEXT("\"")));
                }
            }
            if (!*P)
                break;
        }
    }
    if (Extra.Size)
        Tag->Colors.Assign(Extra, 0, NStlWString::NPos);
    return P;
}
NW_HOOK_METHOD(0x10046b80, NHtmlParser, ReadAttrs)

// 0x10046f80 - NULL when a tag cannot be allocated (the list is lost) or a reader returns NULL
NHtmlTagList* NHtmlParser::Parse(TCHAR* Text)
{
    NHtmlTagList* Tags = (NHtmlTagList*)appMalloc(sizeof(NHtmlTagList), TEXT("ParserTest5"));
    if (Tags)
        Tags = ::new (static_cast<void*>(Tags)) NHtmlTagList;
    if (!Tags)
        return NULL;
    ResetError();
    TCHAR* P = Text;
    if (!*P)
        return Tags;
    do
    {
        NHtmlTag* Tag = NewTag();
        if (!Tag)
            return NULL;
        TCHAR* Start = P;
        UBOOL bRead = 0;
        if (*P == '<')
        {
            TCHAR* N = P + 1;
            while (IsBlank(*N))
                N++;
            INT Id;
            TCHAR* Q = ReadTagName(N, Tag, &Id);
            if (Id > -100)
            {
                if (Id < 0)
                {
                    Tag->Tag = (Id == -1) + HTMLTAG_Unknown;    // HTMLTAG_Comment for -1
                    P = ReadSource(Start, Tag);
                }
                else
                {
                    Tag->Tag = GHtmlTagIds[Id];
                    if (Tag->Tag > HTMLTAG_Extend)
                        P = ReadAttrs(Q, Tag, Id);
                    else if (Tag->Tag != HTMLTAG_Extend)
                        P = ReadSource(Start, Tag);
                    else
                        P = ReadExtend(Start, Tag, Id);
                }
                bRead = Start != P;
            }
        }
        if (!bRead)
        {
            Tag->Tag = HTMLTAG_Text;
            Tag->bOpen = 1;
            P = ReadText(P, Tag);
        }
        if (Tag->Tag == HTMLTAG_Text && Tag->Text.Size == 0)
        {
            Tag->~NHtmlTag();
            appFree(Tag);
        }
        else
            Tags->PushBack(Tag);
        if (Tags->Size() > 0x7FFFFFFE)
            return Tags;
        if (!P)
        {
            FreeTags(Tags);
            return NULL;
        }
    } while (*P);
    return Tags;
}
NW_HOOK_METHOD(0x10046f80, NHtmlParser, Parse)

// C2: no vtable for NW_CHECK_VTABLE(NHtmlParser, 0x100ded78, 1)
// C2: no vtable for NW_CHECK_VTABLE(NHtmlParserBase, 0x100ded74, 1)
