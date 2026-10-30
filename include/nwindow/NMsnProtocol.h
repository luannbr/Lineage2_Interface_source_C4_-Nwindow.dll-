#pragma once
// The messenger's protocol layer (C3 only): the MSN Messenger client NCsoft built into the game (the protocol is MSNP8:
// "VER 1 MSNP8 CVR0", "USR ... TWN", "XFR ... NS", "ADD/REM/ADG/RMG", switchboard "ANS/CAL/JOI/MSG/BYE"). The windows on
// top of it (MsnWnd, ChatWnd...) belong to other lanes; this lane has the objects underneath:
//
//   Msn                  the manager (+4 the window, +8 the dispatch connection, +0xC the notification connection, +0x10
//                        the switchboard sessions by socket, +0x50 the contacts, +0x54 the user)
//   MSNSocket            the base of every connection (a socket, the server, the transaction counter, the pending requests)
//     DispatchProcess      the first connection (messenger.hotmail.com:1863: VER, CVR, USR, then XFR to a notification server)
//     NotificationProcess  the notification server connection (names ours: the stock has no string for it)
//     SwitchboardSession   one conversation (its own vtable 0x101A9AB0)
//   MsnCommand / MsnLine the lines of the wire (verb, transaction id, arguments) and the parsed line the sockets hand over
//   NMsnFriend, NMsnGroup, NMsnServer, NMsnContacts, FriendList  the contact data
//
// Every name is ours except the ones with a guard string (Msn::*, SwitchboardSession::*, DispatchProcess::ProcessXFR,
// FriendList::AddFriend/CompareFriend); the db's C2-mapped names for the other anonymous functions (MSNSocket::CloseSocket,
// NItemEntry::GetIcon, NCWnd::GetPosFlags...) are wrong and are replaced in db/names.csv.

#include "nwindow/NWindow.h"
#include "nwindow/NChatTypes.h"
#include "nwindow/NMsnGroup.h"

#if !NW_HOOK_MODE
#define _WINSOCK_DEPRECATED_NO_WARNINGS   // the stock calls WSAAsyncSelect, inet_addr, gethostbyname
#include <winsock2.h>   // the standalone imports ws2_32 itself (the Msn* wrappers below call the real functions)
#endif

/*-----------------------------------------------------------------------------
    Stock functions of the other lanes the protocol layer calls (msn2: MsnWnd and the trees, chat: ChatWnd and the string
    helpers, dlg: the dialogs, core), reached by address. Names ours.
-----------------------------------------------------------------------------*/
// (The stock addresses this layer used to call through a table of constants - the window's notices, the command's string helpers, the
// conversion helpers - are plain calls of the functions that hold them now: MsnWnd's members, MsnCommand's, DecodeMsnUrl, MsnWideToAnsi,
// MsnUtf8ToWide, MsnAnsiToWide, MsnStrReplaceImpl.)

/*-----------------------------------------------------------------------------
    NMsnFriend: a contact (0x44 bytes, passed by value to the window with ret 0x44).
-----------------------------------------------------------------------------*/
// the list bits of a contact in the server's LST answer (the tests are the stock's 0x1012C6D0 .. 0x1012C700)
enum { MSNLIST_FL = 1, MSNLIST_AL = 2, MSNLIST_BL = 4, MSNLIST_RL = 8 };

struct NMsnFriend
{
    INT Unknown00;           // +0x00 (never written by the constructors)
    INT GroupID;             // +0x04 the group the contact is in (the LST / ADD answers; the constructors write 0, LST gives -1 for none)
    FString PrevStatus;      // +0x08 the status before the last SetStatus
    FString Status;          // +0x14 "FLN" offline, "NLN" online... (the constructors write FLN)
    FString Account;         // +0x20 the account (e-mail), the key of the contact maps
    FString Name;            // +0x2C the friendly name as the server sends it (URL escaped)
    FString DisplayName;     // +0x38 the name unescaped (GetDisplayName)

    NMsnFriend() {}                                                  // 0x1011a140 (chat lane's, inline here): five empty strings, the numbers not written
    NMsnFriend(const FString& InAccount);                            // 0x1012c2c0 (ret 0xC, the string by value)
    NMsnFriend(const FString& InAccount, const FString& InName);     // 0x1012c380 (ret 0x18, both by value)
    NMsnFriend(const NMsnFriend& Other);                             // 0x1012c460 (ret 4)
    ~NMsnFriend();                                                   // 0x1012c530
    NMsnFriend& operator=(const NMsnFriend& Other);                  // 0x1012c5b0 (ret 4)
    FString GetDisplayName();                                        // 0x1012c610 (ret 4: the hidden result)
    void SetName(FString NewName);                                   // 0x1012c800 (ret 0xC)
    void SetStatus(FString NewStatus);                               // 0x1012c850 (ret 0xC)
};
NW_ASSERT_OFFSET(NMsnFriend, GroupID, 0x04);
NW_ASSERT_OFFSET(NMsnFriend, PrevStatus, 0x08);
NW_ASSERT_OFFSET(NMsnFriend, Status, 0x14);
NW_ASSERT_OFFSET(NMsnFriend, Account, 0x20);
NW_ASSERT_OFFSET(NMsnFriend, Name, 0x2C);
NW_ASSERT_OFFSET(NMsnFriend, DisplayName, 0x38);
NW_ASSERT_SIZE(NMsnFriend, 0x44);

