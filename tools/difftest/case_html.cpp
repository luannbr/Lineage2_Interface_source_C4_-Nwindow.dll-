// The HTML tokenizer of the page viewer (NHtmlParser, NHtmlTag, the tag table): built-in pages
// and, for each folder of server pages given (--pages NAME=DIR), every *.htm under it, read in
// place (only the hashes of the text and of what the tokenizer made of it are printed).
#include "difftest.h"

#include <algorithm>

using namespace dt;

namespace {

// stock entry points
enum : DWORD
{
    VA_ParserCtor = 0x1002ca80,
    VA_ParserDtor = 0x1002cb00,
    VA_Parse = 0x1002fae0,
    VA_FreeTags = 0x1002f670,
    VA_GetAttr = 0x10025d70,
    VA_HasAttr = 0x10025dd0,
    VA_GConsole = 0x10122894,
};

struct WStr        // std::wstring of the stock build
{
    DWORD Alloc;
    union
    {
        wchar_t Buf[8];
        wchar_t* Ptr;
    };
    DWORD Size;
    DWORD Res;
    const wchar_t* CStr() const { return Res >= 8 ? Ptr : Buf; }
};

std::string ShowStr(const WStr& S)
{
    char Buf[32];
    sprintf(Buf, "%lu/%lu\"", S.Size, S.Res);
    return Buf + Esc(S.CStr(), S.Size) + "\"";
}

struct Tag         // NHtmlTag (0x60)
{
    int Id;
    BYTE bOpen;
    BYTE Pad[3];
    WStr Text;
    DWORD IdsAlloc;
    unsigned short* Ids;
    unsigned short* IdsLast;
    unsigned short* IdsEnd;
    DWORD ValsAlloc;
    WStr* Vals;
    WStr* ValsLast;
    WStr* ValsEnd;
    WStr Colors;
};
static_assert(sizeof(Tag) == 0x60, "NHtmlTag");

struct TagList
{
    DWORD Alloc;
    Tag** First;
    Tag** Last;
    Tag** End;
};

// the dump of one parse: every tag on a line, the parser's strings, the allocations
std::string Dump(const wchar_t* Page)
{
    std::string S;
    BYTE Parser[0x84 + 16];
    memset(Parser, 0xCD, sizeof(Parser));
    This<void>(VA_ParserCtor, Parser);
    S += "ctor: " + TakeAllocs() + "\n";
    // the stock library throws out_of_range on some pages ("<font color=" with no '>'): the
    // tags read so far are lost, the unwinding frees what the frames held
    TagList* Tags = NULL;
    Caught Thrown = Try([&] { Tags = This<TagList*>(VA_Parse, Parser, Page); });
    S += "parse: " + TakeAllocs() + "\n";
    if (Thrown.bThrown)
        S += Show(Thrown) + "\n";
    else if (!Tags)
        S += "NULL\n";
    else
    {
        char Buf[64];
        sprintf(Buf, "tags %d room %d\n", (int)(Tags->Last - Tags->First), (int)(Tags->End - Tags->First));
        S += Buf;
        for (Tag** It = Tags->First; It != Tags->Last; It++)
        {
            Tag* T = *It;
            sprintf(Buf, "T%d o%d ", T->Id, T->bOpen);
            S += Buf + ShowStr(T->Text) + " c" + ShowStr(T->Colors);
            int NumIds = (int)(T->IdsLast - T->Ids);
            if (NumIds || T->Ids)
            {
                sprintf(Buf, " a%d/%d", NumIds, (int)(T->IdsEnd - T->Ids));
                S += Buf;
                for (int i = 0; i < NumIds; i++)
                {
                    sprintf(Buf, " %u=", T->Ids[i]);
                    S += Buf + (i < (int)(T->ValsLast - T->Vals) ? ShowStr(T->Vals[i]) : "?");
                }
                sprintf(Buf, " v%d", (int)(T->ValsEnd - T->Vals));
                S += Buf;
                // the accessors of the page tree
                std::string Get;
                unsigned Has = 0;
                for (int Id = 1; Id <= 12; Id++)
                {
                    if (This<bool>(VA_HasAttr, T, Id))
                        Has |= 1u << Id;
                    const wchar_t* V = This<const wchar_t*>(VA_GetAttr, T, Id, 0);
                    Get += V ? Esc(V) : "-";
                    Get += '|';
                }
                sprintf(Buf, " has%x get%s", Has, Hex32(Hash32(Get)).c_str());
                S += Buf;
            }
            S += "\n";
        }
    }
    char Buf[64];
    sprintf(Buf, "error %d\n", *(int*)(Parser + 0xC));
    S += Buf;
    S += "name " + ShowStr(*(WStr*)(Parser + 0x10)) + " value " + ShowStr(*(WStr*)(Parser + 0x2C)) +
         " extra " + ShowStr(*(WStr*)(Parser + 0x48)) + " u64 " + ShowStr(*(WStr*)(Parser + 0x64)) + "\n";
    This<void>(VA_FreeTags, Parser, Tags);
    S += "free: " + TakeAllocs() + "\n";
    This<void>(VA_ParserDtor, Parser);
    S += "dtor: " + TakeAllocs() + "\n";
    return S;
}

const wchar_t* const Pages[] = {
    L"<html><body>Hello &nbsp; world<br>Text &lt;tag&gt; &amp; &quot;q&quot; &apos;a&apos; &#160;x</body></html>",
    L"<html><body><center><font color=\"LEVEL\">Title</font></center><br>Some <font color=\"FF0000\">red</font> text and <font color=\"00FF00\">green</font> words.</body></html>",
    L"<html><body><a action=\"bypass -h npc_1_Chat 1\">Link</a><br1><a link=\"page.htm\">file</a> <a action=\"bypass x\" msg=\"Are you sure?\">Q</a></body></html>",
    L"<html><body><button value=\"OK\" action=\"bypass test\" width=80 height=20 back=\"L2UI.DefaultButton_click\" fore=\"L2UI.DefaultButton\"><edit var=\"name\" width=120 height=15><img src=\"L2UI.SquareWhite\" width=260 height=1></body></html>",
    L"<html><body><table border=0 width=270 bgcolor=000000><tr><td align=center width=100>Cell</td><td fixwidth=50>X</td></tr></table><combobox width=100 var=sel list=A;B;C></body></html>",
    L"<html><body><td foo=bar align=left baz=\"qux\" height = 20 >unknown attrs</td></body></html>",
    L"<html><!-- comment > here --><body><blink>x</blink><!- short -><!></body></html>",
    L"<html><body>   \r\n\t leading   and    inner \t\r\n  blanks   </body></html>",
    L"<html><body>&$123; and &$abc; then &$0; and &$; end &#10; nl &$2;&$3;x</body></html>",
    L"<html><body>< font color=red>spaced</font> <font>no color</font> text <font color=\"LEVEL\">only</font> </font> stray</body></html>",
    L"<HTML><BODY><BR><A HREF=\"x\">UPPER</A><Table WIDTH=10></Table></BODY></HTML>",
    L"<html><body><multiedit var=\"m\" width=200 height=50><textarea>t</textarea><select><option>o</option></select></body></html>",
    L"<html><extend>inside <b>bold</b> </extend> after</html>",
    L"<html><body>short</body></html>",
    L"no tags at all, just text with &amp; entities &lt;",
    L"",
    L"<",
    L"<html",
    L"<html><body><font color=\"FF0000\">unterminated",
    L"<html><body>x<font color=\"12\">a</font>y<font color=\"34\">bb</font>z<font color=\"56\">ccc</font></body></html>",
    L"<html><body>text<br>more&nbsp;&nbsp;&nbsp;spaces&nbsp;&nbsp;</body></html>",
    L"<html><body><a action=\"bypass -h npc_%objectId%_Quest\">Quest</a><br><a action=\"bypass -h npc_%objectId%_Chat 0\">Back</a></body></html>",
    L"<html><title>T</title><head>h</head><body>b<p>p</p><h1>h1</h1><pre>pre</pre><ul><li>li</li></ul><b>b</b><i>i</i><u>u</u><tt>tt</tt><strike>s</strike><sub>s</sub><sup>s</sup><left>l</left><right>r</right><address>a</address><var>v</var><spin>s</spin><bar>b</bar><volumn>v</volumn></body></html>",
    L"<html><body><img src=\"a\" width=1 height=2 extra=3><img src=\"b\"></body></html>",
    L"<html><body><button value=Cancel action=\"bypass -h cancel\" width=60 height=20 back=\"b\" fore=\"f\"><button value=\"\" action=\"\"></body></html>",
    L"<html><body>\"quotes\" = equals > gt</body></html>",
    L"<html><body><a>empty a</a><a action>no value</a><a =x>no name</a><a \"q\">quote only</a></body></html>",
    // castle macros (no castle data outside the game: "-") and entities next to each other
    L"<html><body>Castle &%1; at &^2; is &*3;; &%0;x &^;y &*abc;z &lt;&gt;&amp;&amp;amp;</body></html>",
    // a gatekeeper-like page and a quest page (written for the test)
    L"<html><body>Gatekeeper Roxxy:<br>Where would you like to go?<br><a action=\"bypass -h npc_%objectId%_goto 1\">The Village of Gludin - 7300 adena</a><br><a action=\"bypass -h npc_%objectId%_goto 2\">Talking Island Village - 9400 adena</a><br><a action=\"bypass -h npc_%objectId%_Chat 0\">Back.</a></body></html>",
    L"<html><head><body>Trader:<br>Please bring me <font color=\"LEVEL\">5 Wolf Pelts</font>. I will give you <font color=\"LEVEL\">1,000 adena</font>.<br><br><table width=260><tr><td width=130><button value=\"Accept\" action=\"bypass -h Quest Q001 accept\" width=80 height=21 back=\"sek.cbui94\" fore=\"sek.cbui92\"></td><td><button value=\"Decline\" action=\"bypass -h npc_%objectId%_Chat 0\" width=80 height=21 back=\"sek.cbui94\" fore=\"sek.cbui92\"></td></tr></table></body></html>",
    L"<html><body><edit var=\"amount\" type=number width=100 height=15> <edit var=\"pass\" type=password> <edit var=\"d\" type=date> <edit type=time var=t> <combobox var=c list=\"one;two;three\" sel=two width=90></body></html>",
    // long values: the strings leave their 7-character buffer and grow
    L"<html><body><a action=\"bypass -h 0123456789012345678901234567890123456789012345678901234567890123456789\">0123456789abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz</a></body></html>",
    // the stock library throws out_of_range: a <font> whose </font> is past the next tag (37 of
    // the 23649 server pages), "color=" too close to the start
    L"<html><body>Hello <font color=\"LEVEL\">name<br>next line</font> end</body></html>",
    L"acolor=<font>x",
    L"<html><body><font color=\"LEVEL\">a</font> <font color=\"00FF00\">b<br></font></body></html>",
};

// the sysstrings the "&$n;" macros read (FL2GameData::SysString, empty outside the game)
void SetSysStrings()
{
    BYTE* GameData = (BYTE*)GetProcAddress(GetModuleHandleW(L"Engine.dll"), "?GL2GameData@@3VFL2GameData@@A");
    FStr* SysString = (FStr*)(GameData + 0x2E810);
    Set(SysString[1], L"One");
    Set(SysString[2], L"Two words");
    Set(SysString[3], L"&lt;three&gt;");
    Set(SysString[123], L"one hundred twenty-three");
    ClearAllocs();
}

// castle macros ask the console, which only reads the game data: any non-null console will do
struct ConsoleStandIn
{
    BYTE Mem[0x4000];
    DWORD Old;
    ConsoleStandIn()
    {
        memset(Mem, 0, sizeof(Mem));
        Old = Global<DWORD>(VA_GConsole);
        Global<DWORD>(VA_GConsole) = (DWORD)Mem;
    }
    ~ConsoleStandIn() { Global<DWORD>(VA_GConsole) = Old; }
};

}  // namespace

