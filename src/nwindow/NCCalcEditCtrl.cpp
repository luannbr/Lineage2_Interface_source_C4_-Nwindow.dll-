/*=============================================================================
    NCCalcEditCtrl.cpp - the calculator's display and keyboard (new in C2). Stock (C3): 0x10109a40 (WindowProc, shared
    with NCHtmlEdit's), 0x10109a60 .. 0x1010a1a3 (the arithmetic: members that sit between other classes' code),
    0x1010a6b0 .. 0x1010a9bd (OnChar) and 0x1010ab40 .. 0x1010ac0f (constructor, destructor).
=============================================================================*/
#include "nwindow/NCCalcEditCtrl.h"

// the digits of the display are formatted by NCEditBox::ReformatNumber (thousands separators, the caret): a member
// without arguments
static FORCEINLINE void FormatDigits(NCCalcEditCtrl* Edit)
{
    Edit->ReformatNumber();
}

enum { CALC_NoOperator = -1, CALC_Add = 0, CALC_Sub = 1, CALC_Mul = 2, CALC_Div = 3 };
enum { CALC_Typing = 0, CALC_Cleared = 1, CALC_OperatorSet = 2, CALC_TypingSecond = 3 };

// 0x1011fa20 - a digits-only edit box, no extra width
NCCalcEditCtrl::NCCalcEditCtrl()
    : NCEditBox(0, NULL, 1, 0, 0)
{
    Accumulator = 0;
    Operator = 0;
    State = 0;
    ExtraWidth = 0;
    Type = 3;
}
NW_HOOK_CTOR(0x1011fa20, NCCalcEditCtrl)

// 0x1011faa0
NW_HOOK_DTOR(0x1011faa0, NCCalcEditCtrl)

// 0x10109a40 (vslot 25) - NCEditBox's, as a boolean; one stock body with NCHtmlEdit's, which holds the hook
INT NCCalcEditCtrl::WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam)
{
    return NCEditBox::WindowProc(Msg, wParam, lParam) != 0;
}

// 0x1011ea90 - the number as appAtof (a float) reads it, kept as a double
void NCCalcEditCtrl::SetAccumulator(const TCHAR* Text)
{
    if (Text)
        Accumulator = appAtof(Text);
    else
        Accumulator = 0;
}
NW_HOOK_METHOD(0x1011ea90, NCCalcEditCtrl, SetAccumulator)

// 0x1011ead0
void NCCalcEditCtrl::SetOperator(INT Op)
{
    Operator = Op;
}
NW_HOOK_METHOD(0x1011ead0, NCCalcEditCtrl, SetOperator)

// 0x1011eae0 - the display's text, copied (CommitText then NCEditBox's ReformatNumber), as a number
DOUBLE NCCalcEditCtrl::GetCurrent()
{
    TCHAR Buffer[128];
    appStrcpy(Buffer, CommitText());
    FormatDigits(this);
    if (Buffer[0])
        return appAtof(Buffer);
    return 0.0;
}
NW_HOOK_METHOD(0x1011eae0, NCCalcEditCtrl, GetCurrent)

// 0x1011eb40 (ret 8) - the display emptied and the number put in, without decimals
void NCCalcEditCtrl::ShowNumber(DOUBLE Value)
{
    TCHAR Buffer[128];
    ClearText();
    appSprintf(Buffer, TEXT("%.0f"), Value);
    InsertText(Buffer, 1);
}
NW_HOOK_METHOD(0x1011eb40, NCCalcEditCtrl, ShowNumber)

// 0x1011eb90 (ret 0x14) - A op B; no operator: B; a division by zero: 0
DOUBLE NCCalcEditCtrl::Compute(DOUBLE A, DOUBLE B, INT Op)
{
    switch (Op)
    {
    case CALC_NoOperator:
        return B;
    case CALC_Add:
        return A + B;
    case CALC_Sub:
        return A - B;
    case CALC_Mul:
        return A * B;
    case CALC_Div:
        if (B != 0.0)
            return A / B;
        break;
    }
    return 0.0;
}
NW_HOOK_METHOD(0x1011eb90, NCCalcEditCtrl, Compute)

// 0x1011ec10
void NCCalcEditCtrl::ShowZero()
{
    ClearText();
    InsertText(TEXT("0"), 1);
}
NW_HOOK_METHOD(0x1011ec10, NCCalcEditCtrl, ShowZero)