// the list-bit tests of the LST answer (stdcall, one argument: the list bits)
INT __stdcall MsnListHasFL(INT Lists);    // 0x1012c6d0
INT __stdcall MsnListHasAL(INT Lists);    // 0x1012c6e0
INT __stdcall MsnListHasBL(INT Lists);    // 0x1012c6f0
INT __stdcall MsnListHasRL(INT Lists);    // 0x1012c700

/*-----------------------------------------------------------------------------
    NMsnGroup: a contact group (0x1C bytes, by value to the window with ret 0x1C).
-----------------------------------------------------------------------------*/
// (struct NMsnGroup: NMsnGroup.h, the one definition shared with the dialogs' headers; stage 5, sa-verify)

/*-----------------------------------------------------------------------------
    NMsnServer: the address of a server (0x10 bytes).
-----------------------------------------------------------------------------*/
struct NMsnServer
{
    FString Host;            // +0x00
    INT Port;                // +0x0C

    NMsnServer();                                                    // 0x10127350: messenger.hotmail.com, 1863
    NMsnServer(const FString& InHost, INT InPort);                   // 0x101273b0 (ret 0x10, the string by value)
};
NW_ASSERT_OFFSET(NMsnServer, Port, 0x0C);
NW_ASSERT_SIZE(NMsnServer, 0x10);

// the stock helpers (cdecl) of the other lanes, called by address; the strings go by value and the stock function
// destroys its copies, as the compiler's calling convention has it
INT __cdecl MsnUtf8ToWide(const char* In, TCHAR* Out);               // 0x10144260: MultiByteToWideChar(UTF-8) of the whole text into 0x1040 characters
inline void MsnUrlDecode(const TCHAR* In, TCHAR* Out)                // 0x10168CC0 (DecodeMsnUrl, NMsnGroup.cpp)
{
    DecodeMsnUrl(In, Out);
}
// 0x10168890 (cdecl, NMsnProtocol.cpp; hooked in ChatWnd.cpp as NMsnStrReplaceImpl: the same code): Str with every From replaced by To
void __cdecl MsnStrReplaceImpl(FString* Str, FString From, FString To);
inline void MsnStrReplace(FString* Str, const FString& From, const FString& To)
{
    MsnStrReplaceImpl(Str, From, To);
}

// UE2's TMap<INT, INT> as the stock lays it out: pairs of {chain, key, value}, then the hash (name ours)
struct NMsnIntPair
{
    INT HashNext;                // +0x00
    INT Key;                     // +0x04
    INT Value;                   // +0x08
};
NW_ASSERT_SIZE(NMsnIntPair, 0x0C);

struct NMsnIntMap
{
    TArray<NMsnIntPair> Pairs;   // +0x00
    INT* Hash;                   // +0x0C
    INT HashCount;               // +0x10

    __declspec(noinline) ~NMsnIntMap();                                               // 0x10140020: the hash freed and cleared, then the pair array
    INT Remove(const INT& Key);                                  // 0x1012CCD0 (ret 4): every pair of that key removed (from the end), the number removed
    __declspec(noinline) INT FindRef(const INT& Key);            // 0x101514A0 (ret 4): the value, 0 when there is none (a function of its own in the stock)
    __declspec(noinline) void Set(const INT& Key, const INT& Value);   // 0x10151770 (ret 8): the value stored under the key (a function of its own in the stock)
    void Rehash();                                               // 0x100626E0 (other lane)
};
NW_ASSERT_SIZE(NMsnIntMap, 0x14);

// Winsock through the stock image's own import thunks (each a `jmp [IAT]`, stdcall): the hook DLL does not import ws2_32, and
// linking it would move the import table the other lanes' functions are checked against. The functions are the ones the stock calls
// Shared::GetFirstDword_ICF 0x1016EC20 (3 bytes: `mov eax, [ecx]; ret`): the linker folded the getters of the first dword of
// every class that has one into this address (the person lists' forward list, [Lists + 0], among them). Hook mode calls the
// stock one, as before (too short to hook); the standalone has the function itself
#if NW_HOOK_MODE
FORCEINLINE void* MsnFirstDword(void* Object) { return ::nw::StockCallAt<void*>(0x1016EC20, Object); }
#else
FORCEINLINE void* MsnFirstDword(void* Object) { return *(void**)Object; }
#endif

