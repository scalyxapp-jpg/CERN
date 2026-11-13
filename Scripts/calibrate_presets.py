#!/usr/bin/env python3
"""
Kalibriert die Factory-Presets auf einen gemeinsamen Lautheits-Zielwert.

Ablauf:
  1. Messen:  build/Source/CERNLab <outDir> <amountPct> <sekunden>
  2. Rechnen: pro Preset delta = zielLUFS - gemessenLUFS
  3. Schreiben: outputGain im PresetManager.cpp um delta korrigieren

Aufruf:
    python3 Scripts/calibrate_presets.py            # messen + anwenden
    python3 Scripts/calibrate_presets.py --measure  # nur messen (Tabelle ausgeben)

Hinweis: Der Preset-Zielwert ist integrierte Kurzzeit-LUFS ueber 4 Takte bei
120 BPM (identisch zur Messung im Test factory_presets_loudness_spread).
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LAB = ROOT / "build/Source/CERNLab"
PRESETS = ROOT / "Source/Presets/PresetManager.cpp"
TARGET_LUFS = -13.0
MEASURE_SECONDS = 8.0
LAYER_AMOUNT = 100.0


def measure():
    if not LAB.exists():
        sys.exit(
            f"FEHLT: {LAB} (bitte 'cmake --build build --target CERNLab' ausfuehren)"
        )

    out = subprocess.run(
        [str(LAB), "/tmp/cern-lab-cal", str(LAYER_AMOUNT), str(MEASURE_SECONDS)],
        capture_output=True,
        text=True,
        check=True,
    ).stdout

    rows = []
    for line in out.splitlines():
        m = re.match(
            r"^(\d+)\s+(.+?)\s{2,}(-?\d+\.\d+)\s+(-?\d+\.\d+)\s+(-?\d+\.\d+)\s+(-?\d+\.\d+)",
            line,
        )
        if m:
            rows.append(
                {
                    "index": int(m.group(1)),
                    "name": m.group(2).strip(),
                    "lufs": float(m.group(3)),
                    "lufsMono": float(m.group(4)),
                    "peak": float(m.group(5)),
                    "crest": float(m.group(6)),
                }
            )

    if not rows:
        sys.exit("Keine Messdaten aus CERNLab.")

    return rows


def apply_deltas(rows):
    text = PRESETS.read_text(encoding="utf-8")
    changed = 0

    for row in rows:
        delta = TARGET_LUFS - row["lufs"]
        if abs(delta) < 0.05:
            continue

        # Preset-Block finden
        idx = text.find('"%s"' % row["name"])
        if idx < 0:
            print("  ! Preset nicht gefunden: " + row["name"], file=sys.stderr)
            continue

        # outputGain innerhalb des Blocks anpassen
        end = text.find("} })", idx)
        block = text[idx:end]

        m = re.search(r"\{ ParameterIDs::outputGain,\s*([-+]?[\d.]+)f \}", block)
        if not m:
            print(f"  ! kein outputGain in {row['name']}", file=sys.stderr)
            continue

        old = float(m.group(1))
        new = max(-24.0, min(24.0, old + delta))
        new_text = "{ ParameterIDs::outputGain, " + f"{round(new, 2)}" + "f }"
        block_new = block[: m.start()] + new_text + block[m.end() :]
        text = text[:idx] + block_new + text[end:]
        changed += 1
        print(
            f"  {row['name']:<26} {old:+6.2f} dB -> {new:+6.2f} dB  (Delta {delta:+.2f})"
        )

    PRESETS.write_text(text, encoding="utf-8")
    print(f"{changed} Presets angepasst (Ziel {TARGET_LUFS:.1f} LUFS).")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--measure", action="store_true", help="nur messen, nichts schreiben"
    )
    args = ap.parse_args()

    rows = measure()

    print(
        f"{'#':<3}{'Preset':<26}{'LUFS':>8}{'LUFSmono':>10}{'Peak':>8}{'Crest':>7}{'Ziel-Delta':>12}"
    )
    for row in rows:
        print(
            f"{row['index']:<3}{row['name']:<26}{row['lufs']:>8.2f}{row['lufsMono']:>10.2f}"
            f"{row['peak']:>8.3f}{row['crest']:>7.1f}{TARGET_LUFS - row['lufs']:>12.2f}"
        )

    spread = max(r["lufs"] for r in rows) - min(r["lufs"] for r in rows)
    print(f"\nSpread: {spread:.2f} dB (Grenze 1,50 dB)")

    if not args.measure:
        print("\nKorrekturen:")
        apply_deltas(rows)


if __name__ == "__main__":
    main()
