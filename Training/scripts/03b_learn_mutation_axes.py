"""
03b_learn_mutation_axes.py — Aprende ejes de mutacion correlacionados (PCA)

Analiza los parametros optimizados de Capa 2 y extrae los ejes principales
de variacion por instrumento. Esto permite que las mutaciones produzcan
variaciones coherentes en vez de jitter aleatorio.

Uso:
    python 03b_learn_mutation_axes.py
    python 03b_learn_mutation_axes.py --instrument kicks
    python 03b_learn_mutation_axes.py --n-axes 5
"""

import os
import sys
import json
import argparse
import numpy as np
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))

from sklearn.decomposition import PCA
from sklearn.preprocessing import StandardScaler

SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
OPTIMIZED_DIR = TRAINING_DIR / "data" / "optimized_params"
MUTATION_DIR = TRAINING_DIR / "data" / "mutation_axes"

INSTRUMENTS = ["kicks", "snares", "hihats", "claps", "percs",
               "808s", "leads", "plucks", "pads", "textures"]

# Nombres semanticos para los ejes (se asignan post-hoc basandose en los loadings)
AXIS_SEMANTICS = {
    "kicks": ["peso", "punch", "longitud", "brillo", "drive"],
    "snares": ["cuerpo", "snap", "ruido", "tono", "duracion"],
    "hihats": ["apertura", "brillo"],
    "claps": ["duracion", "espacialidad"],
    "percs": ["tono", "duracion", "ruido"],
    "808s": ["profundidad", "sustain", "distorsion", "slide"],
    "leads": ["registro", "detune", "forma", "ataque", "duracion"],
    "plucks": ["registro", "duracion", "brillo"],
    "pads": ["registro", "ataque", "release", "detune"],
    "textures": ["densidad", "brillo", "movimiento"],
}


def load_optimized_params(instrument: str) -> tuple:
    """Carga todos los parametros optimizados para un instrumento.
    Retorna (param_names, param_matrix, genres) donde param_matrix es (n_genres, n_params)."""
    files = sorted(OPTIMIZED_DIR.glob(f"{instrument}_*_optimized.json"))
    if not files:
        return None, None, None

    param_names = None
    rows = []
    genres = []

    for f in files:
        with open(f) as fp:
            data = json.load(fp)
        params = data["optimal_params"]
        if param_names is None:
            param_names = sorted(params.keys())
        row = [params[k] for k in param_names]
        rows.append(row)
        genres.append(data["genre"])

    return param_names, np.array(rows), genres


def learn_axes(instrument: str, n_axes: int = None) -> dict:
    """Aprende ejes de mutacion via PCA para un instrumento."""
    param_names, matrix, genres = load_optimized_params(instrument)
    if matrix is None or len(matrix) < 2:
        return None

    n_params = matrix.shape[1]
    n_samples = matrix.shape[0]
    max_axes = min(n_params, n_samples)
    if n_axes is None:
        n_axes = min(max_axes, len(AXIS_SEMANTICS.get(instrument, [])))
    n_axes = min(n_axes, max_axes)

    # Normalizar antes de PCA
    scaler = StandardScaler()
    matrix_scaled = scaler.fit_transform(matrix)

    pca = PCA(n_components=n_axes)
    pca.fit(matrix_scaled)

    # Construir ejes: cada eje es un dict {param_name: loading}
    axes = []
    semantics = AXIS_SEMANTICS.get(instrument, [])

    for i in range(n_axes):
        components = pca.components_[i]
        # Desnormalizar los componentes para que esten en escala original
        # component_original = component_scaled / scale
        components_original = components / scaler.scale_

        axis = {
            "name": semantics[i] if i < len(semantics) else f"eje_{i+1}",
            "variance_explained": float(pca.explained_variance_ratio_[i]),
            "loadings": {},
            "loadings_normalized": {},
        }

        for j, pname in enumerate(param_names):
            if abs(components[j]) > 0.05:  # Solo incluir loadings significativos
                axis["loadings"][pname] = round(float(components_original[j]), 6)
                axis["loadings_normalized"][pname] = round(float(components[j]), 4)

        axes.append(axis)

    # Estadisticas del espacio de parametros
    param_stats = {}
    for j, pname in enumerate(param_names):
        param_stats[pname] = {
            "mean": round(float(scaler.mean_[j]), 6),
            "std": round(float(scaler.scale_[j]), 6),
            "min": round(float(matrix[:, j].min()), 6),
            "max": round(float(matrix[:, j].max()), 6),
        }

    # Correlacion entre parametros
    if n_params > 1:
        corr_matrix = np.corrcoef(matrix.T)
        strong_correlations = []
        for i in range(n_params):
            for j in range(i + 1, n_params):
                r = corr_matrix[i, j]
                if abs(r) > 0.5:
                    strong_correlations.append({
                        "param_a": param_names[i],
                        "param_b": param_names[j],
                        "correlation": round(float(r), 3),
                    })
        strong_correlations.sort(key=lambda x: abs(x["correlation"]), reverse=True)
    else:
        strong_correlations = []

    result = {
        "instrument": instrument,
        "n_genres_analyzed": len(genres),
        "genres": genres,
        "n_axes": n_axes,
        "total_variance_explained": round(float(sum(pca.explained_variance_ratio_)), 4),
        "axes": axes,
        "param_stats": param_stats,
        "strong_correlations": strong_correlations,
    }

    return result


