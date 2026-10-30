/*=============================================================================
    Md5.cpp - RFC 1321's MD5 (the login challenge of the messenger). Stock: 0x1014b120 .. 0x1014bbbf. Init / Update / Final are
    hooked (NotificationProcess::OnCommandImpl calls them by address); the transform and the two word copiers, which the stock
    compiler built with register arguments, stay stock (see the end of the file): their only callers are Update and Final.
=============================================================================*/
#include "nwindow/Md5.h"

#include <string.h>

// the RFC's PADDING (a 1 bit, then zeros): 64 bytes at 0x102A5B70 in the stock image (read-only; the standalone has its own copy)
#if NW_HOOK_MODE
#define MD5_PADDING ((const BYTE*)::nw::Addr(0x102a5b70))
#else
static const BYTE MD5_PADDING[64] = { 0x80 };
#endif

// 0x1014b120 (cdecl) - the RFC's initial state, the bit count cleared
void Md5::Init(Md5Context* Ctx)
{
    Ctx->BitCount[1] = 0;
    Ctx->BitCount[0] = 0;
    Ctx->State[0] = 0x67452301;
    Ctx->State[1] = 0xefcdab89;
    Ctx->State[2] = 0x98badcfe;
    Ctx->State[3] = 0x10325476;
}
NW_HOOK_FUNC(0x1014b120, Md5::Init)

// 0x1014b150 (source eax, destination ecx, count edx; stock; not hooked) - Count bytes copied, rounded up to groups of four: the
// encoder of Final (the state and the bit count are little-endian words already)
void Md5::CopyWords(const BYTE* Src, BYTE* Dest, UINT Count)
{
    if (Count > 0)
    {
        UINT Groups = ((Count - 1) >> 2) + 1;
        for (UINT i = 0; i < Groups; i++)
        {
            Dest[0] = Src[0];
            Dest[1] = Src[1];
            Dest[2] = Src[2];
            Dest[3] = Src[3];
            Src += 4;
            Dest += 4;
        }
    }
}

// 0x1014b190 (block eax, words edx, count ecx; stock; not hooked) - the bytes as little-endian words, rounded up to groups of four bytes
void Md5::DecodeWords(const BYTE* Block, DWORD* Words, UINT Count)
{
    if (Count > 0)
    {
        UINT Groups = ((Count - 1) >> 2) + 1;
        for (UINT i = 0; i < Groups; i++)
        {
            Words[i] = (DWORD)Block[0] | ((DWORD)Block[1] << 8) | ((DWORD)Block[2] << 16) | ((DWORD)Block[3] << 24);
            Block += 4;
        }
    }
}

#define MD5_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | ~(z)))
#define MD5_STEP(f, a, b, c, d, x, s, ac) \
    { (a) += f((b), (c), (d)) + (x) + (DWORD)(ac); (a) = ((a) << (s)) | ((a) >> (32 - (s))); (a) += (b); }

