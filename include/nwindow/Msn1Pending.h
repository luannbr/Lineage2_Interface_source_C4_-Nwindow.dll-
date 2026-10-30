#pragma once
// C4 stage 4, lane msn1: hooks that wait for the msn2 lane (the mirror of Msn2Pending.h).
//
// These functions of the lane own an NMsnMessage or an NMsnFriend, whose destructors are msn2's (NChatMsg.cpp: ~NMsnMessage 0x1014EFE0;
// NMsnProtocol.cpp: ~NMsnFriend 0x10150FF0) and were `NW_UNPORTED()` (an int3) in the tree this lane was ported in:
// Msn::NotifySessionMessage / NotifySessionMessage2 (0x101504D0 / 0x101506A0), MSNSocket::OnAsyncImpl (0x10164F20), ~ChatWnd (0x10144820) and
// ~AlarmWnd (0x1015E230). tools/port/check_breakers.py refuses a hook that reaches one, so the hook line of each of these is written
// MSN1_AFTER_MSN2_<kind>(...) and registers nothing until the tree has msn2's lane: MSN1_AFTER_MSN2 is 1 by itself when include/nwindow/Msn2Pending.h
// exists (the file msn2's branch adds), else 0. Set it by hand (-DMSN1_AFTER_MSN2=1) only to test.
//
// The bodies were proved in a copy of the tree with both lanes merged (main at the msn2 and items2 merges plus this branch) and the switches on:
// hooks 4935 installed / 4935 registered, 0 errors, verify 4646 ok / 289 explained / 0 unexplained, check_breakers 0, CHECKS: ALL OK.

#ifndef MSN1_AFTER_MSN2
#if defined(__has_include)
#if __has_include("nwindow/Msn2Pending.h")
#define MSN1_AFTER_MSN2 1
#endif
#endif
#endif
#ifndef MSN1_AFTER_MSN2
#define MSN1_AFTER_MSN2 0
#endif

#if MSN1_AFTER_MSN2
#define MSN1_AFTER_MSN2_VARARGS(va, Class, Method, Sig) NW_HOOK_VARARGS(va, Class, Method, Sig)
#define MSN1_AFTER_MSN2_DTOR(va, Class) NW_HOOK_DTOR(va, Class)
#else
#define MSN1_AFTER_MSN2_VARARGS(va, Class, Method, Sig)
#define MSN1_AFTER_MSN2_DTOR(va, Class)
#endif
