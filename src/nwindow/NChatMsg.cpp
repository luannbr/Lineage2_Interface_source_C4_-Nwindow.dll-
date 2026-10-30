/*=============================================================================
    NChatMsg.cpp - the messenger's wire records (C3 only; C3 addresses). Stock: the message's builders 0x10127110 ..
    0x10127980, the command's 0x10127420 .. 0x101276d0, the header map's Rehash 0x1011b220, destructor 0x1011ebe0 and lookups
    0x101279c0 / 0x101298b0 / 0x10129950 (the first stays stock), the arrays' instances 0x1011c510 / 0x1011d380 / 0x10129a80 /
    0x10129ad0 / 0x10127330, the pairs' 0x10127270 / 0x10119fc0, the command's 0x101295a0 / 0x10129770 / 0x10129810 and the
    message's constructor 0x1011ec50, setters 0x1011a050 .. 0x1011a0f0, copy 0x1012a5c0 and destructor 0x1012a510.
=============================================================================*/
#include "nwindow/NChatMsg.h"
#include "nwindow/Msn2Pending.h"

/*-----------------------------------------------------------------------------
    The headers.
-----------------------------------------------------------------------------*/

// 0x1014bd40 (ret 4) - the link, and the two strings copied
NMsnHeaderPair::NMsnHeaderPair(const NMsnHeaderPair& Src)
    : HashNext(Src.HashNext), Key(Src.Key), Value(Src.Value)
{
}
NW_HOOK_CTOR_ARGS(0x1014bd40, NMsnHeaderPair, (const NMsnHeaderPair& A), (A))

// 0x1013e840 - the value, then the key
NMsnHeaderPair::~NMsnHeaderPair()
{
}
NW_HOOK_DTOR(0x1013e840, NMsnHeaderPair)

// the stock's out-of-line instances of TArray<NMsnHeaderPair>: Remove(Index, Count) (UnTemplate.h's checks, the pairs'
// destructors, FArray::Remove), the destructor (Remove of all, ~FArray) and Empty(Slack)
NW_HOOK_METHOD(0x10141080, NMsnHeaderArray, Remove)
NW_HOOK_DTOR(0x10141ef0, NMsnHeaderArray)
NW_HOOK_METHOD(0x1014e550, NMsnHeaderArray, Empty)

// 0x1014be00 (cdecl) - UE2's `new(Array) T` for the header pairs: a pair added at the end, its address
__declspec(noinline) static void* __cdecl NMsnHeaderNew(size_t Size, TArray<NMsnHeaderPair>& Array)
{
    return operator new(Size, Array);
}
NW_HOOK_FUNC(0x1014be00, NMsnHeaderNew)

// The stock calls the array's Empty(Slack) (0x1014e550) as a function of its own; this is that out-of-line instance (the compiler would
// inline the template into every caller)
__declspec(noinline) static void NMsnHeaderEmpty(NMsnHeaderArray& Array, INT Slack)
{
    Array.Empty(Slack);
}

// 0x1014e5a0 (ret 4) - the array's assignment: unless it is the same array, Empty(the other's count) (0x1014e550, out of line) and
// a copy of each pair added at the end through `new(Array)` above. (A member of a helper type so that the hook takes `this` in
// ecx; the array and the helper are the same object.)
NMsnHeaderArray& NMsnHeaderArrayAssigner::Assign(const NMsnHeaderArray& Other)
{
    if (this != &Other)
    {
        NMsnHeaderEmpty(*this, Other.Num());
        for (INT i = 0; i < Other.Num(); i++)
            new (NMsnHeaderNew(sizeof(NMsnHeaderPair), *this)) NMsnHeaderPair(Other(i));
    }
    return *this;
}
NW_HOOK_METHOD(0x1014e5a0, NMsnHeaderArrayAssigner, Assign)

// 0x1013fba0 - UE2's TMapBase::Rehash for FString keys
void NMsnHeaderMap::Rehash()
{
    INT* NewHash = new (TEXT("HashMapHash")) INT[HashCount];
    for (INT i = 0; i < HashCount; i++)
        NewHash[i] = -1;
    for (INT i = 0; i < Pairs.Num(); i++)
    {
        NMsnHeaderPair& Pair = Pairs(i);
        INT iHash = NStrihash(*Pair.Key) & (HashCount - 1);
        Pair.HashNext = NewHash[iHash];
        NewHash[iHash] = i;
    }
    if (Hash)
        appFree(Hash);
    Hash = NewHash;
}
NW_HOOK_METHOD(0x1013fba0, NMsnHeaderMap, Rehash)