// 0x1014b1d0 (the block in eax, the context pushed by the caller; stock; not hooked) - RFC 1321's transform: the block read as 16
// little-endian words (DecodeWords), four rounds of 16 steps, the four results added to the state
void Md5::Transform(Md5Context* Ctx, const BYTE* Block)
{
    DWORD x[16];
    DecodeWords(Block, x, 64);
    DWORD a = Ctx->State[0], b = Ctx->State[1], c = Ctx->State[2], d = Ctx->State[3];

    MD5_STEP(MD5_F, a, b, c, d, x[0], 7, 0xd76aa478);
    MD5_STEP(MD5_F, d, a, b, c, x[1], 12, 0xe8c7b756);
    MD5_STEP(MD5_F, c, d, a, b, x[2], 17, 0x242070db);
    MD5_STEP(MD5_F, b, c, d, a, x[3], 22, 0xc1bdceee);
    MD5_STEP(MD5_F, a, b, c, d, x[4], 7, 0xf57c0faf);
    MD5_STEP(MD5_F, d, a, b, c, x[5], 12, 0x4787c62a);
    MD5_STEP(MD5_F, c, d, a, b, x[6], 17, 0xa8304613);
    MD5_STEP(MD5_F, b, c, d, a, x[7], 22, 0xfd469501);
    MD5_STEP(MD5_F, a, b, c, d, x[8], 7, 0x698098d8);
    MD5_STEP(MD5_F, d, a, b, c, x[9], 12, 0x8b44f7af);
    MD5_STEP(MD5_F, c, d, a, b, x[10], 17, 0xffff5bb1);
    MD5_STEP(MD5_F, b, c, d, a, x[11], 22, 0x895cd7be);
    MD5_STEP(MD5_F, a, b, c, d, x[12], 7, 0x6b901122);
    MD5_STEP(MD5_F, d, a, b, c, x[13], 12, 0xfd987193);
    MD5_STEP(MD5_F, c, d, a, b, x[14], 17, 0xa679438e);
    MD5_STEP(MD5_F, b, c, d, a, x[15], 22, 0x49b40821);

    MD5_STEP(MD5_G, a, b, c, d, x[1], 5, 0xf61e2562);
    MD5_STEP(MD5_G, d, a, b, c, x[6], 9, 0xc040b340);
    MD5_STEP(MD5_G, c, d, a, b, x[11], 14, 0x265e5a51);
    MD5_STEP(MD5_G, b, c, d, a, x[0], 20, 0xe9b6c7aa);
    MD5_STEP(MD5_G, a, b, c, d, x[5], 5, 0xd62f105d);
    MD5_STEP(MD5_G, d, a, b, c, x[10], 9, 0x02441453);
    MD5_STEP(MD5_G, c, d, a, b, x[15], 14, 0xd8a1e681);
    MD5_STEP(MD5_G, b, c, d, a, x[4], 20, 0xe7d3fbc8);
    MD5_STEP(MD5_G, a, b, c, d, x[9], 5, 0x21e1cde6);
    MD5_STEP(MD5_G, d, a, b, c, x[14], 9, 0xc33707d6);
    MD5_STEP(MD5_G, c, d, a, b, x[3], 14, 0xf4d50d87);
    MD5_STEP(MD5_G, b, c, d, a, x[8], 20, 0x455a14ed);
    MD5_STEP(MD5_G, a, b, c, d, x[13], 5, 0xa9e3e905);
    MD5_STEP(MD5_G, d, a, b, c, x[2], 9, 0xfcefa3f8);
    MD5_STEP(MD5_G, c, d, a, b, x[7], 14, 0x676f02d9);
    MD5_STEP(MD5_G, b, c, d, a, x[12], 20, 0x8d2a4c8a);

    MD5_STEP(MD5_H, a, b, c, d, x[5], 4, 0xfffa3942);
    MD5_STEP(MD5_H, d, a, b, c, x[8], 11, 0x8771f681);
    MD5_STEP(MD5_H, c, d, a, b, x[11], 16, 0x6d9d6122);
    MD5_STEP(MD5_H, b, c, d, a, x[14], 23, 0xfde5380c);
    MD5_STEP(MD5_H, a, b, c, d, x[1], 4, 0xa4beea44);
    MD5_STEP(MD5_H, d, a, b, c, x[4], 11, 0x4bdecfa9);
    MD5_STEP(MD5_H, c, d, a, b, x[7], 16, 0xf6bb4b60);
    MD5_STEP(MD5_H, b, c, d, a, x[10], 23, 0xbebfbc70);
    MD5_STEP(MD5_H, a, b, c, d, x[13], 4, 0x289b7ec6);
    MD5_STEP(MD5_H, d, a, b, c, x[0], 11, 0xeaa127fa);
    MD5_STEP(MD5_H, c, d, a, b, x[3], 16, 0xd4ef3085);
    MD5_STEP(MD5_H, b, c, d, a, x[6], 23, 0x04881d05);
    MD5_STEP(MD5_H, a, b, c, d, x[9], 4, 0xd9d4d039);
    MD5_STEP(MD5_H, d, a, b, c, x[12], 11, 0xe6db99e5);
    MD5_STEP(MD5_H, c, d, a, b, x[15], 16, 0x1fa27cf8);
    MD5_STEP(MD5_H, b, c, d, a, x[2], 23, 0xc4ac5665);

    MD5_STEP(MD5_I, a, b, c, d, x[0], 6, 0xf4292244);
    MD5_STEP(MD5_I, d, a, b, c, x[7], 10, 0x432aff97);
    MD5_STEP(MD5_I, c, d, a, b, x[14], 15, 0xab9423a7);
    MD5_STEP(MD5_I, b, c, d, a, x[5], 21, 0xfc93a039);
    MD5_STEP(MD5_I, a, b, c, d, x[12], 6, 0x655b59c3);
    MD5_STEP(MD5_I, d, a, b, c, x[3], 10, 0x8f0ccc92);
    MD5_STEP(MD5_I, c, d, a, b, x[10], 15, 0xffeff47d);
    MD5_STEP(MD5_I, b, c, d, a, x[1], 21, 0x85845dd1);
    MD5_STEP(MD5_I, a, b, c, d, x[8], 6, 0x6fa87e4f);
    MD5_STEP(MD5_I, d, a, b, c, x[15], 10, 0xfe2ce6e0);
    MD5_STEP(MD5_I, c, d, a, b, x[6], 15, 0xa3014314);
    MD5_STEP(MD5_I, b, c, d, a, x[13], 21, 0x4e0811a1);
    MD5_STEP(MD5_I, a, b, c, d, x[4], 6, 0xf7537e82);
    MD5_STEP(MD5_I, d, a, b, c, x[11], 10, 0xbd3af235);
    MD5_STEP(MD5_I, c, d, a, b, x[2], 15, 0x2ad7d2bb);
    MD5_STEP(MD5_I, b, c, d, a, x[9], 21, 0xeb86d391);

    Ctx->State[0] += a;
    Ctx->State[1] += b;
    Ctx->State[2] += c;
    Ctx->State[3] += d;
}

