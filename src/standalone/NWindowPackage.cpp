/*=============================================================================
    NWindowPackage.cpp - IMPLEMENT_PACKAGE(NWindow) of the standalone NWindow.dll.

    The engine never imports NWindow.dll: Core's UPackage::Bind loads it by name
    (package "NWindow" + ".dll") when Engine.dll's StaticLoadClass asks for
    NWindow.ConsoleWnd or NWindow.L2Font, then finds GPackage and every
    autoclass<Class> with GetProcAddress (docs/notas/Standalone.md, section 1).
    The classes register themselves in Classes.gen.cpp; this file only has the
    package name and the entry point (stock C4: 0x10037E00, `_DllMain@12`, run by the
    C runtime's _DllMainCRTStartup after the static initializers, as in the stock image).
=============================================================================*/
#include "nwindow/NWindow.h"

#if NW_HOOK_MODE
#error "NWindowPackage.cpp is for the standalone build only"
#endif

NW_IMPLEMENT_PACKAGE(NWindow)
