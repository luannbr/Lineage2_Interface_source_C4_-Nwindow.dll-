#!/usr/bin/env python3
"""src/standalone/NwGlobals.cpp: the stock globals, defined for the standalone build.

In hook mode our code reaches the globals of the stock image by address; each one is declared
in two branches (docs/notas/C4_Standalone.md, section 2b; the pattern is the C2 project's):

    #if NW_HOOK_MODE
    #define GFocusEntry NW_GLOBAL(NWndEntry*, 0x101a72b4)                        state
    #define GBoardMarks (reinterpret_cast<const TCHAR* const*>(::nw::Addr(0x10160358)))  table
    #else
    extern NWndEntry* GFocusEntry;
    extern const TCHAR* const GBoardMarks[];
    #endif

The standalone build (NW_HOOK_MODE=0) links against the definitions this script writes. The
source gives the address (the hook-mode line) and the type (the extern); the stock image
(orig/NWindow.dll, the C4 build) gives the initial value: .data values, .bss zeros, and the
tables rebuilt from .data/.rdata by following their string pointers. The relocations of the
image tell the pointers from the integers, and every value is checked against them.

SPEC below says how to read each global (and how many entries a table has). The script fails
when a hook-mode global of the source has no SPEC entry or no extern, and when a SPEC entry is
no longer in the source. The stock image never changes, so the output is committed; run it
again after adding or retyping a global. Globals the stock code overflows into their
neighbour are kept together in the stock layout (CHAINS). The extent of every table is also
checked against the image (a run of relocated pointers, the start of the next object).

    python tools/gen_globals.py                     write src/standalone/NwGlobals.cpp, then list what the
                                                    standalone view still reaches by address; exit status 1
                                                    when a NW_GLOBAL or an Addr of a data table is left
                                                    without a definition (the vtables written by hand, the
                                                    UClass objects and the functions are their owners')
    python tools/gen_globals.py --check DLL [MAP]   read every global back from a standalone
                                                    build (the probe links too) through its map,
                                                    compare it with the stock object, and
                                                    compare the offsets inside each chain
"""
from __future__ import annotations

import hashlib
import re
import struct
import sys
import textwrap
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from nwd.image import ORIG, ROOT, STOCK_MD5, Image  # noqa: E402

OUT = ROOT / "src" / "standalone" / "NwGlobals.cpp"

# hook-mode only: the stock std::length_error that the stock code catches (NStdLib.h); the
# standalone build throws its own
HOOK_ONLY = {"GStdLogicErrorVtbl", "GStdLengthErrorVtbl", "GStdLengthErrorThrowInfo"}

# defined by hand in src/standalone/NwGameGuard.cpp, not here: the nProtect GameGuard globals
# (the GameGuard library is not part of the standalone DLL, docs/notas/C4_Standalone.md section 0
# and include/nwindow/NGameGuard.h); they keep their extern in the source. GGameGuardRunningC2
# is a second name of GGameGuardRunning (NConsoleWndC2.cpp #defines it to the first)
OTHER_OWNER = {"GGameGuardConsole", "GGameGuardTimer", "GGameGuardRunning", "GGameGuardRunningC2", "GGameGuardOK"}

# private to their .cpp in the standalone build (a static there, no extern): nothing to define here
STANDALONE_LOCAL = {"GDispatchedMsgs", "GNumDispatchedMsgs", "GGameGuardQueryExtra"}

