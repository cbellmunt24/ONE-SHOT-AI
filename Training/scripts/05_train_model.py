"""
05_train_model.py — Capa 3: Entrenamiento del modelo ML
Entrena un MLP que predice parametros optimos del sintetizador
dado (instrumento, genero, brillo, cuerpo, textura, movimiento, impacto).

Incluye Feature-Space Morphing: genera ejemplos interpolados entre generos
para que el modelo aprenda transiciones suaves en el espacio de parametros.

Uso:
    python 05_train_model.py
    python 05_train_model.py --epochs 300 --lr 0.001
    python 05_train_model.py --generate-dataset-only
    python 05_train_model.py --no-morphing
    python 05_train_model.py --visualize
"""

import os
import sys
import json
import argparse
import itertools
import numpy as np
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
from sklearn.model_selection import train_test_split

from utils.model import (OneShotParamPredictor, INSTRUMENTS, GENRES,
                          N_INSTRUMENTS, N_GENRES, N_SLIDERS,
                          INPUT_DIM, MAX_OUTPUT_PARAMS)


SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
OPTIMIZED_DIR = TRAINING_DIR / "data" / "optimized_params"
PROFILES_DIR = TRAINING_DIR / "data" / "profiles"
MODELS_DIR = TRAINING_DIR / "data" / "models"
DATASET_DIR = TRAINING_DIR / "data" / "dataset"

# Bounds por instrumento (mismos que 03_optimize_params.py)
PARAM_BOUNDS = {
    "kicks": [("subFreq",30,70),("clickAmount",0,1),("bodyDecay",0.05,0.5),
              ("pitchDrop",10,60),("pitchDropTime",0.01,0.1),
              ("driveAmount",0,0.5),("subLevel",0.3,1),("tailLength",0.05,0.6)],
    "snares": [("bodyFreq",120,280),("noiseAmount",0.2,0.9),
               ("bodyDecay",0.03,0.2),("noiseDecay",0.05,0.25),
               ("snapAmount",0,0.8),("noiseColor",0,1),("wireAmount",0,1)],
    "hihats": [("decay",0.01,0.3),("tone",0,1),
               ("freqRange",200,800),("metallic",0,1),
               ("noiseColor",0,1),("ringAmount",0,1)],
    "claps": [("decay",0.05,0.4),("spread",0,1),
              ("numLayers",2,8),("noiseColor",0,1),
              ("thickness",0,1),("transientSnap",0,1)],
    "percs": [("freq",100,2000),("decay",0.02,0.3),("noiseMix",0,0.8),
              ("pitchDrop",0,24),("woodiness",0,1),("metallic",0,1)],
    "808s": [("freq",25,65),("sustain",0.1,1.5),("distortion",0,0.8),("slide",0,1),
             ("reeseMix",0,1),("filterEnvAmt",0,1),("punchiness",0,1)],
    "leads": [("freq",200,1000),("detune",0,0.05),("pulseWidth",0.1,0.9),
              ("attack",0.001,0.1),("decay",0.1,1),
              ("brightness",0,1),("vibratoRate",0.5,8)],
    "plucks": [("freq",150,800),("decay",0.05,0.5),("brightness",0.1,0.9),
               ("damping",0,1),("bodyResonance",0,1),("fmAmount",0,1)],
    "pads": [("freq",80,500),("attack",0.05,1),("release",0.1,2),("detune",0,0.03),
             ("warmth",0,1),("chorusAmount",0,1),("filterSweep",0,1),
             ("evolutionRate",0,1)],
    "textures": [("density",0.1,0.9),("brightness",0.1,0.9),("movement",0,0.8),
                 ("grainSize",0.005,0.1),("noiseColor",0,1),("spectralTilt",-1,1),
                 ("pitchedness",0,1),("pitch",0,1)],
}


