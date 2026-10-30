// Text measured and wrapped with the L2 font (NCGDevice::GetCharSize, FirstWrappedLine,
// NextWrappedLine, NL2Font::GetChar, FL2FontPage::Find) on a font made here: fixed glyph
// sizes in pages, one of them with glyphs of no width, and characters it lacks.
#include "difftest.h"

using namespace dt;

namespace {

enum : DWORD
{
    VA_GetCharSize = 0x1000c700,
    VA_FirstWrappedLine = 0x1000c770,
    VA_NextWrappedLine = 0x1000c8f0,
    VA_GCanvas = 0x10122898,
    VA_NL2FontVtbl = 0x100e39b8,
};

struct Glyph
{
    int X, Width, Y, Height;
};
struct Page
{
    int First, Count;
    Glyph* Chars;
};

struct Font
{
    BYTE Obj[0x40];
    std::vector<Page> Pages;
    std::vector<std::vector<Glyph>> Glyphs;

    void AddPage(int First, int Count, int (*Width)(int), int Height)
    {
        Glyphs.push_back(std::vector<Glyph>(Count));
        for (int i = 0; i < Count; i++)
            Glyphs.back()[i] = {i * 16, Width(First + i), 0, Height};
        Pages.push_back({First, Count, &Glyphs.back()[0]});
    }
    void Build()
    {
        memset(Obj, 0, sizeof(Obj));
        *(void**)Obj = Stock(VA_NL2FontVtbl);
        for (size_t i = 0; i < Pages.size(); i++)
            Pages[i].Chars = &Glyphs[i][0];
        *(int*)(Obj + 0x38) = (int)Pages.size();
        *(Page**)(Obj + 0x3C) = &Pages[0];
    }
};

int AsciiWidth(int C)
{
    return C == '|' ? 0 : 3 + C % 7;   // '|' has no width: the font "lacks" it
}
int HangulWidth(int)
{
    return 12;
}
int JamoWidth(int C)
{
    return 8 + C % 3;
}

}  // namespace

DT_CASE(wrap, "text wrapping and glyph sizes with a made-up L2 font (NCGDevice, NL2Font)")
{
    Font Normal;
    Normal.AddPage(0x20, 0x5F, AsciiWidth, 12);
    Normal.AddPage(0xAC00, 0x2BA4, HangulWidth, 12);
    Normal.AddPage(0x3131, 0x33, JamoWidth, 11);
    Normal.Build();
    Font Small;
    Small.AddPage(0x30, 10, HangulWidth, 8);     // digits only
    Small.Build();
    BYTE* Canvas = (BYTE*)calloc(0x400, 1);
    ((void**)(Canvas + 0x12C))[0] = Normal.Obj;
    ((void**)(Canvas + 0x12C))[1] = Small.Obj;
    DWORD OldCanvas = Global<DWORD>(VA_GCanvas);
    Global<DWORD>(VA_GCanvas) = (DWORD)Canvas;

    Print("== GetCharSize\n");
    wchar_t Chars[] = {L'A', L' ', L'|', L'~', 0xAC00, 0xD7A3, 0xD7A4, 0x3131, 0xE9, 0x7F, 0x1F};
    for (wchar_t C : Chars)
    {
        SIZE S = {-1, -1};
        Cdecl<void>(VA_GetCharSize, &S, (DWORD)C);
        Print("%04x -> %ld x %ld\n", (unsigned)C, S.cx, S.cy);
    }

    Print("== wrap\n");
    const wchar_t* Texts[] = {
        L"Hello world, this is a test of the wrap.",
        L"",
        L"a\nb\r\nc\td",
        L"averyveryverylongwordwithoutanyspacesatallinsideofit",
        L"\xC548\xB155\xD558\xC138\xC694 \xBC18\xAC11\xC2B5\xB2C8\xB2E4 \xC5EC\xB7EC\xBD84",
        L"caf\xE9 \xFC\xFF missing glyphs \x7F\x80",
        L"|||| no width ||||",
        L"\x3131\x3134\x3137 jamo 12345",
        L"   leading spaces and trailing   ",
    };
    int Widths[] = {0, 1, 10, 30, 60, 100, 1000};
    for (const wchar_t* T : Texts)
        for (int FontType = 0; FontType < 3; FontType++)
            for (int W : Widths)
            {
                if (FontType && W != 30)
                    continue;
                std::string S;
                const wchar_t* Line = Cdecl<const wchar_t*>(VA_FirstWrappedLine, T, (DWORD)W, FontType);
                for (int n = 0; Line && n < 100; n++)
                {
                    S += " \"" + Esc(Line) + "\"";
                    Line = Cdecl<const wchar_t*>(VA_NextWrappedLine, (DWORD)W, FontType);
                }
                Print("\"%s\" font %d width %d:%s\n", Esc(T).c_str(), FontType, W, S.c_str());
            }
    // the next line without a first one, after the text ran out
    const wchar_t* After = Cdecl<const wchar_t*>(VA_NextWrappedLine, (DWORD)50, 0);
    Print("next after the end: %s\n", After ? "a line" : "NULL");
    Global<DWORD>(VA_GCanvas) = 0;
    Print("no canvas: first %s next %s\n",
          Cdecl<const wchar_t*>(VA_FirstWrappedLine, L"x", (DWORD)50, 0) ? "a line" : "NULL",
          Cdecl<const wchar_t*>(VA_NextWrappedLine, (DWORD)50, 0) ? "a line" : "NULL");
    Global<DWORD>(VA_GCanvas) = OldCanvas;
}
