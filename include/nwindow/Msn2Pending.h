#pragma once
// C4 stage 4, lane msn2: hooks that wait for the msn1 lane.
//
// The messenger bodies of this lane (NotificationProcess, SwitchboardSession, MsnLogin ...) call functions of the msn1 lane that are
// still `NW_UNPORTED()` (an int3) in the tree they were ported in: MsnLine::~MsnLine (every handler takes the line by value),
// FriendList::AddFriend / FindFriend (the same body as Msn::FindFriend), NMsnGroupList::AddGroup, Msn::NotifyError (folded with
// Msn::NotifyNewMail ...), Msn::NotifySessionMessage2, Msn::CreateSession, MSNSocket::Construct / SendCVR.
// tools/port/check_breakers.py refuses a hook that reaches one, so the hook line of each of these functions is written
// MSN2_AFTER_MSN1_<kind>(...) and registers nothing until MSN2_AFTER_MSN1 is 1.
//
// The bodies were proved with the switch on (verify_hooks, regcheck, verify_data, check_stockargs, verify_args ... green; only
// check_breakers named the msn1 callees). The coordinator sets MSN2_AFTER_MSN1 to 1 in the cycle that follows the merge of msn1 and
// reads check_breakers: a hook it still names waits for a callee that is still a stub (list: grep -n MSN2_AFTER_MSN1_ src/nwindow).

#ifndef MSN2_AFTER_MSN1
#define MSN2_AFTER_MSN1 0
#endif
#if MSN2_AFTER_MSN1
#define MSN2_AFTER_MSN1_VARARGS(va, Class, Method, Sig) NW_HOOK_VARARGS(va, Class, Method, Sig)
#define MSN2_AFTER_MSN1_METHOD(va, Class, Method) NW_HOOK_METHOD(va, Class, Method)
#define MSN2_AFTER_MSN1_CTOR_ARGS(va, Class, Params, Args) NW_HOOK_CTOR_ARGS(va, Class, Params, Args)
#define MSN2_AFTER_MSN1_FUNC(va, Func) NW_HOOK_FUNC(va, Func)
#else
#define MSN2_AFTER_MSN1_VARARGS(va, Class, Method, Sig)
#define MSN2_AFTER_MSN1_METHOD(va, Class, Method)
#define MSN2_AFTER_MSN1_CTOR_ARGS(va, Class, Params, Args)
#define MSN2_AFTER_MSN1_FUNC(va, Func)
#endif

// Hooks this lane wrote for functions that another lane hooks too (one address, one hook: a second NW_HOOK_* on an address is a smoke error).
// msn1 hooks MsnCommand::ToString (0x1014BEF0), MsnCommand::ParseArgs (0x1014E070) and MsnLine::FillAsyncVerbs / FillErrorCodes (0x1014C5A0 /
// 0x1014C8C0) in MsnCommand.cpp; rest1 hooks the Md5 class (0x1014B120 / 0x1014BA90 / 0x1014BB50). The bodies here (NChatMsg.cpp: NMsnCommand::ToWire /
// ParseArgs / InitListA / InitListB, NMd5Init / Update / Final) passed every judge on their own, and the MD5 ones were run against the stock
// code and hashlib (tools/port/emu/t_md5.py), but they register nothing unless MSN2_HOOK_DUPLICATES is 1 (set it only if the other lane's hook
// is dropped).
#ifndef MSN2_HOOK_DUPLICATES
#define MSN2_HOOK_DUPLICATES 0
#endif
#if MSN2_HOOK_DUPLICATES
#define MSN2_DUP_VARARGS(va, Class, Method, Sig) NW_HOOK_VARARGS(va, Class, Method, Sig)
#define MSN2_DUP_METHOD(va, Class, Method) NW_HOOK_METHOD(va, Class, Method)
#define MSN2_DUP_FUNC(va, Func) NW_HOOK_FUNC(va, Func)
#else
#define MSN2_DUP_VARARGS(va, Class, Method, Sig)
#define MSN2_DUP_METHOD(va, Class, Method)
#define MSN2_DUP_FUNC(va, Func)
#endif