def encode_input(instrument_idx: int, genre_idx: int,
                 brillo: float, cuerpo: float, textura: float,
                 movimiento: float, impacto: float) -> np.ndarray:
    """Codifica input como vector de 24 dims."""
    vec = np.zeros(INPUT_DIM)
    vec[instrument_idx] = 1.0
    vec[N_INSTRUMENTS + genre_idx] = 1.0
    vec[N_INSTRUMENTS + N_GENRES + 0] = brillo
    vec[N_INSTRUMENTS + N_GENRES + 1] = cuerpo
    vec[N_INSTRUMENTS + N_GENRES + 2] = textura
    vec[N_INSTRUMENTS + N_GENRES + 3] = movimiento
    vec[N_INSTRUMENTS + N_GENRES + 4] = impacto
    return vec


def normalize_params(params: dict, bounds: list) -> np.ndarray:
    """Normaliza parametros a 0..1 segun sus bounds."""
    normalized = np.zeros(MAX_OUTPUT_PARAMS)
    for i, (name, lo, hi) in enumerate(bounds):
        if name in params and i < MAX_OUTPUT_PARAMS:
            normalized[i] = (params[name] - lo) / max(hi - lo, 1e-10)
    return normalized


def _get_bounds(param_name: str, bounds: list) -> tuple:
    for name, lo, hi in bounds:
        if name == param_name:
            return lo, hi
    return 0.0, 1.0


def modulate_params(base_params: dict, bounds: list,
                    brillo: float, cuerpo: float, textura: float,
                    movimiento: float, impacto: float) -> dict:
    """
    Modula parametros base segun valores de sliders.
    Mapeo simplificado — se refinara con datos reales.
    """
    params = dict(base_params)

    # Brillo afecta filtros y contenido HF
    for key in ["filterCutoff", "tone", "brightness", "noiseColor",
                "freqRange", "spectralTilt"]:
        if key in params:
            lo, hi = _get_bounds(key, bounds)
            params[key] = np.clip(
                params[key] + (brillo - 0.5) * (hi - lo) * 0.4, lo, hi)

    # Cuerpo afecta sub, decay, warmth y body resonance
    for key in ["subLevel", "subFreq", "bodyDecay", "sustain",
                "warmth", "bodyResonance", "thickness", "reeseMix"]:
        if key in params:
            lo, hi = _get_bounds(key, bounds)
            params[key] = np.clip(
                params[key] + (cuerpo - 0.5) * (hi - lo) * 0.3, lo, hi)

    # Textura afecta noise, grains, metallic content y pitchedness
    for key in ["noiseAmount", "noiseMix", "density", "grainSize",
                "metallic", "woodiness", "chorusAmount", "pitchedness"]:
        if key in params:
            lo, hi = _get_bounds(key, bounds)
            params[key] = np.clip(
                params[key] + (textura - 0.5) * (hi - lo) * 0.4, lo, hi)

    # Movimiento afecta modulacion, detune, vibrato, sweep y evolution
    for key in ["detune", "movement", "pitchDrop", "vibratoRate",
                "filterSweep", "ringAmount", "slide", "evolutionRate"]:
        if key in params:
            lo, hi = _get_bounds(key, bounds)
            params[key] = np.clip(
                params[key] + (movimiento - 0.5) * (hi - lo) * 0.3, lo, hi)

    # Impacto afecta transients, drive, punch y FM
    for key in ["clickAmount", "snapAmount", "driveAmount", "distortion",
                "transientSnap", "punchiness", "filterEnvAmt", "wireAmount",
                "damping", "fmAmount"]:
        if key in params:
            lo, hi = _get_bounds(key, bounds)
            params[key] = np.clip(
                params[key] + (impacto - 0.5) * (hi - lo) * 0.4, lo, hi)

    return params


