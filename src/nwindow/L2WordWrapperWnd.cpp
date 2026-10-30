/*=============================================================================
    L2WordWrapperWnd.cpp - the developers' word wrap converter window (C3 addresses). Stock: ConnectServer 0x1003c7a0,
    SendRequest 0x1003c870, OnDestroy 0x1003c910, OnPaint 0x1003c950, ClearWrapCursor 0x1003c9c0, OnCreate 0x1003c9d0,
    WriteOutputFile 0x1003cbd0, the constructor 0x1003cc80, the destructor 0x1003cd10, WrapText 0x1003d130, ConvertDocument
    0x1003d540, LoadInputFile 0x1003da00, OnGenerate 0x1003db80 and WindowProc 0x1003dde0. WrapText, ConvertDocument,
    LoadInputFile and OnGenerate (the string and file work, 3 KB) stay with the stock code.
=============================================================================*/
#include <winsock2.h>
#include "nwindow/L2WordWrapperWnd.h"
#include "nwindow/NChatUtil.h"
#include "nwindow/NCButton.h"
#include "nwindow/NCComboBox.h"
#include "nwindow/NCConsole.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NCGodWnd.h"
#include "nwindow/NCMessageWnd.h"
#include "nwindow/NCHtmlViewer.h"
#include "nwindow/NCLoginWnd.h"
#include "nwindow/NCNPCHtmlViewer.h"
#include "nwindow/NCPetitionBeginWnd.h"
#include "nwindow/NCServerSelectWnd.h"
#include "nwindow/NConsoleWnd.h"

#include <string.h>

enum
{
    NID_WORDWRAP_GENERATE = (INT)0xFF330001,   // the button's id (name ours)
    NID_WORDWRAP_PAGETYPE = (INT)0xFF330002,   // the combo box's
};

#if NW_HOOK_MODE
// ws2_32 is not linked into the hook DLL: the calls go through the stock image's own import thunks (each one a
// `jmp [IAT]`), whose addresses are the stock ones. C4 order (read off the IAT names of the thunks and the stock calls of ConnectServer
// 0x10042450 / SendRequest 0x10042520): WSAGetLastError B02, connect B08, htons B0E, gethostbyname B14, inet_addr B1A, socket B20,
// WSAStartup B26, recv B2C, send B32 (the C3 map had them scrambled)
#define Ws_WSAStartup    ((int (WSAAPI*)(WORD, LPWSADATA))::nw::Addr(0x10170B26))
#define Ws_socket        ((SOCKET (WSAAPI*)(int, int, int))::nw::Addr(0x10170B20))
#define Ws_inet_addr     ((unsigned long (WSAAPI*)(const char*))::nw::Addr(0x10170B1A))
#define Ws_gethostbyname ((struct hostent* (WSAAPI*)(const char*))::nw::Addr(0x10170B14))
#define Ws_htons         ((u_short (WSAAPI*)(u_short))::nw::Addr(0x10170B0E))
#define Ws_connect       ((int (WSAAPI*)(SOCKET, const struct sockaddr*, int))::nw::Addr(0x10170B08))
#define Ws_WSAGetLastError ((int (WSAAPI*)())::nw::Addr(0x10170B02))
#define Ws_send          ((int (WSAAPI*)(SOCKET, const char*, int, int))::nw::Addr(0x10170B32))
#define Ws_recv          ((int (WSAAPI*)(SOCKET, char*, int, int))::nw::Addr(0x10170B2C))
#else
#define Ws_WSAStartup    WSAStartup
#define Ws_socket        socket
#define Ws_inet_addr     inet_addr
#define Ws_gethostbyname gethostbyname
#define Ws_htons         htons
#define Ws_connect       connect
#define Ws_WSAGetLastError WSAGetLastError
#define Ws_send          send
#define Ws_recv          recv
#endif

// the message line at the bottom of the screen (the God window's, as the stock code reaches it through the console)
static FORCEINLINE void WordWrapMessage(const TCHAR* Text)
{
    ((NCGodWnd*)GConsole->GodWnd)->MessageWnd->SetText(Text);
}

// 0x10042940 - the base window's constructor, the string empty, the button, count and cursor cleared (the combo box and the
// socket are left alone)
L2WordWrapperWnd::L2WordWrapperWnd()
{
    GenerateButton = NULL;
    ConvertedCount = 0;
    WrapCursor = 0;
}
NW_HOOK_CTOR(0x10042940, L2WordWrapperWnd)

// 0x100429d0 - the string's block freed (when it is out of line) and emptied, then the base
NW_HOOK_DTOR(0x100429d0, L2WordWrapperWnd)

