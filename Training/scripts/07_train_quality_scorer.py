"""
07_train_quality_scorer.py — Entrena un modelo que puntua la calidad perceptual

Training data:
  - Positivos (score=1.0): features de samples reales de las librerias
  - Negativos (score=0.0): features de audio generado con parametros aleatorios
  - Intermedios (score=0.5): features de audio con parametros semi-optimizados

Modelo: MLP pequeno (features -> 32 -> 16 -> 1 sigmoid)
Output: score 0.0 (mala calidad) a 1.0 (calidad profesional)

Uso:
    python 07_train_quality_scorer.py
    python 07_train_quality_scorer.py --epochs 100
    python 07_train_quality_scorer.py --generate-dataset-only
    python 07_train_quality_scorer.py --export-onnx
"""

import os
import sys
import json
import argparse
import numpy as np
from pathlib import Path

sys.path.insert(0, os.path.dirname(__file__))

import torch
import torch.nn as nn
import torch.optim as optim
from torch.utils.data import DataLoader, TensorDataset
from sklearn.model_selection import train_test_split

from utils.audio_features import extract_all_features, SAMPLE_RATE
from utils.synth_bridge import SynthBridge

SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
FEATURES_DIR = TRAINING_DIR / "data" / "features"
OPTIMIZED_DIR = TRAINING_DIR / "data" / "optimized_params"
MODELS_DIR = TRAINING_DIR / "data" / "models"
DATASET_DIR = TRAINING_DIR / "data" / "dataset"

# Features escalares que se usan como input del scorer
SCORER_FEATURES = [
    "attack_time_ms", "decay_time_ms", "total_duration_ms",
    "peak_amplitude", "crest_factor", "transient_sharpness",
    "spectral_centroid", "spectral_rolloff_85", "spectral_bandwidth",
    "spectral_flatness", "spectral_tilt",
    "fundamental_freq", "harmonic_ratio",
    "pitch_drop_semitones", "pitch_drop_time_ms",
    "brightness_index", "warmth_index", "noise_ratio",
    "stereo_width", "zero_crossing_rate",
]
# + 13 MFCCs = 33 features total
N_SCORER_FEATURES = len(SCORER_FEATURES) + 13

INSTRUMENTS = ["kicks", "snares", "hihats", "claps", "percs",
               "808s", "leads", "plucks", "pads", "textures"]

SYNTH_DURATION = {
    "kicks": 300, "snares": 250, "hihats": 150, "claps": 300,
    "percs": 200, "808s": 500, "leads": 400, "plucks": 300,
    "pads": 1000, "textures": 1000,
}

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


class QualityScorer(nn.Module):
    """MLP pequeno para puntuar calidad perceptual de audio."""
    def __init__(self, input_dim=N_SCORER_FEATURES):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, 32),
            nn.ReLU(),
            nn.Dropout(0.15),
            nn.Linear(32, 16),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(16, 1),
            nn.Sigmoid(),
        )

    def forward(self, x):
        return self.net(x).squeeze(-1)


def features_to_vector(features: dict) -> np.ndarray:
    """Convierte dict de features a vector numerico para el scorer."""
    vec = []
    for key in SCORER_FEATURES:
        val = features.get(key, 0.0)
        if val is None or (isinstance(val, float) and np.isnan(val)):
            val = 0.0
        vec.append(float(val))

    # Anadir MFCCs
    mfcc = features.get("mfcc", [0.0] * 13)
    if len(mfcc) < 13:
        mfcc = mfcc + [0.0] * (13 - len(mfcc))
    vec.extend(mfcc[:13])

    return np.array(vec, dtype=np.float32)


def load_real_features() -> list:
    """Carga features de samples reales (positivos, score=1.0)."""
    vectors = []
    for feat_dir in sorted(FEATURES_DIR.iterdir()):
        if not feat_dir.is_dir() or feat_dir.name.startswith("_"):
            continue
        for json_file in feat_dir.glob("*.json"):
            try:
                with open(json_file) as f:
                    data = json.load(f)
                if "error" in data:
                    continue
                vec = features_to_vector(data)
                if not np.any(np.isnan(vec)):
                    vectors.append(vec)
            except Exception:
                continue
    return vectors


