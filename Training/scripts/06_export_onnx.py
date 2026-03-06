"""
06_export_onnx.py — Capa 3: Exportar modelo entrenado a ONNX
Convierte el modelo PyTorch a formato ONNX para cargarlo en C++ con ONNX Runtime.

Uso:
    python 06_export_onnx.py
    python 06_export_onnx.py --verify
"""

import os
import sys
import argparse
import numpy as np
from pathlib import Path

import torch
import onnx
import onnxruntime as ort

sys.path.insert(0, os.path.dirname(__file__))
from utils.model import OneShotParamPredictor, INPUT_DIM, MAX_OUTPUT_PARAMS


SCRIPT_DIR = Path(__file__).parent
TRAINING_DIR = SCRIPT_DIR.parent
MODELS_DIR = TRAINING_DIR / "data" / "models"


def load_trained_model() -> torch.nn.Module:
    """Carga el modelo entrenado desde .pt."""
    model_path = MODELS_DIR / "oneshot_param_predictor.pt"
    if not model_path.exists():
        raise FileNotFoundError(
            f"No se encontro modelo en {model_path}. "
            "Ejecuta primero: python 05_train_model.py")

    checkpoint = torch.load(model_path, map_location="cpu", weights_only=True)
    model = OneShotParamPredictor(
        input_dim=checkpoint.get("input_dim", INPUT_DIM),
        output_dim=checkpoint.get("output_dim", MAX_OUTPUT_PARAMS),
    )
    model.load_state_dict(checkpoint["model_state_dict"])
    model.eval()
    return model


def export_onnx(model: torch.nn.Module, output_path: Path):
    """Exporta modelo a ONNX."""
    dummy_input = torch.randn(1, INPUT_DIM)

    torch.onnx.export(
        model,
        dummy_input,
        str(output_path),
        input_names=["controls"],
        output_names=["params"],
        opset_version=11,
        dynamic_axes={
            "controls": {0: "batch_size"},
            "params": {0: "batch_size"},
        },
    )

    onnx_model = onnx.load(str(output_path))
    onnx.checker.check_model(onnx_model)

    file_size = output_path.stat().st_size
    print(f"Modelo ONNX exportado: {output_path}")
    print(f"Tamano: {file_size / 1024:.1f} KB")


def verify_onnx(model: torch.nn.Module, onnx_path: Path,
                n_tests: int = 100):
    """Verifica que ONNX produce mismos resultados que PyTorch."""
    session = ort.InferenceSession(str(onnx_path))

    max_diff = 0.0
    for _ in range(n_tests):
        x = torch.randn(1, INPUT_DIM)

        with torch.no_grad():
            pt_output = model(x).numpy()

        ort_output = session.run(None, {"controls": x.numpy()})[0]

        diff = np.max(np.abs(pt_output - ort_output))
        max_diff = max(max_diff, diff)

    print(f"\nVerificacion ONNX vs PyTorch ({n_tests} tests):")
    print(f"  Max diferencia: {max_diff:.2e}")
    if max_diff < 1e-5:
        print("  [OK] Resultados identicos")
    else:
        print("  [WARN] Diferencia significativa — revisar exportacion")


def main():
    parser = argparse.ArgumentParser(
        description="Exporta modelo entrenado a formato ONNX")
    parser.add_argument("--verify", action="store_true",
                        help="Verificar consistencia ONNX vs PyTorch")
    parser.add_argument("--output", type=str, default=None,
                        help="Ruta de salida del .onnx")
    args = parser.parse_args()

    print("=" * 60)
    print("ONE-SHOT AI — Export ONNX (Capa 3)")
    print("=" * 60)

    model = load_trained_model()
    n_params = sum(p.numel() for p in model.parameters())
    print(f"Modelo cargado: {n_params} parametros")

    if args.output:
        onnx_path = Path(args.output)
    else:
        onnx_path = MODELS_DIR / "oneshot_param_predictor.onnx"

    export_onnx(model, onnx_path)

    if args.verify:
        verify_onnx(model, onnx_path)

    print(f"\nPara integrar en el plugin C++:")
    print(f"  1. Copiar {onnx_path.name} a los recursos del plugin")
    print(f"  2. Usar onnxruntime C API para cargar e inferir")
    print(f"  3. Ver MLParameterGenerator.h en PLAN_ML_TRAINING.md")


if __name__ == "__main__":
    main()
