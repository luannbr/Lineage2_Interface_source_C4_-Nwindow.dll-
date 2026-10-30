// The text parts of the edit boxes: NCEditBox's text operations on a box made here, NString,
// NStringArray and NCMultiEdit's line list (NLineList) and its GetText.
#include "difftest.h"

using namespace dt;

namespace {

enum : DWORD
{
    EB_SetText = 0x10008be0,
    EB_CommitText = 0x10008bf0,
    EB_InsertText = 0x10008c40,
    EB_DeleteChar = 0x10008d30,
    EB_ClearText = 0x10008f10,
    NS_Ctor = 0x10021cf0,
    NS_GetAt = 0x10021d10,
    NS_AllocBuffer = 0x10021d30,
    NS_Insert = 0x10021d50,
    NS_Append = 0x10021e80,
    NS_Empty = 0x10021ea0,
    NS_CtorText = 0x10021ec0,
    NS_Dtor = 0x10021ef0,
    SA_Empty = 0x10021f00,
    SA_Dtor = 0x10021f70,
    SA_InsertAt = 0x10021f80,
    SA_Ctor = 0x10022130,
    LL_Ctor = 0x1001bc20,
    LL_TotalLength = 0x1001bca0,
    LL_GetLine = 0x1001bd10,
    LL_Insert = 0x1001de60,
    LL_RemoveAt = 0x1001e000,
    LL_Clear = 0x1001e0c0,
    LL_Dtor = 0x10020a20,
    ME_GetText = 0x1001dd60,
};

// NCEditBox's fields: Width +0x4C, ViewStart +0x1E0, Left / Right / Comp
std::string ShowBox(BYTE* B)
{
    char Buf[48];
    sprintf(Buf, "view %d ", *(int*)(B + 0x1E0));
    return Buf + std::string("left ") + Show(*(FStr*)(B + 0x1F0)) + " right " + Show(*(FStr*)(B + 0x1FC)) +
           " comp " + Show(*(FStr*)(B + 0x208));
}

struct NStr
{
    DWORD Capacity;
    wchar_t* Data;
    DWORD Length;
    DWORD Color;
    int Indent;
};
std::string ShowNStr(const NStr& S)
{
    char Buf[48];
    sprintf(Buf, "%lu/%lu \"", S.Length, S.Capacity);
    return Buf + (S.Data ? Esc(S.Data, (int)S.Length) : std::string("(null)")) + "\"";
}

struct Node
{
    FStr Text;
    Node* Prev;
    Node* Next;
};
struct LineList
{
    int Count;
    int Unknown4;
    Node* Head;
    Node* Current;
    Node* Tail;
    int CurIndex;
};
std::string ShowList(LineList* L)
{
    char Buf[64];
    int Cur = -1, i = 0;
    std::string S;
    for (Node* N = L->Head; N && i < 50; N = N->Next, i++)
    {
        if (N == L->Current)
            Cur = i;
        S += (i ? " " : "") + Show(N->Text);
        if (N->Next && N->Next->Prev != N)
            S += "(bad link)";
    }
    sprintf(Buf, "count %d walked %d cur %d(%d) tail %s [", L->Count, i, L->CurIndex, Cur,
            L->Tail && !L->Tail->Next ? "ok" : "bad");
    return Buf + S + "]";
}

}  // namespace