def generate_dataset(slider_steps: int = 5, morphing: bool = True) -> tuple:
    """
    Genera dataset de entrenamiento interpolando sliders.
    Si morphing=True, tambien genera ejemplos interpolados entre pares de
    generos (Feature-Space Morphing) para transiciones suaves.
    Retorna (X, Y) como numpy arrays.
    """
    slider_values = np.linspace(0.0, 1.0, slider_steps)

    X_list = []
    Y_list = []

    # Cargar parametros optimizados base
    optimized = {}
    for f in OPTIMIZED_DIR.glob("*_optimized.json"):
        if f.name.startswith("_"):
            continue
        with open(f) as fp:
            data = json.load(fp)
        key = (data["instrument"], data["genre"])
        optimized[key] = data["optimal_params"]

    if not optimized:
        print("[!] No hay parametros optimizados. Ejecuta 03 primero.")
        return None, None

    total = 0
    for inst_idx, inst in enumerate(INSTRUMENTS):
        bounds = PARAM_BOUNDS.get(inst)
        if bounds is None:
            continue

        for genre_idx, genre in enumerate(GENRES):
            base_key = (inst, genre)
            if base_key not in optimized:
                continue

            base_params = optimized[base_key]
            print(f"  Generando: {inst}/{genre}...")

            for brillo in slider_values:
                for cuerpo in slider_values:
                    for textura in slider_values:
                        for movimiento in slider_values:
                            for impacto in slider_values:
                                modulated = modulate_params(
                                    base_params, bounds,
                                    brillo, cuerpo, textura,
                                    movimiento, impacto
                                )

                                x = encode_input(inst_idx, genre_idx,
                                                 brillo, cuerpo, textura,
                                                 movimiento, impacto)
                                y = normalize_params(modulated, bounds)

                                X_list.append(x)
                                Y_list.append(y)
                                total += 1

    print(f"Dataset base generado: {total} filas")

    # --- Feature-Space Morphing: cross-genre interpolation ---
    morphing_count = 0
    if morphing:
        alphas = [0.25, 0.5, 0.75]
        print("\n  Generando interpolaciones cross-genre (morphing)...")

        for inst_idx, inst in enumerate(INSTRUMENTS):
            bounds = PARAM_BOUNDS.get(inst)
            if bounds is None:
                continue

            # Collect genres that have optimized params for this instrument
            available_genres = []
            for genre_idx, genre in enumerate(GENRES):
                if (inst, genre) in optimized:
                    available_genres.append((genre_idx, genre))

            # For each pair of available genres
            for (g_idx_a, genre_a), (g_idx_b, genre_b) in itertools.combinations(available_genres, 2):
                params_a = optimized[(inst, genre_a)]
                params_b = optimized[(inst, genre_b)]

                # Normalize both param sets to arrays
                norm_a = normalize_params(params_a, bounds)
                norm_b = normalize_params(params_b, bounds)

                for alpha in alphas:
                    # Interpolate TARGET params
                    params_interp = alpha * norm_a + (1.0 - alpha) * norm_b

                    # Generate slider combinations (use a coarser grid for morphing)
                    morph_slider_values = np.linspace(0.0, 1.0, max(slider_steps - 1, 3))

                    for brillo in morph_slider_values:
                        for cuerpo in morph_slider_values:
                            for textura in morph_slider_values:
                                for movimiento in morph_slider_values:
                                    for impacto in morph_slider_values:
                                        # Blended genre encoding
                                        x = np.zeros(INPUT_DIM)
                                        x[inst_idx] = 1.0
                                        x[N_INSTRUMENTS + g_idx_a] = alpha
                                        x[N_INSTRUMENTS + g_idx_b] = 1.0 - alpha
                                        x[N_INSTRUMENTS + N_GENRES + 0] = brillo
                                        x[N_INSTRUMENTS + N_GENRES + 1] = cuerpo
                                        x[N_INSTRUMENTS + N_GENRES + 2] = textura
                                        x[N_INSTRUMENTS + N_GENRES + 3] = movimiento
                                        x[N_INSTRUMENTS + N_GENRES + 4] = impacto

                                        # Modulate interpolated params with sliders
                                        # Re-denormalize to dict, modulate, re-normalize
                                        interp_dict = {}
                                        for i, (name, lo, hi) in enumerate(bounds):
                                            if i < MAX_OUTPUT_PARAMS:
                                                interp_dict[name] = lo + params_interp[i] * (hi - lo)

                                        modulated = modulate_params(
                                            interp_dict, bounds,
                                            brillo, cuerpo, textura,
                                            movimiento, impacto
                                        )
                                        y = normalize_params(modulated, bounds)

                                        X_list.append(x)
                                        Y_list.append(y)
                                        morphing_count += 1

        print(f"  Interpolaciones morphing anadidas: {morphing_count} filas")

    total += morphing_count
    print(f"Dataset total: {total} filas")
    return np.array(X_list), np.array(Y_list)


