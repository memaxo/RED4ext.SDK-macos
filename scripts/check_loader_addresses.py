#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def normalize_hash_to_dec_string(h: str | int) -> str:
    if isinstance(h, int):
        return str(h)
    return str(int(str(h).strip(), 0))


def main() -> int:
    ap = argparse.ArgumentParser(description="Validate loader address DB (SDK DB + loader hooks)")
    ap.add_argument("--strict", action="store_true")
    ap.add_argument(
        "--db",
        type=Path,
        default=None,
        help="Path to loader DB (default: <repo>/cyberpunk2077_addresses.loader.json)",
    )
    ap.add_argument(
        "--hooks",
        type=Path,
        default=None,
        help="Path to loader_hook_targets.json (default: <repo>/scripts/loader_hook_targets.json)",
    )
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[1]
    db_path = args.db or (root / "cyberpunk2077_addresses.loader.json")
    hooks_path = args.hooks or (root / "scripts" / "loader_hook_targets.json")

    obj = json.loads(db_path.read_text(encoding="utf-8"))
    hooks_obj = json.loads(hooks_path.read_text(encoding="utf-8"))

    addrs = obj.get("Addresses", [])
    if not isinstance(addrs, list):
        raise SystemExit("Addresses must be a list")

    seen: set[str] = set()
    dup: list[str] = []
    zero: list[str] = []
    by_hash: dict[str, str] = {}

    for a in addrs:
        h = normalize_hash_to_dec_string(a.get("hash"))
        off = a.get("offset")
        if h in seen:
            dup.append(h)
        seen.add(h)
        if isinstance(off, str) and off.endswith(":0x0"):
            zero.append(h)
        if isinstance(off, str):
            by_hash[h] = off

    hooks = hooks_obj.get("hooks", [])
    if not isinstance(hooks, list):
        raise SystemExit("hooks must be a list")

    missing: list[str] = []
    for h in hooks:
        hh = normalize_hash_to_dec_string(h.get("hash"))
        if hh not in by_hash:
            missing.append(hh)
        elif by_hash[hh].endswith(":0x0"):
            zero.append(hh)

    expected_total = None
    if "stats" in obj and isinstance(obj["stats"], dict):
        # If caller already provides stats.total, treat it as expectation.
        expected_total = obj["stats"].get("total")

    print(f"db={db_path}")
    print(f"total={len(addrs)}")
    print(f"duplicates={len(dup)}")
    print(f"zero_offsets={len(set(zero))}")
    print(f"missing_required_hooks={len(missing)}")
    if missing:
        print("missing_hooks=", ",".join(missing))
    if dup:
        print("duplicate_hashes=", ",".join(dup[:20]))
    if zero:
        print("zero_offset_hashes=", ",".join(sorted(set(zero))[:20]))
    if expected_total is not None and int(expected_total) != len(addrs):
        print(f"WARNING: stats.total={expected_total} but Addresses has {len(addrs)}")

    if args.strict:
        return 1 if dup or zero or missing else 0
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