// (the standalone calls the real imports: WSAAsyncSelect, WSAGetLastError, closesocket, recv, htonl of ws2_32)
inline INT MsnWSAAsyncSelect(UINT Socket, HWND Window, UINT Message, INT Events)   // thunk 0x10170B38
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)(UINT, HWND, UINT, INT);
    return ((Fn)::nw::Addr(0x10170B38))(Socket, Window, Message, Events);
#else
    return WSAAsyncSelect(Socket, Window, Message, Events);
#endif
}
inline INT MsnWSAGetLastError()                                                    // thunk 0x10170B02
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)();
    return ((Fn)::nw::Addr(0x10170B02))();   // C4: 0x10170B14 is gethostbyname (the C3 map was off by neighbours)
#else
    return WSAGetLastError();
#endif
}
inline INT MsnCloseSocket(UINT Socket)                                             // thunk 0x10170B44
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)(UINT);
    return ((Fn)::nw::Addr(0x10170B44))(Socket);
#else
    return closesocket(Socket);
#endif
}
inline INT MsnRecv(UINT Socket, char* Buffer, INT Length, INT Flags)               // thunk 0x10170B2C
{
#if NW_HOOK_MODE
    typedef INT(__stdcall * Fn)(UINT, char*, INT, INT);
    return ((Fn)::nw::Addr(0x10170B2C))(Socket, Buffer, Length, Flags);   // C4: 0x10170B3E is htonl
#else
    return recv(Socket, Buffer, Length, Flags);
#endif
}
inline DWORD MsnHtonl(DWORD Value)                                                 // thunk 0x10170B3E
{
#if NW_HOOK_MODE
    typedef DWORD(__stdcall * Fn)(DWORD);
    return ((Fn)::nw::Addr(0x10170B3E))(Value);
#else
    return htonl(Value);
#endif
}

// UE2's appStrihash of a text (the case-insensitive CRC the string maps hash their keys with): the stock's 0x10119B20, whose port is
// NStrihash (NChatTypes.cpp; not hooked, regcheck reads its partial cx as a register argument: every caller of the stock function is
// ported and uses this copy, so the stock one is dead, db/morto.csv)
DWORD NStrihash(const TCHAR* Data);
inline DWORD MsnStrHash(const TCHAR* Text)
{
    return NStrihash(Text);
}

// UE2's TMap<FString, FString> as the stock lays it out (name ours): pairs {chain, key, value} (0x1C bytes) and the hash
struct NMsnStrStrPair
{
    INT HashNext;            // +0x00
    FString Key;             // +0x04
    FString Value;           // +0x10

    NMsnStrStrPair(const TCHAR* InKey, const TCHAR* InValue);   // 0x101272D0 (ret 8)
};
NW_ASSERT_SIZE(NMsnStrStrPair, 0x1C);

struct NMsnStrStrMap
{
    TArray<NMsnStrStrPair> Pairs;   // +0x00
    INT* Hash;                      // +0x0C
    INT HashCount;                  // +0x10

    FString* Add(const TCHAR* Key, const TCHAR* Value);          // 0x10127A50 (ret 8): a new pair, its value
    FString* Set(const TCHAR* Key, const TCHAR* Value);          // 0x10129A00 (ret 8): the pair of that key gets the value (a new pair when there is none)
    void Rehash();                                               // 0x1011B220 (ret 0, the chat lane's)
};
NW_ASSERT_SIZE(NMsnStrStrMap, 0x14);

/*-----------------------------------------------------------------------------
    NMsnMessage: a MSG of a switchboard session (0x58 bytes, by value with ret 0x58). Defined in NChatMsg.h (the chat lane's: the
    constructor 0x1011EC50, the copy 0x1012A5C0 and the destructor 0x1012A510 are ported there); the parser 0x1012A270 (the MIME types
    text/plain, text/x-msmsgscontrol, text/x-msmsgsinvite, text/x-msmsgsprofile, text/x-msmsgsemailnotification) over the header
    lines (0x1012A140) is in NMsnProtocol.cpp. The header map is NMsnHeaderMap there; the same 0x14-byte map is NMsnStrStrMap here
    (the one with the Add / Set that this lane ported), MsnMessageHeaders() gives the message's as that type.
-----------------------------------------------------------------------------*/
#include "nwindow/NChatMsg.h"

NW_ASSERT_SIZE(NMsnMessage, 0x58);
inline NMsnStrStrMap& MsnMessageHeaders(NMsnMessage& Message) { return *(NMsnStrStrMap*)&Message.Headers; }
