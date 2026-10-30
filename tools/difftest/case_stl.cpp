// The C++ library instances of the stock build: std::wstring and the vectors of the HTML
// parser (NStl), std::string, the point deque and the exceptions (NStdLib).
#include "difftest.h"

using namespace dt;

namespace {

/*-----------------------------------------------------------------------------
    std::wstring (NStlWString).
-----------------------------------------------------------------------------*/

enum : DWORD
{
    WS_Find = 0x10027bb0,
    WS_Dtor = 0x100286d0,
    WS_Copy = 0x10028820,
    WS_Grow = 0x1002a000,
    WS_ReplaceS = 0x1002a0d0,
    WS_Erase = 0x1002a8a0,
    WS_EraseIt = 0x1002a970,
    WS_ReplaceP = 0x1002a9e0,
    WS_AssignS = 0x1002abd0,
    WS_AssignP = 0x1002ac90,
    WS_AssignZ = 0x1002ad40,
    WS_AppendS = 0x1002cbf0,
    WS_AppendP = 0x1002ccf0,
    WS_InsertS = 0x1002ce10,
    WS_InsertP = 0x1002d030,
    WS_InsertC = 0x1002d1b0,
    WS_Substr = 0x1002d3e0,
};

struct WS
{
    DWORD Alloc;
    union
    {
        wchar_t Buf[8];
        wchar_t* Ptr;
    };
    DWORD Size;
    DWORD Res;

