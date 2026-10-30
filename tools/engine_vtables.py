#!/usr/bin/env python3
"""Virtual-method order of the engine classes, read from Core.dll / Engine.dll themselves (and
the client's Network.dll, when orig/ has it: the C1 client had one, in C2 UNetworkHandler is exported
by Engine.dll).

Both DLLs export their vtables (??_7Class@@6B@) and every method of their exported classes, so
each slot of a vtable maps back to a decorated name (Engine is an /INCREMENTAL build: slots point
at `jmp body` thunks, both addresses are tried). Pure virtual slots show as _purecall.
Our headers must declare the virtuals in exactly this order.

  python tools/engine_vtables.py UCanvas UObject FOutputDevice FMalloc
  python tools/engine_vtables.py --all     # every exported vtable -> analysis/engine_vtables.txt
"""
from __future__ import annotations

import struct
import subprocess
import sys
from functools import lru_cache
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parents[1]
UNDNAME = None  # resolved lazily (undname.exe from the MSVC toolset, optional)


def read_exports(pe):
    """[(name, rva)] of the whole named export table. pefile names only the first 8,192 exports
    (the C2 Engine has 9,050: everything from `?execEnableChannelNotify` on came without a name),
    so the table is read by hand (the same fix as tools/gen_import_defs.py)."""
    dd = pe.OPTIONAL_HEADER.DATA_DIRECTORY[0]
    hdr = pe.get_data(dd.VirtualAddress, 40)
    (_, _, _, _, _, _base, n_funcs, n_names, a_funcs, a_names, a_ords) = struct.unpack("<IIHHIIIIIII", hdr)
    funcs = struct.unpack("<%dI" % n_funcs, pe.get_data(a_funcs, 4 * n_funcs))
    names = struct.unpack("<%dI" % n_names, pe.get_data(a_names, 4 * n_names))
    ords = struct.unpack("<%dH" % n_names, pe.get_data(a_ords, 2 * n_names))
    return [(pe.get_string_at_rva(n).decode(), funcs[o]) for n, o in zip(names, ords)]


class Dll:
    def __init__(self, name: str, orig: Path | None = None):
        """`orig`: another folder of binaries than this repo's orig/ (tools/ue2_vtable_diff.py
        reads the C1 repo's to compare the two builds)."""
        self.name = name
        self.pe = pefile.PE(str((orig or ROOT / "orig") / f"{name}.dll"))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.mem = self.pe.get_memory_mapped_image()
        self.exp: dict[str, int] = {}
        self.names: dict[int, list[str]] = {}
        for n, rva in read_exports(self.pe):
            va = self.base + rva
            self.exp[n] = va
            self.names.setdefault(va, []).append(n)
        self.text = [(self.base + s.VirtualAddress, self.base + s.VirtualAddress + s.Misc_VirtualSize)
                     for s in self.pe.sections if s.Characteristics & 0x20000000]
        self.imports = {}
        for e in getattr(self.pe, "DIRECTORY_ENTRY_IMPORT", []):
            for i in e.imports:
                self.imports[i.address] = (i.name or b"").decode()

    def u32(self, va: int) -> int:
        return struct.unpack_from("<I", self.mem, va - self.base)[0]

    def is_code(self, va: int) -> bool:
        return any(a <= va < b for a, b in self.text)

    def label(self, t: int) -> str:
        cand = [t]
        b = self.mem[t - self.base:t - self.base + 6]
        if b[:1] == b"\xe9":  # incremental-link thunk
            cand.append(t + 5 + struct.unpack("<i", b[1:5])[0])
        for c in cand:
            if c in self.names:
                return self.names[c][0]
        for c in cand:
            bb = self.mem[c - self.base:c - self.base + 6]
            if bb[:2] == b"\xff\x25":  # jmp [IAT]
                slot = struct.unpack("<I", bb[2:6])[0]
                if slot in self.imports:
                    return "import:" + self.imports[slot]
        return f"sub_{t:08x}"

    def vtable(self, sym: str, limit: int = 600):
        va = self.exp[sym]
        out = []
        for k in range(limit):
            t = self.u32(va + 4 * k)
            if not self.is_code(t):
                break
            if k and (va + 4 * k) in self.names and any(n.startswith("??_7") for n in self.names[va + 4 * k]):
                break  # next exported vtable
            out.append((k, t, self.label(t)))
        return out


@lru_cache(None)
def dll(name: str, orig: Path | None = None) -> Dll:
    return Dll(name, orig)


def undname(names: list[str]) -> list[str]:
    global UNDNAME
    if UNDNAME is None:
        hits = sorted(Path("C:/Program Files/Microsoft Visual Studio").glob(
            "2022/*/VC/Tools/MSVC/*/bin/Hostx64/x86/undname.exe"))
        UNDNAME = str(hits[-1]) if hits else ""
    if not UNDNAME:
        return names
    out = []
    for n in names:
        if not n.startswith("?"):
            out.append(n)
            continue
        r = subprocess.run([UNDNAME, n], capture_output=True, text=True)
        line = [x for x in r.stdout.splitlines() if x.startswith("is :- ")]
        out.append(line[0][7:-1] if line else n)
    return out


def dll_names() -> tuple[str, ...]:
    """Core and Engine, plus the client's Network.dll when orig/ has it (UNetworkHandler, the
    implementation of Engine's abstract UL2NetHandler, exports every method)."""
    return ("Core", "Engine") + (("Network",) if (ROOT / "orig" / "Network.dll").exists() else ())


def find_vtables(cls: str):
    hits = []
    for d in dll_names():
        for n in dll(d).exp:
            if n.startswith(f"??_7{cls}@@6B"):
                hits.append((d, n))
    return hits


def main(argv: list[str]) -> int:
    if argv and argv[0] == "--all":
        lines = []
        for d in dll_names():
            for n in sorted(x for x in dll(d).exp if x.startswith("??_7")):
                vt = dll(d).vtable(n)
                lines.append(f"== {d} {n} ({len(vt)} slots)")
                for k, t, lab in vt:
                    lines.append(f"  {k:3d} +0x{4 * k:03x} {t:08x} {lab}")
        out = ROOT / "analysis" / "engine_vtables.txt"
        out.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"wrote {out}")
        return 0
    for cls in argv:
        for d, n in find_vtables(cls):
            vt = dll(d).vtable(n)
            pretty = undname([lab for _, _, lab in vt])
            print(f"== {d} {n} ({len(vt)} slots)")
            for (k, t, lab), p in zip(vt, pretty):
                print(f"  {k:3d} +0x{4 * k:03x} {t:08x} {p}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
