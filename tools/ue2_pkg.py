#!/usr/bin/env python3
"""Reader for the Lineage II (UE2, package version 123) .u/.utx packages of the C1/C2 client (copy of the C1
project's tools/engine/ue_pkg.py; the C2 Engine.u and Core.u are Lineage2Ver111 too).

The "encrypted" packages are a plain UPackage after a 28-byte UTF-16 header ("Lineage2Ver111"):
every byte after the header is XORed with 0xAC (Ver111, the .u script packages) or 0x68 (Ver121, the
.utx textures). This module decrypts, reads the name, import and export tables, and pulls out the
ScriptText of each class (the .uc source the script compiler kept in the package).

usage: ue_pkg.py <Engine.u> [--list] [--uc <outdir>]
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

HEADER = 28
XOR = {"Lineage2Ver111": 0xAC, "Lineage2Ver121": 0x68}


class Reader:
    def __init__(self, data: bytes, pos: int = 0):
        self.d = data
        self.p = pos

    def u8(self):
        v = self.d[self.p]
        self.p += 1
        return v

    def i32(self):
        v = struct.unpack_from("<i", self.d, self.p)[0]
        self.p += 4
        return v

    def u32(self):
        v = struct.unpack_from("<I", self.d, self.p)[0]
        self.p += 4
        return v

    def compact(self):
        """UE2 compact index: bit 7 of the first byte is the sign, bit 6 the continuation."""
        b = self.u8()
        neg = b & 0x80
        v = b & 0x3F
        if b & 0x40:
            shift = 6
            while True:
                b = self.u8()
                v |= (b & 0x7F) << shift
                shift += 7
                if not b & 0x80:
                    break
        return -v if neg else v

    def fstring(self):
        n = self.compact()
        if n >= 0:
            s = self.d[self.p:self.p + n]
            self.p += n
            return s.rstrip(b"\0").decode("latin1")
        n = -n
        s = self.d[self.p:self.p + 2 * n]
        self.p += 2 * n
        return s.decode("utf-16le").rstrip("\0")


class Package:
    def __init__(self, path):
        raw = bytearray(Path(path).read_bytes())
        self.tag = bytes(raw[:HEADER]).decode("utf-16le", "replace")
        key = XOR.get(self.tag)
        if key is None:
            raise ValueError(f"{path}: unknown header {self.tag!r}")
        for i in range(HEADER, len(raw)):
            raw[i] ^= key
        self.d = bytes(raw[HEADER:])
        d = self.d
        magic, ver, lic, flags, nn, no, ne, eo, ni, io = struct.unpack_from("<IHHIIIIIII", d, 0)
        if magic != 0x9E2A83C1:
            raise ValueError(f"{path}: bad package tag {magic:#x}")
        self.version, self.license = ver, lic
        self.names, self.imports, self.exports = [], [], []
        r = Reader(d, no)
        for _ in range(nn):
            self.names.append((r.fstring(), r.u32()))
        r = Reader(d, io)
        for _ in range(ni):
            self.imports.append(dict(pkg=r.compact(), cls=r.compact(), outer=r.i32(), name=r.compact()))
        r = Reader(d, eo)
        for _ in range(ne):
            e = dict(cls=r.compact(), sup=r.compact(), outer=r.i32(), name=r.compact(), flags=r.u32(),
                     size=r.compact())
            e["off"] = r.compact() if e["size"] else 0
            self.exports.append(e)
        self.end_of_exports = r.p

    def name(self, i):
        return self.names[i][0]

    def ref_name(self, idx):
        """Object reference (0 = none, >0 export idx-1, <0 import -idx-1) as a short name."""
        if idx == 0:
            return "None"
        if idx > 0:
            return self.name(self.exports[idx - 1]["name"])
        return self.name(self.imports[-idx - 1]["name"])

    def class_of(self, e):
        return self.ref_name(e["cls"])

    def path(self, e):
        parts = [self.name(e["name"])]
        o = e["outer"]
        while o > 0:
            ee = self.exports[o - 1]
            parts.append(self.name(ee["name"]))
            o = ee["outer"]
        return ".".join(reversed(parts))

    def blob(self, e):
        return self.d[e["off"]:e["off"] + e["size"]]

    def texts(self, kind="ScriptText"):
        """{class name: text} of the TextBuffer objects called `kind` ("ScriptText": the .uc source,
        "CppText": the cpptext block with the native members) whose outer is the class. A TextBuffer is
        serialized as the property list (just the `None` terminator), Pos, Top, then the FString."""
        out = {}
        for e in self.exports:
            if self.class_of(e) == "TextBuffer" and self.name(e["name"]) == kind and e["outer"] > 0:
                r = Reader(self.blob(e))
                r.compact()  # None
                r.i32()  # Pos
                r.i32()  # Top
                out[self.name(self.exports[e["outer"] - 1]["name"])] = r.fstring()
        return out

    def scripts(self):
        return self.texts("ScriptText")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("pkg")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--uc", help="write every class's script text under this directory")
    a = ap.parse_args()
    pk = Package(a.pkg)
    kinds = {}
    for e in pk.exports:
        kinds[pk.class_of(e)] = kinds.get(pk.class_of(e), 0) + 1
    print(f"{a.pkg}: {pk.tag} v{pk.version}/{pk.license}, {len(pk.names)} names, {len(pk.imports)} imports, "
          f"{len(pk.exports)} exports, exports end at {pk.end_of_exports:#x} of {len(pk.d):#x}")
    if a.list:
        for k, v in sorted(kinds.items(), key=lambda kv: -kv[1]):
            print(f"  {v:6} {k}")
    if a.uc:
        out = Path(a.uc)
        out.mkdir(parents=True, exist_ok=True)
        n = 0
        for kind, ext in (("ScriptText", "uc"), ("CppText", "cpptext")):
            for name, text in pk.texts(kind).items():
                (out / f"{name}.{ext}").write_text(text, encoding="latin1", newline="")
                n += 1
        print(f"{n} texts (.uc and .cpptext) -> {out}")


if __name__ == "__main__":
    sys.exit(main())