    WS() { Init(); }
    ~WS() { Free(); }
    void Init()
    {
        memset(this, 0xAB, sizeof(*this));
        Size = 0;
        Res = 7;
        Buf[0] = 0;
    }
    void Free() { This<void>(WS_Dtor, this); }
    wchar_t* P() { return Res >= 8 ? Ptr : Buf; }
    std::string Show()
    {
        char B[32];
        sprintf(B, "%lu/%lu\"", Size, Res);
        return B + Esc(P(), Size) + "\"" + (P()[Size] == 0 ? "" : " NO TERMINATOR");
    }
    WS& Assign(const wchar_t* S, DWORD N)
    {
        This<void*>(WS_AssignP, this, S, N);
        return *this;
    }
};

const wchar_t* const Texts[] = {L"", L"abc", L"abcdefgh", L"The quick brown fox jumps over the lazy dog",
                                L"0123456789012345678901234567890123456789012345678901234567890123456789"};
const int NumTexts = sizeof(Texts) / sizeof(Texts[0]);

// every member on one pair of texts and one set of positions: its results, hashed
std::string StringOps(const wchar_t* A, const wchar_t* B, DWORD Off, DWORD Cnt, DWORD Roff)
{
    std::string R;
    DWORD La = (DWORD)wcslen(A), Lb = (DWORD)wcslen(B);
    DWORD CntB = Cnt > Lb ? Lb : Cnt;
    WS Sa, Sb, C;
    This<void*>(WS_AssignP, &Sa, A, La);
    This<void*>(WS_AssignZ, &Sb, B);
    R += "a " + Sa.Show() + " b " + Sb.Show() + " [" + TakeAllocs() + "]\n";
    auto Step = [&](const char* Name) {
        R += std::string(Name) + " " + C.Show() + " [" + TakeAllocs() + "]\n";
        C.Free();
        C.Init();
        TakeAllocs();
    };
    This<void*>(WS_AssignS, &C, &Sb, Roff, Cnt);
    Step("assignS");
    C.Assign(A, La);
    This<void*>(WS_AppendS, &C, &Sb, Roff, Cnt);
    Step("appendS");
    C.Assign(A, La);
    This<void*>(WS_AppendP, &C, B, CntB);
    Step("appendP");
    C.Assign(A, La);
    This<void*>(WS_InsertS, &C, Off, &Sb, Roff, Cnt);
    Step("insertS");
    C.Assign(A, La);
    This<void*>(WS_InsertP, &C, Off, B, CntB);
    Step("insertP");
    C.Assign(A, La);
    This<void*>(WS_InsertC, &C, Off, Cnt > 20 ? 20 : Cnt, (DWORD)'x');
    Step("insertC");
    C.Assign(A, La);
    This<void*>(WS_ReplaceS, &C, Off, Cnt, &Sb, Roff, Cnt == 0xFFFFFFFF ? 5 : Cnt);
    Step("replaceS");
    C.Assign(A, La);
    This<void*>(WS_ReplaceP, &C, Off, Cnt, B, CntB);
    Step("replaceP");
    C.Assign(A, La);
    This<void*>(WS_Erase, &C, Off, Cnt);
    Step("erase");
    if (La)
    {
        C.Assign(A, La);
        wchar_t* Res;
        wchar_t* P0 = C.P();
        DWORD E = Off + (Cnt > La ? La : Cnt);
        if (E > La)
            E = La;
        This<wchar_t**>(WS_EraseIt, &C, &Res, P0 + Off, P0 + E);
        char Buf[32];
        sprintf(Buf, "eraseIt@%d", (int)(Res - C.P()));
        Step(Buf);
    }
    // the string as its own source
    C.Assign(A, La);
    This<void*>(WS_AssignS, &C, &C, Off, Cnt);
    Step("assignSelf");
    C.Assign(A, La);
    This<void*>(WS_AppendS, &C, &C, Off, Cnt);
    Step("appendSelf");
    C.Assign(A, La);
    This<void*>(WS_InsertS, &C, Off, &C, Roff > La ? La : Roff, Cnt);
    Step("insertSelf");
    C.Assign(A, La);
    This<void*>(WS_ReplaceS, &C, Off, Cnt == 0xFFFFFFFF ? 2 : Cnt, &C, Roff > La ? La : Roff, 4);
    Step("replaceSelf");
    if (La > 1)
    {
        C.Assign(A, La);
        C.Assign(C.P() + 1, La - 1);
        Step("assignInside");
        C.Assign(A, La);
        This<void*>(WS_AppendP, &C, C.P() + 1, La - 1);
        Step("appendInside");
        C.Assign(A, La);
        This<void*>(WS_InsertP, &C, Off, C.P() + 1, La - 1);
        Step("insertInside");
        C.Assign(A, La);
        This<void*>(WS_ReplaceP, &C, Off, Cnt, C.P() + La / 2, La - La / 2);
        Step("replaceInside");
    }
    C.Assign(A, La);
    char Buf[96];
    sprintf(Buf, "find %u %u %u %u\n", This<DWORD>(WS_Find, &C, B, Off, CntB), This<DWORD>(WS_Find, &C, L"o", Off, 1),
            This<DWORD>(WS_Find, &C, L"fox", Off, 3), This<DWORD>(WS_Find, &C, L"", Off, 0));
    R += Buf;
    WS D;
    D.Free();
    This<void*>(WS_Substr, &C, &D, Off, Cnt);
    R += "substr " + D.Show() + " [" + TakeAllocs() + "]\n";
    return R;
}

}  // namespace

