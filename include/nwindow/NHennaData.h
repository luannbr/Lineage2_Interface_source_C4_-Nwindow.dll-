#pragma once
// FL2HennaData - a tattoo of the game data (the Engine's, found by FL2GameData::GetHennaData(HennaID); only what NWindow
// reads is placed: the strings). Offsets from NCEquipItemWnd::AddHenna 0x1007F980, NCHennaInfoWnd 0x10079240 and the
// console's slot 139 (0x100597D0), which copy them into their entries.

#include "nwindow/NWindow.h"

struct FL2HennaData
{
    BYTE _Unknown00[8];                // +0x00 (the id and the like: nothing in NWindow reads them)
    FString Name;                      // +0x08 (C2: FStrings)
    FString IconName;                  // +0x14 the icon's texture
    FString Text;                      // +0x20 the line the paperdoll's tool tip shows as the description
    FString Description;               // +0x2C the long description of the henna windows
};
NW_ASSERT_OFFSET(FL2HennaData, Name, 0x8);
NW_ASSERT_OFFSET(FL2HennaData, IconName, 0x14);
NW_ASSERT_OFFSET(FL2HennaData, Text, 0x20);
NW_ASSERT_OFFSET(FL2HennaData, Description, 0x2C);