# How to read each global from the image. Kinds:
#   int              4-byte integer                    bool       1-byte bool
#   null             pointer, NULL in the image        zero N     N zero bytes, no initializer
#                                                                 (zeroed or constructed)
#   package          defined by NW_IMPLEMENT_PACKAGE (NWindow.h), only checked here
#   wstr             pointer to a string
#   tchars           the string stored in place (TCHAR array; slot = its size in TCHARs)
#   wrows N W        N strings stored in place, each in a TCHAR[W] row
#   wstrs N          N string pointers                 wstr_pairs N   N pairs of them
#   ints N           N integers                        floats N   N 4-byte floats
#   fstrings N       N FStrings made by a static initializer (init = its address): the stock code
#                    pushes each string and the address of its FString (push str; mov ecx, dst; call)
#   records N (f..)  N structs of integer fields f..   debugcats N    NDebugCategory[N]
#   htmltags N       NHtmlTagInfo[N] and the attribute name arrays they point to
# A table read past its end by the stock code keeps, as its last entries, what follows it in
# the image ("past": how many, "past_what": what they are; the note says why). The globals of a
# chain (CHAINS) are members of one object instead.
# C4 numbers (the C2 ones in brackets in the notes when they differed): the extent of each
# table was checked against the image and against the code that indexes it.
SPEC: dict[str, dict] = {
    # NWindow.h
    "GConsole": dict(kind="null"),
    "GCanvas": dict(kind="null"),
    "GTextAlignParser": dict(kind="null"),
    "GNWindowDebug": dict(kind="int"),
    "GViewportHWnd": dict(kind="null"),
    "GNWindowInstance": dict(kind="package"),
    # NCWnd.h, NCConsole.cpp, NCScrollWnd.h, NCTabControl.h
    "GFocusEntry": dict(kind="null"),
    "GShiftDown": dict(kind="int"),
    "GNextFrameID": dict(kind="int"),
    "GNextWndNumber": dict(kind="int"),
    "GNextScrollCtrlID": dict(kind="int"),
    "GNextTabGroupID": dict(kind="int"),
    # NCGDevice.h
    "GWrapText": dict(kind="zero", size=0x1000),
    "GWrapLine": dict(kind="zero", size=0x1000),
    "GWrapLen": dict(kind="int"),
    "GWrapPos": dict(kind="int"),
    # NCIME.h, NCChineseIME.h, NCTaiwanIME.h
    "GImmLockIMC": dict(kind="null"),
    "GImmUnlockIMC": dict(kind="null"),
    "GImmLockIMCC": dict(kind="null"),
    "GImmUnlockIMCC": dict(kind="null"),
    "GIMEOSChecked": dict(kind="int"),
    "GIMEUnicodeOS": dict(kind="int"),
    "GChineseGetReadingString": dict(kind="null"),
    "GChineseOSVersion": dict(kind="zero", size=0x94),
    "GChineseOSVersionRead": dict(kind="bool"),
    "GChineseNT": dict(kind="bool"),
    "GChineseNTChecked": dict(kind="bool"),
    "GTaiwanGetReadingString": dict(kind="null"),
    # NCCoolTimeIcon.h, NHtmlLayout.h: TArrays (the stock initializers construct them and
    # register their destructors, as the compiler does for ours)
    "GCoolTimeTextures": dict(kind="zero", size=0xC),
    "GCoolTimeEndTextures": dict(kind="zero", size=0xC),
    "GHtmlAlignStack": dict(kind="zero", size=0xC),
    "GHtmlVAlignStack": dict(kind="zero", size=0xC),
    "GHtmlLinkTarget": dict(kind="zero", size=0x200),
    "GHtmlLinkMsg": dict(kind="zero", size=0x200),
    # the messenger's "blocked" flag (MessengerWnd.cpp, MsnWnd.cpp, StatusWnd.cpp, ChatWnd.cpp), the chat toggle and the shortcut
    # chat key flags (both 1 in .data and written by nothing), the GM server change's last step (NConsoleWndC4.cpp)
    "GMsnBlocked": dict(kind="int"),
    "GChatToggleEnabled": dict(kind="int", note="C4: 1 in .data, nothing writes it; NCChatWnd::OnToggleChat tests it"),
    "GShortCutChatKey": dict(kind="int", note="C4: 1 in .data, nothing writes it"),
    "GGMChangeLastStep": dict(kind="int"),
    # NCMinimapWnd.cpp: 17 maps and, in each table, the stock's trailing 0 (18 slots of 4 bytes); the 16 town buttons' places; the
    # 16 town map names, FStrings made by a static initializer (0x10194640) and edited by LoadMapData
    "GMapWidths": dict(kind="ints", count=18, note="C4: 17 maps and a trailing 0"),
    "GMapHeights": dict(kind="ints", count=18, note="C4: 17 maps and a trailing 0"),
    "GMapOriginX": dict(kind="ints", count=18, note="C4: 17 maps and a trailing 0"),
    "GMapOriginY": dict(kind="ints", count=18, note="C4: 17 maps and a trailing 0"),
    "GMapScaleX": dict(kind="floats", count=18, note="C4: 17 maps and a trailing 0"),
    "GMapScaleY": dict(kind="floats", count=18, note="C4: 17 maps and a trailing 0"),
    "GTownPosX": dict(kind="ints", count=16, note="C4: the 16 town buttons of the world map"),
    "GTownPosY": dict(kind="ints", count=16, note="C4: the 16 town buttons of the world map"),
    "GTownMapNames": dict(kind="fstrings", count=16, init=0x10194640,
                          note="C4: FString[16] built from L\"map_talkin_island_village\" .. by the static initializer 0x10194640"),
    # NCTrainWnd.cpp: the trainer's copy of the experience table (levels 0..88)
    "GTrainExpTable": dict(kind="ints", count=89, note="C4: exp at the start of each level, 89 entries"),
    # NToolTip.cpp: FormatNumber's buffer
    "GNumberBuffer": dict(kind="zero", size=0x40, note="C2 only: TCHAR[32], only FormatNumber touches it"),
    # NCAutoLogWnd.h, NCDebugWnd.h, NConsoleWnd*.cpp, NL2Font.h
    "GAutoLogID": dict(kind="zero", size=0x80),
    "GAutoLogPassword": dict(kind="zero", size=0x80),
    "GAutoLogSlot": dict(kind="int"),
    "GNumDebugCategories": dict(kind="int"),
    "GCursorLocked": dict(kind="int"),
    "GL2DebugLog": dict(kind="int"),
    "GIsL2AutoLogOn": dict(kind="int"),
    "GFontBytesRead": dict(kind="int"),
    "GFontBlendA": dict(kind="null", note="C2 only: the font's blend material, GetMaterial(1)"),
    "GFontBlendB": dict(kind="null", note="C2 only: the font's blend material, GetMaterial(other)"),
    # the nProtect GameGuard library's success code (NConsoleWndMisc.cpp StartGameGuard)
    # one "/" pointer per unit that builds a macro line (the stock keeps a copy of the pointer in
    # each unit's .data: all four point at the string at 0x10107e78)
    "GCommandSlash": dict(kind="wstr", note="NCMacroArrayWnd's pointer to L\"/\""),
    "GShortCutCmdPrefix": dict(kind="wstr", note="NCShortCutWnd's pointer to L\"/\""),
    "GActionItemCmdPrefix": dict(kind="wstr", note="NCActionItemWnd's pointer to L\"/\""),
    "GSkillItemCmdPrefix": dict(kind="wstr", note="NCSkillItemWnd's pointer to L\"/\""),
    # tables
    "GKeypadTextures": dict(kind="wstr_pairs", count=13,
                            note="the keypad's buttons 1..9, 0, backspace, all, C (up, down); C1 had 11 pairs"),
    "GCalcKeyTextures": dict(kind="wstr_pairs", count=19, note="C2 only: the calculator's 19 keys (up, down)"),
    "GChatPrefix": dict(kind="wstr_pairs", count=18,
                        note="C4: Say2 types 0..17 (ASCII, full width; 10..16 have none, 17 is the hero voice '%'); C2 had 10"),
    "GAutoLogMessages": dict(kind="wstrs", count=4),
    "GGradeNames": dict(kind="wstrs", count=6, note="crystal grades none, D, C, B, A, S"),
    "GServerNames": dict(kind="wstrs", count=42, note="C4: FormatServerLabel tests Index <= 40: [0] is the end of the table before "
                                                      "(a pointer to the ASCII \"v1.1\"), [1..40] the names from \"L2Server\", then a NULL"),
    "GRaceEventNames": dict(kind="wstrs", count=5),
    "GClassTypeEventNames": dict(kind="wstrs", count=2),
    "GSexEventNames": dict(kind="wstrs", count=2),
    "GCommandSeparators": dict(kind="tchars", slot=8, note="TCHAR[8]: the 16 bytes up to the next object"),
    "GCommandPrefix": dict(kind="wstrs", count=2, note="C2 only: L\"/\" and full width; the stock table goes on "
                                                       "with the pairs of \"//\" and \"///\""),
    "GCommandTokenNames": dict(kind="wstrs", count=11, note="C2 only: %target, %self, %pet, %party1..8"),
    "GCreditSoundKeys": dict(kind="wrows", count=5, width=255, note="C2 only: \"CreditSound1\"..\"CreditSound5\""),
    "GBoardMarks": dict(kind="wstrs", count=20, note="C4: 18 post fields, <!--COMMENT-->, <!--THREAD--> (C2: 17 fields)"),
    "GHtmlAlignNames": dict(kind="wstrs", count=5),
    "GHtmlEntityNames": dict(kind="wstrs", count=8),
    "GHtmlEntityValues": dict(kind="wstrs", count=8, past=1, past_what="GHtmlSysStringMark",
                              note="7 in the stock table, ReplaceEntities reads 8"),
    "GHtmlSysStringMark": dict(kind="wstr"),
    "GHtmlCastleNameMark": dict(kind="wstr"),
    "GHtmlCastleLocMark": dict(kind="wstr"),
    "GHtmlCastleDescMark": dict(kind="wstr"),
    "GHtmlNpcNameMark": dict(kind="wstr", note="C4"),
    "GHtmlItemNameMark": dict(kind="wstr", note="C4"),
    "GHtmlTagInfo": dict(kind="htmltags", count=51, note="51 tags in C2 (C1: 50)"),
    "GHtmlTagIds": dict(kind="ints", count=51, note="C2: 51 (C1: 50)"),
    "GDebugCategories": dict(kind="debugcats", count=20),
    "GMSViewCharacters": dict(kind="records", count=19, past=1, fields=("Race", "Sex", "ClassID"),
                              past_what="the next table's pointers, as numbers",
                              note="18 in the stock table, AddAllCharacter reads 19"),
    "GDefaultCharacters": dict(kind="records", count=19, past=1, fields=("Race", "Sex", "ClassID"),
                               past_what="the next table's pointers, as numbers",
                               note="18 in the stock table, NCPawnViewrWnd::AddAllCharacter reads 19"),
    "GExpTable": dict(kind="ints", count=90, past=1, past_what="the low word of the next object, a double (100.0)",
                      note="C4: levels 0..88 (C2: 0..81); level 88 reads the next object"),
    "GGMExpTable": dict(kind="ints", count=89, past=1, past_what="the first two characters of the next object, L'NC'",
                        note="C4: levels 1..88 at 0..87 (C2: 1..81); level 88 reads the next object"),
    "GLobbyExpTable": dict(kind="ints", count=90, past=1, past_what="the first two characters of the next object, L'NC'",
                           note="C4: levels 0..88 (C2: 0..81 and a 0); level 88 reads the next object"),
}

