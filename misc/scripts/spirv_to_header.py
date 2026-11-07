"""Embed a SPIR-V shader with one main entry point into a C++ header."""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import struct


def make_header(data: bytes, symbol: str) -> str:
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", symbol):
        raise ValueError("The array name must be a C++ identifier")
    if len(data) < 20 or len(data) % 4:
        raise ValueError("SPIR-V must contain an aligned five-word header")
    words = struct.unpack(f"<{len(data) // 4}I", data)
    if words[0] != 0x07230203:
        raise ValueError("Invalid SPIR-V magic")
    entries = []
    cursor = 5
    while cursor < len(words):
        count, opcode = words[cursor] >> 16, words[cursor] & 0xFFFF
        if count == 0 or cursor + count > len(words):
            raise ValueError("Malformed SPIR-V instruction")
        if opcode == 15:  # OpEntryPoint: model, id, null-terminated name, interfaces.
            if count < 4:
                raise ValueError("Malformed SPIR-V entry point")
            raw = struct.pack(f"<{count - 3}I", *words[cursor + 3:cursor + count])
            entries.append(raw.split(b"\0", 1)[0])
        cursor += count
    if entries != [b"main"]:
        raise ValueError("Expected one SPIR-V entry named main")
    rows = ["    " + ", ".join(f"0x{word:08x}u" for word in words[i:i + 8]) + ","
            for i in range(0, len(words), 8)]
    return ("#pragma once\n\n#include <cstdint>\n\n"
            + f"static const std::uint32_t {symbol}[] = {{\n"
            + "\n".join(rows) + "\n};\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("symbol")
    args = parser.parse_args()
    try:
        header = make_header(args.input.read_bytes(), args.symbol)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(header, encoding="utf-8")
    except (OSError, ValueError) as error:
        parser.exit(1, f"SPIR-V header generation failed: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
