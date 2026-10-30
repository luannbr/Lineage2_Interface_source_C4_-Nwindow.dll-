// check_ue2.cpp - compile-only translation unit for include/ue2/UE2Core.h and UE2Engine.h (C4).
//
// tools/ue2_check/run_check.py compiles it with the project's flags (NWD_CXX_FLAGS / NWD_DEFS of CMakeLists.txt,
// NW_HOOK_MODE=1 and 0) and requires: no diagnostic at all, which includes every static_assert of the headers
// (NW_ASSERT_OFFSET / NW_ASSERT_SIZE of the engine classes). The other two checks of the same script work on units it
// generates from the headers (tools/ue2_check/gen_checks.py): the link test of every ENGINE_API / CORE_API name
// against the import libraries of third_party/ue_import/*.def, and the slot and signature test of every declared
// virtual against the C4 DLLs.
//
// The extra asserts below are the facts of the C4 binaries the headers rest on, written once more here from the
// DLL analysis (tools/ue2_vtable_diff.py, tools/ue2_layouts.py), so that a header edit that drifts away from them
// fails here even if its own NW_ASSERT was edited along.
#include "ue2/UE2Core.h"
#include "ue2/UE2Engine.h"

// UObject is 0x34 bytes in the 27/05/2006 Core.dll (three dwords at +0x28: UObject::UObject(ENativeConstructor...) writes -1 at +0x28
// and 0 at +0x2C, ~UObject indexes a second object table with +0x30; C1-C3: 0x28); its vtable keeps its 25 slots (ClearL2Game at 22).
// Class sizes from the IMPLEMENT_CLASS initializers of the C4 Engine.dll (tools/ue2_engine_classes.py) and the script layouts
// (tools/ue2_layouts.py: the variables of the C4 Engine.u end exactly at these sizes, no native-only tail)
static_assert(sizeof(UObject) == 0x34, "UObject is 0x34 bytes in C4 (C1-C3 0x28)");
static_assert(sizeof(AActor) == 0x3A8, "AActor is 0x3A8 bytes in C4 (C3 0x398)");
static_assert(sizeof(APawn) == 0x1770, "APawn is 0x1770 bytes in C4 (C3 0x16CC)");
static_assert(sizeof(AController) == 0x548, "AController is 0x548 bytes in C4 (C3 0x538)");
static_assert(sizeof(UCanvas) == 0x1D0, "UCanvas is 0x1D0 bytes in C4 (C3 0x1A4)");
static_assert(sizeof(UEngine) == 0x190, "UEngine is 0x190 bytes in C4 (C3 0x184)");
static_assert(sizeof(UGameEngine) == 0x374, "UGameEngine is 0x374 bytes in C4 (C3 0x36C)");
static_assert(sizeof(UL2Font) == 0x38, "UL2Font is 0x38 bytes in C4 (C3 0x2C)");
static_assert(sizeof(FParser) == 0x418, "FParser::operator= copies 0x106 dwords in every build");
static_assert(sizeof(FMemMark) == 0xC, "the FMemMark of the C4 NWindow is Top, Frame, SavedChunk");

// the by-value sizes that changed in C4 (tools/ue2_ret_diff.py --c1 <C3 orig>: the `ret N` of the by-value exports; the code
// that copies them): User 0x248 (C3 0x230: `imul 0x248` in SelectedCharacterNum, ret 0x248 of FL2NetNotify::AddCharacterInfo),
// UL2ConsoleWnd 0x3BA8 (its IMPLEMENT_CLASS size; 26 Users of 0x248 after a base 0xC bigger), FL2GameData 0xADC24 (the copy constructor's
// last block, tools/ue2_gamedata_regions.py); PetInfo 0xB4 (GetPetInfo returns it by value through a hidden pointer, so only its `mov ecx, 0x2D` shows
// it: C3 0x2B dwords); ItemInfo stays 0x30 (no `ret N` of a by-value export changed)
static_assert(sizeof(User) == 0x248, "User is 0x248 bytes in C4 (C3 0x230)");
static_assert(sizeof(UL2ConsoleWnd) == 0x3BA8, "UL2ConsoleWnd is 0x3BA8 bytes in C4 (C3 0x392C)");
static_assert(sizeof(FL2GameData) == 0xADC24, "FL2GameData is 0xADC24 bytes in C4 (C3 0x7AF90)");
static_assert(sizeof(FL2SystemMsgData) == 0x30, "the SystemMsg table steps by 0x30 in C3 and C4");
static_assert(sizeof(FServerStaticObject) == 0x18, "FServerStaticObject is 0x18 bytes in C4 (its constructor clears six dwords, operator= copies six; C3 0x10)");
static_assert(sizeof(PetInfo) == 0xB4, "PetInfo is 0xB4 bytes in C4 (UNetworkHandler::GetPetInfo copies 0x2D dwords; C3 0x2B)");