def generate_random_features(synth: SynthBridge, n_per_instrument: int = 50) -> list:
    """Genera features con parametros aleatorios (negativos, score=0.0)."""
    import soundfile as sf
    import tempfile

    vectors = []
    temp_path = Path(tempfile.mktemp(suffix=".wav"))

    for inst in INSTRUMENTS:
        bounds = PARAM_BOUNDS.get(inst)
        if bounds is None:
            continue

        duration = SYNTH_DURATION.get(inst, 300)

        for _ in range(n_per_instrument):
            # Parametros completamente aleatorios dentro de bounds
            params = {}
            for name, lo, hi in bounds:
                params[name] = np.random.uniform(lo, hi)

            try:
                audio = synth.generate(inst, params, duration)
                sf.write(str(temp_path), audio, SAMPLE_RATE)
                features = extract_all_features(str(temp_path))
                if "error" not in features:
                    vec = features_to_vector(features)
                    if not np.any(np.isnan(vec)):
                        vectors.append(vec)
            except Exception:
                continue

    if temp_path.exists():
        temp_path.unlink()

    return vectors


def generate_semi_optimized_features(synth: SynthBridge,
                                      n_per_combo: int = 10) -> list:
    """Genera features con parametros semi-optimizados (intermedios, score=0.5).
    Toma parametros optimizados y les anade ruido moderado."""
    import soundfile as sf
    import tempfile

    vectors = []
    temp_path = Path(tempfile.mktemp(suffix=".wav"))

    for opt_file in OPTIMIZED_DIR.glob("*_optimized.json"):
        if opt_file.name.startswith("_"):
            continue
        with open(opt_file) as f:
            data = json.load(f)

        inst = data["instrument"]
        bounds = PARAM_BOUNDS.get(inst)
        if bounds is None:
            continue

        base_params = data["optimal_params"]
        duration = SYNTH_DURATION.get(inst, 300)

        for _ in range(n_per_combo):
            # Anadir jitter del 10-30% a los parametros optimizados
            params = {}
            for name, lo, hi in bounds:
                if name in base_params:
                    noise = np.random.uniform(-0.2, 0.2) * (hi - lo)
                    params[name] = np.clip(base_params[name] + noise, lo, hi)
                else:
                    params[name] = np.random.uniform(lo, hi)

            try:
                audio = synth.generate(inst, params, duration)
                sf.write(str(temp_path), audio, SAMPLE_RATE)
                features = extract_all_features(str(temp_path))
                if "error" not in features:
                    vec = features_to_vector(features)
                    if not np.any(np.isnan(vec)):
                        vectors.append(vec)
            except Exception:
                continue

    if temp_path.exists():
        temp_path.unlink()

    return vectors


def generate_dataset() -> tuple:
    """Genera dataset completo para el quality scorer."""
    print("  Cargando features reales (positivos)...")
    real_vecs = load_real_features()
    print(f"    {len(real_vecs)} samples reales cargados")

    if not real_vecs:
        print("[!] No hay features extraidas. Ejecuta 01 primero.")
        return None, None

    synth = SynthBridge()

    print("  Generando features aleatorias (negativos)...")
    random_vecs = generate_random_features(synth, n_per_instrument=50)
    print(f"    {len(random_vecs)} samples aleatorios generados")

    print("  Generando features semi-optimizadas (intermedios)...")
    semi_vecs = generate_semi_optimized_features(synth, n_per_combo=10)
    print(f"    {len(semi_vecs)} samples semi-optimizados generados")

    # Construir X, Y
    X_list = []
    Y_list = []

    for vec in real_vecs:
        X_list.append(vec)
        Y_list.append(1.0)

    for vec in random_vecs:
        X_list.append(vec)
        Y_list.append(0.0)

    for vec in semi_vecs:
        X_list.append(vec)
        Y_list.append(0.5)

    X = np.array(X_list, dtype=np.float32)
    Y = np.array(Y_list, dtype=np.float32)

    # Normalizar features (guardar stats para inference)
    feat_mean = X.mean(axis=0)
    feat_std = X.std(axis=0)
    feat_std[feat_std < 1e-8] = 1.0
    X = (X - feat_mean) / feat_std

    print(f"\n  Dataset total: {len(X)} samples "
          f"(real={len(real_vecs)}, random={len(random_vecs)}, semi={len(semi_vecs)})")

    return X, Y, feat_mean, feat_std