DT_CASE(html, "HTML tokenizer: built-in pages (tags, attributes, strings, allocations)")
{
    SetSysStrings();
    ConsoleStandIn Console;
    int N = sizeof(Pages) / sizeof(Pages[0]);
    for (int i = 0; i < N; i++)
    {
        Print("== page %d (%d characters)\n", i, (int)wcslen(Pages[i]));
        Print("%s", Dump(Pages[i]).c_str());
    }
    // a longer page: the tag list and the strings grow several times
    std::wstring Big = L"<html><body>";
    for (int i = 0; i < 40; i++)
    {
        wchar_t Buf[256];
        swprintf(Buf, 256, L"line %d <font color=\"%06X\">word%d</font> &amp; <a action=\"bypass -h x %d\">link %d</a><br>\r\n",
                 i, i * 4321, i, i, i);
        Big += Buf;
    }
    Big += L"</body></html>";
    Print("== long page (%d characters)\n", (int)Big.size());
    Print("%s", Dump(Big.c_str()).c_str());

    // long macros: what lies between the mark and ';' goes into a 512-character buffer of the
    // stock frame, so at most 511 here. A number with leading zeros (replaced), then one that
    // is not a number (removed, the search ends)
    const wchar_t* Marks[] = {L"&$", L"&%", L"&^", L"&*"};
    for (const wchar_t* Mark : Marks)
        for (int Len : {300, 511})
        {
            std::wstring Page = L"<html><body>a ";
            Page += Mark + std::wstring(Len - 1, L'0') + L"2; b ";
            Page += Mark + std::wstring(Len, L'x') + L"; c</body></html>";
            Print("== long macro %s (%d characters)\n", Esc(Mark).c_str(), Len);
            Print("%s", Dump(Page.c_str()).c_str());
        }
}