# Neighbours the stock code overflows into: an unbounded wcscpy runs from one global into the
# next (NCGDevice::FirstWrappedLine; NHtmlLayout's <a> and the HTML viewers' handlers), and what
# happens then depends on which global comes next. The standalone build keeps each chain as one
# object with the stock layout (a struct here, its offsets asserted), and the names are
# references to its members (extern T& GName in the source); --check reads the references back
# and compares the offsets with the stock ones.
CHAINS: dict[str, tuple[str, ...]] = {
    "Wrap": ("GWrapText", "GWrapLine", "GWrapLen", "GWrapPos"),
    "HtmlLink": ("GHtmlLinkTarget", "GHtmlLinkMsg", "GHtmlVAlignStack"),
}
CHAIN_OF = {m: c for c, ms in CHAINS.items() for m in ms}

DEFINE_RE = re.compile(r"^#define\s+(G\w+)\s+(.*(?:NW_GLOBAL|nw::Addr)\(.*)$", re.M)
EXTERN_RE = re.compile(r"^extern\s+([^;]+?)\s*;", re.M)
VA_RE = re.compile(r"0x[0-9A-Fa-f]{8}")


class Global:
    def __init__(self, name: str):
        self.name = name
        self.va: int | None = None
        self.decl: str | None = None       # the standalone extern, without "extern" and ";"
        self.files: list[Path] = []


