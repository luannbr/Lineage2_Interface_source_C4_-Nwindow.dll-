#pragma once
// Chat-style adding of a message to a list box (names ours). NCChatListBox::AddString and
// NCPetitionListBox::AddString have identical bodies with this inlined: the text is wrapped to
// the list width - first line up to Width - 22 px, the next ones up to Width - 34 px, indented
// 12 px - and, unlike NCListBox::AddString, the view always follows the last line.

#include "nwindow/NCListBox.h"
#include "nwindow/NCGDevice.h"
#include "nwindow/NString.h"

// one wrapped line at the end of the list (inlined twice in each AddString)
static __forceinline void NAppendWrappedLine(NCListBox* List, NString* Str, INT Param)
{
    if (List->NumLines < List->MaxLines)
    {
        if (++List->NumLines > List->VisibleLines)
            List->TopIndex++;
    }
    else
    {
        NString* Oldest = List->Items(0).Text;
        if (Oldest)
        {
            delete Oldest;
            List->Items(0).Text = NULL;
        }
        List->Items.Remove(0, 1);
    }
    INT i = List->Items.Add();
    List->Items(i).Text = Str;
    List->Items(i).Param = Param;
    List->UpdateScrollBar();
}

static __forceinline void NAddWrappedString(NCListBox* List, const TCHAR* Text, DWORD Color,
                                            INT Param)
{
    if (!Text)
        return;
    TCHAR* Line = NCGDevice::FirstWrappedLine(Text, List->Width - 22, 0);
    if (!Line)
        return;
    NString* Str = new NString(Line);
    Str->Color = Color;
    NAppendWrappedLine(List, Str, Param);
    for (Line = NCGDevice::NextWrappedLine(List->Width - 34, 0); Line;
         Line = NCGDevice::NextWrappedLine(List->Width - 34, 0))
    {
        NString* Next = new NString(Line);
        Next->Color = Color;
        Next->Indent = 12;
        NAppendWrappedLine(List, Next, Param);
    }
}