// 0x10143760 - the table freed and forgotten, then the pairs
NMsnHeaderMap::~NMsnHeaderMap()
{
    if (Hash)
        appFree(Hash);
    Hash = NULL;
    HashCount = 0;
}
NW_HOOK_DTOR(0x10143760, NMsnHeaderMap)

// 0x1014c490 (ret 8) - the value of the header (case-insensitive key, hashed by NStrihash), a copy; an empty string when there is none.
// Not hooked here: ChatWnd.cpp hooks this address with the same body (NChatHeaderMap::Lookup); this is the copy the message's own functions call
__declspec(noinline) FString NMsnHeaderMap::Lookup(const FString& Key)
{
    for (INT i = Hash[NStrihash(*Key) & (HashCount - 1)]; i != -1; i = Pairs(i).HashNext)
    {
        if (Pairs(i).Key == Key)
            return Pairs(i).Value;
    }
    return FString();
}

// 0x1014e420 (ret 0xC) - the header's value (the lookup 0x1014c490 finds it, an empty string when there is none): is there one.
// C4 stage 4: the `this` of this function and of GetHeader is the MESSAGE (the stock callers pass ecx = the message and the function
// does `add ecx, 0x28` before the lookup: the map is the Headers member). The header declares them on NMsnHeaderMap (the C3
// source did the lookup on `this` itself, which reads the message's first fields as a hash table): here `this` is cast back.
UBOOL NMsnHeaderMap::HasHeader(FString Key)
{
    NMsnHeaderMap* Map = &((NMsnMessage*)this)->Headers;
    FString Value = Map->Lookup(Key);
    return Value.Len() > 0;
}
typedef UBOOL (NMsnHeaderMap::*SigHasHeader)(FString);
NW_HOOK_VARARGS(0x1014e420, NMsnHeaderMap, HasHeader, SigHasHeader)

// 0x1014e380 (ret 0x10) - the value of the header (the lookup takes the key as a pointer to a copy; `this` is the message, see above)
FString NMsnHeaderMap::GetHeader(FString Key)
{
    NMsnHeaderMap* Map = &((NMsnMessage*)this)->Headers;
    FString Copy(*Key);
    return Map->Lookup(Copy);
}
typedef FString (NMsnHeaderMap::*SigGetHeader)(FString);
NW_HOOK_VARARGS(0x1014e380, NMsnHeaderMap, GetHeader, SigGetHeader)

/*-----------------------------------------------------------------------------
    NMsnMessage.
-----------------------------------------------------------------------------*/

// 0x101437d0 - the strings and the map empty (the kind and the color are left as they were)
NMsnMessage::NMsnMessage()
{
}
NW_HOOK_CTOR(0x101437d0, NMsnMessage)

// 0x1014f090 (ret 4) - the members made empty first; then the kind, the three strings, the headers (the pairs emptied, the map
// rebuilt empty, then the other message's pairs copied and the map rebuilt with its table size), the font name, the effects
// and the color. The pair arrays' Empty and operator= are the out-of-line instances
NMsnMessage::NMsnMessage(const NMsnMessage& Src)
{
    Kind = Src.Kind;
    Body = Src.Body;
    Unknown10 = Src.Unknown10;
    Cookie = Src.Cookie;
    NMsnHeaderEmpty(Headers.Pairs, 0);
    Headers.HashCount = 8;
    Headers.Rehash();
    ((NMsnHeaderArrayAssigner&)Headers.Pairs).Assign(Src.Headers.Pairs);
    Headers.HashCount = Src.Headers.HashCount;
    Headers.Rehash();
    FontName = Src.FontName;
    Effects = Src.Effects;
    Color = Src.Color;
}
NW_HOOK_CTOR_ARGS(0x1014f090, NMsnMessage, (const NMsnMessage& A), (A))

// 0x1014efe0 - the headers emptied (the pairs through the out-of-line Empty), the table size back to 8; the strings and the map
// go in reverse
NMsnMessage::~NMsnMessage()
{
    NMsnHeaderEmpty(Headers.Pairs, 0);
    Headers.HashCount = 8;
    Headers.Rehash();
}
NW_HOOK_DTOR(0x1014efe0, NMsnMessage)