def scan() -> dict[str, Global]:
    found: dict[str, Global] = {}
    paths = sorted((ROOT / "include" / "nwindow").glob("*.h")) + sorted((ROOT / "src" / "nwindow").glob("*.cpp"))
    for path in paths:
        text = path.read_text(encoding="utf-8")
        for m in DEFINE_RE.finditer(text):
            name = m.group(1)
            if name in HOOK_ONLY or name in STANDALONE_LOCAL:
                continue
            vas = VA_RE.findall(m.group(2).split("//")[0])      # not the addresses a comment mentions
            if len(vas) != 1:
                sys.exit(f"{path.name}: {name}: expected one address in the hook-mode line")
            g = found.setdefault(name, Global(name))
            va = int(vas[0], 16)
            if g.va is not None and g.va != va:
                sys.exit(f"{name}: two addresses ({g.va:#x} and {va:#x})")
            g.va = va
            g.files.append(path)
        for m in EXTERN_RE.finditer(text):
            decl = " ".join(m.group(1).split())
            names = [n for n in re.findall(r"\bG\w+", decl)]
            if len(names) != 1 or re.search(names[0] + r"\s*\(", decl):
                continue                # not one object (a function, a list)
            g = found.setdefault(names[0], Global(names[0]))
            if g.decl is not None and g.decl != decl:
                sys.exit(f"{names[0]}: two standalone declarations ({g.decl!r} and {decl!r})")
            g.decl = decl
    problems: list[str] = []
    for name, g in sorted(found.items()):
        if g.va is None:
            del found[name]            # an extern of something else (not a stock global)
            continue
        if name in OTHER_OWNER:
            del found[name]            # defined by its owner (see OTHER_OWNER)
            continue
        if g.decl is None:
            problems.append(f"{name} ({g.va:#x}): no standalone extern next to its hook-mode line")
            continue
        if name not in SPEC:
            problems.append(f"{name} ({g.va:#x}): not in tools/gen_globals.py's SPEC")
            continue
        if (name in CHAIN_OF) != is_reference(g.decl, name):
            problems.append(f"{name}: a chain member is declared as a reference (extern T& {name}), any "
                            f"other global is not; got {g.decl!r}")
    for name in SPEC:
        if name not in found:
            problems.append(f"SPEC has {name}, which the source no longer declares")
    if problems:
        sys.exit("\n".join(problems))
    return found


def is_reference(decl: str, name: str) -> bool:
    return bool(re.search(r"&\s*" + name + r"\b", decl))


def object_decl(decl: str, name: str) -> str:
    """The object a reference declares: 'T& N' -> 'T N', 'T (&N)[256]' -> 'T N[256]'."""
    d = re.sub(r"\(\s*&\s*" + name + r"\s*\)", name, decl)
    return re.sub(r"&\s*" + name + r"\b", " " + name, d).replace("  ", " ")


def member_size(spec: dict) -> int:
    return {"int": 4, "null": 4, "bool": 1}.get(spec["kind"]) or spec["size"]


def bsearch_misses(names: list[str]) -> list[str]:
    """The names a binary search with wcscmp (NHtmlTagTable::FindTag / FindAttr) does not find."""
    missed = []
    for want in names:
        lo, hi, found = 0, len(names), False
        while hi > lo:
            mid = lo + (hi - lo) // 2
            if names[mid] == want:
                found = True
                break
            if names[mid] < want:
                lo = mid + 1
            else:
                hi = mid
        if not found:
            missed.append(want)
    return missed


# ---- C++ output helpers --------------------------------------------------------------------
def wlit(s: str) -> str:
    """Wide string literal; hex escapes end their literal so a following hex digit stays out."""
    out, hexed = [], False
    for ch in s:
        o = ord(ch)
        if 0x20 <= o < 0x7F:
            if hexed and ch in "0123456789abcdefABCDEF":
                out.append('" L"')
            out.append({"\\": "\\\\", '"': '\\"'}.get(ch, ch))
            hexed = False
        elif ch in "\t\n\r":
            out.append({"\t": "\\t", "\n": "\\n", "\r": "\\r"}[ch])
            hexed = False
        else:
            out.append(f"\\x{o:04x}")
            hexed = True
    return 'L"' + "".join(out) + '"'


def cint(v: int) -> str:
    return str(v) if -256 < v < 256 else (f"0x{v:X}" if v >= 0 else f"-0x{-v:X}")


def cfloat(v: float) -> str:
    """The shortest decimal literal that reads back as the same 4-byte float."""
    for digits in range(1, 10):
        s = f"{v:.{digits}g}"
        if struct.unpack("<f", struct.pack("<f", float(s)))[0] == v:
            break
    return (s if ("." in s or "e" in s) else s + ".0") + "f"


def fstring_inits(img: Image, init: int, base: int, count: int, name: str) -> list[str]:
    """The strings a static initializer gives the FStrings of an array of `count` at `base`
    (each 12 bytes), in the order of the array: the stock code is  push <string>; mov ecx,
    <FString>; call [FString::FString]  once per element (a file-scope array of FString)."""
    code = img.read(init, 0x400)
    found: dict[int, int] = {}
    for off in range(len(code) - 14):
        if code[off] == 0x68 and code[off + 5] == 0xB9 and code[off + 10] == 0xFF and code[off + 11] == 0x15:
            src, dst = struct.unpack_from("<II", code, off + 1)[0], struct.unpack_from("<I", code, off + 6)[0]
            found[dst] = src
    out = []
    for i in range(count):
        src = found.get(base + 12 * i)
        if src is None:
            sys.exit(f"{name}[{i}]: the initializer {init:#x} does not build the FString at {base + 12 * i:#x}")
        if img.section(src) not in (".rdata", ".data"):
            sys.exit(f"{name}[{i}]: {src:#x} is not a string of the image")
        s = img.wstr(src, 256)
        if s is None:
            sys.exit(f"{name}[{i}]: no terminator at {src:#x}")
        out.append(s)
    if len(found) != count:
        sys.exit(f"{name}: the initializer builds {len(found)} FStrings, the SPEC says {count}")
    return out


