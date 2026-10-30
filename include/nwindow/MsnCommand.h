#pragma once
// MsnCommand: one line of the messenger's wire protocol (names ours; the db's "MSNSocket::MSNSocket" for its constructor is the
// C2 map's guess), and MsnLine, the line a socket received, with the tables the parser needs. The text is
// "VERB [TrID] arg arg...\r\n" (MsnCommand::ToString, stock 0x10127420).

#include "nwindow/NMsnProtocol.h"

class MsnCommand
{
public:
    FString Verb;                // +0x00 the verb ("USR", "XFR", "MSG"...), or the error number of a reply
    INT TrID;                    // +0x0C the transaction id, -1 for none
    TArray<FString*> Args;       // +0x10 the arguments, heap strings the command owns

    MsnCommand();                                            // 0x10129b30
    MsnCommand(const MsnCommand& Other);                     // 0x10129b50 (ret 4): the arguments are copied one by one
    MsnCommand(const FString& InVerb);                       // 0x1012e020 (ret 0xC, the string by value)
    ~MsnCommand();                                           // 0x10129c30: the argument strings freed, the array emptied
    void Release();                                          // 0x1012a0e0: the array emptied WITHOUT freeing the strings, then ~MsnCommand's body
    FString GetArg(INT Index);                               // 0x10127520 (ret 8: the hidden result, the index): a copy, empty when there is none
    INT GetArgInt(INT Index);                                // 0x10127560 (ret 4): appAtoi of the argument, -1 when there is none
    NMsnServer GetArgServer(INT Index);                      // 0x10127770 (ret 8): "host:port" of the argument, the default server when there is none
    INT GetArgCount();                                       // 0x101270F0 (name ours; `mov eax, [ecx+0x14]`, folded with any class that has a count there): the number of arguments
    FString ToString();                                      // 0x1014BEF0 (ret 4: the hidden result) the wire text "VERB TrID args" (C4 address; the C3 one was 0x10127420)

    // the functions the chat lane had written as methods of its own NMsnCommand (NChatMsg.h, now this class: one definition)
    INT GetLength();                                         // 0x1014C0C0: the characters of the verb and of the arguments (a space before each)
    void AddArg(FString Arg);                                // 0x1014E240 (ret 0xC, the string by value): a copy of the text appended to the arguments
    void AddIntArg(INT Value);                               // 0x1014E2E0 (ret 4): the number as "%d" appended to the arguments
    void ParseArgs(FString Text);                            // 0x1014E070 (ret 0xC, by value): the text cut at its first CRLF and split at the spaces into arguments
};
NW_ASSERT_OFFSET(MsnCommand, TrID, 0x0C);
NW_ASSERT_OFFSET(MsnCommand, Args, 0x10);
NW_ASSERT_SIZE(MsnCommand, 0x1C);

// A command a sender builds as a local: the stock destroys it with MsnCommand::Release (0x1012A0E0), which empties the argument array
// first, so the argument strings are never freed (the stock leaks them). The destructor body below does the same before
// ~MsnCommand runs.
class NMsnLocalCommand : public MsnCommand
{
public:
    NMsnLocalCommand(FString InVerb);                        // out of line, the verb by value: the senders build it in place
    NMsnLocalCommand(const MsnCommand& Other);               // out of line: the copy a sender hands to SendLine (by value, released there)
    ~NMsnLocalCommand();                                     // out of line: Args.Empty(), then ~MsnCommand
};

class MsnLine : public MsnCommand
{
public:
    TArray<FString*> AsyncVerbs; // +0x1C the verbs that come unprompted (MSG, NLN, FLN, BYE, OUT, BPR...): filled by 0x10127AD0
    TArray<FString*> ErrorCodes; // +0x28 the error numbers (200 ... 9xx): filled by 0x10127DF0
    INT bAsync;                  // +0x34 the line is one of AsyncVerbs (no transaction)
    INT bError;                  // +0x38 the line is an error number

    MsnLine();                                               // 0x10129ce0: the command part empty, both tables filled
    MsnLine(const MsnCommand& Command);                      // 0x10129d50 (ret 4): the command part copied, the tables and flags NOT touched
    MsnLine(const MsnLine& Other);                           // the by-value copy the sockets make for the virtuals: the same constructor
    ~MsnLine();                                              // 0x10129d80
    INT Parse(FString Raw);                                  // 0x10129EB0 (ret 0xC, the line by value): the text of a received line split into verb, transaction and
                                                             //   arguments; 0 for an empty line, else 1
    void FillAsyncVerbs();                                   // 0x1014C5A0: the ten verbs that come unprompted into AsyncVerbs
    void FillErrorCodes();                                   // 0x1014C8C0: the 80 server error numbers into ErrorCodes
    INT IsAsyncVerb(FString Text);                           // 0x1014C100 (ret 0xC, the text by value): is the text one of AsyncVerbs
    INT IsErrorCode(FString Text);                           // 0x1014C1A0 (ret 0xC, the text by value): is the text one of ErrorCodes
    // the two flags have out-of-line getters in the stock (0x10088580 bError, 0x1014BBD0 bAsync: folded with the getters of other classes)
};
NW_ASSERT_OFFSET(MsnLine, AsyncVerbs, 0x1C);
NW_ASSERT_OFFSET(MsnLine, ErrorCodes, 0x28);
NW_ASSERT_OFFSET(MsnLine, bAsync, 0x34);
NW_ASSERT_OFFSET(MsnLine, bError, 0x38);
NW_ASSERT_SIZE(MsnLine, 0x3C);
