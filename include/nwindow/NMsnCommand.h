#pragma once
// NMsnCommand - a command of the MSN protocol being built (name ours): the object the session's request functions (NMsnSession.cpp) make
// on their stack. A name ("ADD", "REM", "ADG", "RMG", "REG"), the transaction number and the parameters (texts, numbers written as "%d"),
// which the session then sends (a copy of the command goes to NMsnSender::SendLine 0x101607E0 by value, which releases it).
//
// There is ONE definition of the messenger's command: MsnCommand (MsnCommand.h, 0x1C bytes: the verb at +0x00, the transaction number at
// +0x0C, the arguments at +0x10; constructor 0x10152C20 (the verb by value), copy constructor 0x1014E620, Release 0x1014EBB0, AddArg
// 0x1014E240, AddIntArg 0x1014E2E0). The two lanes that ported the protocol had each written a struct of this name (this header's 0x1C-byte
// proxy that called the stock by address, and NChatMsg.h's with the fields); this is the dialogs' view of it: the local a request builds
// and releases as the stock does (NMsnLocalCommand), with the names the dialog code uses.

#include "nwindow/MsnCommand.h"
#pragma pack(push, 4)

struct NMsnCommand : public NMsnLocalCommand
{
    // 0x10152C20 (ret 0xC): the command of that name, no transaction yet
    NMsnCommand(const TCHAR* InName) : NMsnLocalCommand(FString(InName)) {}

    INT& TrId() { return TrID; }

    // 0x1014E240 (ret 0xC): a copy of the text is appended to the parameters
    void AddParam(const TCHAR* Text) { AddArg(FString(Text)); }
    void AddParam(const FString& Text) { AddArg(Text); }
    // 0x1014E2E0 (ret 4): the number as "%d"
    void AddParam(INT Number) { AddIntArg(Number); }
};
NW_ASSERT_SIZE(NMsnCommand, 0x1C);

#pragma pack(pop)