// 0x10043ab0 (vslot 25)
INT L2WordWrapperWnd::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (NCWnd::WindowProc(Msg, wParam, lParam))
        return 1;
    if (Msg == NWM_COMMAND && (INT)wParam == NID_WORDWRAP_GENERATE)
        return OnGenerate() != 0;
    return 0;
}
NW_HOOK_METHOD(0x10043ab0, L2WordWrapperWnd, WindowProc)

// 0x10042450 (ret 8) - the connection: Winsock started, a stream socket made (kept in Socket), the host as an address or by
// name, connected to Port
BYTE L2WordWrapperWnd::ConnectServer(const char* Host, INT Port)
{
    WSADATA Data;
    Ws_WSAStartup(0x101, &Data);
    Socket = Ws_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Socket == INVALID_SOCKET)
        return 0;
    struct sockaddr_in Address;
    unsigned long IP = Ws_inet_addr(Host);
    if (IP == INADDR_NONE)
    {
        struct hostent* Entry = Ws_gethostbyname(Host);
        if (!Entry)
            return 0;
        memcpy(&Address.sin_addr, *Entry->h_addr_list, Entry->h_length);
        IP = Address.sin_addr.s_addr;
    }
    Address.sin_addr.s_addr = IP;
    Address.sin_family = AF_INET;
    Address.sin_port = Ws_htons((u_short)Port);
    if (Ws_connect(Socket, (struct sockaddr*)&Address, sizeof(Address)) != 0)
    {
        Ws_WSAGetLastError();
        return 0;
    }
    return 1;
}
NW_HOOK_METHOD(0x10042450, L2WordWrapperWnd, ConnectServer)

// 0x10042520 (ret 0xC) - the request sent whole; the answer read into the 4 KB reply until its first number (a count of
// characters) says it is complete or the buffer is full (the stock reads on at the previous read's size, not the total).
// A failed send says "Disconnect!!!!"
BYTE L2WordWrapperWnd::SendRequest(INT Len, const void* Request, void* Reply)
{
    if (Request && Reply && Socket)
    {
        if (Ws_send(Socket, (const char*)Request, Len, 0) > 0)
        {
            INT Got = 0;
            INT Total = 0;
            do
            {
                Got = Ws_recv(Socket, (char*)Reply + Got, 0x1000 - Got, 0);
                Total += Got;
                if (*(INT*)Reply * 2 + 4 == Total)
                    return 1;
            } while (Total < 0x1000);
            return 1;
        }
        WordWrapMessage(TEXT("Disconnect!!!!"));
    }
    return 0;
}
NW_HOOK_METHOD(0x10042520, L2WordWrapperWnd, SendRequest)

// 0x100425c0 (vslot 38) - the base, then the button
INT L2WordWrapperWnd::OnDestroy()
{
    NCWnd::OnDestroy();
    NDestroyChild(this, GenerateButton);
    return 1;
}
NW_HOOK_METHOD(0x100425c0, L2WordWrapperWnd, OnDestroy)

// 0x10042600 (vslot 62) - nothing is drawn of its own
INT L2WordWrapperWnd::OnPaint(NCGDevice* Device)
{
    Device->BeginPaint(ScreenX, ScreenY);
    Device->SetClip(ScreenX, ScreenY, (INT)Width, (INT)Height);
    Device->EndPaint();
    Device->ReleaseClip();
    return 1;
}
NW_HOOK_METHOD(0x10042600, L2WordWrapperWnd, OnPaint)

// 0x10042670 (name ours); the stock callers call it (OnGenerate), so it stays out of line
__declspec(noinline) void L2WordWrapperWnd::ClearWrapCursor()
{
    WrapCursor = 0;
}
NW_HOOK_METHOD(0x10042670, L2WordWrapperWnd, ClearWrapCursor)

// 0x10042680 (vslot 37) - the page-type combo box (four kinds of page, the first chosen), the Generate button, and the
// connection to the server (localhost:7778); the message line tells the result
INT L2WordWrapperWnd::OnCreate()
{
    NCWnd::OnCreate();

    PageCombo = new NCComboBox;
    PageCombo->ID = NID_WORDWRAP_PAGETYPE;
    PageCombo->SetPosition(0, 0x18, 0x7D, 0x11);
    CreateChild(PageCombo, NWS_VISIBLE | 0x10);
    PageCombo->AddItem(TEXT("Npc HTML"));
    PageCombo->AddItem(TEXT("Petition HTML"));
    PageCombo->AddItem(TEXT("Help HTML"));
    PageCombo->AddItem(TEXT("ServerInfo HTML"));
    PageCombo->SetSelected(0);

    GenerateButton = new NCButton(TEXT("L2UI.control.DefaultButton"), TEXT("L2UI.control.DefaultButton_click"), 0);
    GenerateButton->ID = NID_WORDWRAP_GENERATE;
    GenerateButton->SetPosition(0, 0, 0x43, 0x13);
    CreateChild(GenerateButton, NWS_VISIBLE);
    GenerateButton->SetText(TEXT("Generate!"));

    Socket = 0;
    if (!ConnectServer("localhost", 7778))
    {
        Socket = 0;
        WordWrapMessage(TEXT("Connection Failed to WordWrap Server!!!!!"));
        return 1;
    }
    WordWrapMessage(TEXT("Connection O~~~K~~~~ to WordWrap Server!!!!!"));
    return 1;
}
NW_HOOK_METHOD(0x10042680, L2WordWrapperWnd, OnCreate)