def train_model(X: np.ndarray, Y: np.ndarray,
                epochs: int = 200, lr: float = 1e-3,
                batch_size: int = 256) -> nn.Module:
    """Entrena el modelo MLP."""
    X_train, X_val, Y_train, Y_val = train_test_split(
        X, Y, test_size=0.2, random_state=42)

    train_ds = TensorDataset(torch.FloatTensor(X_train), torch.FloatTensor(Y_train))
    val_ds = TensorDataset(torch.FloatTensor(X_val), torch.FloatTensor(Y_val))
    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=batch_size)

    model = OneShotParamPredictor()
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=lr)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=10,
                                                      factor=0.5)

    best_val_loss = float('inf')
    patience_counter = 0
    patience_limit = 25
    best_state = None

    print(f"\nEntrenando modelo: {sum(p.numel() for p in model.parameters())} parametros")
    print(f"Train: {len(X_train)}, Val: {len(X_val)}")
    print(f"Epochs: {epochs}, LR: {lr}, Batch: {batch_size}")
    print()

    for epoch in range(epochs):
        model.train()
        train_loss = 0.0
        for xb, yb in train_loader:
            optimizer.zero_grad()
            pred = model(xb)
            loss = criterion(pred, yb)
            loss.backward()
            optimizer.step()
            train_loss += loss.item() * xb.size(0)
        train_loss /= len(X_train)

        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for xb, yb in val_loader:
                pred = model(xb)
                loss = criterion(pred, yb)
                val_loss += loss.item() * xb.size(0)
        val_loss /= len(X_val)

        scheduler.step(val_loss)

        if (epoch + 1) % 10 == 0 or epoch == 0:
            print(f"  Epoch {epoch+1:4d}/{epochs}  "
                  f"train_loss={train_loss:.6f}  val_loss={val_loss:.6f}  "
                  f"lr={optimizer.param_groups[0]['lr']:.2e}")

        if val_loss < best_val_loss:
            best_val_loss = val_loss
            best_state = model.state_dict().copy()
            patience_counter = 0
        else:
            patience_counter += 1
            if patience_counter >= patience_limit:
                print(f"\n  Early stopping en epoch {epoch+1}")
                break

    if best_state is not None:
        model.load_state_dict(best_state)
    print(f"\n  Mejor val_loss: {best_val_loss:.6f}")

    return model