class Reader:
    def __init__(self, img: Image):
        self.img = img
        self.relocs = img.reloc_set
        lo, _ = img.sect_range(".data")
        s = next(s for s in img.pe.sections if s.Name.rstrip(b"\0") == b".data")
        self.data_raw_end = lo + s.SizeOfRawData

    def where(self, va: int) -> str:
        sect = self.img.section(va)
        if sect == ".data" and va >= self.data_raw_end:
            return ".bss"
        return sect

    def plain(self, va: int, n: int, what: str) -> bytes:
        """n bytes that must not hold a pointer."""
        for a in range(va - 3, va + n):
            if a in self.relocs:
                sys.exit(f"{what}: {a:#x} is a relocated pointer, expected plain data")
        return self.img.read(va, n)

    def i32(self, va: int, what: str) -> int:
        return struct.unpack("<i", self.plain(va, 4, what))[0]

    def ptr(self, va: int, what: str) -> int:
        p = self.img.u32(va)
        if p and va not in self.relocs:
            sys.exit(f"{what}: {va:#x} holds {p:#x} without a relocation, expected a pointer")
        return p

    def wstr_at(self, va: int, what: str) -> str:
        if self.img.section(va) not in (".rdata", ".data"):
            sys.exit(f"{what}: {va:#x} is not a string of the image")
        s = self.img.wstr(va, 1024)
        if s is None:
            sys.exit(f"{what}: no terminator at {va:#x}")
        return s

    def wstr_ptr(self, va: int, what: str) -> str:
        p = self.ptr(va, what)
        return "NULL" if not p else wlit(self.wstr_at(p, what))


def definition(g: Global, spec: dict, rd: Reader) -> list[str]:
    """The lines defining g (comment + definition)."""
    kind, va, name = spec["kind"], g.va, g.name
    decl = g.decl
    decl_type = decl.split(name)[0].strip()
    # a const object has internal linkage unless declared extern: say it
    const_obj = decl_type.endswith("const") or ("*" not in decl_type and decl_type.startswith("const "))
    lead = "extern " if const_obj else ""
    src = ", ".join(sorted({p.relative_to(ROOT).as_posix() for p in g.files}))
    head = [f"// {va:#010x} {rd.where(va)}; {src}" + (f"; {spec['note']}" if "note" in spec else "")]

    def sized(n: int) -> str:
        """decl with the table's size in its first empty bound."""
        if "[]" not in decl:
            m = re.search(r"\[(\d+)\]", decl)
            if not m or int(m.group(1)) != n:
                sys.exit(f"{name}: a table needs an unsized array extern or one of {n} entries, got {decl!r}")
            return decl
        return decl.replace("[]", f"[{n}]", 1)

    if kind == "int":
        return head + [f"{lead}{decl} = {cint(rd.i32(va, name))};"]
    if kind == "bool":
        b = rd.plain(va, 1, name)[0]
        if b not in (0, 1):
            sys.exit(f"{name}: bool {b}")
        return head + [f"{lead}{decl} = {'true' if b else 'false'};"]
    if kind == "null":
        if rd.ptr(va, name):
            sys.exit(f"{name}: not NULL in the image")
        return head + [f"{lead}{decl} = NULL;"]
    if kind == "zero":
        if any(rd.plain(va, spec["size"], name)):
            sys.exit(f"{name}: not zero in the image")
        return head + [f"{decl};"]
    if kind == "package":
        if any(rd.plain(va, 4, name)):
            sys.exit(f"{name}: not zero in the image")
        return head + [f"// {name}: defined by NW_IMPLEMENT_PACKAGE (NWindow.h), set by DllMain"]
    if kind == "wstr":
        return head + [f"{lead}{decl} = {rd.wstr_ptr(va, name)};"]
    if kind == "tchars":
        s = rd.wstr_at(va, name)
        slot = spec.get("slot")
        if slot is None:
            return head + [f"{lead}{decl} = {wlit(s)};"]
        if len(s) >= slot or any(rd.plain(va + 2 * (len(s) + 1), 2 * (slot - len(s) - 1), name)):
            sys.exit(f"{name}: the string does not fill a clean TCHAR[{slot}] slot")
        return head + [f"{lead}{sized(slot)} = {wlit(s)};"]
    if kind == "wrows":
        width, rows = spec["width"], []
        for i in range(spec["count"]):
            a = va + 2 * width * i
            s = rd.wstr_at(a, f"{name}[{i}]")
            if len(s) >= width or any(rd.plain(a + 2 * (len(s) + 1), 2 * (width - len(s) - 1), name)):
                sys.exit(f"{name}[{i}]: the string does not fill a clean TCHAR[{width}] row")
            rows.append(wlit(s))
        return head + [f"{lead}{decl} =", "{"] + [f"    {r}," for r in rows] + ["};"]

    count, past = spec["count"], spec.get("past", 0)
    rows: list[str] = []
    if kind == "wstrs":
        rows = [rd.wstr_ptr(va + 4 * i, f"{name}[{i}]") for i in range(count)]
    elif kind == "wstr_pairs":
        rows = [f"{{ {rd.wstr_ptr(va + 8 * i, name)}, {rd.wstr_ptr(va + 8 * i + 4, name)} }}"
                for i in range(count)]
    elif kind == "ints":
        vals = [rd.i32(va + 4 * i, f"{name}[{i}]") for i in range(count)]
        if spec.get("sentinel") and (vals[-1] != 0 or 0 in vals[2:-1]):
            sys.exit(f"{name}: the table does not end at its first 0 after level 1")
        per, real = 8, count - past
        body = [f"    {', '.join(str(v) for v in vals[i:min(i + per, real)])}," for i in range(0, real, per)]
        body += [f"    {v},   // past the stock table: {spec['past_what']}" for v in vals[real:]]
        return head + [f"{lead}{sized(count)} =", "{"] + body + ["};"]
    elif kind == "floats":
        vals = [struct.unpack("<f", rd.plain(va + 4 * i, 4, f"{name}[{i}]"))[0] for i in range(count)]
        body = [f"    {', '.join(cfloat(v) for v in vals[i:i + 6])}," for i in range(0, count, 6)]
        return head + [f"{lead}{sized(count)} =", "{"] + body + ["};"]
    elif kind == "fstrings":
        if any(rd.plain(va, 12 * count, name)):
            sys.exit(f"{name}: not zero in the image (it is built at start-up)")
        rows = [f"FString({wlit(s)})" for s in fstring_inits(rd.img, spec["init"], va, count, name)]
    elif kind == "records":
        fields = spec["fields"]
        size = 4 * len(fields)
        for i in range(count):
            a = va + size * i
            if i < count - past:
                vals = [str(rd.i32(a + 4 * k, name)) for k in range(len(fields))]
            else:
                # past the stock table the image goes on with pointers (the next table's), which
                # the stock code reads as the fields of one more record: their stock values
                vals = [f"0x{rd.ptr(a + 4 * k, name):08X}" for k in range(len(fields))]
            rows.append("{ " + ", ".join(vals) + " }")
    elif kind == "debugcats":
        for i in range(count):
            a = va + 0x48 * i
            raw = rd.plain(a, 0x48, f"{name}[{i}]")
            s = rd.img.wstr(a, 32)
            if s is None or any(raw[2 * (len(s) + 1):0x40]):
                sys.exit(f"{name}[{i}]: the name is not a clean TCHAR[32]")
            enabled, color = struct.unpack_from("<iI", raw, 0x40)
            rows.append(f"{{ {wlit(s)}, {enabled}, 0x{color:08X} }}")
    elif kind == "htmltags":
        pre: list[str] = []
        tags: list[str] = []
        unsorted: list[str] = []
        for i in range(count):
            a = va + 12 * i
            tag = rd.wstr_at(rd.ptr(a, f"{name}[{i}].Name"), name)
            tags.append(tag)
            attrs = rd.ptr(a + 4, f"{name}[{i}].Attrs")
            n = rd.i32(a + 8, f"{name}[{i}].NumAttrs")
            if attrs:
                arr = f"GHtmlTagAttrs_{tag}"
                names = [rd.wstr_at(rd.ptr(attrs + 4 * k, arr), arr) for k in range(n)]
                missed = bsearch_misses(names)
                if missed:
                    unsorted.append(f"{tag} (never finds {', '.join(missed)})")
                pre.append(f"static const TCHAR* const {arr}[{n}] = {{ {', '.join(wlit(x) for x in names)} }};")
                rows.append(f"{{ {wlit(tag)}, {arr}, {n} }}")
            else:
                rows.append(f"{{ {wlit(tag)}, NULL, {n} }}")
        if bsearch_misses(tags):
            sys.exit(f"{name}: FindTag would miss {bsearch_misses(tags)}")
        about = ("The attribute names of each tag, which NHtmlTagTable::FindAttr searches by halves "
                 "with wcscmp. The stock arrays are sorted except " + "; ".join(unsorted) + ": faithful "
                 "data. A tag without attributes has Attrs NULL and NumAttrs 1, and FindAttr reads "
                 "through the NULL when such a tag has an attribute, as the stock code does.")
        head = ["// " + line for line in textwrap.wrap(about, 93)] + pre + [""] + head
    else:
        sys.exit(f"{name}: unknown kind {kind}")
    body = []
    for i, r in enumerate(rows):
        c = f"   // past the stock table: {spec['past_what']}" if i >= count - past else ""
        body.append(f"    {r},{c}")
    return head + [f"{lead}{sized(count)} =", "{"] + body + ["};"]


