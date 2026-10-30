#pragma once
// NCPetitionBeginWnd - the form that opens a petition (UClass "CPetitionBeginWnd", 0xEC; the system menu
// and the console open it): a combo box with the kinds of petition (C3: the categories of localization.ini,
// [CategoryNum] Num and [Category<n>] Title / Link), the help page of the kind chosen (an NCHtmlViewer), a
// multi-line edit for the text and OK / Cancel. OK checks the text (5 to 254 characters, a kind chosen),
// sends the petition and starts the conversation window (NCPetitionWnd) with it; Cancel withdraws one
// already open.

#include "nwindow/NCWnd.h"
#pragma pack(push, 4)   // the C2 layout puts 64-bit members at 4-aligned offsets

class NCButton;
class NCComboBox;
class NCHtmlViewer;
class NCMultiEdit;

// A kind of petition as localization.ini lists it (0x204 bytes, name ours): the sysstring of its name and the
// help page the form loads when it is chosen ("..\L2text\<Link>")
struct NPetitionCategory
{
    INT TitleID;             // +0x000 [Category<n>] Title: the combo's item (AddItem(INT))
    TCHAR Link[256];         // +0x004 [Category<n>] Link: the help file
};
NW_ASSERT_SIZE(NPetitionCategory, 0x204);

class NCPetitionBeginWnd : public NCWnd
{
    NW_DECLARE_CLASS(NCPetitionBeginWnd, NCWnd, 0, 0x102F63B8)

    NCButton* OKButton;              // +0xE8 text 694 (name ours)
    NCButton* CancelButton;          // +0xEC text 473 (name ours)
    NCHtmlViewer* HelpViewer;        // +0xF0 the help page of the kind chosen (name ours)
    NCComboBox* TypeCombo;           // +0xF4 the kinds: 1 move .. 9 etc (0 = none chosen) (name ours)
    NCMultiEdit* TextEdit;           // +0xF8 the petition (name ours)
    UTexture* BackTex;               // +0xFC "L2UI.PetitionWnd.PetitionBeginBack"
    TArray<NPetitionCategory> Categories;  // +0x100 (new in C3, name ours) the kinds the combo lists, in order
                                     //   (the combo's item N is Categories(N - 1): item 0 is the "choose" line)

    NCPetitionBeginWnd();                                            // 0x1005f370
    // ~NCPetitionBeginWnd                                           // 0x1005f3d0 (the array's is 0x10055640)

    virtual INT WindowProc(UINT Msg, WPARAM wParam, LPARAM lParam);  // 25 0x100e00d0
    virtual INT OnCreate();                                          // 37 0x100e2b50
    virtual INT OnDestroy();                                         // 38 0x100e23d0
    virtual INT OnPaint(NCGDevice* Device);                          // 62 0x100e0af0
    virtual INT OnSend();                                            // 74 0x100e0c40 (name ours) OK
    virtual INT OnCancel();                                          // 75 0x100e0dc0 (name ours)
    virtual void NewVirtual76();  // 76 0x100e0df0 (only this client has it; defined in NNewVirtuals.gen.cpp):
                                  // the body is OnTypeChosen's; WindowProc calls the slot (the combo's selection)
    INT OnTypeChosen(INT Type);                              // 0x100e0df0 (name ours): the help page of kind Type

    // name ours: the form emptied (kind 0, no text, no help page)
    __declspec(noinline) void ResetForm();                           // 0x100e0170
    // name ours (0x100d1f00, sub_100d1f00): the combo and Categories from localization.ini
    __declspec(noinline) void LoadCategories();
};
NW_ASSERT_OFFSET(NCPetitionBeginWnd, OKButton, 0xE8);
NW_ASSERT_OFFSET(NCPetitionBeginWnd, HelpViewer, 0xF0);
NW_ASSERT_OFFSET(NCPetitionBeginWnd, TextEdit, 0xF8);
NW_ASSERT_OFFSET(NCPetitionBeginWnd, BackTex, 0xFC);
NW_ASSERT_OFFSET(NCPetitionBeginWnd, Categories, 0x100);
NW_ASSERT_SIZE(NCPetitionBeginWnd, 0x10C);

#pragma pack(pop)
