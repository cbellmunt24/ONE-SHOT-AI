#!/bin/bash
# Pipeline COMPLETO: Fases 4 → 7 (automatico)
cd "$(dirname "$0")"
PYTHON="venv/Scripts/python"
LOG="/tmp/pipeline_full.log"
export PYTHONUNBUFFERED=1
export PYTHONIOENCODING=utf-8

echo "=== Pipeline FULL started $(date) ===" > "$LOG"

echo "=== [FASE 4] 03_optimize_params.py ===" >> "$LOG"
$PYTHON scripts/03_optimize_params.py --skip-existing --diagnostics >> "$LOG" 2>&1
echo "Exit code 03: $?" >> "$LOG"

echo "=== [FASE 4] 03b_learn_mutation_axes.py ===" >> "$LOG"
$PYTHON scripts/03b_learn_mutation_axes.py --export-cpp >> "$LOG" 2>&1
echo "Exit code 03b: $?" >> "$LOG"

echo "=== [FASE 4] 04_export_genre_rules.py ===" >> "$LOG"
$PYTHON scripts/04_export_genre_rules.py >> "$LOG" 2>&1
echo "Exit code 04: $?" >> "$LOG"

echo "=== [FASE 5] 05_train_model.py ===" >> "$LOG"
$PYTHON scripts/05_train_model.py --visualize >> "$LOG" 2>&1
echo "Exit code 05: $?" >> "$LOG"

echo "=== [FASE 6] 06_export_onnx.py ===" >> "$LOG"
$PYTHON scripts/06_export_onnx.py --verify >> "$LOG" 2>&1
echo "Exit code 06: $?" >> "$LOG"

echo "=== [FASE 7] 07_train_quality_scorer.py ===" >> "$LOG"
$PYTHON scripts/07_train_quality_scorer.py --export-onnx >> "$LOG" 2>&1
echo "Exit code 07: $?" >> "$LOG"

echo "=== PIPELINE COMPLETO $(date) ===" >> "$LOG"