DT_CASE(wstring, "std::wstring members (NStl): every member on text and position sets, errors")
{
    for (int i = 0; i < NumTexts; i++)
        for (int j = 0; j < NumTexts; j++)
        {
            DWORD La = (DWORD)wcslen(Texts[i]), Lb = (DWORD)wcslen(Texts[j]);
            DWORD Offs[] = {0, 1, La / 2, La};
            DWORD Cnts[] = {0, 1, 3, 9, 0xFFFFFFFF};
            DWORD Roffs[] = {0, Lb / 2, Lb};
            for (DWORD Off : Offs)
            {
                if (Off > La)
                    continue;
                for (DWORD Cnt : Cnts)
                    for (DWORD Roff : Roffs)
                    {
                        Hashed H;
                        H.Add(StringOps(Texts[i], Texts[j], Off, Cnt, Roff));
                        char Label[96];
                        sprintf(Label, "a%d b%d off %u cnt %u roff %u", i, j, Off, Cnt, Roff);
                        H.Flush(Label);
                    }
            }
        }

    // growth: capacities and allocations of the in-place / heap switch
    Print("== growth\n");
    {
        WS S;
        for (int i = 0; i < 40; i++)
        {
            This<void*>(WS_AppendP, &S, L"ab", 2);
            Print("append %d %s [%s]\n", i, S.Show().c_str(), TakeAllocs().c_str());
        }
        for (int i = 0; i < 12; i++)
        {
            This<void*>(WS_Erase, &S, 0, 7);
            Print("erase %d %s [%s]\n", i, S.Show().c_str(), TakeAllocs().c_str());
        }
        This<bool>(WS_Grow, &S, 3, 1);
        Print("grow 3 trim %s [%s]\n", S.Show().c_str(), TakeAllocs().c_str());
        This<bool>(WS_Grow, &S, 100, 0);
        Print("grow 100 %s [%s]\n", S.Show().c_str(), TakeAllocs().c_str());
        S.Assign(L"keep this text", 14);
        This<bool>(WS_Grow, &S, 200, 1);
        Print("grow 200 trim %s [%s]\n", S.Show().c_str(), TakeAllocs().c_str());
        S.Assign(L"keep this text", 14);
        This<void>(WS_Copy, &S, 300, 5);
        Print("copy 300 5 %s [%s]\n", S.Show().c_str(), TakeAllocs().c_str());
        bool bMore = This<bool>(WS_Grow, &S, 0, 0);
        Print("grow 0 -> %d %s [%s]\n", bMore, S.Show().c_str(), TakeAllocs().c_str());
    }

    // the range and length errors (the stock library's out_of_range / length_error)
    Print("== errors\n");
    {
        WS S;
        S.Assign(L"hello", 5);
        WS O;
        O.Assign(L"xy", 2);
        TakeAllocs();
        Print("insert 6: %s %s\n", Show(Try([&] { This<void*>(WS_InsertP, &S, 6, L"z", 1); })).c_str(), S.Show().c_str());
        Print("erase 9: %s %s\n", Show(Try([&] { This<void*>(WS_Erase, &S, 9, 1); })).c_str(), S.Show().c_str());
        Print("assign roff 3: %s %s\n", Show(Try([&] { This<void*>(WS_AssignS, &S, &O, 3, 1); })).c_str(), S.Show().c_str());
        Print("append roff 3: %s %s\n", Show(Try([&] { This<void*>(WS_AppendS, &S, &O, 3, 1); })).c_str(), S.Show().c_str());
        Print("replace 7: %s %s\n", Show(Try([&] { This<void*>(WS_ReplaceP, &S, 7, 1, L"q", 1); })).c_str(), S.Show().c_str());
        Print("replaceS roff 5: %s %s\n", Show(Try([&] { This<void*>(WS_ReplaceS, &S, 0, 1, &O, 5, 1); })).c_str(), S.Show().c_str());
        Print("insertC 8: %s %s\n", Show(Try([&] { This<void*>(WS_InsertC, &S, 8, 1, (DWORD)'c'); })).c_str(), S.Show().c_str());
        Print("append npos: %s %s\n", Show(Try([&] { This<void*>(WS_AppendP, &S, L"z", 0xFFFFFFFF); })).c_str(), S.Show().c_str());
        Print("insertC npos: %s %s\n", Show(Try([&] { This<void*>(WS_InsertC, &S, 0, 0xFFFFFFFF, (DWORD)'c'); })).c_str(), S.Show().c_str());
        Print("grow huge: %s %s\n", Show(Try([&] { This<bool>(WS_Grow, &S, 0x7FFFFFFF, 0); })).c_str(), S.Show().c_str());
        Print("allocs [%s]\n", TakeAllocs().c_str());
    }
}