namespace {

bool IsPage(const std::wstring& Name)
{
    size_t Dot = Name.rfind(L'.');
    return Dot != std::wstring::npos &&
           (_wcsicmp(Name.c_str() + Dot, L".htm") == 0 || _wcsicmp(Name.c_str() + Dot, L".html") == 0);
}

// the pages under Root, as paths relative to it with '/'
void ListPages(const std::wstring& Root, const std::wstring& Rel, std::vector<std::wstring>& Out)
{
    WIN32_FIND_DATAW F;
    HANDLE H = FindFirstFileW((Root + L"/" + Rel + (Rel.empty() ? L"*" : L"/*")).c_str(), &F);
    if (H == INVALID_HANDLE_VALUE)
        return;
    do
    {
        std::wstring Name = F.cFileName;
        if (Name == L"." || Name == L"..")
            continue;
        std::wstring Sub = Rel.empty() ? Name : Rel + L"/" + Name;
        if (F.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ListPages(Root, Sub, Out);
        else if (IsPage(Name))
            Out.push_back(Sub);
    } while (FindNextFileW(H, &F));
    FindClose(H);
}

// a page file as the server sends it: UTF-16 with a BOM, else UTF-8 when it decodes, else 1252
std::wstring ReadPage(const std::wstring& Path)
{
    FILE* F = _wfopen(Path.c_str(), L"rb");
    if (!F)
        return L"";
    std::string Bytes;
    char Buf[4096];
    size_t N;
    while ((N = fread(Buf, 1, sizeof(Buf), F)) > 0)
        Bytes.append(Buf, N);
    fclose(F);
    if (Bytes.size() >= 2 && (BYTE)Bytes[0] == 0xFF && (BYTE)Bytes[1] == 0xFE)
        return std::wstring((const wchar_t*)(Bytes.data() + 2), (Bytes.size() - 2) / 2);
    size_t Skip = Bytes.size() >= 3 && (BYTE)Bytes[0] == 0xEF && (BYTE)Bytes[1] == 0xBB && (BYTE)Bytes[2] == 0xBF ? 3 : 0;
    int Len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, Bytes.data() + Skip, (int)(Bytes.size() - Skip), NULL, 0);
    UINT Cp = Len > 0 || Bytes.size() == Skip ? CP_UTF8 : 1252;
    DWORD Flags = Cp == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0;
    Len = MultiByteToWideChar(Cp, Flags, Bytes.data() + Skip, (int)(Bytes.size() - Skip), NULL, 0);
    std::wstring W(Len, 0);
    MultiByteToWideChar(Cp, Flags, Bytes.data() + Skip, (int)(Bytes.size() - Skip), &W[0], Len);
    return W;
}

}  // namespace