// 0x1014ba90 (cdecl) - Length bytes more of the message. The bit count (two words) grows by Length * 8; the bytes fill the
// block, which is transformed each time it is full (the whole 64-byte blocks of Data are transformed from where they are)
void Md5::Update(Md5Context* Ctx, const BYTE* Data, UINT Length)
{
    UINT Index = (Ctx->BitCount[0] >> 3) & 0x3F;
    UINT Bits = Length << 3;
    UINT Low = Ctx->BitCount[0] + Bits;
    Ctx->BitCount[0] = Low;
    if (Low < Bits)
        Ctx->BitCount[1]++;
    Ctx->BitCount[1] += Length >> 29;
    UINT PartLen = 64 - Index;
    UINT i;
    if (Length >= PartLen)
    {
        memcpy(&Ctx->Buffer[Index], Data, PartLen);
        Transform(Ctx, Ctx->Buffer);
        for (i = PartLen; i + 63 < Length; i += 64)
            Transform(Ctx, &Data[i]);
        Index = 0;
    }
    else
        i = 0;
    memcpy(&Ctx->Buffer[Index], &Data[i], Length - i);
}
NW_HOOK_FUNC(0x1014ba90, Md5::Update)

// 0x1014bb50 (cdecl) - the bit count saved, the padding (a 1 bit and zeros up to 56 bytes of the block) and the count appended,
// the state written as the digest (16 bytes), the context cleared
void Md5::Final(BYTE* Digest, Md5Context* Ctx)
{
    static const BYTE Padding[64] = {0x80};   // the stock's PADDING table (0x102A5B70: 0x80, then zeros)
    BYTE Bits[8];
    CopyWords((const BYTE*)Ctx->BitCount, Bits, 8);
    UINT Index = (Ctx->BitCount[0] >> 3) & 0x3F;
    UINT PadLen = Index < 56 ? 56 - Index : 120 - Index;
    Update(Ctx, Padding, PadLen);
    Update(Ctx, Bits, 8);
    CopyWords((const BYTE*)Ctx->State, Digest, 16);
    memset(Ctx, 0, sizeof(Md5Context));
}
NW_HOOK_FUNC(0x1014bb50, Md5::Final)

/*-----------------------------------------------------------------------------
    Not hooked: the three routines the stock compiler built with register arguments (0x1014b150 CopyWords: source eax, destination
    ecx, count edx; 0x1014b190 DecodeWords: block eax, words edx, count ecx; 0x1014b1d0 Transform: block eax, the context pushed).
    regcheck refuses a hook of a function that reads a register at entry, and their only callers are Update and Final, which are
    ours: the stock bodies are dead in hook mode (the C++ above is their proof of what they do; tools/port/emu/t_md5_class.py compares
    Init / Update / Final of both sides on random messages, the transform included).
-----------------------------------------------------------------------------*/