def train_scorer(X: np.ndarray, Y: np.ndarray,
                 epochs: int = 100, lr: float = 1e-3,
                 batch_size: int = 128) -> nn.Module:
    """Entrena el quality scorer."""
    X_train, X_val, Y_train, Y_val = train_test_split(
        X, Y, test_size=0.2, random_state=42, stratify=(Y > 0.25).astype(int))

    train_ds = TensorDataset(torch.FloatTensor(X_train), torch.FloatTensor(Y_train))
    val_ds = TensorDataset(torch.FloatTensor(X_val), torch.FloatTensor(Y_val))
    train_loader = DataLoader(train_ds, batch_size=batch_size, shuffle=True)
    val_loader = DataLoader(val_ds, batch_size=batch_size)

    model = QualityScorer(input_dim=X.shape[1])
    criterion = nn.MSELoss()
    optimizer = optim.Adam(model.parameters(), lr=lr)
    scheduler = optim.lr_scheduler.ReduceLROnPlateau(optimizer, patience=8, factor=0.5)

    best_val_loss = float('inf')
    patience_counter = 0
    patience_limit = 20
    best_state = None

    n_params = sum(p.numel() for p in model.parameters())
    print(f"\nEntrenando Quality Scorer: {n_params} parametros")
    print(f"Train: {len(X_train)}, Val: {len(X_val)}")
    print(f"Epochs: {epochs}, LR: {lr}\n")

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
                  f"train={train_loss:.6f}  val={val_loss:.6f}  "
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

    # Evaluar accuracy
    model.eval()
    with torch.no_grad():
        val_pred = model(torch.FloatTensor(X_val)).numpy()
        # Accuracy: real (>0.7) vs bad (<0.3)
        real_mask = Y_val > 0.7
        bad_mask = Y_val < 0.3
        if real_mask.sum() > 0:
            real_acc = (val_pred[real_mask] > 0.5).mean()
            print(f"  Accuracy (reales detectados como buenos): {real_acc*100:.1f}%")
        if bad_mask.sum() > 0:
            bad_acc = (val_pred[bad_mask] < 0.5).mean()
            print(f"  Accuracy (aleatorios detectados como malos): {bad_acc*100:.1f}%")

    return model


def main():
    parser = argparse.ArgumentParser(
        description="Entrena quality scorer para evaluar calidad de audio generado")
    parser.add_argument("--epochs", type=int, default=100)
    parser.add_argument("--lr", type=float, default=1e-3)
    parser.add_argument("--batch-size", type=int, default=128)
    parser.add_argument("--generate-dataset-only", action="store_true")
    parser.add_argument("--export-onnx", action="store_true",
                        help="Exporta modelo a ONNX")
    args = parser.parse_args()

    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    DATASET_DIR.mkdir(parents=True, exist_ok=True)

    print("=" * 60)
    print("ONE-SHOT AI — Train Quality Scorer")
    print("=" * 60)

    dataset_path = DATASET_DIR / "quality_scorer_data.npz"

    if dataset_path.exists() and not args.generate_dataset_only:
        print(f"Cargando dataset existente: {dataset_path}")
        data = np.load(dataset_path)
        X, Y = data["X"], data["Y"]
        feat_mean, feat_std = data["feat_mean"], data["feat_std"]
    else:
        print("Generando dataset...")
        result = generate_dataset()
        if result[0] is None:
            return
        X, Y, feat_mean, feat_std = result
        np.savez(dataset_path, X=X, Y=Y, feat_mean=feat_mean, feat_std=feat_std)
        print(f"Dataset guardado: {dataset_path}")

    if args.generate_dataset_only:
        print("Dataset generado. Saliendo.")
        return

    print(f"\nDataset: X={X.shape}, Y={Y.shape}")
    print(f"Distribucion: score=0: {(Y<0.25).sum()}, "
          f"score=0.5: {((Y>=0.25)&(Y<=0.75)).sum()}, "
          f"score=1: {(Y>0.75).sum()}")

    model = train_scorer(X, Y, epochs=args.epochs, lr=args.lr,
                         batch_size=args.batch_size)

    # Guardar modelo
    model_path = MODELS_DIR / "quality_scorer.pt"
    torch.save({
        "model_state_dict": model.state_dict(),
        "input_dim": X.shape[1],
        "feature_names": SCORER_FEATURES + [f"mfcc_{i}" for i in range(13)],
        "feat_mean": feat_mean.tolist(),
        "feat_std": feat_std.tolist(),
    }, model_path)
    print(f"\nModelo guardado: {model_path}")

    # Exportar ONNX
    if args.export_onnx:
        try:
            dummy = torch.randn(1, X.shape[1])
            onnx_path = MODELS_DIR / "quality_scorer.onnx"
            torch.onnx.export(
                model, dummy, str(onnx_path),
                input_names=["features"],
                output_names=["quality_score"],
                opset_version=11,
            )
            print(f"ONNX exportado: {onnx_path}")

            # Verificar
            import onnxruntime as ort
            sess = ort.InferenceSession(str(onnx_path))
            onnx_out = sess.run(None, {"features": dummy.numpy()})
            torch_out = model(dummy).detach().numpy()
            diff = np.abs(onnx_out[0].flatten() - torch_out.flatten()).max()
            print(f"Verificacion ONNX: max diff = {diff:.8f} {'OK' if diff < 1e-5 else 'WARN'}")
        except Exception as e:
            print(f"[warn] Error exportando ONNX: {e}")


if __name__ == "__main__":
    main()
