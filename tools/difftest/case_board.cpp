// The community board's page (NCBBSHtmlViewer / NCBoardWnd): the console's pieces gathered and
// assembled into the page handed to LoadHtml, which is replaced by a recorder here.
#include "difftest.h"

using namespace dt;

namespace {

enum : DWORD
{
    VA_NextField = 0x100264c0,
    VA_AddPiece = 0x1002c470,
    VA_SetCommands = 0x100269a0,
    VA_LoadPage = 0x1002c6c0,
    VA_LoadHtml = 0x1002bd60,
    VA_HtmlEditClass = 0x101293d8,       // NCHtmlEdit's UClass
};

int __fastcall LoadHtmlRecorder(void* Viewer, void*, const wchar_t* Text)
{
    Print("  LoadHtml \"%s\"\n", Esc(Text).c_str());
    return 1;
}

void __fastcall EnableRecorder(void* Button, void*, int bEnable)
{
    Print("  button %d enabled %d\n", *((int*)Button + 1), bEnable);
}

void ShowViewer(BYTE* V)
{
    Print("  kind %d locked %d post %d form %d parts %d\n", *(int*)(V + 0x118), *(int*)(V + 0x11C),
          *(int*)(V + 0x120), *(int*)(V + 0x124), *(int*)(V + 0x128));
    const char* Names[] = {"post1", "post2", "post3", "post4", "form1", "form2", "part1", "part2", "part3"};
    for (int i = 0; i < 9; i++)
    {
        const wchar_t* B = (const wchar_t*)(V + 0x12C + i * 0x4000);
        if (B[0])
            Print("  %s \"%s\"\n", Names[i], Esc(B, -1, 80).c_str());
    }
}

}  // namespace

DT_CASE(board, "community board pages: NextField, AddPiece (post, form, parts), SetCommands, LoadPage")
{
    if (!PatchBoth(VA_LoadHtml, (void*)LoadHtmlRecorder, "NCHtmlViewer::LoadHtml"))
        return;

    Print("== fields\n");
    const wchar_t* Lists[] = {L"", L"\b", L"\b\b", L"a", L"a\bb", L"\b\ba\b\bb\b", L"key\bvalue with spaces\b",
                              L"1\b<html>x</html>"};
    for (const wchar_t* L : Lists)
    {
        wchar_t Buf[64];
        wcscpy(Buf, L);
        wchar_t* Field = (wchar_t*)1;
        wchar_t* Rest = Cdecl<wchar_t*>(VA_NextField, Buf, &Field);
        Print("\"%s\": rest %d field %s\n", Esc(L).c_str(), (int)(Rest - Buf),
              Field == (wchar_t*)1 ? "unset" : ("\"" + Esc(Field) + "\"").c_str());
    }

    // the viewer: its page controls (+0xEC) are one HTML edit box, whose text a form's title
    // goes into
    BYTE* V = (BYTE*)calloc(0x2412C + 64, 1);
    BYTE* Edit = (BYTE*)calloc(0x240, 1);
    *(void**)(Edit + 0x24) = Stock(VA_HtmlEditClass);
    BYTE* Controls[1] = {Edit};
    *(BYTE***)(V + 0xEC) = Controls;
    *(int*)(V + 0xF0) = 1;
    *(int*)(V + 0xF4) = 1;

    Print("== pieces\n");
    const wchar_t* Pieces[] = {
        L"<html><body>direct page</body></html>",
        L"1\b<html>x<!--TITLE--> by <!--CHAR_NAME--> #<!--BID--> <!--COMMENT--> | <!--THREAD--> <!--ROOT_BID--> <!--IP--> <!--STATUS--></html>",
        L"2\bR1\bT2\bB3\bS4\bP5\bC6\bNAME\bA8\bAN9\bW10\bTITLE\bCONTENT\bD13\bM14\bRC15\bST16\bIP17",
        L"3\b<comments>",
        L"4\b<thread>",
        L"1\b<!--ROOT_BID--> at start <!--BID--><!--BID--> <!--CHAR_ID-->",
        L"2\bonly\btwo",
        L"3\b",
        L"4\bt",
        L"1001\b<html><edit var=title><multiedit var=content><!--BID--> <!--TITLE--> <!--CONTENT--> <!--CHAR_NAME--></html>",
        L"1002\b1\b2\b33\b4\b5\b6\bNN\b8\b9\b10\bThe title\b \b13",
        L"101\bpart one ",
        L"102\b",
        L"103\bpart three",
        L"5\bunknown key",
        L"101\bP1",
        L"102\bP2",
        L"103\bP3",
        L"1001\b<!--TITLE-->:<!--CONTENT-->",
        L"1002\bx\b\b",
        // no field: the key stays at the text itself, which matches none
        L"",
        L"\b",
        L"\b\b\b",
    };
    for (int i = 0; i < (int)(sizeof(Pieces) / sizeof(Pieces[0])); i++)
    {
        wchar_t Buf[512];
        wcscpy(Buf, Pieces[i]);
        Print("piece %d \"%s\"\n", i, Esc(Pieces[i], -1, 60).c_str());
        *(int*)(V + 0x11C) = 1;
        This<void>(VA_AddPiece, V, Buf);
        ShowViewer(V);
        Print("  edit box %s [%s]\n", Show(*(FStr*)(Edit + 0x1F0)).c_str(), TakeAllocs().c_str());
    }

    Print("== board window\n");
    BYTE* W = (BYTE*)calloc(0x6B8, 1);
    static void* Vtbl[80];
    for (void*& Slot : Vtbl)
        Slot = (void*)EnableRecorder;
    struct Button
    {
        void** Vtbl;
        int Index;
    } Buttons[6];
    for (int i = 0; i < 6; i++)
    {
        Buttons[i].Vtbl = Vtbl;
        Buttons[i].Index = i;
        *(Button**)(W + 0x9C + i * 4) = &Buttons[i];
    }
    *(BYTE**)(W + 0x94) = V;
    *(int*)(W + 0x6B4) = 1;
    wchar_t C1[] = L"bypass _bbshome", C2[] = L"", C3[] = L"bypass _bbsclan", C4[] = L"x", C5[] = L"",
            C6[] = L"bypass _bbsmemo";
    This<void>(VA_SetCommands, W, C1, C2, C3, C4, C5, C6);
    Print("locked %d\n", *(int*)(W + 0x6B4));
    for (int i = 0; i < 6; i++)
        Print("  command %d \"%s\"\n", i, Esc((const wchar_t*)(W + 0xB4 + i * 256)).c_str());
    wchar_t Page[] = L"<html>via the window</html>";
    This<void>(VA_LoadPage, W, Page);
    This<void>(VA_LoadPage, W, (wchar_t*)NULL);
    ShowViewer(V);
    Print("allocs [%s]\n", TakeAllocs().c_str());
}
