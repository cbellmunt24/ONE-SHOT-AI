"""
04_export_genre_rules.py — Capa 2: Exportar GenreRules.h calibrado
Lee los parametros optimizados y genera un nuevo GenreRules.h
con valores calibrados contra samples reales.

Uso:
    python 04_export_genre_rules.py
    python 04_export_genre_rules.py --output ../data/GenreRules_calibrated.h
"""

import os
import sys
import json
import argparse
from pathlib import Path
from datetime import datetime


SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
OPTIMIZED_DIR = TRAINING_DIR / "data" / "optimized_params"
PLUGIN_DIR = TRAINING_DIR.parent / "ONE-SHOT AI" / "Source" / "AI"


def load_all_optimized() -> dict:
    """Carga todos los parametros optimizados."""
    results = {}
    for f in sorted(OPTIMIZED_DIR.glob("*_optimized.json")):
        if f.name.startswith("_"):
            continue
        with open(f, 'r') as fp:
            data = json.load(fp)
        inst = data["instrument"]
        genre = data["genre"]
        if inst not in results:
            results[inst] = {}
        results[inst][genre] = data["optimal_params"]
    return results


def generate_genre_rules_header(optimized: dict, output_path: Path):
    """Genera el archivo GenreRules.h con valores calibrados."""

    lines = []
    lines.append("// GenreRules_calibrated.h")
    lines.append(f"// Generado automaticamente por 04_export_genre_rules.py")
    lines.append(f"// Fecha: {datetime.now().strftime('%Y-%m-%d %H:%M')}")
    lines.append(f"// Calibrado contra {sum(len(g) for g in optimized.values())} combinaciones instrumento/genero")
    lines.append("//")
    lines.append("// NOTA: Estos valores reemplazan los del GenreRules.h original.")
    lines.append("// Revisar y mergear manualmente con el archivo existente.")
    lines.append("")
    lines.append("#pragma once")
    lines.append("")
    lines.append("// ── Parametros optimizados por instrumento/genero ──────────")
    lines.append("// Formato: JSON-like comments + constexpr values")
    lines.append("")

    for instrument in sorted(optimized.keys()):
        lines.append(f"// ═══ {instrument.upper()} ═══")
        lines.append("")

        genres = optimized[instrument]
        for genre in sorted(genres.keys()):
            params = genres[genre]
            lines.append(f"// ── {instrument}/{genre} ──")
            lines.append(f"// Parametros optimizados:")

            for param_name, value in sorted(params.items()):
                lines.append(f"//   {param_name}: {value}")

            lines.append("")

    # Generar estructura de datos como JSON embebido para facilitar integracion
    lines.append("")
    lines.append("/*")
    lines.append("JSON de parametros optimizados para integracion programatica:")
    lines.append("")
    lines.append(json.dumps(optimized, indent=2))
    lines.append("")
    lines.append("*/")

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))

    return len(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Exporta parametros optimizados como GenreRules.h")
    parser.add_argument("--output", type=str, default=None,
                        help="Ruta de salida (default: data/GenreRules_calibrated.h)")
    args = parser.parse_args()

    print("=" * 60)
    print("ONE-SHOT AI — Export Genre Rules (Capa 2)")
    print("=" * 60)

    optimized = load_all_optimized()
    if not optimized:
        print("\n[!] No se encontraron parametros optimizados.")
        print("    Ejecuta primero: python 03_optimize_params.py")
        return

    total_combos = sum(len(g) for g in optimized.values())
    print(f"Instrumentos: {len(optimized)}")
    print(f"Combinaciones: {total_combos}")

    if args.output:
        output_path = Path(args.output)
    else:
        output_path = TRAINING_DIR / "data" / "GenreRules_calibrated.h"

    n_lines = generate_genre_rules_header(optimized, output_path)
    print(f"\nGenerado: {output_path} ({n_lines} lineas)")

    # Mostrar diff con el original si existe
    original = PLUGIN_DIR / "GenreRules.h"
    if original.exists():
        print(f"\nGenreRules.h original: {original}")
        print("Para comparar:")
        print(f"  diff \"{original}\" \"{output_path}\"")
        print("\nRECUERDA: Revisar y mergear manualmente antes de copiar al plugin.")
    else:
        print(f"\n[info] No se encontro GenreRules.h original en {PLUGIN_DIR}")

    # Tambien exportar como JSON limpio
    json_path = TRAINING_DIR / "data" / "optimized_params" / "_all_optimized.json"
    with open(json_path, 'w', encoding='utf-8') as f:
        json.dump(optimized, f, indent=2)
    print(f"JSON completo: {json_path}")


if __name__ == "__main__":
    main()
