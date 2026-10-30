#pragma once
// The messenger's wire records as the conversation windows build them (names ours; the layouts come from the stock
// constructors, copy functions, destructors and the builders 0x10127110 .. 0x10129ad0). An NMsnMessage is the payload of a
// "MSG" command (the text of a talk, a typing notice, an invitation to cancel): a kind, three strings, the MIME headers of
// the answer (a hash map of strings) and the font of the text. An NMsnCommand is the command line itself: its name, a
// transaction number and the arguments.

#include "nwindow/NChatTypes.h"
#pragma pack(push, 4)

/*-----------------------------------------------------------------------------
    The headers: a TMap<FString, FString> in UE2's layout (pairs of 0x1C bytes).
-----------------------------------------------------------------------------*/
struct NMsnHeaderPair
{
    INT HashNext;            // +0x00
    FString Key;             // +0x04
    FString Value;           // +0x10

    __declspec(noinline) NMsnHeaderPair(const NMsnHeaderPair& Src);  // 0x10127270 (ret 4)
    __declspec(noinline) ~NMsnHeaderPair();                          // 0x10119FC0
};
NW_ASSERT_SIZE(NMsnHeaderPair, 0x1C);

typedef TArray<NMsnHeaderPair> NMsnHeaderArray;    // Remove 0x10141080, destructor 0x10141EF0, Empty 0x1014E550, operator= 0x1014E5A0

// 0x1014E5A0 (ret 4): the array's assignment, a function of its own in the stock (a member of a helper type: the array and the helper are
// the same object)
struct NMsnHeaderArrayAssigner : public NMsnHeaderArray
{
    __declspec(noinline) NMsnHeaderArray& Assign(const NMsnHeaderArray& Other);
};

struct NMsnHeaderMap
{
    NMsnHeaderArray Pairs;           // +0x00
    INT* Hash;                       // +0x0C
    INT HashCount;                   // +0x10

    NMsnHeaderMap() : Hash(NULL), HashCount(8) { Rehash(); }
    __declspec(noinline) ~NMsnHeaderMap();                           // 0x1011EBE0: the table freed, then the pairs (0x1011D380)
    void Rehash();                                                   // 0x1013FBA0 (FString keys, hashed with NStrihash)
    // 0x1014C490 (ret 8: the hidden result, the key by reference): the value of the header (the key compared without case, hashed by
    // NStrihash), a copy; an empty string when there is none. (Hooked in ChatWnd.cpp as NChatHeaderMap::Lookup: the same body)
    FString Lookup(const FString& Key);
    // 0x1014E420 (ret 0xC, name ours): whether the header Key has a value (the lookup 0x1014C490 gives a non-empty string)
    UBOOL HasHeader(FString Key);
    // 0x1014E380 (ret 0x10, name ours): the value of the header Key (the lookup 0x1014C490 on a copy of the key)
    FString GetHeader(FString Key);
};
NW_ASSERT_SIZE(NMsnHeaderMap, 0x14);

/*-----------------------------------------------------------------------------
    NMsnMessage (0x58).
-----------------------------------------------------------------------------*/
enum
{
    NMSN_TextMessage = 0,    // the text of the talk (the default)
    NMSN_Typing = 1,         // "TypingUser: <account>"
    NMSN_Unknown2 = 2,       // builds like a text message
    NMSN_CancelInvite = 3,   // an invitation refused
};

struct NMsnMessage
{
    INT Kind;                // +0x00 NMSN_* (the constructor leaves it)
    FString Body;            // +0x04 the text (a typing notice: the account of the one who types)
    FString Unknown10;       // +0x10
    FString Cookie;          // +0x1C the invitation's cookie (NMSN_CancelInvite)
    NMsnHeaderMap Headers;   // +0x28 the MIME headers
    FString FontName;        // +0x3C the font of the text ("Arial")
    FString Effects;         // +0x48 the font effects code
    INT Color;               // +0x54 (the constructor leaves it)

    __declspec(noinline) NMsnMessage();                              // 0x1011EC50
    __declspec(noinline) NMsnMessage(const NMsnMessage& Src);        // 0x1012A5C0 (ret 4)
    __declspec(noinline) ~NMsnMessage();                             // 0x1012A510: the headers emptied, the strings

    void SetBody(FString Value);                                     // 0x1011A050 (ret 0xC)
    void SetFontName(FString Value);                                 // 0x1011A0A0 (ret 0xC)
    void SetEffects(FString Value);                                  // 0x1011A0F0 (ret 0xC)

    // the payload as the server gets it (hidden FString return; ret 4)
    FString Serialize();                                             // 0x10127980 by Kind
    FString BuildText();                                             // 0x10127110 MIME text/plain with the font ("X-MMS-IM-Format")
    FString BuildCancel();                                           // 0x10127180 text/x-msmsgsinvite, CANCEL / REJECT for the Cookie
    FString BuildTyping();                                           // 0x101278C0 text/x-msmsgscontrol, "TypingUser: <Body>"
    FString MakeHeader(FString ContentType);                         // 0x101271D0 (ret 0x10) "MIME-Version: 1.0 / Content-Type: <type>"
    // 0x1012A140 (ret 0xC) / 0x1012A270 (ret 0xC), the messenger protocol lane's (NMsnProtocol.cpp): the payload of a received MSG
    INT ParseHeader(FString Line);                                   // "Name: value" into Headers (1), 0 for a line without ": "
    void Parse(TArray<FString*> Lines);                              // the lines: the Kind, the headers and the text
};
NW_ASSERT_OFFSET(NMsnMessage, Cookie, 0x1C);
NW_ASSERT_OFFSET(NMsnMessage, Headers, 0x28);
NW_ASSERT_OFFSET(NMsnMessage, FontName, 0x3C);
NW_ASSERT_OFFSET(NMsnMessage, Effects, 0x48);
NW_ASSERT_OFFSET(NMsnMessage, Color, 0x54);
NW_ASSERT_SIZE(NMsnMessage, 0x58);

/*-----------------------------------------------------------------------------
    The command line ("<Name> <TransactionID> <argument> ...") is MsnCommand (MsnCommand.h) and the parsed line a socket hands over is
    MsnLine: the struct of this name that stood here (the chat lane's) was the same object as MsnLine without its two flags, with the
    same functions under other names; its functions (ToWire, GetLength, InListA/B, AddArg, AddIntArg, ParseArgs, InitListA/B) are
    MsnCommand's and MsnLine's now. The MD5 context is Md5Context (Md5.h).
-----------------------------------------------------------------------------*/

#pragma pack(pop)