// 0x10042890 (ret 4, name ours) - the page (Document) written into ..\OUTPUT_THAI_HTML\<Name>: the characters copied into a
// buffer of 10240 and written whole (2 bytes each, no terminator)
__declspec(safebuffers) void L2WordWrapperWnd::WriteOutputFile(const TCHAR* Name)
{
    TCHAR Path[256];
    TCHAR Text[10240];
    appSprintf(Path, TEXT("..\\OUTPUT_THAI_HTML\\%s"), Name);
    FArchive* Ar = GFileManager->CreateFileWriter(Path, FILEWRITE_EvenIfReadOnly | FILEWRITE_Unbuffered, GNull);
    if (Ar)
    {
        appStrcpy(Text, Document.CStr());
        Ar->Serialize(Text, appStrlen(Text) * 2);
        Ar->Flush();
        delete Ar;
    }
}
NW_HOOK_METHOD(0x10042890, L2WordWrapperWnd, WriteOutputFile)

// The std::wstring members the stock code reaches: insert(Off, Str) (0x10042D80) and insert(Off, Right, ROff, Count) (0x10042A60), ported in
// NStl.cpp
static FORCEINLINE void DocumentInsert(NStlWString& Doc, UINT Off, const TCHAR* Str)
{
    Doc.Insert(Off, Str);
}
static FORCEINLINE void DocumentInsertString(NStlWString& Doc, UINT Off, const NStlWString& Right)
{
    Doc.Insert(Off, Right, 0, NStlWString::NPos);
}

// 0x10042df0 (ret 8) - the server's word wrapping of Text (Width pixels): the request {Width, length, the characters} goes out and
// the answer {count, the characters with "<br1>" at each break} is split at the marks. Each piece is looked for in the page
// (Document) from WrapCursor on, character by character, skipping over the tags <a ..>, <font ..>, </a.. and </f.. between
// them; where it ends a "<br1>" is inserted in the page and the search goes on after it
__declspec(safebuffers) void L2WordWrapperWnd::WrapText(TCHAR* Text, INT Width)
{
    TCHAR Piece[512];
    BYTE Request[0x1000];
    BYTE Reply[0x1000];
    TCHAR Wrapped[0x800];
    TCHAR Page[10240];
    DWORD* Fill;
    Fill = (DWORD*)Wrapped;
    for (INT n = 0x400; n != 0; n--)
        *Fill++ = 0;
    Fill = (DWORD*)Request;
    for (INT n = 0x400; n != 0; n--)
        *Fill++ = 0;
    Fill = (DWORD*)Reply;
    for (INT n = 0x400; n != 0; n--)
        *Fill++ = 0;

    INT Len = appStrlen(Text);
    *(INT*)Request = Width;
    *(INT*)(Request + 4) = Len;
    appMemcpy(Request + 8, Text, Len * 2);
    if (SendRequest(Len * 2 + 8, Request, Reply) && *Reply)
    {
        INT Count = *(INT*)Reply;
        memcpy(Wrapped, Reply + 4, Count * 2);
        NStlWString Rest;
        Rest.Assign(Wrapped, wcslen(Wrapped));
        for (;;)
        {
            UINT Mark = Rest.Find(TEXT("<br1>"), 0, wcslen(TEXT("<br1>")));
            if (Mark > Rest.Size)
                break;
            NStlWString Line;
            {
                NStlWString Tmp;
                Rest.Substr(&Tmp, 0, Mark);
                Line.Assign(Tmp, 0, NStlWString::NPos);
            }
            Rest.Erase(0, Mark + 5);
            appStrcpy(Piece, Line.CStr());
            appStrcpy(Page, Document.CStr());
            INT Skip = 0;
            INT i = 0;
            while (WrapCursor < Document.Size)
            {
                if (Piece[0] != Page[WrapCursor])
                    Skip = 0;
                else
                {
                    Skip++;
                    i = 1;
                    if (appStrlen(Piece) > 1)
                    {
                        UBOOL bDone = 0;
                        do
                        {
                            INT At = WrapCursor + Skip;
                            TCHAR Ch = Page[At];
                            if ((Ch == '<' && Page[At + 1] == 'a' && Page[At + 2] == ' ')
                                || (Ch == '<' && Page[At + 1] == 'f' && Page[At + 2] == 'o' && Page[At + 3] == 'n' && Page[At + 4] == 't')
                                || (Ch == '<' && Page[At + 1] == '/' && Page[At + 2] == 'a')
                                || (Ch == '<' && Page[At + 1] == '/' && Page[At + 2] == 'f'))
                            {
                                TCHAR* Tag = &Page[At];
                                if (*Tag != '>')
                                {
                                    do
                                    {
                                        Tag++;
                                        Skip++;
                                    } while (*Tag != '>');
                                }
                                Skip++;
                            }
                            if (Piece[i] != Page[WrapCursor + Skip])
                            {
                                i = 0;
                                break;
                            }
                            Skip++;
                            if (i == appStrlen(Piece) - 1)
                            {
                                bDone = 1;
                                break;
                            }
                            i++;
                        } while (i < appStrlen(Piece));
                        if (bDone || i >= appStrlen(Piece))
                        {
                            UINT Where = WrapCursor + Skip;
                            WrapCursor = Where + 5;
                            DocumentInsert(Document, Where, TEXT("<br1>"));
                            i = 0;
                            break;
                        }
                    }
                    else
                        i = 0;
                }
                WrapCursor++;
            }
        }
    }
}
NW_HOOK_METHOD(0x10042df0, L2WordWrapperWnd, WrapText)

