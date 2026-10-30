#pragma once
// Md5 - RFC 1321's MD5 as the stock image has it (the login challenge of the messenger uses it: NotificationProcess calls
// Init / Update / Final through the stock addresses). The class only groups the six functions of 0x1014B120 .. 0x1014BBBF
// (static members, cdecl; the stock names come from their guard-less symbols: "Md5" is the db's class name).
//
// The context is the usual MD_CTX of 0x58 bytes: the state (four words), the count of bits (two words) and the block being
// filled (64 bytes). CopyWords and DecodeWords take their arguments in registers (the stock compiler's whole-program
// optimization), which the shims in Md5.cpp adapt.

#include "nwindow/NWindow.h"

struct Md5Context
{
    DWORD State[4];                  // +0x00 A, B, C, D
    DWORD BitCount[2];               // +0x10 low, high (Update adds the length times 8)
    BYTE Buffer[64];                 // +0x18 the block being filled
};
NW_ASSERT_SIZE(Md5Context, 0x58);

struct Md5
{
    // 0x1014B120 (cdecl, ret 0) - the four state words of the RFC, the count cleared
    static void Init(Md5Context* Ctx);
    // 0x1014BA90 (cdecl) - Length bytes more of the message: the block is transformed each time it fills up
    static void Update(Md5Context* Ctx, const BYTE* Data, UINT Length);
    // 0x1014BB50 (cdecl) - the padding and the length appended, the 16 bytes of the digest written, the context cleared
    static void Final(BYTE* Digest, Md5Context* Ctx);
    // 0x1014B1D0 (stock: the block in eax, the context pushed by the caller) - one block (RFC 1321's transform)
    static void Transform(Md5Context* Ctx, const BYTE* Block);
    // 0x1014B150 (stock: source eax, destination ecx, count edx) - Count bytes copied, in groups of four (Final's "Encode")
    static void CopyWords(const BYTE* Src, BYTE* Dest, UINT Count);
    // 0x1014B190 (stock: block eax, words edx, count ecx) - Count bytes read as little-endian words (Transform's "Decode")
    static void DecodeWords(const BYTE* Block, DWORD* Words, UINT Count);
};
