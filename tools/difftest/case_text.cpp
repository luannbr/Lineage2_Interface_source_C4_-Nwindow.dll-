// Text and number helpers that only depend on their arguments: the system message format
// ($s1 and the Korean particles), thousands separators, newline escapes, HTML colors, list
// tokens, edit types and the calendar arithmetic of the server list.
#include "difftest.h"

using namespace dt;

namespace {

enum : DWORD
{
    VA_FormatNumber = 0x100230d0,        // NCWnd (this not used)
    VA_FormatSysMsg = 0x10046120,        // NConsoleWnd
    VA_ConvertNewLines = 0x10046de0,
    VA_HasNoAmpersand = 0x10046320,
    VA_SetParam = 0x10047380,            // NSysMsgParam
    VA_SetParamNumber = 0x100473d0,
    VA_ParseColor = 0x10026c00,
    VA_NextListToken = 0x1002fd30,
    VA_ParseEditType = 0x10025e90,       // NHtmlItem (this not used)
    VA_IsLeapYear = 0x100a20a0,          // NCServerSelectWnd statics
    VA_DaysInYears = 0x100a20e0,
    VA_DaysInMonths = 0x100a2120,
    VA_HoursInDays = 0x100a2190,
};

BYTE Dummy[0x4000];                      // `this` for the members that do not use it

}  // namespace

