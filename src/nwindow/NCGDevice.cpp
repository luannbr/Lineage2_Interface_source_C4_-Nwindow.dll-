/*=============================================================================
    NCGDevice.cpp - paint device (UCanvas wrapper with window origin).
    Stock (C3): 0x100106b0 .. 0x10011260, 0x10011ef0 .. 0x10012550 (the origins' deque is NStdLib.cpp's).
    The text align parser (Init / Destroy) is allocated by hand: its C3 size is 0x78, the engine header's
    FL2TextAlignParser says 0x6C.
=============================================================================*/
#include "nwindow/NCGDevice.h"
#include "nwindow/NL2Font.h"
#include "nwindow/NCWnd.h"
#include "nwindow/NConsoleWnd.h"

#include <wchar.h>

/*-----------------------------------------------------------------------------
    Construction / destruction
-----------------------------------------------------------------------------*/

// 0x10014210 - NCObject's constructor, then the canvas, the origin, the deque's four words and the +0x70..+0x7C words
// (bTransparent and the Thai array) cleared; the digit textures and the dirty range are left to the object's zeroed memory
NCGDevice::NCGDevice()
{
    Canvas = NULL;
    OriginX = 0;
    OriginY = 0;
    bTransparent = 0;
}
NW_HOOK_CTOR(0x10014210, NCGDevice)

// 0x10014280 - the Thai array and the origins' deque go (the base's destructor last)
NW_HOOK_DTOR(0x10014280, NCGDevice)

// 0x10013620 (vslot 10) - the text align parser (made by Init, 0x78 bytes in C3) is cleared, destroyed and freed
void NCGDevice::Destroy()
{
    NCObject::Destroy();
    if (GTextAlignParser)
    {
        GTextAlignParser->Clear();
        FL2TextAlignParser* Parser = GTextAlignParser;
        if (Parser)
        {
            Parser->~FL2TextAlignParser();
            appFree(Parser);
        }
        GTextAlignParser = NULL;
    }
}
NW_HOOK_METHOD(0x10013620, NCGDevice, Destroy)

// 0x10013570 - the three digit textures (C4: the extra large one) and, for GLanguageType 1, the text align parser: the
// stock allocates 0x78 bytes for it
void NCGDevice::Init()
{
    SmallDigits = FindTexture(TEXT("SEK.cbui13"));
    LargeDigits = FindTexture(TEXT("L2UI.NWindow.Number"));
    XLargeDigits = FindTexture(TEXT("L2UI_CH3.Etc.XLargeNumber"));
    if (GLanguageType == 1)
    {
        void* Mem = appMalloc(0x78, TEXT("new"));
        FL2TextAlignParser* Parser = NULL;
        if (Mem)
            Parser = new (Mem) FL2TextAlignParser;
        GTextAlignParser = Parser;
    }
}
NW_HOOK_METHOD(0x10013570, NCGDevice, Init)

// 0x100129a0 - NCVirtualWndMain::SetGDevice, each frame
UBOOL NCGDevice::SetCanvas(UCanvas* InCanvas)
{
    Canvas = InCanvas;
    return 1;
}
NW_HOOK_METHOD(0x100129a0, NCGDevice, SetCanvas)

// 0x100129b0 - NCVirtualWndMain::ReleaseGDevice
void NCGDevice::ClearCanvas()
{
    Canvas = NULL;
}
NW_HOOK_METHOD(0x100129b0, NCGDevice, ClearCanvas)

/*-----------------------------------------------------------------------------
    Text
-----------------------------------------------------------------------------*/

