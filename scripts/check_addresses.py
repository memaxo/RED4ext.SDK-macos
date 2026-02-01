#!/usr/bin/env python3

from __future__ import annotations

import json
import re
import sys
from pathlib import Path


def load_hash_name_map(root: Path) -> dict[str, str]:
    # Map decimal hash string -> constant name from include/RED4ext/Detail/AddressHashes.hpp
    p = root / "include" / "RED4ext" / "Detail" / "AddressHashes.hpp"
    text = p.read_text(encoding="utf-8", errors="replace")
    rx = re.compile(r"^\s*constexpr\s+std::uint32_t\s+(?P<name>\w+)\s*=\s*(?P<val>0x[0-9A-Fa-f]+|\d+)(?:UL)?;", re.M)
    out: dict[str, str] = {}
    for m in rx.finditer(text):
        name = m.group("name")
        val_s = m.group("val")
        val = int(val_s, 0)
        out[str(val)] = name
    return out


def main() -> int:
    strict = "--strict" in sys.argv[1:]

    root = Path(__file__).resolve().parents[1]
    db_path = root / "cyberpunk2077_addresses.json"
    obj = json.loads(db_path.read_text(encoding="utf-8"))

    hash_names = load_hash_name_map(root)

    addrs = obj.get("Addresses", [])
    if not isinstance(addrs, list):
        raise SystemExit("Addresses must be a list")

    zero = []
    dup = []
    seen = set()

    for a in addrs:
        h = a.get("hash")
        off = a.get("offset")

        if h in seen:
            dup.append(h)
        seen.add(h)

        if isinstance(off, str) and off.endswith(":0x0"):
            zero.append(h)

    print(f"total={len(addrs)}")
    print(f"duplicates={len(dup)}")
    if dup:
        print("duplicate_hashes=", ",".join(map(str, dup[:20])))

    print(f"zero_offsets={len(zero)}")
    if zero:
        pairs = [f"{h}({hash_names.get(str(h), 'UNKNOWN')})" for h in zero[:20]]
        print("zero_offset_hashes=", ",".join(pairs))

    if strict:
        return 1 if dup or zero else 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
