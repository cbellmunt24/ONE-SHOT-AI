#!/usr/bin/env python3
"""
copy_samples_to_training.py
============================
Copies and classifies one-shot .wav samples from USB drive into
Training/libraries/{instrument}/{genre}/.

ONLY COPIES — source files are NEVER modified, moved, or deleted.

Usage:
    python copy_samples_to_training.py --dry-run   # preview only
    python copy_samples_to_training.py              # execute copy
"""

import os
import sys
import shutil
import re
from pathlib import Path
from collections import defaultdict

# ============================================================
# PATHS
# ============================================================

DRUM_KITS_ROOT = Path("E:/1.SAMPLES/01 DRUM KITS")
MELODIC_ROOT   = Path("E:/1.SAMPLES/02 SAMPLE MELODIA")
TRAINING_LIB   = Path(__file__).resolve().parent / "libraries"

# ============================================================
# GENRE MAPPING — Drum Kits top-level folder → training genre
# ============================================================

DRUM_GENRE = {
    "TRAP":       "trap",
    "RAP":        "hiphop",
    "TECHNO":     "techno",
    "REAGGETON":  "reggaeton",
    "AFROBEAT":   "afrobeat",
    "JAZZ R&B":   "rnb",
    "DRILL":      "trap",
    "DETROIT":    "hiphop",
    "#CHARLIEE DRUM KIT": "trap",
    "Cymatics - 8 For 8 Anniversary Bundle": "edm",
    "SPLICE":     "edm",
    "NEW JAZZ":   "rnb",
}

# Inside TECHNO, kits whose path contains these go to "house"
HOUSE_KW = ["deep house", "deep_house"]

# ============================================================
# GENRE MAPPING — Melodic kits top-level folder → training genre
# ============================================================

MELODIC_GENRE = {
    "01 KSHMR SOUNDS":                          "edm",
    "Synth Palace (PBJ & Ellis Lost)":           "edm",
    "Cymatics Anthem Vocal Loops & One Shots":   "edm",
    "[DEMO] Nuclear Vol. 1":                     "edm",
    "DEMO SOUNDS OF AVIOTHIC Tools Kit":         "edm",
    "Cymatics - Essential MIDI Collection Bundle": None,   # MIDI only
    "Whole_Loops-GOLDEN_BUNDLE":                 None,     # loops, skip
    "Big Fish Audio Elements Indian":            None,     # loops, skip
    "1. LIBRERIAS - SAMPLES":                    None,     # unknown, skip
    "HEAT UP 3":                                 "trap",
    "HSNGNG #REPRESSION# ONE SHOT KIT(+600)":   "trap",
    "Gods - (OneShotKit) Vol.2":                 "trap",
    "02 PRODBY LJS SOUNDS":                      "trap",
    "icyje55e) One shot kit":                    "trap",
    "KIMY - DARK (One Shot Kit)":                "trap",
    "ASTRO One Shot Kit (Producergrind)":        "trap",
    "Sirius_OneShots_RipSpeakers (1)":           "trap",
    "Tone Forge Tool Kit":                       "hiphop",
    "TEXTURES":                                  "ambient",
    "prodtmbeats - Twizzy (One Shot Kit)":       None,     # empty
}

# ============================================================
# INSTRUMENT DETECTION — regex patterns on folder names
# ============================================================

DRUM_INST = {
    "kicks":  [r'\bkick', r'\bkicks\b'],
    "snares": [r'\bsnare', r'\bsnares\b'],
    "hihats": [r'\bhihat', r'\bhi[\s_-]?hat', r'\bhh\b',
               r'\bclosed[\s_-]?hat', r'\bopen[\s_-]?hat',
               r'\bhats\b', r'\bhatz\b'],
    "claps":  [r'\bclap', r'\bclaps\b'],
    "808s":   [r'\b808'],
    "percs":  [r'\bperc\b', r'\bpercs\b', r'\bpercussion\b', r'\bpercz\b',
               r'\btom\b', r'\btoms\b', r'\brim\b', r'\brimshot',
               r'\bshaker', r'\bconga', r'\bbongo', r'\bcowbell',
               r'\btabla', r'\bcymbal', r'\bcrash', r'\bride\b',
               r'\bsnap\b', r'\bsnaps\b'],
}

MELODIC_INST = {
    "leads":    [r'\blead\b', r'\bleads\b', r'\bsynth\b', r'\bsynths\b',
                 r'\bsynth[\s_-]?shot', r'\bstab\b', r'\bstabs\b'],
    "plucks":   [r'\bpluck\b', r'\bplucks\b', r'\bbell\b', r'\bbells\b',
                 r'\bmallet'],
    "pads":     [r'\bpad\b', r'\bpads\b', r'\bstring\b', r'\bstrings\b',
                 r'\bchoir\b', r'\bchoirs\b'],
    "textures": [r'\btexture', r'\bambien', r'\batmospher',
                 r'\bdrone', r'\bnoise\b'],
    "808s":     [r'\bbass\b', r'\bbasses\b', r'\bbass[\s_-]shot'],
}

# Folders to skip entirely (loops, vocals, midi, etc.)
SKIP_FOLDER_RE = [
    r'\bloop',       r'\bmidi\b',      r'\bpreset',
    r'\bvocal',      r'\bvox\b',       r'\bacapella',
    r'\bad[\s_-]?lib', r'\btransition', r'\briser',
    r'__macosx',     r'\bchant',       r'\bchop\b',
    r'\bchops\b',    r'\bprogressi',   r'\bscratch',
    r'\bfill\b',     r'\bfills\b',     r'\bwhistle',
    r'\bscale\b',
]