// 0x100134b0 (cdecl) - the size of one glyph. TrueType text: the renderer the first viewport's controller reaches through
// three pointers (+0xE4, +0x74, +0x60; its virtual 53 gives the object that measures, its virtual 47 takes the character
// and the SIZE), the size then scaled by the canvas' font scale (+0x1BC). Else the L2 font's glyph record: its width and
// height (0, 0 without a font or a glyph)
void NCGDevice::GetCharSize(SIZE* Size, TCHAR C, INT FontType)
{
    if (GIsTrueFontUse)
    {
        APlayerController* Controller = GConsole->Engine->Client->Viewports(0)->Actor;
        BYTE* Source = ::nw::Field<BYTE*>(::nw::Field<BYTE*>(::nw::Field<BYTE*>(Controller, 0xE4), 0x74), 0x60);
        BYTE* Measurer = ::nw::VirtualCall<BYTE*>(Source, 53);
        ::nw::VirtualCall<void>(Measurer, 47, (INT)C, Size, 0);
        Size->cx = (INT)((DOUBLE)Size->cx * ::nw::Field<FLOAT>(GCanvas, 0x1BC));
        Size->cy = (INT)((DOUBLE)Size->cy * ::nw::Field<FLOAT>(GCanvas, 0x1BC));
        return;
    }
    UL2Font* Font = GCanvas->L2Fonts[FontType];
    Size->cx = 0;
    Size->cy = 0;
    if (Font)
    {
        FL2FontChar* Char = Font->GetChar(C);
        if (Char)
        {
            Size->cx = Char->Width;
            Size->cy = Char->Height;
        }
    }
}
NW_HOOK_FUNC(0x100134b0, NCGDevice::GetCharSize)

// 0x10012960 - the canvas of the console measures the text (its virtual 51 takes the text first, then the size); 0
// for no text
DWORD NCGDevice::GetTextExtent(SIZE* Size, const TCHAR* Text, DWORD P3, DWORD P4, INT P5, INT P7, DWORD P8,
                               L2FontType FontType)
{
    DWORD Result = 0;
    if (Text)
        Result = GCanvas->GetTextExtent(Text, Size, P3, P4, P5, FontType, P7, P8);
    return Result;
}
NW_HOOK_FUNC(0x10012960, NCGDevice::GetTextExtent)

// 0x100129c0 - DrawText's (see there) with the ranges and the three arguments the canvas's
// colored text takes (1, the origin's move since BeginPaint, 1). The 13th argument is not read.
DWORD NCGDevice::DrawColoredLine(INT X, INT Y, DWORD Color, const TCHAR* Text, DWORD P5, DWORD P6,
                                 DWORD P7, L2FontType FontType, INT P9, INT P10, DWORD P11,
                                 TArray<FL2ColorFontInfo*>* ColorInfo, INT P13)
{
    if (!Canvas)
        return 0;
    if (!Text)
        return 0;
    SIZE Size;
    GetTextExtent(&Size, Text, P5, P6, X, 0, 0, L2FONT_Normal);  // X really is the 5th argument, as in DrawText
    LineHeight = Size.cy;
    Touch(OriginY + Y, Size.cy);
    return Canvas->DrawTextToCanvas(OriginX + X, OriginY + Y, Color, Text, 0, P5, P6, 0.f, P7, FontType, P9,
                                    P10, P11, 1, OriginX - PaintOriginX, 1, ColorInfo, 0,
                                    (EFontExceptionType)0);
}
NW_HOOK_METHOD(0x100129c0, NCGDevice, DrawColoredLine)

/*-----------------------------------------------------------------------------
    The Thai text path (new in C2) and the line wrapper.
-----------------------------------------------------------------------------*/

// 0x1000d560 (name ours) - how many characters the cluster holds
INT ThaiCharacter::CharCount() const
{
    INT Count = 1;
    if (W[1] != 0)
    {
        if (W[2] != 0)
            Count = 2;
        if (W[3] != 0)
            Count++;
        if (W[0] != 0)
            Count++;
        if (W[4] != 0)
            Count++;
        if (W[6] != 0)
            Count++;
    }
    return Count;
}
NW_HOOK_METHOD(0x1000d560, ThaiCharacter, CharCount)