def main():
    parser = argparse.ArgumentParser(
        description="Entrena modelo ML de prediccion de parametros")
    parser.add_argument("--epochs", type=int, default=200)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--batch-size", type=int, default=256)
    parser.add_argument("--slider-steps", type=int, default=5,
                        help="Pasos por slider (5=3125 combos por inst/genero)")
    parser.add_argument("--generate-dataset-only", action="store_true")
    parser.add_argument("--no-morphing", action="store_true",
                        help="Desactivar Feature-Space Morphing (activado por defecto)")
    parser.add_argument("--visualize", action="store_true",
                        help="Generar mapa PCA 2D del espacio de parametros")
    args = parser.parse_args()

    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    DATASET_DIR.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("ONE-SHOT AI — Train Model (Capa 3)")
    print("=" * 60)

    dataset_path = DATASET_DIR / "training_data.npz"

    if dataset_path.exists() and not args.generate_dataset_only:
        print(f"Cargando dataset existente: {dataset_path}")
        data = np.load(dataset_path)
        X, Y = data["X"], data["Y"]
    else:
        print("Generando dataset...")
        X, Y = generate_dataset(slider_steps=args.slider_steps,
                                morphing=not args.no_morphing)
        if X is None:
            return
        np.savez(dataset_path, X=X, Y=Y)
        print(f"Dataset guardado: {dataset_path}")

    if args.generate_dataset_only:
        print("Dataset generado. Saliendo.")
        return

    print(f"\nDataset: X={X.shape}, Y={Y.shape}")

    model = train_model(X, Y, epochs=args.epochs, lr=args.lr,
                        batch_size=args.batch_size)

    model_path = MODELS_DIR / "oneshot_param_predictor.pt"
    torch.save({
        "model_state_dict": model.state_dict(),
        "input_dim": INPUT_DIM,
        "output_dim": MAX_OUTPUT_PARAMS,
        "instruments": INSTRUMENTS,
        "genres": GENRES,
    }, model_path)
    print(f"\nModelo guardado: {model_path}")

    # --- Visualization: PCA map of feature space ---
    if args.visualize:
        print("\nGenerando mapa PCA del espacio de parametros...")
        try:
            import matplotlib
            matplotlib.use("Agg")
            import matplotlib.pyplot as plt
            from sklearn.decomposition import PCA

            # Load all optimized params
            all_params = []
            all_genres = []
            all_labels = []

            for f in OPTIMIZED_DIR.glob("*_optimized.json"):
                if f.name.startswith("_"):
                    continue
                with open(f) as fp:
                    data = json.load(fp)
                inst = data["instrument"]
                genre = data["genre"]
                bounds = PARAM_BOUNDS.get(inst)
                if bounds is None:
                    continue
                norm = normalize_params(data["optimal_params"], bounds)
                all_params.append(norm)
                all_genres.append(genre)
                all_labels.append(f"{inst}/{genre}")

            if len(all_params) >= 2:
                param_matrix = np.array(all_params)
                pca = PCA(n_components=2)
                projected = pca.fit_transform(param_matrix)

                # Color by genre
                unique_genres = sorted(set(all_genres))
                cmap = plt.cm.get_cmap("tab10", len(unique_genres))
                genre_to_color = {g: cmap(i) for i, g in enumerate(unique_genres)}

                fig, ax = plt.subplots(figsize=(12, 8))
                for i, (x_pt, y_pt) in enumerate(projected):
                    color = genre_to_color[all_genres[i]]
                    ax.scatter(x_pt, y_pt, c=[color], s=60, edgecolors="k",
                               linewidths=0.5, zorder=3)
                    ax.annotate(all_labels[i], (x_pt, y_pt),
                                fontsize=6, alpha=0.7,
                                xytext=(4, 4), textcoords="offset points")

                # Legend
                for genre, color in genre_to_color.items():
                    ax.scatter([], [], c=[color], label=genre, s=60,
                               edgecolors="k", linewidths=0.5)
                ax.legend(loc="best", fontsize=8)

                ax.set_xlabel(f"PC1 ({pca.explained_variance_ratio_[0]:.1%} var)")
                ax.set_ylabel(f"PC2 ({pca.explained_variance_ratio_[1]:.1%} var)")
                ax.set_title("ONE-SHOT AI — Feature Space Map (PCA)")
                ax.grid(True, alpha=0.3)

                viz_path = MODELS_DIR / "feature_space_map.png"
                fig.savefig(viz_path, dpi=150, bbox_inches="tight")
                plt.close(fig)
                print(f"  Mapa guardado: {viz_path}")
            else:
                print("  [!] Se necesitan al menos 2 puntos para PCA.")

        except ImportError as e:
            print(f"  [!] No se pudo generar visualizacion: {e}")
            print("      Instala matplotlib: pip install matplotlib")

    print("Para exportar a ONNX: python 06_export_onnx.py")


if __name__ == "__main__":
    main()