namespace {

/*-----------------------------------------------------------------------------
    The vectors.
-----------------------------------------------------------------------------*/

enum : DWORD
{
    VW_PushBack = 0x1002f470,
    VW_InsertN = 0x1002e7b0,
    VW_Tidy = 0x1002d360,
    VP_InsertN = 0x1002ea60,
    VS_PushBack = 0x1002f5b0,
    VS_Insert = 0x1002f4e0,
    VS_InsertN = 0x1002edd0,
    VS_Size = 0x1002c720,
    VS_Dtor = 0x1002ed50,
    V_Xlen = 0x1002e050,
};

template <class T> struct Vec
{
    DWORD Alloc;
    T* First;
    T* Last;
    T* End;
    Vec() : Alloc(0xABABABAB), First(NULL), Last(NULL), End(NULL) {}
    std::string Head()
    {
        char B[48];
        sprintf(B, "%d/%d", (int)(Last - First), (int)(End - First));
        return B;
    }
};

std::string ShowWords(Vec<unsigned short>& V)
{
    std::string S = V.Head() + " [";
    for (unsigned short* P = V.First; P != V.Last; P++)
    {
        char B[16];
        sprintf(B, P == V.First ? "%u" : " %u", *P);
        S += B;
    }
    return S + "]";
}

std::string ShowPtrs(Vec<DWORD>& V)
{
    std::string S = V.Head() + " [";
    for (DWORD* P = V.First; P != V.Last; P++)
    {
        char B[16];
        sprintf(B, P == V.First ? "%lu" : " %lu", *P);
        S += B;
    }
    return S + "]";
}

std::string ShowStrings(Vec<WS>& V)
{
    std::string S = V.Head() + " [";
    for (WS* P = V.First; P != V.Last; P++)
        S += (P == V.First ? "" : " ") + P->Show();
    return S + "]";
}

}  // namespace

