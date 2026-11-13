#!/usr/bin/env python3
"""
Aktualisiert die Parameter-Tabellen im README aus dem Quelltext.

Quellen der Wahrheit: Scripts/gen_param_docs.py (Layout, Defaults, Sektionen).
Die handgeschriebenen Tabellen werden durch die generierten ersetzt; die
Fliesstext-Abschnitte im README bleiben unveraendert.

Aufruf:
    python3 Scripts/update_readme_params.py            # README.md im Repo aktualisieren
    python3 Scripts/update_readme_params.py --check    # nur pruefen (Exit 1 bei Abweichung)
"""

import argparse
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
README = ROOT / "README.md"

sys.path.insert(0, str(ROOT / "Scripts"))
import gen_param_docs as gen


def build_tables():
    """-> {Sektionsname (kleinbuchstaben): markdown-tabelle}"""
    layout = gen.parse_layout_cpp()
    defaults = gen.parse_defaults()
    comments = gen.parse_id_comments()
    tables = {}

    for title, ids, _enables in gen.parse_sections():
        if not ids:
            continue

        lines = [
            "| Parameter | Werte | Default | Bedeutung |",
            "| --------- | ----- | ------- | --------- |",
        ]

        for pid in ids:
            info = layout.get(pid)
            if info is None:
                info = {
                    "display": pid,
                    "min": None,
                    "max": None,
                    "step": None,
                    "skew": None,
                    "defaultExpr": "",
                    "tooltip": "",
                    "isBool": False,
                }
            meaning = info["tooltip"] or comments.get(pid, "")
            lines.append(
                f"| `{pid}` | {gen.fmt_range(info)} | "
                f"{gen.fmt_default(pid, info, defaults)} | {meaning} |"
            )

        tables[title.lower()] = "\n".join(lines)

    return tables


def match_section(heading, tables):
    """README-Ueberschrift -> Sektionsname (längste passende Ueberschrift gewinnt)."""
    heading = heading.lower()
    best = None

    for name in tables:
        if name in heading and (best is None or len(name) > len(best)):
            best = name

    return best


def replace_section(section, tables):
    """Ersetzt in jedem '### '-Abschnitt die erste Tabelle durch die generierte."""
    lines = section.split("\n")
    out = []
    current = None
    i = 0
    replaced = 0

    while i < len(lines):
        line = lines[i]

        if line.startswith("### "):
            current = match_section(line[4:].strip(), tables)
            out.append(line)
            i += 1
            continue

        if current and current in tables and line.startswith("|"):
            out.append(tables[current])
            replaced += 1
            i += 1
            while i < len(lines) and lines[i].startswith("|"):
                i += 1
            continue

        out.append(line)
        i += 1

    return "\n".join(out), replaced


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()

    tables = build_tables()
    text = README.read_text(encoding="utf-8")

    start = text.index("## Parameter")
    end = text.index("## GUI-Features")

    new_section, replaced = replace_section(text[start:end], tables)
    new_text = text[:start] + new_section + text[end:]

    if args.check:
        if new_text != text:
            print("README-Parametertabellen sind veraltet.", file=sys.stderr)
            return 1
        print("README-Parametertabellen sind aktuell.")
        return 0

    README.write_text(new_text, encoding="utf-8")
    print(f"README aktualisiert ({replaced} Tabellen).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