// 0x1011ef80 - everything cleared, the display shows 0
void NCCalcEditCtrl::Reset()
{
    Accumulator = 0;
    Operator = CALC_NoOperator;
    State = CALC_Cleared;
    ClearText();
    ShowNumber(0.0);
}
NW_HOOK_METHOD(0x1011ef80, NCCalcEditCtrl, Reset)

// 0x1011efc0 (ret 0x1c) - the sum or the product of two positive numbers that came out negative or past 1e20 (as a
// float: 1.00000002004087734e20): the calculator is cleared
UBOOL NCCalcEditCtrl::IsOverflow(DOUBLE A, DOUBLE B, INT Op, DOUBLE Result)
{
    if ((Op == CALC_Add || Op == CALC_Mul) && A > 0.0 && B > 0.0 && (Result < 0.0 || Result > 1.0000000200408773e+20))
    {
        Reset();
        return 1;
    }
    return 0;
}
NW_HOOK_METHOD(0x1011efc0, NCCalcEditCtrl, IsOverflow)

// 0x1011f640 (vslot 64) - a digit goes into the display (a leading 0 is replaced); an operator or "=" (Enter) works the
// arithmetic out. Only digits and operators are looked at (the box takes digits), and only while it can take more
// (bCanInsert), except the backspace's 8.
INT NCCalcEditCtrl::OnChar(WPARAM wParam, LPARAM lParam)
{
    UINT Key = (UINT)wParam;
    if (Key != 8 && !bCanInsert)
        return 1;
    if (Key == 0xD)
        Key = '=';
    else if (Key >= '0' && Key <= '9')
    {
        if (State == CALC_Typing || State == CALC_TypingSecond)
        {
            if (appStrcmp(*Left, TEXT("0")) == 0)
            {
                if (Key == '0')
                    return 1;
                ClearText();
            }
        }
        if (State == CALC_Cleared)
        {
            ClearText();
            State = CALC_Typing;
        }
        else if (State == CALC_OperatorSet)
        {
            SetAccumulator(CommitText());
            ClearText();
            State = CALC_TypingSecond;
        }
        TCHAR Digit[20];
        appSprintf(Digit, TEXT("%c"), Key);
        if (Left.Num() != 0 && Left.Num() - 1 > 0)
            Left += Digit;
        else
            Left = Digit;
        FormatDigits(this);
        return 1;
    }
    else if (Key != '*' && Key != '+' && Key != '-' && Key != '/' && Key != '=')
        goto Tail;

    // an operator, or "=": what is on the display is the second operand
    switch (State)
    {
    case CALC_Cleared:
        State = CALC_OperatorSet;
        break;
    case CALC_Typing:
        SetAccumulator(CommitText());
        FormatDigits(this);
        State = CALC_OperatorSet;
        break;
    case CALC_TypingSecond:
    {
        DOUBLE Second = GetCurrent();
        DOUBLE Result = Compute(Accumulator, Second, Operator);
        Second = GetCurrent();
        if (!IsOverflow(Accumulator, Second, Operator, Result))
        {
            SetAccumulator(CommitText());
            FormatDigits(this);
            ShowNumber(Result);
            State = CALC_OperatorSet;
        }
        break;
    }
    case CALC_OperatorSet:
        if (Key == '=')
        {
            // "=" again: the display's number with the same operand and operator
            DOUBLE Shown = GetCurrent();
            DOUBLE Result = Compute(Shown, Accumulator, Operator);
            if (IsOverflow(Shown, Accumulator, Operator, Result))
                return 1;
            TCHAR Buffer[128];
            appSprintf(Buffer, TEXT("%.0f"), Result);
            ClearText();
            InsertText(Buffer, 1);
            return 1;
        }
        break;
    }

Tail:
    if (Key == '*')
        SetOperator(CALC_Mul);
    else if (Key == '+')
        SetOperator(CALC_Add);
    else if (Key == '-')
        SetOperator(CALC_Sub);
    else if (Key == '/')
        SetOperator(CALC_Div);
    return 1;
}
NW_HOOK_METHOD(0x1011f640, NCCalcEditCtrl, OnChar)

NW_CHECK_VTABLE(NCCalcEditCtrl, 0x101d0790, 87)