// 0x10013410 (name ours) - GCanvas->LoadFont (vslot 34)
void NCGDevice::LoadFont(INT Index, TCHAR* P2, TCHAR* P3)
{
    GCanvas->LoadFont(Index, P2, P3);
}
NW_HOOK_FUNC(0x10013410, NCGDevice::LoadFont)

// 0x10013430 (name ours) - GCanvas->DestroyFont (vslot 35)
void NCGDevice::DestroyFont(INT Index)
{
    GCanvas->DestroyFont(Index);
}
NW_HOOK_FUNC(0x10013430, NCGDevice::DestroyFont)

// 0x10013450 (name ours) - GCanvas->MakeThaiCharArray: Text split into clusters, Buffer (as long as Text) gets
// the characters in the order the clusters give them
void NCGDevice::MakeThaiCharArray(const TCHAR* Text, TArray<ThaiCharacter>& Chars, TCHAR* Buffer)
{
    GCanvas->MakeThaiCharArray(Text, Chars, Buffer);
}
NW_HOOK_FUNC(0x10013450, NCGDevice::MakeThaiCharArray)

// width of one glyph of Font added to Width (the Thai measuring in both wrappers)
static FORCEINLINE void AddGlyphWidth(UL2Font* Font, TCHAR C, DWORD& Width)
{
    FL2FontChar* Char = Font->GetChar(C);
    if (Char)
        Width += Char->Width;
}

// the width of one cluster is that of its glyphs: the mark at +0xE alone when there is one, else the base at +2 and
// the one at +0, else the one at +0 or the one at +0xA
static FORCEINLINE void AddClusterWidth(UL2Font* Font, const ThaiCharacter& Cluster, DWORD& Width)
{
    if (Cluster.W[7] != 0)
        AddGlyphWidth(Font, Cluster.W[7], Width);
    else if (Cluster.W[1] != 0)
    {
        AddGlyphWidth(Font, Cluster.W[1], Width);
        if (Cluster.W[0] != 0)
            AddGlyphWidth(Font, Cluster.W[0], Width);
    }
    else if (Cluster.W[0] != 0)
        AddGlyphWidth(Font, Cluster.W[0], Width);
    else if (Cluster.W[5] != 0)
        AddGlyphWidth(Font, Cluster.W[5], Width);
}


