#pragma once
// C4 stage 4, lane msn2: what the C4 stock does differently from NMsnDlgCommon.h (that header is shared with the dialogs of the msn1
// lane and keeps the C3 shape) in the dialogs this lane ports.
//
// The push button of the dialogs (repeated inline in each stock OnCreate): in C4 the text is given by its number,
// NCButton::SetTextID(n) (0x100014A0), and not by `SetText(*SysString[n])`; "OK" is the SysString 140 and "Cancel" 141 as in C3.
// Evidence: OfflineConfirmDialog::OnCreate 0x10146740 (push 0x8C / push 0x8D before the two calls of 0x100014A0).

#include "nwindow/NMsnDlgCommon.h"

static FORCEINLINE NCButton* NewDlgButtonC4(NCWnd* Parent, INT ID, INT X, INT Y, INT TextID)
{
    NCButton* Button = new NCButton(TEXT("L2UI_CH3.Button.btn1_normal"), TEXT("L2UI_CH3.Button.btn1_normalon"), 0);
    Button->ID = ID;
    Button->SetPosition(X, Y, 76, 23);
    Parent->CreateChild(Button, NWS_VISIBLE);
    Button->SetTextID(TextID);
    return Button;
}
enum { DLGSTR_OK = 140, DLGSTR_CANCEL = 141 };