DT_CASE(edit, "edit box text operations, NString, NStringArray, the multi-line box's line list")
{
    Print("== NCEditBox\n");
    for (int Width : {5, 100})
    {
        BYTE* B = (BYTE*)calloc(0x240, 1);
        *(int*)(B + 0x4C) = Width;
        This<void>(EB_SetText, B, L"hello");
        Print("width %d set: %s\n", Width, ShowBox(B).c_str());
        This<void>(EB_InsertText, B, L" world", 1);
        Print("append: %s\n", ShowBox(B).c_str());
        This<void>(EB_InsertText, B, L">> ", 0);
        Print("prepend: %s\n", ShowBox(B).c_str());
        Set(*(FStr*)(B + 0x1FC), L"RIGHT");
        Set(*(FStr*)(B + 0x208), L"cmp");
        for (int Index : {0, 3, 13, 14, 15, 16, 17, 19, 20, 30})
        {
            This<void>(EB_DeleteChar, B, Index);
            Print("delete %d: %s\n", Index, ShowBox(B).c_str());
        }
        const wchar_t* Text = This<const wchar_t*>(EB_CommitText, B);
        Print("commit \"%s\": %s\n", Esc(Text).c_str(), ShowBox(B).c_str());
        This<void>(EB_ClearText, B);
        Print("clear: %s commit %s\n", ShowBox(B).c_str(), This<const wchar_t*>(EB_CommitText, B) ? "text" : "NULL");
        Print("allocs [%s]\n", TakeAllocs().c_str());
    }

    Print("== NString\n");
    {
        NStr S;
        memset(&S, 0xCD, sizeof(S));
        This<void>(NS_Ctor, &S);
        Print("ctor %s [%s]\n", ShowNStr(S).c_str(), TakeAllocs().c_str());
        Print("getat 0 %s\n", This<wchar_t*>(NS_GetAt, &S, 0) ? "set" : "NULL");
        Print("append %d %s [%s]\n", This<int>(NS_Append, &S, L"abc"), ShowNStr(S).c_str(), TakeAllocs().c_str());
        Print("insert@1 %d %s [%s]\n", This<int>(NS_Insert, &S, L"XY", 1), ShowNStr(S).c_str(), TakeAllocs().c_str());
        Print("insert@9 %d %s [%s]\n", This<int>(NS_Insert, &S, L"far", 9), ShowNStr(S).c_str(), TakeAllocs().c_str());
        std::wstring Long(300, L'z');
        Print("append 300 %d %s [%s]\n", This<int>(NS_Append, &S, Long.c_str()), ShowNStr(S).c_str(),
              TakeAllocs().c_str());
        Print("insert@0 300 %d %s [%s]\n", This<int>(NS_Insert, &S, Long.c_str(), 0), ShowNStr(S).c_str(),
              TakeAllocs().c_str());
        for (DWORD i : {0u, 2u, 604u, 605u, 606u})
        {
            wchar_t* P = This<wchar_t*>(NS_GetAt, &S, i);
            Print("getat %lu %s\n", i, P ? ("'" + Esc(P, 1) + "'").c_str() : "NULL");
        }
        Print("insert NULL %d %s\n", This<int>(NS_Insert, &S, (const wchar_t*)NULL, 0), ShowNStr(S).c_str());
        wchar_t* Buf = This<wchar_t*>(NS_AllocBuffer, &S, 10);
        Print("allocbuffer 10 %s %s [%s]\n", Buf ? "set" : "NULL", ShowNStr(S).c_str(), TakeAllocs().c_str());
        This<void>(NS_Empty, &S);
        Print("empty %s [%s]\n", ShowNStr(S).c_str(), TakeAllocs().c_str());
        NStr T;
        This<void>(NS_CtorText, &T, L"from text");
        Print("ctor text %s [%s]\n", ShowNStr(T).c_str(), TakeAllocs().c_str());
        This<void>(NS_Dtor, &T);
        This<void>(NS_Dtor, &S);
        Print("dtors [%s]\n", TakeAllocs().c_str());
    }

    Print("== NStringArray\n");
    {
        struct
        {
            int Num;
            NStr* Data;
            int Max;
        } A;
        This<void>(SA_Ctor, &A);
        Print("ctor num %d max %d [%s]\n", A.Num, A.Max, TakeAllocs().c_str());
        for (int i = 0; i < 3; i++)
        {
            This<void>(SA_InsertAt, &A, i == 1 ? 0 : -1);
            Print("insert num %d max %d [%s]\n", A.Num, A.Max, TakeAllocs().c_str());
        }
        Print("empty -> %d num %d max %d [%s]\n", This<int>(SA_Empty, &A), A.Num, A.Max, TakeAllocs().c_str());
        This<void>(SA_InsertAt, &A, -1);
        This<void>(SA_Dtor, &A);
        Print("dtor [%s]\n", TakeAllocs().c_str());
    }

    Print("== NLineList\n");
    {
        LineList L;
        memset(&L, 0xCD, sizeof(L));
        This<void>(LL_Ctor, &L);
        Print("ctor %s [%s]\n", ShowList(&L).c_str(), TakeAllocs().c_str());
        for (int i = 0; i < 6; i++)
        {
            int At = i % 3 == 0 ? -1 : i % 3 == 1 ? 0 : 2;
            int R = This<int>(LL_Insert, &L, At);
            Node* N = This<Node*>(LL_GetLine, &L, R);
            wchar_t Buf[32];
            swprintf(Buf, 32, L"line%d", i);
            if (N)
                Set(N->Text, Buf);
            Print("insert %d -> %d: %s [%s]\n", At, R, ShowList(&L).c_str(), TakeAllocs().c_str());
        }
        for (int i : {0, 6, 3, 1, 5, 2, -1, 7, 4})
        {
            Node* N = This<Node*>(LL_GetLine, &L, i);
            Print("getline %d -> %s cur %d\n", i, N ? Show(N->Text).c_str() : "NULL", L.CurIndex);
        }
        Print("total %d\n", This<int>(LL_TotalLength, &L));

        // the multi-line box's text: its lines joined (the box made here holds the list)
        BYTE* Box = (BYTE*)calloc(0x270, 1);
        *(LineList*)(Box + 0x1B8) = L;
        const wchar_t* Text = This<const wchar_t*>(ME_GetText, Box);
        Print("gettext \"%s\" [%s]\n", Esc(Text).c_str(), TakeAllocs().c_str());
        L = *(LineList*)(Box + 0x1B8);

        for (int i : {2, 0, 9, -1, 3})
        {
            This<void>(LL_RemoveAt, &L, i);
            Print("remove %d: %s [%s]\n", i, ShowList(&L).c_str(), TakeAllocs().c_str());
        }
        This<void>(LL_Clear, &L);
        Print("clear: %s total %d [%s]\n", ShowList(&L).c_str(), This<int>(LL_TotalLength, &L), TakeAllocs().c_str());
        *(LineList*)(Box + 0x1B8) = L;
        Text = This<const wchar_t*>(ME_GetText, Box);
        Print("gettext %s [%s]\n", Text ? ("\"" + Esc(Text) + "\"").c_str() : "NULL", TakeAllocs().c_str());
        L = *(LineList*)(Box + 0x1B8);
        This<void>(LL_Dtor, &L);
        Print("dtor [%s]\n", TakeAllocs().c_str());
    }
}