// 0x100137e0 - the first line of Text that fits MaxWidth pixels (NULL when none does). English: the text align
// parser; Thai (GLanguageType 5): clusters; else one character at a time. The line is built in GWrapLine, the text
// copied to GWrapText, GWrapPos is how far it was taken. C3: a carriage return stays in the line, a line feed ends it
// (it stays too); the other control characters are skipped (C2 skipped both of those); the parser takes three more
// arguments (0, NULL, 0).
TCHAR* NCGDevice::FirstWrappedLine(const TCHAR* Text, DWORD MaxWidth, INT FontType)
{
    DWORD LineWidth = 0;
    if (!Text || !GCanvas)
        return NULL;
    UL2Font* Font = GCanvas->L2Fonts[FontType];
    if (Font)
    {
        if (GLanguageType == 1 && GTextAlignParser)
        {
            INT LineStart = 0;   // GetLineBuffer's out value: the offset in Text where the line starts (not a length)
            GTextAlignParser->Clear();
            GTextAlignParser->SetParserParam((TCHAR*)Text, MaxWidth, Font, 0, 0, 1, 0, NULL, 0);
            return GTextAlignParser->GetLineBuffer(LineStart);
        }
        if (GLanguageType != 5)
        {
            wcscpy(GWrapText, Text);
            GWrapLen = (INT)wcslen(GWrapText);
            GWrapPos = 0;
            GWrapLine[0] = 0;
            INT Count = 0;
            if (GWrapText[0])
            {
                const TCHAR* p = GWrapText;
                do
                {
                    TCHAR C = *p++;
                    if (GWrapPos >= GWrapLen)
                        break;
                    if (C == 0x0D)
                        GWrapLine[Count++] = C;
                    else if (C == 0x0A)
                    {
                        GWrapPos++;
                        GWrapLine[Count++] = C;
                        break;
                    }
                    else if (C >= ' ')
                    {
                        FL2FontChar* Char = Font->GetChar(C);
                        if (Char)
                        {
                            LineWidth = (DWORD)((DOUBLE)Char->Width + (DOUBLE)LineWidth);
                            if (LineWidth > MaxWidth)
                                break;
                        }
                        GWrapLine[Count++] = C;
                    }
                    GWrapPos++;
                } while (*p);
            }
            GWrapLine[Count] = 0;
            return Count ? GWrapLine : NULL;
        }

        // Thai
        TArray<ThaiCharacter> Chars;
        TCHAR* Buffer = new TCHAR[appStrlen(Text) + 1];
        MakeThaiCharArray(Text, Chars, Buffer);
        wcscpy(GWrapText, Buffer);
        GWrapLen = (INT)wcslen(GWrapText);
        GWrapLine[0] = 0;
        UL2Font* ThaiFont = GCanvas->L2Fonts[FontType];
        GWrapPos = 0;
        INT Taken = 0;
        if (ThaiFont)
        {
            for (INT i = 0; i < Chars.Num(); i++)
            {
                ThaiCharacter Cluster = Chars(i);
                AddClusterWidth(ThaiFont, Cluster, LineWidth);
                if (MaxWidth < LineWidth)
                    break;
                Taken++;
                GWrapPos += Cluster.CharCount();
            }
            INT Total = 0;
            for (INT i = 0; i < Taken; i++)
                Total += Chars(i).CharCount();
            if (Total > 0 && Buffer)
            {
                appStrncpy(GWrapLine, Buffer, Total + 1);
                GWrapLine[Total + 1] = 0;   // one past the terminator appStrncpy wrote, as in the stock code
                delete[] Buffer;
                return GWrapLine;
            }
        }
    }
    return NULL;
}
NW_HOOK_FUNC(0x100137e0, NCGDevice::FirstWrappedLine)


// 0x10013be0 - continues where the previous line stopped (GWrapPos); the same rules for the characters
TCHAR* NCGDevice::NextWrappedLine(DWORD MaxWidth, INT FontType)
{
    DWORD LineWidth = 0;
    if (!GCanvas)
        return NULL;
    if (GLanguageType == 1 && GTextAlignParser)
    {
        INT LineStart = 0;   // GetLineBuffer's out value: the offset in Text where the line starts (not a length)
        GTextAlignParser->SetWidthParam(MaxWidth);
        return GTextAlignParser->GetLineBuffer(LineStart);
    }
    if (GLanguageType == 5)
    {
        if (GWrapPos < GWrapLen)
        {
            TCHAR* Rest = &GWrapText[GWrapPos];
            TArray<ThaiCharacter> Chars;
            TCHAR* Buffer = new TCHAR[appStrlen(Rest) + 1];
            MakeThaiCharArray(Rest, Chars, Buffer);
            GWrapLine[0] = 0;
            UL2Font* Font = GCanvas->L2Fonts[FontType];
            INT Taken = 0;
            if (Font)
            {
                for (INT i = 0; i < Chars.Num(); i++)
                {
                    ThaiCharacter Cluster = Chars(i);
                    AddClusterWidth(Font, Cluster, LineWidth);
                    if (MaxWidth < LineWidth)
                        break;
                    Taken++;
                    GWrapPos += Cluster.CharCount();
                }
                INT Total = 0;
                for (INT i = 0; i < Taken; i++)
                    Total += Chars(i).CharCount();
                if (Total > 0 && Buffer)
                {
                    appStrncpy(GWrapLine, Buffer, Total + 1);
                    GWrapLine[Total + 1] = 0;   // one past the terminator appStrncpy wrote, as in the stock code
                    delete[] Buffer;
                    return GWrapLine;
                }
            }
        }
    }
    else if (GWrapPos < GWrapLen)
    {
        GWrapLine[0] = 0;
        UL2Font* Font = GCanvas->L2Fonts[FontType];
        INT Count = 0;
        const TCHAR* p = &GWrapText[GWrapPos];
        if (Font)
        {
            for (TCHAR C = *p; C; C = *p)
            {
                p++;
                if (GWrapPos >= GWrapLen)
                    break;
                if (C == 0x0D)
                    GWrapLine[Count++] = C;
                else if (C == 0x0A)
                {
                    GWrapPos++;
                    GWrapLine[Count++] = C;
                    break;
                }
                else if (C >= ' ')
                {
                    FL2FontChar* Char = Font->GetChar(C);
                    if (Char)
                    {
                        LineWidth = (DWORD)((DOUBLE)Char->Width + (DOUBLE)LineWidth);
                        if (LineWidth > MaxWidth)
                            break;
                    }
                    GWrapLine[Count++] = C;
                }
                GWrapPos++;
            }
            GWrapLine[Count] = 0;
            return Count ? GWrapLine : NULL;
        }
    }
    return NULL;
}
NW_HOOK_FUNC(0x10013be0, NCGDevice::NextWrappedLine)

