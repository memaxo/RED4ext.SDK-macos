#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
from pathlib import Path


def parse_offset(s: str) -> tuple[int, int]:
    # "1:0xABC" -> (1, 0xABC)
    seg_s, off_s = s.split(":", 1)
    seg = int(seg_s, 0)
    off = int(off_s, 0)
    return seg, off


def format_offset(seg: int, off: int) -> str:
    return f"{seg}:0x{off:X}"


def normalize_hash_to_dec_string(h: str | int) -> str:
    if isinstance(h, int):
        return str(h)
    h = h.strip()
    return str(int(h, 0))


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate loader address DB by merging SDK DB + loader hook targets")
    ap.add_argument(
        "--sdk-db",
        type=Path,
        default=None,
        help="Path to SDK cyberpunk2077_addresses.json (default: <repo>/cyberpunk2077_addresses.json)",
    )
    ap.add_argument(
        "--hooks",
        type=Path,
        default=None,
        help="Path to loader_hook_targets.json (default: <repo>/scripts/loader_hook_targets.json)",
    )
    ap.add_argument(
        "--out",
        type=Path,
        default=None,
        help="Output path (default: <repo>/cyberpunk2077_addresses.loader.json)",
    )
    args = ap.parse_args()

    root = Path(__file__).resolve().parents[1]
    sdk_db = args.sdk_db or (root / "cyberpunk2077_addresses.json")
    hooks_path = args.hooks or (root / "scripts" / "loader_hook_targets.json")
    out_path = args.out or (root / "cyberpunk2077_addresses.loader.json")

    sdk_obj = json.loads(sdk_db.read_text(encoding="utf-8"))
    hooks_obj = json.loads(hooks_path.read_text(encoding="utf-8"))

    sdk_game = str(sdk_obj.get("game_version", ""))
    hooks_game = str(hooks_obj.get("game_version", ""))
    if sdk_game and hooks_game and sdk_game != hooks_game:
        raise SystemExit(f"game_version mismatch: sdk={sdk_game!r} hooks={hooks_game!r}")
    game_version = sdk_game or hooks_game

    sdk_addrs = sdk_obj.get("Addresses", [])
    if not isinstance(sdk_addrs, list):
        raise SystemExit("SDK Addresses must be a list")

    out_map: dict[str, str] = {}

    for a in sdk_addrs:
        h = normalize_hash_to_dec_string(a.get("hash"))
        off = a.get("offset")
        if not isinstance(off, str):
            continue
        # Normalize offset formatting
        seg, off_v = parse_offset(off)
        out_map[h] = format_offset(seg, off_v)

    hooks = hooks_obj.get("hooks", [])
    if not isinstance(hooks, list):
        raise SystemExit("hooks must be a list")
    for h in hooks:
        hh = normalize_hash_to_dec_string(h.get("hash"))
        off = h.get("offset")
        if not isinstance(off, str):
            raise SystemExit(f"hook {hh} missing offset")
        seg, off_v = parse_offset(off)
        out_map[hh] = format_offset(seg, off_v)

    # Emit stable order
    def key_fn(k: str) -> int:
        return int(k, 10)

    addresses = [{"hash": k, "offset": out_map[k]} for k in sorted(out_map.keys(), key=key_fn)]
    resolved = sum(1 for a in addresses if isinstance(a.get("offset"), str) and not a["offset"].endswith(":0x0"))

    out_obj = {
        "version": str(sdk_obj.get("version", "1.0")),
        "game_version": game_version,
        "stats": {"total": len(addresses), "resolved": resolved, "unresolved": len(addresses) - resolved},
        "Addresses": addresses,
    }

    out_path.write_text(json.dumps(out_obj, indent=2, sort_keys=False) + "\n", encoding="utf-8")
    print(f"Wrote {out_path} (total={len(addresses)} resolved={resolved})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
