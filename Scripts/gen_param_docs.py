#!/usr/bin/env python3
"""
Generiert die Parameter-Tabellen fuer das README direkt aus dem Quelltext.

Quellen der Wahrheit:
  * Source/Parameters/ParameterLayout.cpp  -> ID, Anzeigename, Bereich, Step, Skew, Tooltip
  * Source/Parameters/ParameterLayout.h    -> DefaultValues
  * Source/Parameters/ParameterIDs.h      -> Kommentar-Hinweis je ID
  * Source/GUI/MainComponent.cpp          -> Zuordnung ID -> Sektion (addSection)

Aufruf:
    python3 Scripts/gen_param_docs.py                 # Markdown auf stdout
    python3 Scripts/gen_param_docs.py -o file.md      # in Datei schreiben
    python3 Scripts/gen_param_docs.py --sections      # nur Sektions-Uebersicht

Damit die README-Tabellen nicht mehr veralten (Review M11).
"""

import argparse
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAYOUT_CPP = ROOT / "Source/Parameters/ParameterLayout.cpp"
LAYOUT_H = ROOT / "Source/Parameters/ParameterLayout.h"
IDS_H = ROOT / "Source/Parameters/ParameterIDs.h"
MAIN_CPP = ROOT / "Source/GUI/MainComponent.cpp"


def parse_layout_cpp():
    """ID -> dict(display, min, max, step, skew, tooltip, isBool)"""
    text = LAYOUT_CPP.read_text(encoding="utf-8")

    out = {}

    # addFloat (ID, "Name", { min, max, step[, skew] }, default)
    for m in re.finditer(
        r'addFloat\s*\(\s*ParameterIDs::([A-Za-z0-9_]+)\s*,\s*"([^"]+)"\s*,\s*\{([^}]*)\}\s*,\s*([^)]*)\)',
        text,
    ):
        pid, name, rng, default = m.groups()
        nums = [x.strip() for x in rng.split(",") if x.strip()]

        def parse_num(tok):
            tok = tok.strip()
            mnum = re.match(r"^(-?[0-9]*\.?[0-9]+)f?$", tok)
            return float(mnum.group(1)) if mnum else None

        vals = [parse_num(x) for x in nums]
        out[pid] = {
            "display": name,
            "min": vals[0] if len(vals) > 0 else None,
            "max": vals[1] if len(vals) > 1 else None,
            "step": vals[2] if len(vals) > 2 else None,
            "skew": vals[3] if len(vals) > 3 else None,
            "defaultExpr": default.strip(),
            "tooltip": "",
            "isBool": False,
        }

    # addBool
    for m in re.finditer(
        r'addBool\s*\(\s*ParameterIDs::([A-Za-z0-9_]+)\s*,\s*"([^"]*)"\s*,\s*([^,)]+)\s*,\s*"([^"]*)"',
        text,
    ):
        pid, name, default, tip = m.groups()
        out[pid] = {
            "display": name,
            "min": 0.0,
            "max": 1.0,
            "step": 1.0,
            "skew": None,
            "defaultExpr": default.strip(),
            "tooltip": tip,
            "isBool": True,
        }

    # AudioParameterBool
    for m in re.finditer(
        r'AudioParameterBool>\s*\(\s*ParameterIDs::([A-Za-z0-9_]+)\s*,\s*"([^"]*)"\s*,\s*([^,]+)\s*,\s*"([^"]*)"',
        text,
    ):
        pid, name, default, tip = m.groups()
        out[pid] = {
            "display": name,
            "min": 0.0,
            "max": 1.0,
            "step": 1.0,
            "skew": None,
            "defaultExpr": default.strip(),
            "tooltip": tip,
            "isBool": True,
        }

    return out


def parse_defaults():
    """ID -> Defaultwert (aus DefaultValues)"""
    text = LAYOUT_H.read_text(encoding="utf-8")
    out = {}
    for m in re.finditer(
        r"static constexpr\s+(?:float|bool|int)\s+([A-Za-z0-9_]+)\s*=\s*([^;]+);", text
    ):
        out[m.group(1)] = m.group(2).strip()
    return out


def parse_id_comments():
    """ID -> Kommentar-Hinweis (rechte Spalte der ID-Deklaration)"""
    text = IDS_H.read_text(encoding="utf-8")
    out = {}
    for m in re.finditer(
        r'static constexpr auto\*\s+([A-Za-z0-9_]+)\s*=\s*"([^"]+)"\s*;(.*)', text
    ):
        name, _value, tail = m.groups()
        comment = tail.split("//", 1)[1].strip() if "//" in tail else ""
        out[name] = comment
    return out


def parse_sections():
    """Sektionstitel -> [IDs]"""
    text = MAIN_CPP.read_text(encoding="utf-8")
    out = []
    for m in re.finditer(
        r'addSection\s*\(\s*"([^"]+)"\s*,\s*\{(.*?)\}\s*(?:,\s*\{(.*?)\}\s*)?\)',
        text,
        re.DOTALL,
    ):
        title, body, enables = m.groups()
        ids = re.findall(r"ParameterIDs::([A-Za-z0-9_]+)", body or "")
        en = re.findall(r"ParameterIDs::([A-Za-z0-9_]+)", enables or "")
        out.append((title, ids, en))
    return out


def fmt_range(info):
    if info["isBool"]:
        return "Bool"
    lo, hi = info["min"], info["max"]
    if lo is None or hi is None:
        return ""
    if lo == int(lo) and hi == int(hi):
        return f"{int(lo)}–{int(hi)}"
    return f"{lo:g}–{hi:g}"


def fmt_default(pid, info, defaults):
    expr = info["defaultExpr"]
    if expr.startswith("DefaultValues::"):
        key = expr.split("::", 1)[1].strip()
        raw = defaults.get(key)
        if raw is not None:
            if info["isBool"]:
                return "an" if raw in ("true", "1") else "aus"
            return raw.rstrip("f")
    if info["isBool"]:
        return "an" if expr in ("true", "1") else "aus"
    return expr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-o", "--output")
    ap.add_argument("--sections", action="store_true")
    args = ap.parse_args()

    layout = parse_layout_cpp()
    defaults = parse_defaults()
    comments = parse_id_comments()
    sections = parse_sections()

    lines = []
    for title, ids, enables in sections:
        if not ids and not enables:
            continue
        lines.append(f"### {title}\n")
        lines.append("| Parameter | Werte | Default | Bedeutung |")
        lines.append("| --------- | ----- | ------- | --------- |")
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
                f"| `{pid}` | {fmt_range(info)} | {fmt_default(pid, info, defaults)} | {meaning} |"
            )
        lines.append("")

    text = "\n".join(lines)

    if args.sections:
        text = "\n".join(
            f"- {t}: {len(i)} Parameter, {len(e)} Enables" for t, i, e in sections
        )

    if args.output:
        Path(args.output).write_text(text, encoding="utf-8")
        print(f"geschrieben: {args.output}", file=sys.stderr)
    else:
        print(text)


if __name__ == "__main__":
    main()