// 0x10012aa0 - text at (X, Y) of the device origin; LineHeight and the dirty range follow the
// measured text. C2: the canvas's DrawText is DrawTextToCanvas (vslot 46) with two more
// arguments (0, 0), the measure takes the font (0), and the 15th argument is not read.
DWORD NCGDevice::DrawText(INT X, INT Y, DWORD Color, const TCHAR* Text, DWORD P5, DWORD P6, DWORD P7,
                          L2FontType FontType, INT P9, INT P10, DWORD P11, INT P12, INT P13, INT P14,
                          INT P15)
{
    if (!Canvas)
        return 0;
    if (!Text)
        return 0;
    SIZE Size;
    GetTextExtent(&Size, Text, P5, P6, X, 0, 0, L2FONT_Normal);  // X really is the 5th argument in the stock code
    LineHeight = Size.cy;
    Touch(OriginY + Y, Size.cy);
    return Canvas->DrawTextToCanvas(OriginX + X, OriginY + Y, Color, Text, 0, P5, P6, 0.f, P7, FontType, P9,
                                    P10, P11, P12, P13, P14, NULL, 0, (EFontExceptionType)0);
}
NW_HOOK_METHOD(0x10012aa0, NCGDevice, DrawText)

// 0x10013090 - text over a world position (the canvas's virtual 49; its last word is 0); no origin
void NCGDevice::Draw3DCoordText(FVector Loc, DWORD Color, TCHAR* Text, UTexture* Tex, INT P5, INT P6,
                                L2FontType FontType)
{
    guard(NCGDevice::Draw3DCoordText);
    Canvas->Draw3DCoordText(Loc, Color, Text, Tex, P5, P6, FontType, 0);
    unguard;
}
NW_HOOK_METHOD(0x10013090, NCGDevice, Draw3DCoordText)

// 0x10013140 (name ours: the stock names it in its guard) - the chat balloon over a world position (the canvas's
// virtual 73, new in C3); every argument goes through
void NCGDevice::DrawChatting(FVector Loc, DWORD Color, TCHAR* Text, INT P4, INT P5, L2FontType FontType, INT P7,
                             INT P8, DWORD P9)
{
    guard(NCGDevice::DrawChatting);
    Canvas->DrawChatting(Loc, Color, Text, P4, P5, FontType, P7, P8, P9);
    unguard;
}
NW_HOOK_METHOD(0x10013140, NCGDevice, DrawChatting)