def chain_definition(chain: str, gs: list[Global], rd: Reader) -> list[str]:
    """One object holding the globals of a chain at their stock offsets, and the references the
    source names them by."""
    gs = sorted(gs, key=lambda g: g.va)
    base = gs[0].va
    stype, obj = f"N{chain}Chain", f"G{chain}Chain"
    files = sorted({p.relative_to(ROOT).as_posix() for g in gs for p in g.files})
    body, asserts, off = [], [], 0
    for g in gs:
        spec, delta = SPEC[g.name], g.va - base
        if spec["kind"] not in ("int", "null", "bool", "zero"):
            sys.exit(f"{g.name}: a chain member must be state (int, null, bool, zero)")
        if delta < off:
            sys.exit(f"{g.name}: overlaps the member before it in the stock image")
        if delta > off:
            body.append(f"    BYTE Gap{off:X}[0x{delta - off:X}];   // +0x{off:04X}, stock padding")
        size = member_size(spec)
        if any(rd.plain(g.va, size, g.name)):
            sys.exit(f"{g.name}: not zero in the stock image (the chain has no initializer)")
        body.append(f"    {object_decl(g.decl, g.name)};   // +0x{delta:04X} (stock {g.va:#010x})")
        if delta:
            asserts.append(f"NW_ASSERT_OFFSET({stype}, {g.name}, 0x{delta:X});")
        off = delta + size
    return ([f"// {base:#010x}..{base + off:#010x} {rd.where(base)}; {', '.join(files)}",
             f"// {' -> '.join(g.name for g in gs)}: the stock code overflows each one into the",
             "// next, so they stay one object in the stock order; the names are references to its",
             "// members. Zero in the stock image" + (" (the TArray constructs itself)." if any(
                 "TArray" in g.decl for g in gs) else "."),
             f"struct {stype}", "{"] + body + ["};"] + asserts
            + [f"NW_ASSERT_SIZE({stype}, 0x{off:X});", f"static {stype} {obj};"]
            + [f"{g.decl} = {obj}.{g.name};" for g in gs])


