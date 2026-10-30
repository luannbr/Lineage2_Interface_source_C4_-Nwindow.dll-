"""Add one import (dll!function) to a PE32 image by appending a section.

The new section holds a copy of the import descriptor table plus the new descriptor, the
names and the thunks; the import data directory is repointed to it, bound imports are dropped.
Used to make the stock NWindow.dll load NWindowDecomp.dll (same technique the NWindow_C1
project uses for LineageWindow.dll).

The descriptors of the image are copied as they are, so any import layout survives: the C2
NWindow.dll has 10 (Engine, Core, npkcrypt.dll by ordinal, WINMM, IMM32, VERSION, WS2_32, KERNEL32,
USER32, ADVAPI32) and the new one goes last, so every other DLL is initialised before ours.
verify_added() re-reads the result with pefile and checks exactly that.
"""
from __future__ import annotations

import struct


def _align(v: int, a: int) -> int:
    return (v + a - 1) & ~(a - 1)


def add_import(data: bytes, dll: str, func: str, section_name: bytes = b".nwdimp") -> bytes:
    d = bytearray(data)
    if d[:2] != b"MZ":
        raise ValueError("not an MZ image")
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        raise ValueError("no PE header")
    coff = pe + 4
    nsec = struct.unpack_from("<H", d, coff + 2)[0]
    opt_size = struct.unpack_from("<H", d, coff + 16)[0]
    opt = coff + 20
    if struct.unpack_from("<H", d, opt)[0] != 0x10B:
        raise ValueError("PE32 only")
    sect_align, file_align = struct.unpack_from("<II", d, opt + 32)
    size_of_headers = struct.unpack_from("<I", d, opt + 60)[0]
    dd = opt + 96
    imp_rva = struct.unpack_from("<I", d, dd + 8)[0]
    sec_tab = opt + opt_size

    secs = []
    for i in range(nsec):
        o = sec_tab + 40 * i
        vsize, va, rsize, roff = struct.unpack_from("<IIII", d, o + 8)
        secs.append((va, vsize, roff, rsize))

    def rva2off(rva: int) -> int:
        for va, vsize, roff, rsize in secs:
            if va <= rva < va + max(vsize, rsize) and rva - va < rsize:
                return roff + rva - va
        raise ValueError(f"rva {rva:#x} not in file")

    descs = []
    o = rva2off(imp_rva)
    while True:
        desc = struct.unpack_from("<5I", d, o)
        if desc == (0, 0, 0, 0, 0):
            break
        n = rva2off(desc[3])
        name = bytes(d[n:d.index(b"\0", n)]).decode("ascii")
        if name.lower() == dll.lower():
            raise ValueError(f"already imports {dll}")
        descs.append(desc)
        o += 20

    if sec_tab + 40 * (nsec + 1) > min(s[2] for s in secs if s[2]):
        raise ValueError("no room for another section header")

    last_va, last_vsize, _, last_rsize = secs[-1]
    new_va = _align(last_va + max(last_vsize, last_rsize), sect_align)
    new_off = _align(len(d), file_align)

    blob = bytearray()
    desc_off = 0
    blob += b"\0" * (20 * (len(descs) + 2))
    dll_off = len(blob)
    blob += dll.encode("ascii") + b"\0"
    if len(blob) & 1:
        blob += b"\0"
    ibn_off = len(blob)
    blob += struct.pack("<H", 0) + func.encode("ascii") + b"\0"
    while len(blob) & 3:
        blob += b"\0"
    int_off = len(blob)
    blob += struct.pack("<II", new_va + ibn_off, 0)
    iat_off = len(blob)
    blob += struct.pack("<II", new_va + ibn_off, 0)
    for i, desc in enumerate(descs):
        struct.pack_into("<5I", blob, desc_off + 20 * i, *desc)
    struct.pack_into("<5I", blob, desc_off + 20 * len(descs),
                     new_va + int_off, 0, 0, new_va + dll_off, new_va + iat_off)
    content = len(blob)
    blob += b"\0" * (_align(content, file_align) - content)

    d += b"\0" * (new_off - len(d))
    d += blob
    so = sec_tab + 40 * nsec
    d[so:so + 8] = section_name.ljust(8, b"\0")[:8]
    struct.pack_into("<IIIIIIHHI", d, so + 8, content, new_va, len(blob), new_off, 0, 0, 0, 0,
                     0xC0000040)  # initialized data, read/write (the loader writes the IAT)
    struct.pack_into("<H", d, coff + 2, nsec + 1)
    struct.pack_into("<I", d, opt + 56, _align(new_va + content, sect_align))  # SizeOfImage
    struct.pack_into("<II", d, dd + 8, new_va + desc_off, 20 * (len(descs) + 1))
    struct.pack_into("<II", d, dd + 11 * 8, 0, 0)  # bound imports would be stale
    assert size_of_headers >= sec_tab + 40 * (nsec + 1)
    return bytes(d)


def verify_added(original: bytes, patched: bytes, dll: str, func: str) -> None:
    """Raise ValueError unless `patched` has every import of `original` unchanged (DLL, IAT slot,
    name or ordinal) plus `dll`!`func` as the last descriptor."""
    import pefile

    def imports(data: bytes):
        pe = pefile.PE(data=data)
        return [(e.dll.decode(), [(i.address, i.name, i.ordinal) for i in e.imports])
                for e in pe.DIRECTORY_ENTRY_IMPORT]

    a, b = imports(original), imports(patched)
    if b[:len(a)] != a:
        raise ValueError("the patched image changed an existing import")
    if len(b) != len(a) + 1 or b[-1][0].lower() != dll.lower() or [x[1] for x in b[-1][1]] != [func.encode("ascii")]:
        raise ValueError(f"the patched image does not end with {dll}!{func}")