DT_CASE(vectors, "std::vector instances (NStl): WORD, pointers, wstring; growth, inserts, errors")
{
    Print("== vector<WORD> push_back\n");
    Vec<unsigned short> W;
    for (unsigned short i = 1; i <= 40; i++)
    {
        This<void>(VW_PushBack, &W, &i);
        Print("%u: %s [%s]\n", i, W.Head().c_str(), TakeAllocs().c_str());
    }
    Print("== vector<WORD> insert\n");
    struct
    {
        int Where;
        DWORD Count;
        unsigned short Val;
    } Ins[] = {{0, 1, 100}, {5, 3, 200}, {40, 2, 300}, {10, 30, 400}, {0, 0, 1}, {3, 100, 7}, {70, 1, 9}};
    for (auto& I : Ins)
    {
        int N = (int)(W.Last - W.First);
        unsigned short* Where = W.First + (I.Where > N ? N : I.Where);
        This<void>(VW_InsertN, &W, Where, I.Count, &I.Val);
        Print("at %d x%lu %u: %s [%s]\n", I.Where, I.Count, I.Val, ShowWords(W).c_str(), TakeAllocs().c_str());
    }
    // a value that is inside the vector (the stock code copies it first)
    This<void>(VW_InsertN, &W, W.First + 2, 5, W.First + 3);
    Print("own value: %s [%s]\n", ShowWords(W).c_str(), TakeAllocs().c_str());
    This<void>(VW_Tidy, &W);
    Print("tidy: %s [%s]\n", W.Head().c_str(), TakeAllocs().c_str());

    Print("== vector<pointer> insert\n");
    Vec<DWORD> P;
    DWORD Seq = 1;
    for (int i = 0; i < 12; i++)
    {
        int N = (int)(P.Last - P.First);
        DWORD* Where = P.First + (i % 3 == 0 ? 0 : i % 3 == 1 ? N : N / 2);
        DWORD Count = (DWORD)(i % 4 + 1);
        This<void>(VP_InsertN, &P, Where, Count, &Seq);
        Seq++;
        Print("%d: %s [%s]\n", i, ShowPtrs(P).c_str(), TakeAllocs().c_str());
    }
    This<void>(VW_Tidy, &P);   // one body for both trivial vectors
    Print("tidy [%s]\n", TakeAllocs().c_str());

    Print("== vector<wstring>\n");
    Vec<WS> S;
    const wchar_t* Words[] = {L"a", L"bb", L"a longer string here", L"", L"1234567", L"12345678"};
    for (int i = 0; i < 6; i++)
    {
        WS V;
        V.Assign(Words[i], (DWORD)wcslen(Words[i]));
        TakeAllocs();
        This<void>(VS_PushBack, &S, &V);
        Print("push %d: %s size %u [%s]\n", i, ShowStrings(S).c_str(), This<DWORD>(VS_Size, &S), TakeAllocs().c_str());
    }
    for (int i = 0; i < 4; i++)
    {
        WS V;
        V.Assign(Words[5 - i], (DWORD)wcslen(Words[5 - i]));
        TakeAllocs();
        WS* Res;
        int N = (int)(S.Last - S.First);
        This<WS**>(VS_Insert, &S, &Res, S.First + (i * 2 > N ? N : i * 2), &V);
        Print("insert %d at %d: %s [%s]\n", i, (int)(Res - S.First), ShowStrings(S).c_str(), TakeAllocs().c_str());
    }
    struct
    {
        int Where;
        DWORD Count;
    } SIns[] = {{1, 2}, {0, 9}, {3, 1}, {12, 3}, {2, 0}};
    for (auto& I : SIns)
    {
        WS V;
        V.Assign(L"inserted text value", 19);
        TakeAllocs();
        int N = (int)(S.Last - S.First);
        This<void>(VS_InsertN, &S, S.First + (I.Where > N ? N : I.Where), I.Count, &V);
        Print("insertN at %d x%lu: %s [%s]\n", I.Where, I.Count, ShowStrings(S).c_str(), TakeAllocs().c_str());
    }
    This<void>(VS_InsertN, &S, S.First + 1, 2, S.First + 4);
    Print("own value: %s [%s]\n", ShowStrings(S).c_str(), TakeAllocs().c_str());
    This<void>(VS_Dtor, &S);
    Print("dtor: %s [%s]\n", S.Head().c_str(), TakeAllocs().c_str());

    Print("== errors\n");
    Vec<unsigned short> E;
    unsigned short One = 1;
    Print("xlen: %s\n", Show(Try([&] { This<void>(V_Xlen, &E); })).c_str());
    Print("insert 0x80000000: %s %s\n", Show(Try([&] { This<void>(VW_InsertN, &E, E.First, 0x80000000, &One); })).c_str(),
          E.Head().c_str());
    Vec<WS> ES;
    WS V;
    Print("wstring insert 0x10000000: %s %s\n",
          Show(Try([&] { This<void>(VS_InsertN, &ES, ES.First, 0x10000000, &V); })).c_str(), ES.Head().c_str());
    Print("allocs [%s]\n", TakeAllocs().c_str());
}