def local_structs(found: dict[str, Global]) -> list[str]:
    """Structs a .cpp declares for its own table: copied here (identical, as the ODR allows)."""
    out: dict[str, str] = {}
    for g in found.values():
        for path in g.files:
            if path.suffix != ".cpp":
                continue
            text = path.read_text(encoding="utf-8").replace("\r\n", "\n")
            for tname in re.findall(r"\b(N\w+)\b", g.decl):
                m = re.search(r"^struct " + tname + r"\n\{.*?^\};\n", text, re.M | re.S)
                if not m:
                    continue
                if tname in out and out[tname] != m.group(0):
                    sys.exit(f"struct {tname}: the copies in the source differ")
                out[tname] = m.group(0)
    lines: list[str] = []
    for tname in sorted(out):
        lines += out[tname].rstrip("\n").split("\n") + [""]
    return lines


def check(dll: Path, mapfile: Path) -> int:
    """Every global of a linked standalone NWindow.dll equals the stock one: the same bytes, or
    for pointers to strings the same strings; and the members of each chain sit at the stock
    distances from one another."""
    found = scan()
    stock, ours = Image(), Image(dll)
    addr: dict[str, int] = {}
    refs: set[str] = set()
    for m in re.finditer(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+\?(G\w+)@@3(\S*)\s+([0-9a-f]{8})",
                         mapfile.read_text(encoding="latin-1"), re.M):
        if m.group(1) not in addr:
            addr[m.group(1)] = int(m.group(3), 16)
            if m.group(2).startswith("AA"):     # "T& G": a reference
                refs.add(m.group(1))
    bad = 0
    # a chain member's symbol is its reference: follow it (it must be bound at link time)
    for name in CHAIN_OF:
        ref = addr.get(name)
        if ref is None or name not in refs:
            continue
        if ref not in ours.reloc_set:
            print(f"DIFF {name}: the reference at {ref:#x} is not bound statically (no relocation)")
            bad += 1
        addr[name] = ours.u32(ref)
    for chain, names in CHAINS.items():
        if any(n not in addr for n in names):
            continue
        base = min(names, key=lambda n: found[n].va)
        got = [addr[n] - addr[base] for n in names]
        want = [found[n].va - found[base].va for n in names]
        where = ", ".join(f"{n} {d:+#x}" for n, d in zip(names, got))
        if got != want:
            print(f"DIFF chain {chain}: {where}; stock " + ", ".join(f"{d:+#x}" for d in want))
            bad += 1
        else:
            print(f"chain {chain} at {addr[base]:#x} as in the stock image: {where}")

    def text(img: Image, p: int) -> str | None:
        return None if p == 0 else img.wstr(p, 1024)

    def reloc(a: int) -> bool:
        """The pointer at a is relocated, so it survives a load at another base (NULL needs none)."""
        return ours.u32(a) == 0 or a in ours.reloc_set

    def same_strs(a: int, va: int, n: int) -> bool:
        return all(text(ours, ours.u32(a + 4 * i)) == text(stock, stock.u32(va + 4 * i)) and reloc(a + 4 * i)
                   for i in range(n))

    for g in sorted(found.values(), key=lambda g: g.va):
        spec, a, va = SPEC[g.name], addr.get(g.name), g.va
        kind = spec["kind"]
        if a is None:
            print(f"MISSING {g.name}")
            bad += 1
            continue
        size = {"int": 4, "null": 4, "package": 4, "bool": 1, "zero": spec.get("size", 0),
                "ints": 4 * spec.get("count", 0), "floats": 4 * spec.get("count", 0), "fstrings": 0, "debugcats": 0x48 * spec.get("count", 0),
                "records": 4 * len(spec.get("fields", ())) * spec.get("count", 0),
                "wrows": 2 * spec.get("width", 0) * spec.get("count", 0)}.get(kind)
        if size is not None:
            ok = ours.read(a, size) == stock.read(va, size)
        elif kind == "wstr":
            ok = same_strs(a, va, 1)
        elif kind == "tchars":
            ok = ours.wstr(a) == stock.wstr(va)
            if "slot" in spec:          # the slot after the string is zero in both
                ok = ok and ours.read(a, 2 * spec["slot"]) == stock.read(va, 2 * spec["slot"])
        elif kind in ("wstrs", "wstr_pairs"):
            ok = same_strs(a, va, spec["count"] * (2 if kind == "wstr_pairs" else 1))
        else:  # htmltags
            ok = True
            for i in range(spec["count"]):
                on, oa, oc = struct.unpack("<IIi", ours.read(a + 12 * i, 12))
                sn, sa, sc = struct.unpack("<IIi", stock.read(va + 12 * i, 12))
                ok &= text(ours, on) == text(stock, sn) and oc == sc and (oa == 0) == (sa == 0)
                ok &= reloc(a + 12 * i) and reloc(a + 12 * i + 4)
                ok &= not oa or same_strs(oa, sa, sc)
        if not ok:
            print(f"DIFF {g.name} (stock {va:#x}, ours {a:#x})")
            bad += 1
    print(f"{len(found)} globals checked against the stock image, {bad} different or missing")
    return 1 if bad else 0


def real_size(spec: dict, name: str) -> int:
    """Bytes of the stock object itself (what the table has past its end is not part of it)."""
    kind, past = spec["kind"], spec.get("past", 0)
    if kind in ("int", "null", "package", "wstr"):
        return 4
    if kind == "bool":
        return 1
    if kind == "zero":
        return spec["size"]
    if kind == "tchars":
        return 2 * spec["slot"] if "slot" in spec else 2
    if kind == "wrows":
        return 2 * spec["width"] * spec["count"]
    if kind in ("wstrs", "ints", "floats"):
        return 4 * (spec["count"] - past)
    if kind == "fstrings":
        return 12 * spec["count"]
    if kind == "wstr_pairs":
        return 8 * spec["count"]
    if kind == "records":
        return 4 * len(spec["fields"]) * (spec["count"] - past)
    if kind == "debugcats":
        return 0x48 * spec["count"]
    if kind == "htmltags":
        return 12 * spec["count"]
    sys.exit(f"{name}: unknown kind {kind}")