// 0x10013350 - the digits of one of the three digit textures (the canvas's virtual 58; C4: three kinds, an alpha as the
// last argument, which the canvas takes as 255 - Alpha, and two numbers per kind where C3 had one); 0 for no text or no
// canvas. The stock leaves the texture and the sizes undefined for a Type above 2 (the callers pass 0, 1 or 2): ours
// draws nothing then.
DWORD NCGDevice::DrawSpecialDigit(INT Type, INT X, INT Y, DWORD Color, TCHAR* Text, BYTE Alpha)
{
    if (Text && Canvas)
    {
        UTexture* Digits;
        INT W1, W2;
        if (Type == 0)
        {
            Digits = SmallDigits;
            W1 = 5;
            W2 = 5;
        }
        else if (Type == 1)
        {
            Digits = LargeDigits;
            W1 = 8;
            W2 = 8;
        }
        else if (Type == 2)
        {
            Digits = XLargeDigits;
            W1 = 0xB;
            W2 = 0xE;
        }
        else
            return 0;
        return Canvas->DrawSpecialDigit(W1, W2, OriginX + X, OriginY + Y, Color, Text, Digits, (BYTE)(255 - Alpha));
    }
    return 0;
}
NW_HOOK_METHOD(0x10013350, NCGDevice, DrawSpecialDigit)

/*-----------------------------------------------------------------------------
    Shapes and textures
-----------------------------------------------------------------------------*/

// C2 CANDIDATE: the C2 function is not the same code as the C1 one; kept only while it matches the stock one (verify_data)
// 0x10012b80
void NCGDevice::DrawRect(INT X, INT Y, INT XL, INT YL, DWORD Color)
{
    UTexture* White = FindTexture(TEXT("NWindow.WhiteTexture"));
    Canvas->DrawRect(OriginX + X, OriginY + Y, XL, YL, White, Color);
    Touch(OriginY + Y, YL);
}
NW_HOOK_METHOD(0x10012b80, NCGDevice, DrawRect)

// 0x10012bf0 - the texture stretched to the rectangle (nothing for no texture, or one of no size). C3: an alpha of 0xFF
// (the low byte) is drawn as 0x80 while the transparency flag (+0x60, set by the main window for the dragged stack)
// is up
void NCGDevice::DrawStretchedTex(INT X, INT Y, INT XL, INT YL, UTexture* Tex, INT Param6, DWORD Alpha)
{
    if (Tex && (Tex->USize || Tex->VSize))
    {
        if ((BYTE)Alpha == 0xFF && bTransparent)
            Canvas->DrawStretchedTex(OriginX + X, OriginY + Y, XL, YL, Tex, Param6 != 0, 0x80);
        else
            Canvas->DrawStretchedTex(OriginX + X, OriginY + Y, XL, YL, Tex, Param6 != 0, (BYTE)Alpha);
        Touch(OriginY + Y, YL);
    }
}
NW_HOOK_METHOD_SIG(0x10012bf0, NCGDevice, DrawStretchedTex,
                   void (NCGDevice::*)(INT, INT, INT, INT, UTexture*, INT, DWORD))

// 0x10012cb0 - the same for a part of the texture (SrcX, SrcY, SrcXL, SrcYL in texels)
void NCGDevice::DrawStretchedTex(INT X, INT Y, INT XL, INT YL, INT SrcX, INT SrcY, INT SrcXL, INT SrcYL,
                                 UTexture* Tex, DWORD Alpha)
{
    if (Tex && (Tex->USize || Tex->VSize))
    {
        if ((BYTE)Alpha == 0xFF && bTransparent)
            Canvas->DrawStretchedTex(OriginX + X, OriginY + Y, XL, YL, SrcX, SrcY, SrcXL, SrcYL, Tex, 0x80);
        else
            Canvas->DrawStretchedTex(OriginX + X, OriginY + Y, XL, YL, SrcX, SrcY, SrcXL, SrcYL, Tex, (BYTE)Alpha);
        Touch(OriginY + Y, YL);
    }
}
NW_HOOK_METHOD_SIG(0x10012cb0, NCGDevice, DrawStretchedTex,
                   void (NCGDevice::*)(INT, INT, INT, INT, INT, INT, INT, INT, UTexture*, DWORD))