namespace {

/*-----------------------------------------------------------------------------
    std::string, std::deque<POINT>, the exceptions (NStdLib).
-----------------------------------------------------------------------------*/

enum : DWORD
{
    SS_Copy = 0x1000d2d0,
    SS_Dtor = 0x1000d410,
    SS_Grow = 0x1000d440,
    SS_Erase = 0x1000d5e0,
    SS_AssignS = 0x1000d810,
    SS_AssignP = 0x1000d8c0,
    SS_CtorP = 0x1000d9d0,
    LE_Ctor = 0x1000d970,
    LE_CopyCtor = 0x1000dad0,
    LE_Dtor = 0x1000d520,
    LE_What = 0x1000d5a0,
    LEN_CopyCtor = 0x1000db40,
    LEN_Dtor = 0x1000da40,
    DQ_Tidy = 0x1000d260,
    DQ_Dtor = 0x1000d510,
    DQ_Xlen = 0x1000da50,
    DQ_GrowMap = 0x1000db60,
    DQ_PushBack = 0x1000dca0,
};

struct SS
{
    BYTE Alval;
    BYTE Pad[3];
    union
    {
        char Buf[16];
        char* Ptr;
    };
    DWORD Size;
    DWORD Res;
    SS()
    {
        memset(this, 0xAB, sizeof(*this));
        Res = 15;
        Size = 0;
        Buf[0] = 0;
    }
    ~SS() { This<void>(SS_Dtor, this); }
    char* P() { return Res >= 16 ? Ptr : Buf; }
    std::string Show()
    {
        char B[32];
        sprintf(B, "%lu/%lu\"", Size, Res);
        return B + EscA(P(), (int)Size) + "\"" + (P()[Size] == 0 ? "" : " NO TERMINATOR");
    }
};
static_assert(sizeof(SS) == 0x1C, "std::string");

struct Deque
{
    BYTE Alval;
    BYTE Pad[3];
    POINT** Map;
    DWORD Mapsize;
    DWORD Myoff;
    DWORD Mysize;
    Deque() : Map(NULL), Mapsize(0), Myoff(0), Mysize(0) { Alval = 0; }
    // pop_back as the stock code inlines it
    void PopBack()
    {
        if (Mysize != 0 && --Mysize == 0)
            Myoff = 0;
    }
    std::string Show()
    {
        char B[64];
        sprintf(B, "map %lu off %lu size %lu [", Mapsize, Myoff, Mysize);
        std::string S = B;
        for (DWORD i = 0; i < Mysize; i++)
        {
            DWORD Off = Myoff + i;
            DWORD Block = Off / 2;
            if (Block >= Mapsize)
                Block -= Mapsize;
            POINT& Pt = Map[Block][Off % 2];
            sprintf(B, i ? " %ld,%ld" : "%ld,%ld", Pt.x, Pt.y);
            S += B;
        }
        return S + "]";
    }
};

}  // namespace

