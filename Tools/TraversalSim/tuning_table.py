#!/usr/bin/env python3
"""Print the traversal tuning table (Markdown) from the C++ source, so the docs never drift from the code.

Usage: python Tools/TraversalSim/tuning_table.py > table.md
Reads FTraversalTuning in Source/WebOfTheCity/Public/Traversal/TraversalTypes.h: each UPROPERTY's
category, name, default and the /** comment */ above it.
"""

import re
import sys
from pathlib import Path

HEADER = Path(__file__).resolve().parents[2] / "Source/WebOfTheCity/Public/Traversal/TraversalTypes.h"

FIELD = re.compile(
    r"(?:/\*\*(?P<doc>.*?)\*/\s*)?"
    r"UPROPERTY\((?P<spec>[^)]*(?:\([^)]*\))?[^)]*)\)\s*"
    r"(?P<type>double|int32|bool)\s+(?P<name>\w+)\s*=\s*(?P<default>[^;]+);",
    re.S,
)


def rows(source):
    body = source[source.index("struct FTraversalTuning"):]
    body = body[: body.index("\n};")]
    for match in FIELD.finditer(body):
        doc = " ".join((match.group("doc") or "").split())
        category = re.search(r'Category\s*=\s*"Traversal\|(\w+)"', match.group("spec"))
        yield (category.group(1) if category else "", match.group("name"), match.group("default").strip(),
               doc)


def main():
    source = HEADER.read_text(encoding="utf-8")
    print("| Group | Setting | Default | What it does |")
    print("|---|---|---|---|")
    count = 0
    for category, name, default, doc in rows(source):
        print(f"| {category} | `{name}` | {default} | {doc} |")
        count += 1
    print(f"\n{count} settings.", file=sys.stderr)


if __name__ == "__main__":
    main()