// 0x10012d90 - a part of the texture (U, V, UL, VL in texels) at X, Y, XL x YL; the same alpha rule
void NCGDevice::DrawTexture(INT X, INT Y, INT XL, INT YL, INT U, INT V, INT UL, INT VL, UTexture* Tex, DWORD Alpha,
                            INT Param11)
{
    if (Tex && (Tex->USize || Tex->VSize))
    {
        if ((BYTE)Alpha == 0xFF && bTransparent)
            Canvas->DrawTexture(OriginX + X, OriginY + Y, XL, YL, (FLOAT)U, (FLOAT)V, (FLOAT)UL, (FLOAT)VL, Tex,
                                0x80, Param11 != 0);
        else
            Canvas->DrawTexture(OriginX + X, OriginY + Y, XL, YL, (FLOAT)U, (FLOAT)V, (FLOAT)UL, (FLOAT)VL, Tex,
                                (BYTE)Alpha, Param11 != 0);
        Touch(OriginY + Y, YL);
    }
}
NW_HOOK_METHOD(0x10012d90, NCGDevice, DrawTexture)

// 0x10012e90
void NCGDevice::DrawBrightenTexture(INT X, INT Y, INT XL, INT YL, INT U, INT V, INT UL, INT VL, UTexture* Tex,
                                    DWORD Brightness)
{
    if (Tex && (Tex->USize || Tex->VSize))
    {
        Canvas->DrawBrightenTexture(OriginX + X, OriginY + Y, XL, YL, (FLOAT)U, (FLOAT)V, (FLOAT)UL, (FLOAT)VL,
                                    Tex, Brightness);
        Touch(OriginY + Y, YL);
    }
}
NW_HOOK_METHOD(0x10012e90, NCGDevice, DrawBrightenTexture)

// 0x10013010 - a line (both ends moved by the origin); the dirty range takes both ends' Y
void NCGDevice::DrawLine(INT X1, INT Y1, INT X2, INT Y2, DWORD Color, INT Width)
{
    Canvas->DrawLine(OriginX + X1, OriginY + Y1, X2 + OriginX, OriginY + Y2, Color, Width);
    Touch(OriginY + Y1, 0);
    Touch(OriginY + Y2, 0);
}
NW_HOOK_METHOD(0x10013010, NCGDevice, DrawLine)

