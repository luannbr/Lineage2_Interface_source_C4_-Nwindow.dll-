/*=============================================================================
    NwCrtCompat.cpp - printf of this module writes infinity and NaN like the VC7.0 CRT the
    stock NWindow.dll was built with (docs/CONVENCOES.md, CRT).

    The UCRT keeps its printf options per module (*__local_stdio_printf_options(), filled by
    the startup code). _CRT_INTERNAL_PRINTF_LEGACY_MSVCRT_COMPATIBILITY brings back the old
    strings for the special values: "%.2f" of +infinity is "1.#J", of -infinity and of 0/0
    "-1.#J", "%f" of +infinity "1.#INF00" (the UCRT writes "inf", "-inf", "-nan(ind)"). The
    only other thing the bit changes is that the size prefixes F and N are accepted and
    ignored, as the old CRT did. The rounding of exact halves is the other difference, fixed
    in the link (legacy_stdio_float_rounding.obj, NWD_CRT_COMPAT_OBJS).

    init_seg(lib): this initializer runs before every C++ initializer of ours (they are in
    the user segment) and before DllMain, so no text of ours is formatted without the bit.
=============================================================================*/
#include <stdio.h>

#pragma warning(disable : 4073)  // initializers put in library initialization area
#pragma init_seg(lib)

namespace {

struct NwPrintfCompat
{
    NwPrintfCompat() { *__local_stdio_printf_options() |= _CRT_INTERNAL_PRINTF_LEGACY_MSVCRT_COMPATIBILITY; }
} GNwPrintfCompat;

}  // namespace
