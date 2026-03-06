"""
model.py — Definicion del modelo MLP para ONE-SHOT AI
Compartido entre 05_train_model.py y 06_export_onnx.py
"""

import torch
import torch.nn as nn


INSTRUMENTS = ["kicks", "snares", "hihats", "claps", "percs",
               "808s", "leads", "plucks", "pads", "textures"]
GENRES = ["trap", "hiphop", "techno", "house", "reggaeton",
          "afrobeat", "rnb", "edm", "ambient"]

N_INSTRUMENTS = len(INSTRUMENTS)
N_GENRES = len(GENRES)
N_SLIDERS = 5  # brillo, cuerpo, textura, movimiento, impacto
INPUT_DIM = N_INSTRUMENTS + N_GENRES + N_SLIDERS  # 24
MAX_OUTPUT_PARAMS = 35


class OneShotParamPredictor(nn.Module):
    def __init__(self, input_dim=INPUT_DIM, hidden1=128, hidden2=64,
                 output_dim=MAX_OUTPUT_PARAMS):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(input_dim, hidden1),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(hidden1, hidden2),
            nn.ReLU(),
            nn.Dropout(0.1),
            nn.Linear(hidden2, output_dim),
            nn.Sigmoid()
        )

    def forward(self, x):
        return self.net(x)