// 0x100131f0 - the pledge's crest: the texture the console keeps for the id (CrestID > 0), 16 x 12 texels from the
// fifth row; 1 when drawn
UBOOL NCGDevice::DrawPledgeCrest(INT X, INT Y, INT CrestID)
{
    if (CrestID > 0)
    {
        // C4: the stock calls 0x1006A900 (NetHandler slot 45, by the clan's id), not GetPledgeCrestTexture 0x1006A8E0 (slot 43)
        UTexture* Tex = GConsole->GetPledgeCrestTex(CrestID);
        if (Tex && (Tex->USize || Tex->VSize))
        {
            Canvas->DrawTexture(OriginX + X, OriginY + Y, 16, 12, 0.0f, 4.0f, 16.0f, 12.0f, Tex, 0xFF, 1);
            Touch(OriginY + Y, 12);
            return 1;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x100131f0, NCGDevice, DrawPledgeCrest)

// 0x10013290 - the alliance's crest, 8 x 12 texels from the fifth row
UBOOL NCGDevice::DrawAllyCrest(INT X, INT Y, INT CrestID)
{
    if (CrestID > 0)
    {
        // C4: the stock calls 0x1006A910 (NetHandler slot 46), not GetAllyCrestTexture 0x1006A8F0 (slot 44)
        UTexture* Tex = GConsole->GetAllyCrestTex(CrestID);
        if (Tex && (Tex->USize || Tex->VSize))
        {
            Canvas->DrawTexture(OriginX + X, OriginY + Y, 8, 12, 0.0f, 4.0f, 8.0f, 12.0f, Tex, 0xFF, 1);
            Touch(OriginY + Y, 12);
            return 1;
        }
    }
    return 0;
}
NW_HOOK_METHOD(0x10013290, NCGDevice, DrawAllyCrest)

/*-----------------------------------------------------------------------------
    Canvas passthroughs
-----------------------------------------------------------------------------*/

// 0x10012f30 - the minimap's piece of the map (C4: the canvas takes a UMaterial and an alpha; nothing for no texture).
// An alpha of 0xFF (the low byte) is drawn as 0x80 while the transparency flag is up, as the texture draws do.
void NCGDevice::DrawMinimapLoc(INT X, INT Y, UMaterial* Tex, BYTE Alpha)
{
    guard(NCGDevice::DrawMinimapLoc);
    if (Tex)
    {
        if (Alpha == 0xFF && bTransparent)
            Canvas->DrawMinimapLoc(OriginX + X, OriginY + Y, Tex, 0x80);
        else
            Canvas->DrawMinimapLoc(OriginX + X, OriginY + Y, Tex, Alpha);
    }
    unguard;
}
NW_HOOK_METHOD(0x10012f30, NCGDevice, DrawMinimapLoc)

// 0x10012fe0 - the radar's back (C3: the canvas draws the radar in three steps, each one a jump to the import)
void NCGDevice::DrawRadar()
{
    Canvas->DrawRadarBack();
}
NW_HOOK_METHOD(0x10012fe0, NCGDevice, DrawRadar)

// 0x10012ff0 (name ours) - the rest of the radar
void NCGDevice::DrawRadarEtc()
{
    Canvas->DrawRadarEtc();
}
NW_HOOK_METHOD(0x10012ff0, NCGDevice, DrawRadarEtc)

// 0x10013000 - one mark of the radar (a world position: no origin)
void NCGDevice::DrawRadarTarget(INT X, INT Y)
{
    Canvas->DrawRadarTarget(X, Y);
}
NW_HOOK_METHOD(0x10013000, NCGDevice, DrawRadarTarget)

// 0x10013330 - clip rectangles are absolute (no origin)
void NCGDevice::SetClip(INT X, INT Y, INT XL, INT YL)
{
    Canvas->SetClip(X, Y, XL, YL);
}
NW_HOOK_METHOD(0x10013330, NCGDevice, SetClip)

// 0x10013340
void NCGDevice::ReleaseClip()
{
    Canvas->ReleaseClip();
}
NW_HOOK_METHOD(0x10013340, NCGDevice, ReleaseClip)

/*-----------------------------------------------------------------------------
    Tool tip box (no stock function: inlined in NCButton::OnDrawToolTip 0x10001dd0
    and NCIcon::OnDrawToolTip 0x10015910).
-----------------------------------------------------------------------------*/

// Clip, background and text are placed relative to the device origin (the window): above it
// (Y = -height) or below it (Y = Wnd->Height), moved left when the box would leave the screen.
// C3: there is no such function in the image (each OnDrawToolTip repeats the box) and nobody of ours calls it: empty
void NCGDevice::DrawToolTipBox(NCWnd* Wnd, const TCHAR* Text, UTexture* Back, UBOOL bAbove)
{
}

/*-----------------------------------------------------------------------------
    Origin stack
-----------------------------------------------------------------------------*/

// 0x10014870
void NCGDevice::PushOrigin(INT DX, INT DY)
{
    POINT Move;
    Move.x = DX;
    Move.y = DY;
    Origins.PushBack(Move);
    OriginX += Move.x;
    OriginY += Move.y;
}
NW_HOOK_METHOD(0x10014870, NCGDevice, PushOrigin)

// 0x100142f0 - back to the origin before the last PushOrigin (the deque's back() and pop_back(), inline in the stock)
void NCGDevice::PopOrigin()
{
    POINT Last = Origins.Back();
    OriginX -= Last.x;
    OriginY -= Last.y;
    Origins.PopBack();
}
NW_HOOK_METHOD(0x100142f0, NCGDevice, PopOrigin)

NW_CHECK_VTABLE(NCGDevice, 0x101a20b0, 25)