// 0x10043200 (name ours) - for the <font ..> tags and then the <a ..> tags (twenty rounds each): a copy of the page is walked
// tag by tag; when the text under a tag holds a "<br1>" before the closing tag, the page gets the closing tag in front of the
// break and the opening tag again after it (so the lines break outside the tag)
void L2WordWrapperWnd::ConvertDocument()
{
    NStlWString Work;
    NStlWString Tag;
    NStlWString* Doc = &Document;
    for (INT Round = 20; Round != 0; Round--)
    {
        UINT Pos = 0;
        Work = NStlWString(*Doc);
        for (;;)
        {
            UINT At = Work.Find(TEXT("<font "), 0, wcslen(TEXT("<font ")));
            if (At > Work.Size)
                break;
            Work.Erase(0, At);
            Pos += At;
            UINT Gt = Work.Find(TEXT(">"), 0, wcslen(TEXT(">")));
            UINT TagLen = Gt + 1;
            {
                NStlWString Tmp;
                Work.Substr(&Tmp, 0, TagLen);
                Tag = Tmp;
            }
            Work.Erase(0, TagLen);
            Pos += Gt + 1;
            UINT Close = Work.Find(TEXT("</font>"), 0, wcslen(TEXT("</font>")));
            UINT Br = Work.Find(TEXT("<br1>"), 0, wcslen(TEXT("<br1>")));
            if (Br > Work.Size)
            {
                Work.Erase(0, Close + 7);
                break;
            }
            if (Br < Close)
            {
                Work.Erase(0, Br);
                Pos += Br;
                Doc->Insert(Pos, TEXT("</font>"), wcslen(TEXT("</font>")));
                DocumentInsertString(*Doc, Pos + 12, Tag);
                Pos += Tag.Size + 7;
            }
            else
            {
                Work.Erase(0, Close + 7);
                Pos += Close + 7;
            }
        }
    }
    for (INT Round = 20; Round != 0; Round--)
    {
        UINT Pos = 0;
        Work = NStlWString(*Doc);
        for (;;)
        {
            UINT At = Work.Find(TEXT("<a "), 0, wcslen(TEXT("<a ")));
            if (At > Work.Size)
                break;
            Work.Erase(0, At);
            Pos += At;
            UINT Gt = Work.Find(TEXT(">"), 0, wcslen(TEXT(">")));
            UINT TagLen = Gt + 1;
            {
                NStlWString Tmp;
                Work.Substr(&Tmp, 0, TagLen);
                Tag = Tmp;
            }
            Work.Erase(0, TagLen);
            Pos += Gt + 1;
            UINT Close = Work.Find(TEXT("</a>"), 0, wcslen(TEXT("</a>")));
            UINT Br = Work.Find(TEXT("<br1>"), 0, wcslen(TEXT("<br1>")));
            if (Br > Work.Size)
            {
                Work.Erase(0, Close + 4);
                break;
            }
            if (Br < Close)
            {
                Work.Erase(0, Br);
                Pos += Br;
                Doc->Insert(Pos, TEXT("</a>"), wcslen(TEXT("</a>")));
                DocumentInsertString(*Doc, Pos + 9, Tag);
                Pos += Tag.Size + 4;
            }
            else
            {
                Work.Erase(0, Close + 4);
                Pos += Close + 4;
            }
        }
    }
}
NW_HOOK_METHOD(0x10043200, L2WordWrapperWnd, ConvertDocument)