def verify_extents(found: dict[str, Global]) -> None:
    """No two stock objects of the SPEC overlap and each one sits inside one section of the
    image: a wrong count shows up here before it shows up as a wrong value."""
    img = Image()
    objs = sorted(((g.va, real_size(SPEC[g.name], g.name), g.name) for g in found.values()))
    for (a, n, name), (b, _, other) in zip(objs, objs[1:]):
        if a + n > b:
            sys.exit(f"{name} ({a:#x}, {n:#x} bytes) overlaps {other} ({b:#x})")
    for a, n, name in objs:
        if img.section(a) is None or img.section(a) != img.section(a + max(n, 1) - 1):
            sys.exit(f"{name} ({a:#x}, {n:#x} bytes) leaves its section")


def report_reaches() -> int:
    """What the standalone build still reaches by address (NW_GLOBAL / nw::Addr outside the
    #if NW_HOOK_MODE blocks), classified by what the stock image has there. A NW_GLOBAL, or an
    Addr of a data table, is a link error until it has a definition: this script's job, so they
    are listed as errors (exit status 1). The other Addr uses belong to the owners of their
    files (the vtables written by hand and the UClass objects; functions are listed by
    tools/port/sa_sites.py)."""
    import standalone_report
    img = Image()
    rd = Reader(img)
    sites = standalone_report.source_sites()
    left = [(k, s) for k, v in sites.items() if k.startswith("NW_GLOBAL") or k == "nw::Addr" for s in v]
    kinds: dict[str, list] = {"global": [], "data": [], "vtable": [], "class object": [], "function": [], "dynamic": []}
    for k, (where, va, name) in left:
        if k.startswith("NW_GLOBAL"):
            kinds["global"].append((where, va, name))
        elif not va.lower().startswith("0x"):
            kinds["dynamic"].append((where, va, name))
        else:
            a = int(va, 16)
            sect = rd.where(a)
            if img.in_text(a):
                kinds["function"].append((where, va, name))
            elif sect == ".rdata" and img.in_text(img.u32(a)):
                kinds["vtable"].append((where, va, name))
            elif sect == ".bss":
                kinds["class object"].append((where, va, name))
            else:
                kinds["data"].append((where, va, name))
    print("what the standalone view still reaches by address: " + ", ".join(f"{len(v)} {k}" for k, v in kinds.items()))
    bad = kinds["global"] + kinds["data"]
    for where, va, name in sorted(bad):
        print(f"  NO DEFINITION  {where}  {va}  {name}")
    return 1 if bad else 0


def main() -> int:
    # SPEC and the globals of include/nwindow are the C4 image's: refuse any other
    md5 = hashlib.md5(ORIG.read_bytes()).hexdigest() if ORIG.exists() else None
    if md5 != STOCK_MD5:
        print(f"gen_globals.py: its tables are written for the C4 image (md5 {STOCK_MD5}); "
              f"orig/NWindow.dll is {md5}")
        return 2
    if len(sys.argv) > 2 and sys.argv[1] == "--check":
        dll = Path(sys.argv[2])
        return check(dll, Path(sys.argv[3]) if len(sys.argv) > 3 else dll.with_suffix(".map"))
    found = scan()
    verify_extents(found)
    rd = Reader(Image())
    headers = sorted({p.relative_to(ROOT / "include").as_posix() for g in found.values() for p in g.files
                      if p.suffix == ".h"} - {"nwindow/NWindow.h"})
    lines = [
        "/*=============================================================================",
        "    NwGlobals.cpp - the stock globals, for the standalone build.",
        "    Generated by tools/gen_globals.py from orig/NWindow.dll - do not edit.",
        "",
        "    Hook mode reaches these objects in the stock image (NW_GLOBAL / nw::Addr);",
        "    the standalone build defines them here with the stock initial values:",
        "    .data values, .bss zeros, the tables rebuilt from .data/.rdata. Ordered",
        "    by stock address; each comment says where the stock object is and which",
        "    file declares it. " + ", ".join(sorted(OTHER_OWNER)) + " are defined by the",
        "    nProtect group (src/standalone/NwGameGuard.cpp), not here.",
        "=============================================================================*/",
        '#include "nwindow/NWindow.h"',
    ] + [f'#include "{h}"' for h in headers] + [""]
    structs = local_structs(found)
    if structs:
        lines += ["// the record types of the tables that a .cpp declares for itself (copies)"] + structs
    n_state = n_table = 0
    for g in sorted(found.values(), key=lambda g: g.va):
        spec = SPEC[g.name]
        chain = CHAIN_OF.get(g.name)
        if chain is None:
            lines += definition(g, spec, rd) + [""]
        elif g.name == min(CHAINS[chain], key=lambda n: found[n].va):
            lines += chain_definition(chain, [found[n] for n in CHAINS[chain]], rd) + [""]
        if spec["kind"] in ("int", "bool", "null", "zero", "package"):
            n_state += 1
        else:
            n_table += 1
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text("\n".join(lines).rstrip("\n") + "\n", encoding="utf-8", newline="\r\n")
    print(f"{len(found)} globals ({n_state} state, {n_table} tables and strings) -> "
          f"{OUT.relative_to(ROOT).as_posix()}")
    return report_reaches()


if __name__ == "__main__":
    raise SystemExit(main())