# ============================================================
# HELPERS
# ============================================================

def _match(text, patterns):
    """Case-insensitive regex match against any pattern."""
    low = text.lower()
    return any(re.search(p, low) for p in patterns)


def _skip_folder(name):
    return _match(name, SKIP_FOLDER_RE)


def _detect(parts, inst_map):
    """Return instrument name by checking path parts deepest-first."""
    for part in reversed(parts):
        for inst, pats in inst_map.items():
            if _match(part, pats):
                return inst
    return None


def _unique_dest(dest):
    """Avoid overwriting: add _1, _2, … suffix when dest exists."""
    if not dest.exists():
        return dest
    stem, ext, parent = dest.stem, dest.suffix, dest.parent
    n = 1
    while True:
        candidate = parent / f"{stem}_{n}{ext}"
        if not candidate.exists():
            return candidate
        n += 1


# ============================================================
# SCAN FUNCTIONS
# ============================================================

def scan_drum_kits():
    plan = []
    if not DRUM_KITS_ROOT.exists():
        print(f"[WARN] Drum kits path not found: {DRUM_KITS_ROOT}")
        return plan

    for genre_dir in DRUM_KITS_ROOT.iterdir():
        if not genre_dir.is_dir():
            continue
        base_genre = DRUM_GENRE.get(genre_dir.name)
        if base_genre is None:
            continue

        for root, dirs, files in os.walk(genre_dir):
            rpath = Path(root)
            try:
                parts = rpath.relative_to(genre_dir).parts
            except (ValueError, OSError):
                continue
            if any(_skip_folder(p) for p in parts):
                continue

            # TECHNO → house override
            genre = base_genre
            if genre_dir.name == "TECHNO":
                full_lower = rpath.as_posix().lower()
                if any(kw in full_lower for kw in HOUSE_KW):
                    genre = "house"

            inst = _detect(parts, DRUM_INST)
            if inst is None:
                continue

            for f in files:
                if not f.lower().endswith(".wav"):
                    continue
                plan.append((rpath / f, TRAINING_LIB / inst / genre / f))

    return plan


def scan_melodic():
    plan = []
    if not MELODIC_ROOT.exists():
        print(f"[WARN] Melodic path not found: {MELODIC_ROOT}")
        return plan

    for kit_dir in MELODIC_ROOT.iterdir():
        if not kit_dir.is_dir():
            continue
        genre = MELODIC_GENRE.get(kit_dir.name)
        if genre is None:
            continue

        for root, dirs, files in os.walk(kit_dir):
            rpath = Path(root)
            parts = rpath.relative_to(kit_dir).parts
            if any(_skip_folder(p) for p in parts):
                continue

            # Try melodic instruments first, then drum instruments
            inst = _detect(parts, MELODIC_INST)
            if inst is None:
                inst = _detect(parts, DRUM_INST)
            if inst is None:
                continue

            for f in files:
                if not f.lower().endswith(".wav"):
                    continue
                plan.append((rpath / f, TRAINING_LIB / inst / genre / f))

    return plan


# ============================================================
# REPORTING
# ============================================================

def print_summary(plan, title):
    counts = defaultdict(int)
    for _, dst in plan:
        parts = dst.relative_to(TRAINING_LIB).parts
        counts[(parts[0], parts[1])] += 1

    print(f"\n{'='*56}")
    print(f"  {title}")
    print(f"{'='*56}")
    print(f"  {'Instrument':<12} {'Genre':<12} {'Samples':>8}")
    print(f"  {'-'*12} {'-'*12} {'-'*8}")

    total = 0
    for (inst, genre) in sorted(counts):
        c = counts[(inst, genre)]
        print(f"  {inst:<12} {genre:<12} {c:>8}")
        total += c
    print(f"  {'-'*34}")
    print(f"  {'TOTAL':<24} {total:>8}")
    return total


# ============================================================
# COPY
# ============================================================

def execute(plan):
    copied = errors = skipped = 0
    for src, dst in plan:
        try:
            dst.parent.mkdir(parents=True, exist_ok=True)
            final = _unique_dest(dst)
            shutil.copy2(str(src), str(final))
            copied += 1
            if copied % 500 == 0:
                print(f"  ... {copied} copied")
        except Exception as e:
            print(f"  ERROR: {src.name} → {e}")
            errors += 1
    return copied, errors


# ============================================================
# MAIN
# ============================================================

def main():
    dry = "--dry-run" in sys.argv

    print("Scanning drum kits...")
    drum = scan_drum_kits()
    d_total = print_summary(drum, "DRUM KITS -> Training")

    print("\nScanning melodic samples...")
    melo = scan_melodic()
    m_total = print_summary(melo, "MELODIC SAMPLES -> Training")

    grand = d_total + m_total
    print(f"\n{'='*56}")
    print(f"  GRAND TOTAL: {grand} samples to copy")
    print(f"{'='*56}")

    if dry:
        print("\n  [DRY RUN] No files were copied.\n")
        return

    ans = input("\n  Proceed with copy? (y/n): ").strip().lower()
    if ans != "y":
        print("  Cancelled.")
        return

    full = drum + melo
    print(f"\n  Copying {grand} files...")
    copied, errors = execute(full)
    print(f"\n  Done! Copied: {copied} | Errors: {errors}")


if __name__ == "__main__":
    main()