// 0x100436c0 (ret 4, name ours) - the file read whole (the block is zero-filled, 2 bytes longer than the file, and never freed)
// as UTF-16 into Document; every double space becomes a single one. 0 when the file cannot be opened
INT L2WordWrapperWnd::LoadInputFile(const TCHAR* Path)
{
    INT Size = GFileManager->FileSize(Path);
    FArchive* Ar = GFileManager->CreateFileReader(Path, 0, GNull);
    if (!Ar)
        return 0;
    BYTE* Buffer = (BYTE*)appMalloc(Size + 2, TEXT("ThaiHtmlConvertor"));
    appMemset(Buffer, 0, Size + 2);
    if (Buffer)
    {
        Ar->Serialize(Buffer, Size);
        {
            NStlWString Text;
            Text.Assign((const TCHAR*)Buffer, wcslen((const TCHAR*)Buffer));
            Document.Assign(Text, 0, NStlWString::NPos);
        }
        for (;;)
        {
            UINT At = Document.Find(TEXT("  "), 0, wcslen(TEXT("  ")));
            if (At >= Document.Size)
                break;
            Document.Replace(At, 2, TEXT(" "), wcslen(TEXT(" ")));
        }
    }
    delete Ar;
    return 1;
}
NW_HOOK_METHOD(0x100436c0, L2WordWrapperWnd, LoadInputFile)

// 0x10043840 - the Generate button: every file of ..\INPUT_THAI_HTML is loaded (LoadInputFile), shown in the page viewer of the
// kind the combo box has chosen (0 the NPC dialog, 1 the petition's help, 2 the help window, 3 the server list's page; the
// stock puts the four kinds into the combo again for every file), converted and written to ..\OUTPUT_THAI_HTML; the log says
// when each file begins and ends (the second line even when the file could not be loaded)
INT L2WordWrapperWnd::OnGenerate()
{
    ConvertedCount = 0;
    TCHAR Pattern[] = TEXT("..\\INPUT_THAI_HTML\\*.*");
    WIN32_FIND_DATAW Find;
    HANDLE hFind = FindFirstFileW(Pattern, &Find);
    if (hFind == INVALID_HANDLE_VALUE)
        return 1;
    do
    {
        TCHAR Path[256];
        appSprintf(Path, TEXT("..\\INPUT_THAI_HTML\\%s"), Find.cFileName);
        if (GLog)
        {
            GLog->Logf2(TEXT("%s parsing begin....."), Find.cFileName);
            GLog->Flush();
        }
        if (LoadInputFile(Path))
        {
            PageCombo->AddItem(TEXT("Npc HTML"));
            PageCombo->AddItem(TEXT("Petition HTML"));
            PageCombo->AddItem(TEXT("Help HTML"));
            PageCombo->AddItem(TEXT("ServerInfo HTML"));
            switch (PageCombo->Selected)
            {
            case 0:
                GDesktop->NpcHtmlWnd->ShowPage(Document.CStr());
                break;
            case 1:
                static_cast<NCPetitionBeginWnd*>(GDesktop->PetitionBeginWnd)->HelpViewer->LoadHtml(Document.CStr());
                break;
            case 2:
                GDesktop->HelpWnd->ShowPage(Document.CStr());
                break;
            case 3:
                static_cast<NCLoginServerWnd*>(static_cast<NCGodWnd*>(GConsole->GodWnd)->LoginWnd->ServerWnd)->ServerInfoWnd->Viewer->LoadHtml(Document.CStr());
                break;
            }
            ConvertDocument();
            WriteOutputFile(Find.cFileName);
            ClearWrapCursor();
            ConvertedCount++;
        }
        if (GLog)
        {
            GLog->Logf2(TEXT("%s parsing SUCCESS!!"), Find.cFileName);
            GLog->Flush();
        }
    } while (FindNextFileW(hFind, &Find));
    return 1;
}
NW_HOOK_METHOD(0x10043840, L2WordWrapperWnd, OnGenerate)

NW_CHECK_VTABLE(L2WordWrapperWnd, 0x101a9780, 74)