def generate_cpp_mutation_axes(all_results: list) -> str:
    """Genera codigo C++ con los ejes de mutacion para ParameterGenerator.h."""
    lines = [
        "// Auto-generated by 03b_learn_mutation_axes.py",
        "// Mutation axes learned from PCA on optimized parameters",
        "",
        "#pragma once",
        "#include <map>",
        "#include <string>",
        "#include <vector>",
        "",
        "struct MutationAxis {",
        "    std::string name;",
        "    float varianceExplained;",
        "    std::map<std::string, float> loadings;",
        "};",
        "",
        "inline std::map<std::string, std::vector<MutationAxis>> getMutationAxes() {",
        "    std::map<std::string, std::vector<MutationAxis>> axes;",
        "",
    ]

    for result in all_results:
        inst = result["instrument"]
        lines.append(f'    // {inst} ({result["n_axes"]} axes, '
                     f'{result["total_variance_explained"]*100:.1f}% variance)')
        lines.append(f'    axes["{inst}"] = {{')

        for axis in result["axes"]:
            loadings_str = ", ".join(
                f'{{"{k}", {v}f}}'
                for k, v in axis["loadings"].items()
            )
            lines.append(f'        {{"{axis["name"]}", '
                         f'{axis["variance_explained"]:.3f}f, '
                         f'{{{loadings_str}}}}},')

        lines.append("    };")
        lines.append("")

    lines.append("    return axes;")
    lines.append("}")
    lines.append("")

    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(
        description="Aprende ejes de mutacion correlacionados via PCA")
    parser.add_argument("--instrument", type=str, default=None,
                        help="Solo este instrumento")
    parser.add_argument("--n-axes", type=int, default=None,
                        help="Numero de ejes PCA (default: auto)")
    parser.add_argument("--export-cpp", action="store_true",
                        help="Genera MutationAxes.h para C++")
    args = parser.parse_args()

    MUTATION_DIR.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("ONE-SHOT AI — Learn Mutation Axes (PCA)")
    print("=" * 60)

    instruments = [args.instrument] if args.instrument else INSTRUMENTS
    all_results = []

    for inst in instruments:
        print(f"\n--- {inst} ---")
        result = learn_axes(inst, n_axes=args.n_axes)

        if result is None:
            print(f"  [skip] No hay suficientes parametros optimizados para {inst}")
            continue

        # Guardar JSON
        out_path = MUTATION_DIR / f"{inst}_mutation_axes.json"
        with open(out_path, 'w') as f:
            json.dump(result, f, indent=2)

        # Imprimir resumen
        print(f"  Generos analizados: {result['n_genres_analyzed']}")
        print(f"  Ejes encontrados: {result['n_axes']} "
              f"({result['total_variance_explained']*100:.1f}% varianza explicada)")

        for ax in result["axes"]:
            top_loadings = sorted(ax["loadings_normalized"].items(),
                                  key=lambda x: abs(x[1]), reverse=True)[:3]
            top_str = ", ".join(f"{k}={v:+.2f}" for k, v in top_loadings)
            print(f"    {ax['name']}: {ax['variance_explained']*100:.1f}% — {top_str}")

        if result["strong_correlations"]:
            print(f"  Correlaciones fuertes:")
            for c in result["strong_correlations"][:3]:
                print(f"    {c['param_a']} <-> {c['param_b']}: r={c['correlation']}")

        all_results.append(result)

    # Exportar C++
    if args.export_cpp and all_results:
        cpp_code = generate_cpp_mutation_axes(all_results)
        cpp_path = MUTATION_DIR / "MutationAxes.h"
        with open(cpp_path, 'w') as f:
            f.write(cpp_code)
        print(f"\nC++ header exportado: {cpp_path}")

    print(f"\nResultados en: {MUTATION_DIR}")


if __name__ == "__main__":
    main()