// 0x1013e8b0 / 0x1011a0a0 / 0x1011a0f0 (ret 0xC) - the string given by value is stored; the argument is destroyed
void NMsnMessage::SetBody(FString Value)
{
    Body = Value;
}
void NMsnMessage::SetFontName(FString Value)
{
    FontName = Value;
}
void NMsnMessage::SetEffects(FString Value)
{
    Effects = Value;
}
typedef void (NMsnMessage::*SigSetString)(FString);
NW_HOOK_VARARGS(0x1013e8b0, NMsnMessage, SetBody, SigSetString)
NW_HOOK_VARARGS(0x1013e900, NMsnMessage, SetFontName, SigSetString)
NW_HOOK_VARARGS(0x1013e950, NMsnMessage, SetEffects, SigSetString)

// 0x1014bca0 (ret 0x10) - the header block of a payload of that content type
__declspec(safebuffers) FString NMsnMessage::MakeHeader(FString ContentType)
{
    TCHAR Buffer[1040];
    appSprintf(Buffer, TEXT("MIME-Version: 1.0\r\nContent-Type: %s\r\n"), *ContentType);
    return FString(Buffer);
}
typedef FString (NMsnMessage::*SigMakeHeader)(FString);
NW_HOOK_VARARGS(0x1014bca0, NMsnMessage, MakeHeader, SigMakeHeader)

// 0x1014bbe0 (ret 4) - a text of the talk: plain UTF-8 text with the font, its effects and the color ("X-MMS-IM-Format")
FString NMsnMessage::BuildText()
{
    TCHAR Buffer[1040];
    appSprintf(Buffer,
               TEXT("MIME-Version: 1.0\r\nContent-Type: text/plain; charset=UTF-8\r\nX-MMS-IM-Format: FN=%s; EF=%s; CO=%d; CS=0; PF=22\r\n\r\n%s"),
               *FontName, *Effects, Color, *Body);
    return FString(Buffer);
}
NW_HOOK_METHOD(0x1014bbe0, NMsnMessage, BuildText)

// 0x1014bc50 (ret 4) - the refusal of an invitation: its cookie and "Cancel-Code: REJECT"
__declspec(safebuffers) FString NMsnMessage::BuildCancel()
{
    TCHAR Buffer[1040];
    appSprintf(Buffer,
               TEXT("MIME-Version: 1.0\r\nContent-Type: text/x-msmsgsinvite; charset=UTF-8\r\n\r\nInvitation-Command: CANCEL\r\nInvitation-Cookie: %s\r\nCancel-Code: REJECT\r\n\r\n"),
               *Cookie);
    return FString(Buffer);
}
NW_HOOK_METHOD(0x1014bc50, NMsnMessage, BuildCancel)

// 0x1014c390 (ret 4) - the typing notice: the control header, "TypingUser: <account>" and an empty line
FString NMsnMessage::BuildTyping()
{
    FString Result = MakeHeader(FString(TEXT("text/x-msmsgscontrol")));
    Result += TEXT("TypingUser: ");
    Result += Body;
    Result += TEXT("\r\n\r\n");
    return Result;
}
NW_HOOK_METHOD(0x1014c390, NMsnMessage, BuildTyping)

// 0x1014c450 (ret 4) - the payload by kind: typing (1), cancel (3), else the text
FString NMsnMessage::Serialize()
{
    if (Kind == NMSN_Typing)
        return BuildTyping();
    if (Kind != NMSN_Unknown2 && Kind == NMSN_CancelInvite)
        return BuildCancel();
    return BuildText();
}
NW_HOOK_METHOD(0x1014c450, NMsnMessage, Serialize)

/*-----------------------------------------------------------------------------
    The text as the server gets it.
-----------------------------------------------------------------------------*/

// 0x10168c60 (cdecl) - the characters as UTF-8 (code page 65001, the terminator included) into a buffer of 0x1040 bytes
INT __cdecl NUtf8Encode(const WCHAR* Src, char* Dest)
{
    return WideCharToMultiByte(CP_UTF8, 0, Src, -1, Dest, 0x1040, NULL, NULL);
}
NW_HOOK_FUNC(0x10168c60, NUtf8Encode)