DT_CASE(text, "text helpers: system message format, number commas, newlines, colors, tokens")
{
    Print("== FormatNumber\n");
    const wchar_t* Numbers[] = {L"", L"0", L"7", L"12", L"123", L"1234", L"12345", L"123456", L"1234567",
                                L"12345678901", L"-1234", L"12a34", L"000001"};
    for (const wchar_t* N : Numbers)
    {
        wchar_t Out[64];
        wcscpy(Out, L"(untouched)");
        This<void>(VA_FormatNumber, Dummy, Out, N);
        Print("\"%s\" -> \"%s\"\n", Esc(N).c_str(), Esc(Out).c_str());
    }

    Print("== FormatSystemMessage\n");
    struct
    {
        const wchar_t* Format;
        const wchar_t* P1;
        const wchar_t* P2;
    } Msgs[] = {
        {L"$s1 has joined the party.", L"Bob", NULL},
        {L"$s1 attacked $s2.", L"Bob", L"Alice"},
        {L"$s1 and $s1 again", L"one", L"two"},
        {L"no parameters $s1 here", NULL, NULL},
        {L"third $s1 $s2 $s3 dropped", L"A", L"B"},
        {L"trailing $", L"X", NULL},
        {L"$", L"X", L"Y"},
        {L"\xC0AC\xACFC$s1%\xC774 \xCC45$s2%\xC744 \xAC00\xC9C0\xB2E4", L"\xC0AC\xACFC", L"\xCC45"},  // no final / final
        {L"$s1%\xC740 $s2%\xC640 $s1%\xC744", L"\xCC45", L"Bob"},
        {L"$s1%\xB294 unknown particle", L"\xCC45", NULL},
        {L"%\xC774 before any text", L"A", NULL},
        {L"end with %", L"A", NULL},
        {L"", L"A", L"B"},
    };
    for (auto& M : Msgs)
    {
        struct
        {
            wchar_t Guard[4];
            wchar_t Out[512];
        } B;
        wcscpy(B.Guard, L"zzz");
        wmemset(B.Out, L'#', 511);
        B.Out[511] = 0;
        This<void>(VA_FormatSysMsg, Dummy, B.Out, M.Format, M.P1, M.P2);
        Print("\"%s\" \"%s\" \"%s\" -> \"%s\"\n", Esc(M.Format).c_str(), Esc(M.P1).c_str(), Esc(M.P2).c_str(),
              Esc(B.Out).c_str());
    }

    Print("== NSysMsgParam\n");
    {
        struct
        {
            wchar_t* Text;
            int Len;
        } P;
        int Values[] = {0, -1, 7, 123456789, (int)0x80000000};
        for (int V : Values)
        {
            P.Text = NULL;
            P.Len = 0;
            This<void>(VA_SetParamNumber, &P, V);
            Print("%d -> \"%s\" %d [%s]\n", V, Esc(P.Text).c_str(), P.Len, TakeAllocs().c_str());
        }
        P.Text = NULL;
        P.Len = -5;
        This<void>(VA_SetParam, &P, (const wchar_t*)NULL);
        Print("NULL -> %s %d [%s]\n", P.Text ? "set" : "unset", P.Len, TakeAllocs().c_str());
        This<void>(VA_SetParam, &P, L"text param");
        Print("text -> \"%s\" %d [%s]\n", Esc(P.Text).c_str(), P.Len, TakeAllocs().c_str());
    }

    Print("== ConvertNewLines\n");
    const wchar_t* Escaped[] = {L"no newline", L"a\\nb", L"\\n\\n", L"end\\n", L"\\nstart", L"x\\\\ny",
                                L"line one\\nline two\\nline three and more text\\n", L""};
    for (const wchar_t* E : Escaped)
    {
        FStr R = {NULL, 0, 0};
        wchar_t Buf[256];
        wcscpy(Buf, E);
        This<void>(VA_ConvertNewLines, Dummy, Buf, &R);
        Print("\"%s\" -> %s [%s]\n", Esc(E).c_str(), Show(R).c_str(), TakeAllocs().c_str());
    }

    Print("== HasNoAmpersand\n");
    const wchar_t* Amps[] = {L"", L"abc", L"a&b", L"&", L"end&"};
    for (const wchar_t* A : Amps)
    {
        wchar_t Buf[32];
        wcscpy(Buf, A);
        Print("\"%s\" -> %d\n", Esc(A).c_str(), This<int>(VA_HasNoAmpersand, Dummy, Buf));
    }
    Print("NULL -> %d\n", This<int>(VA_HasNoAmpersand, Dummy, (wchar_t*)NULL));

    Print("== HtmlParseColor\n");
    const wchar_t* Colors[] = {L"LEVEL", L"FF0000", L"00ff00", L"0000FF", L"", L"zzzzzz", L"123", L"FFFFFFFF",
                               L"level", L"b09b79", L"1"};
    for (const wchar_t* C : Colors)
        Print("\"%s\" -> %08x [%s]\n", Esc(C).c_str(), Cdecl<DWORD>(VA_ParseColor, C), TakeAllocs().c_str());
    Print("NULL -> %08x\n", Cdecl<DWORD>(VA_ParseColor, (const wchar_t*)NULL));

    Print("== NextListToken\n");
    const wchar_t* Lists[] = {L"a;b;;c;", L";;x", L"", L";", L"one", L"two;three"};
    for (const wchar_t* L : Lists)
    {
        wchar_t Buf[64];
        wcscpy(Buf, L);
        std::string S = "\"" + Esc(L) + "\":";
        wchar_t* P = Buf;
        for (int i = 0; i < 6; i++)
        {
            wchar_t* Token = (wchar_t*)1;
            wchar_t* Next = Cdecl<wchar_t*>(VA_NextListToken, P, &Token);
            char B[32];
            sprintf(B, " @%d", (int)(Next - Buf));
            S += B + (Token == (wchar_t*)1 ? std::string("=unset") : "=\"" + Esc(Token) + "\"");
            if (Next == P)
                break;
            P = Next;
        }
        Print("%s\n", S.c_str());
    }

    Print("== ParseEditType\n");
    const wchar_t* Types[] = {L"normal", L"password", L"date", L"time", L"number", L"NUMBER", L"", L"x"};
    for (const wchar_t* T : Types)
        Print("\"%s\" -> %d\n", Esc(T).c_str(), This<int>(VA_ParseEditType, Dummy, T));

    Print("== calendar\n");
    int Years[] = {0, 1, 3, 4, 100, 200, 400, 1900, 2000, 2003, 2004, 2100, -4, -100};
    for (int Y : Years)
        Print("leap %d -> %d\n", Y, Cdecl<int>(VA_IsLeapYear, Y));
    int Spans[] = {0, 1, 4, 100, 400, 2003, 2004, -1};
    for (int Y : Spans)
        Print("days in %d years -> %d\n", Y, Cdecl<int>(VA_DaysInYears, Y));
    for (int Y : {2003, 2004, 2100})
    {
        std::string S;
        for (int M = 0; M <= 13; M++)
        {
            char B[16];
            sprintf(B, " %d", Cdecl<int>(VA_DaysInMonths, Y, M));
            S += B;
        }
        Print("days before month 0..13 of %d:%s\n", Y, S.c_str());
    }
    int Days[][2] = {{0, 0}, {1, 0}, {1, 23}, {2, 5}, {31, 23}, {-3, 4}};
    for (auto& D : Days)
        Print("hours %d %d -> %d\n", D[0], D[1], Cdecl<int>(VA_HoursInDays, D[0], D[1]));
}