std::vector<std::wstring> dt::OnlyPages;

// "page text-hash result-hash" for each page (the golden files keep only the two hashes: a page
// edited on the server is no longer compared, it does not fail); --verbose writes the results
void dt::RunPages(const std::vector<std::wstring>& Args)
{
    SetSysStrings();
    ConsoleStandIn Console;
    const std::wstring& Root = Args[0];
    std::vector<std::wstring> Files;
    ListPages(Root, L"", Files);
    std::sort(Files.begin(), Files.end());
    if (!OnlyPages.empty())
    {
        std::vector<std::wstring> Some;
        for (const std::wstring& Rel : Files)
            if (std::find(OnlyPages.begin(), OnlyPages.end(), Rel) != OnlyPages.end())
                Some.push_back(Rel);
        Files.swap(Some);
    }
    for (const std::wstring& Rel : Files)
    {
        std::wstring Page = ReadPage(Root + L"/" + Rel);
        std::string Name = Esc(Rel.c_str());
        std::string In = Hex32(Hash32(std::string((const char*)Page.data(), Page.size() * sizeof(wchar_t))));
        Where = Name;
        std::string D = Dump(Page.c_str());
        Print("%s %s %s\n", Name.c_str(), In.c_str(), Hex32(Hash32(D)).c_str());
        if (Verbose)
            Print("%s", D.c_str());
    }
}