DT_CASE(stdlib, "std::string, std::deque<POINT> and the exceptions (NStdLib)")
{
    Print("== string\n");
    const char* Texts8[] = {"", "abc", "0123456789abcde", "0123456789abcdef", "a string that is long enough for the heap"};
    for (const char* T : Texts8)
    {
        SS A;
        This<void>(SS_CtorP, &A, T);
        Print("ctor %s [%s]\n", A.Show().c_str(), TakeAllocs().c_str());
        DWORD L = (DWORD)strlen(T);
        DWORD Offs[] = {0, 1, L / 2, L};
        DWORD Cnts[] = {0, 1, 5, 0xFFFFFFFF};
        for (DWORD Off : Offs)
            for (DWORD Cnt : Cnts)
            {
                if (Off > L)
                    continue;
                SS B;
                This<void*>(SS_AssignS, &B, &A, Off, Cnt);
                std::string R1 = B.Show() + " [" + TakeAllocs() + "]";
                This<void*>(SS_Erase, &B, Off > B.Size ? B.Size : Off / 2, Cnt);
                std::string R2 = B.Show() + " [" + TakeAllocs() + "]";
                This<void*>(SS_AssignS, &B, &B, Off > B.Size ? B.Size : 0, Cnt);
                std::string R3 = B.Show() + " [" + TakeAllocs() + "]";
                This<void*>(SS_AssignP, &B, B.P() + (B.Size ? 1 : 0), B.Size ? B.Size - 1 : 0);
                std::string R4 = B.Show() + " [" + TakeAllocs() + "]";
                Print("  off %u cnt %u: assign %s erase %s self %s inside %s\n", Off, Cnt, R1.c_str(), R2.c_str(),
                      R3.c_str(), R4.c_str());
            }
        for (int i = 0; i < 6; i++)
        {
            bool More = This<bool>(SS_Grow, &A, (DWORD)(i * 7), i & 1);
            Print("  grow %d trim %d -> %d %s [%s]\n", i * 7, i & 1, More, A.Show().c_str(), TakeAllocs().c_str());
        }
        This<void>(SS_Copy, &A, 40, A.Size > 3 ? 3 : A.Size);
        Print("  copy 40 %s [%s]\n", A.Show().c_str(), TakeAllocs().c_str());
    }
    {
        SS A;
        This<void>(SS_CtorP, &A, "x");
        TakeAllocs();
        Print("erase 5: %s %s\n", Show(Try([&] { This<void*>(SS_Erase, &A, 5, 1); })).c_str(), A.Show().c_str());
        Print("assign roff 3: %s %s\n", Show(Try([&] { This<void*>(SS_AssignS, &A, &A, 3, 1); })).c_str(),
              A.Show().c_str());
        Print("grow huge: %s %s [%s]\n", Show(Try([&] { This<bool>(SS_Grow, &A, 0xFFFFFFFF, 0); })).c_str(),
              A.Show().c_str(), TakeAllocs().c_str());
    }

    Print("== logic_error / length_error\n");
    {
        SS Msg;
        This<void>(SS_CtorP, &Msg, "a message long enough for the heap buffer");
        TakeAllocs();
        BYTE E1[0x28], E2[0x28], E3[0x28];
        This<void>(LE_Ctor, E1, &Msg);
        Print("ctor \"%s\" [%s]\n", EscA(This<const char*>(LE_What, E1)).c_str(), TakeAllocs().c_str());
        This<void>(LE_CopyCtor, E2, E1);
        Print("copy \"%s\" [%s]\n", EscA(This<const char*>(LE_What, E2)).c_str(), TakeAllocs().c_str());
        This<void>(LEN_CopyCtor, E3, E1);
        Print("length_error copy \"%s\" [%s]\n", EscA(This<const char*>(LE_What, E3)).c_str(), TakeAllocs().c_str());
        This<void>(LEN_Dtor, E3);
        This<void>(LE_Dtor, E2);
        This<void>(LE_Dtor, E1);
        Print("dtors [%s]\n", TakeAllocs().c_str());
    }

    Print("== deque\n");
    {
        Deque D;
        LONG N = 0;
        for (int Round = 0; Round < 4; Round++)
        {
            for (int i = 0; i < 9 + Round * 5; i++)
            {
                POINT Pt = {N, -N * 2};
                N++;
                This<void>(DQ_PushBack, &D, &Pt);
            }
            Print("push: %s [%s]\n", D.Show().c_str(), TakeAllocs().c_str());
            for (int i = 0; i < 4 + Round * 3; i++)
                D.PopBack();
            Print("pop: %s\n", D.Show().c_str());
        }
        This<void>(DQ_GrowMap, &D, 3);
        Print("growmap 3: %s [%s]\n", D.Show().c_str(), TakeAllocs().c_str());
        while (D.Mysize)
            D.PopBack();
        for (int i = 0; i < 5; i++)
        {
            POINT Pt = {100 + i, i};
            This<void>(DQ_PushBack, &D, &Pt);
        }
        Print("refill: %s [%s]\n", D.Show().c_str(), TakeAllocs().c_str());
        This<void>(DQ_Tidy, &D);
        Print("tidy: map %lu size %lu [%s]\n", D.Mapsize, D.Mysize, TakeAllocs().c_str());
        This<void>(DQ_Dtor, &D);
        Print("dtor [%s]\n", TakeAllocs().c_str());
        Print("xlen: %s\n", Show(Try([&] { This<void>(DQ_Xlen, &D); })).c_str());
        Print("allocs [%s]\n", TakeAllocs().c_str());
    }
}
