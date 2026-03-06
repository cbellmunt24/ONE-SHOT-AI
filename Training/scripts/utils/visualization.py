"""
visualization.py — Graficas comparativas para ONE-SHOT AI Training
"""

import os
import json
import numpy as np
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')  # Backend no-interactivo para guardar PNGs


INSTRUMENTS = ["kicks", "snares", "hihats", "claps", "percs",
               "808s", "leads", "plucks", "pads", "textures"]

GENRES = ["trap", "hiphop", "techno", "house", "reggaeton",
          "afrobeat", "rnb", "edm", "ambient"]

# Features principales para comparativas (excluye arrays como rms_envelope y mfcc)
KEY_FEATURES = [
    "attack_time_ms", "decay_time_ms", "total_duration_ms",
    "crest_factor", "transient_sharpness",
    "spectral_centroid", "spectral_rolloff_85", "spectral_bandwidth",
    "spectral_flatness", "spectral_tilt",
    "fundamental_freq", "harmonic_ratio",
    "brightness_index", "warmth_index", "noise_ratio",
    "zero_crossing_rate",
    "pitch_drop_semitones",
]


def plot_genre_comparison(profiles_dir: str, instrument: str,
                          output_dir: str):
    """
    Genera graficas comparativas de un instrumento entre todos los generos.
    Una grafica por feature con barras por genero.
    """
    os.makedirs(output_dir, exist_ok=True)

    genre_data = {}
    for genre in GENRES:
        profile_path = os.path.join(profiles_dir,
                                     f"{instrument}_{genre}_profile.json")
        if os.path.exists(profile_path):
            with open(profile_path, 'r') as f:
                profile = json.load(f)
            genre_data[genre] = profile["profile"]

    if len(genre_data) < 2:
        print(f"  [skip] {instrument}: menos de 2 generos con perfil")
        return

    available_genres = list(genre_data.keys())

    for feature in KEY_FEATURES:
        means = []
        stds = []
        labels = []
        for genre in available_genres:
            if feature in genre_data[genre]:
                means.append(genre_data[genre][feature]["mean"])
                stds.append(genre_data[genre][feature]["std"])
                labels.append(genre)

        if len(labels) < 2:
            continue

        fig, ax = plt.subplots(figsize=(10, 5))
        x = np.arange(len(labels))
        bars = ax.bar(x, means, yerr=stds, capsize=4,
                      color=plt.cm.Set3(np.linspace(0, 1, len(labels))),
                      edgecolor='black', linewidth=0.5)
        ax.set_xticks(x)
        ax.set_xticklabels(labels, rotation=45, ha='right')
        ax.set_ylabel(feature)
        ax.set_title(f"{instrument} — {feature} por genero")
        ax.grid(axis='y', alpha=0.3)
        plt.tight_layout()

        fig_path = os.path.join(output_dir,
                                f"{instrument}_{feature}.png")
        fig.savefig(fig_path, dpi=100)
        plt.close(fig)

    print(f"  [ok] {instrument}: graficas guardadas en {output_dir}")


def plot_instrument_comparison(profiles_dir: str, genre: str,
                               output_dir: str):
    """
    Compara todos los instrumentos dentro de un mismo genero.
    """
    os.makedirs(output_dir, exist_ok=True)

    inst_data = {}
    for inst in INSTRUMENTS:
        profile_path = os.path.join(profiles_dir,
                                     f"{inst}_{genre}_profile.json")
        if os.path.exists(profile_path):
            with open(profile_path, 'r') as f:
                profile = json.load(f)
            inst_data[inst] = profile["profile"]

    if len(inst_data) < 2:
        return

    available_insts = list(inst_data.keys())

    for feature in KEY_FEATURES:
        means = []
        labels = []
        for inst in available_insts:
            if feature in inst_data[inst]:
                means.append(inst_data[inst][feature]["mean"])
                labels.append(inst)

        if len(labels) < 2:
            continue

        fig, ax = plt.subplots(figsize=(10, 5))
        x = np.arange(len(labels))
        ax.bar(x, means,
               color=plt.cm.Set2(np.linspace(0, 1, len(labels))),
               edgecolor='black', linewidth=0.5)
        ax.set_xticks(x)
        ax.set_xticklabels(labels, rotation=45, ha='right')
        ax.set_ylabel(feature)
        ax.set_title(f"{genre} — {feature} por instrumento")
        ax.grid(axis='y', alpha=0.3)
        plt.tight_layout()

        fig_path = os.path.join(output_dir,
                                f"{genre}_{feature}.png")
        fig.savefig(fig_path, dpi=100)
        plt.close(fig)


def plot_rms_envelopes(profiles_dir: str, instrument: str,
                       output_dir: str):
    """
    Superpone las envolventes RMS medias de un instrumento por genero.
    """
    os.makedirs(output_dir, exist_ok=True)

    fig, ax = plt.subplots(figsize=(12, 5))
    found = False

    for genre in GENRES:
        profile_path = os.path.join(profiles_dir,
                                     f"{instrument}_{genre}_profile.json")
        if not os.path.exists(profile_path):
            continue
        with open(profile_path, 'r') as f:
            profile = json.load(f)

        if "rms_envelope_mean" in profile.get("profile", {}):
            env = profile["profile"]["rms_envelope_mean"]
            time_ms = np.arange(len(env)) * 5.0  # 5ms frames
            ax.plot(time_ms, env, label=genre, linewidth=1.5)
            found = True

    if found:
        ax.set_xlabel("Tiempo (ms)")
        ax.set_ylabel("RMS")
        ax.set_title(f"{instrument} — Envolvente RMS media por genero")
        ax.legend()
        ax.grid(alpha=0.3)
        plt.tight_layout()
        fig.savefig(os.path.join(output_dir,
                                  f"{instrument}_rms_envelopes.png"), dpi=100)
    plt.close(fig)


def generate_all_plots(profiles_dir: str, output_dir: str):
    """Genera todas las graficas comparativas."""
    print("Generando graficas comparativas...")

    # Por instrumento (compara generos)
    for inst in INSTRUMENTS:
        plot_genre_comparison(profiles_dir, inst,
                              os.path.join(output_dir, "by_instrument"))

    # Por genero (compara instrumentos)
    for genre in GENRES:
        plot_instrument_comparison(profiles_dir, genre,
                                   os.path.join(output_dir, "by_genre"))

    # Envolventes RMS
    for inst in INSTRUMENTS:
        plot_rms_envelopes(profiles_dir, inst,
                           os.path.join(output_dir, "envelopes"))

    print("Graficas completadas.")
