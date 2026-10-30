#!/usr/bin/env python3
"""tools/extract_classes.py for another NWindow.dll (the C2 client's, say), writing to <outdir>
instead of analysis/ (nothing of the C1 analysis is touched).
usage: extract_classes.py <dll> <outdir>      -> <outdir>/analysis/classes.json, classes.md, vtable_starts.txt"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import extract_classes as ec  # noqa: E402
from nwd.image import Image  # noqa: E402

dll, out = sys.argv[1], Path(sys.argv[2])
(out / "analysis").mkdir(parents=True, exist_ok=True)
ec.Image = lambda: Image(dll)
ec.ROOT = out
raise SystemExit(ec.main())
