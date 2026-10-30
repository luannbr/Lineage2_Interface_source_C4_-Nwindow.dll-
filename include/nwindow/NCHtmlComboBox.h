#pragma once
// NCHtmlComboBox - the <combobox> of the HTML pages (UClass "CHtmlComboBox", 0x200): an
// NCComboBox whose drop button is an NCHtmlButton (kept in its own field) and that paints
// clipped to the viewer while closed. Its items come from a ';'-separated list; its value is
// the chosen item.

#include "nwindow/NCComboBox.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCHtmlButton;

class NCHtmlComboBox : public NCComboBox
{
    NW_DECLARE_CLASS(NCHtmlComboBox, NCComboBox, 0, 0x102CB8B0)

    INT Unknown1F8;                  // +0x24C   // C2: offset derived, no evidence   // C2: offset derived, no evidence   // C2: offset derived, no evidence
    NCHtmlButton* HtmlDropButton;    // +0x250 (NCComboBox::DropButton stays NULL)   // C2: offset derived, no evidence   // C2: offset derived, no evidence

    NCHtmlComboBox();                                                // 0x1003ebc0
    // ~NCHtmlComboBox                                               // 0x100325f0 (C1 only: no C2 match) (C1 only: no C2 match)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x10047180
    virtual INT OnCreate();                                          // 37 0x1004b280
    virtual INT OnDestroy();                                         // 38 0x10047640
    virtual void ShowWindow();                                       // 48 0x100efc40 (shared)
    virtual void HideWindow();                                       // 49 0x100471a0 (shared)
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x10047700
    virtual const TCHAR* GetValue();                                 // 74 0x100476f0

    INT SetList(TCHAR* List, TCHAR* Sel);                            // 0x10047200 (name ours)
};
NW_ASSERT_OFFSET(NCHtmlComboBox, HtmlDropButton, 0x250);
NW_ASSERT_SIZE(NCHtmlComboBox, 0x254);

#pragma pack(pop)