#pragma once

#include "../Params/InstrumentParams.h"
#include "../Params/SynthEnums.h"
#include "../Effects/EffectParams.h"

// Reglas musicales por género para cada instrumento.
// Cada función devuelve un perfil base de parámetros
// que luego el ParameterGenerator modula con carácter, impacto, energía y randomización.
//
// VALORES CALIBRADOS POR ML — generados por 04_export_genre_rules.py (2026-03-09)
// Calibrado contra 86 combinaciones instrumento/género reales.

namespace genrerules
{

// ============================================================
// KICK — ML-calibrated
// ============================================================
inline KickParams kickBase (GenreStyle genre)
{
    KickParams p;
    p.base.volume    = 0.85f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.001f;
    p.base.oscType   = OscillatorType::Sine;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 8000.0f;
    p.base.filterResonance = 0.1f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.subFreq = 61.252779f;  p.clickAmount = 0.447151f;  p.bodyDecay = 0.051566f;
            p.pitchDrop = 52.93434f; p.pitchDropTime = 0.056627f;
            p.driveAmount = 0.497015f; p.subLevel = 0.987909f; p.tailLength = 0.069821f;
            break;
        case GenreStyle::HipHop:
            p.subFreq = 59.98911f;  p.clickAmount = 0.312211f;  p.bodyDecay = 0.050226f;
            p.pitchDrop = 58.557915f; p.pitchDropTime = 0.053997f;
            p.driveAmount = 0.390901f; p.subLevel = 0.798468f; p.tailLength = 0.057389f;
            break;
        case GenreStyle::Techno:
            p.subFreq = 42.220957f;  p.clickAmount = 0.78762f;  p.bodyDecay = 0.050245f;
            p.pitchDrop = 52.860438f; p.pitchDropTime = 0.070909f;
            p.driveAmount = 0.295452f; p.subLevel = 0.406878f; p.tailLength = 0.087049f;
            break;
        case GenreStyle::House:
            p.subFreq = 34.13188f;  p.clickAmount = 0.021665f;  p.bodyDecay = 0.050084f;
            p.pitchDrop = 59.503204f; p.pitchDropTime = 0.041729f;
            p.driveAmount = 0.493777f; p.subLevel = 0.994695f; p.tailLength = 0.063599f;
            break;
        case GenreStyle::Reggaeton:
            p.subFreq = 32.467962f;  p.clickAmount = 0.940643f;  p.bodyDecay = 0.05011f;
            p.pitchDrop = 46.189253f; p.pitchDropTime = 0.022703f;
            p.driveAmount = 0.420973f; p.subLevel = 0.56373f; p.tailLength = 0.067322f;
            p.base.filterCutoff = 10000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.subFreq = 39.728581f;  p.clickAmount = 0.078636f;  p.bodyDecay = 0.051481f;
            p.pitchDrop = 59.396796f; p.pitchDropTime = 0.056168f;
            p.driveAmount = 0.463313f; p.subLevel = 0.547602f; p.tailLength = 0.067803f;
            break;
        case GenreStyle::RnB:
            p.subFreq = 63.950675f;  p.clickAmount = 0.551495f;  p.bodyDecay = 0.050733f;
            p.pitchDrop = 54.361073f; p.pitchDropTime = 0.049023f;
            p.driveAmount = 0.212818f; p.subLevel = 0.956535f; p.tailLength = 0.064868f;
            break;
        case GenreStyle::EDM:
            p.subFreq = 62.016925f;  p.clickAmount = 0.960749f;  p.bodyDecay = 0.097631f;
            p.pitchDrop = 53.690025f; p.pitchDropTime = 0.069986f;
            p.driveAmount = 0.494495f; p.subLevel = 0.937248f; p.tailLength = 0.170576f;
            break;
        case GenreStyle::Ambient:
            p.subFreq = 44.0f;  p.clickAmount = 0.05f;  p.bodyDecay = 0.32f;
            p.pitchDrop = 18.0f; p.pitchDropTime = 0.065f;
            p.driveAmount = 0.00f; p.subLevel = 0.55f; p.tailLength = 0.45f;
            p.base.filterCutoff = 4000.0f;
            break;
    }
    return p;
}

// ============================================================
// SNARE — ML-calibrated
// ============================================================
inline SnareParams snareBase (GenreStyle genre)
{
    SnareParams p;
    p.base.volume  = 0.80f;
    p.base.pan     = 0.5f;
    p.base.attack  = 0.001f;
    p.base.oscType = OscillatorType::Sine;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 12000.0f;
    p.base.filterResonance = 0.05f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.bodyFreq = 263.749892f; p.bodyDecay = 0.083141f;
            p.noiseDecay = 0.094073f; p.noiseColor = 0.560071f;
            p.snapAmount = 0.038715f; p.ringFreq = 340.0f;
            p.ringAmount = 0.15f; p.wireAmount = 0.404946f;
            p.noiseTightness = 0.55f; p.bodyMix = 0.35f;
            p.noiseLP = 7000.0f;
            break;
        case GenreStyle::HipHop:
            p.bodyFreq = 277.092663f; p.bodyDecay = 0.052912f;
            p.noiseDecay = 0.058809f; p.noiseColor = 0.007201f;
            p.snapAmount = 0.632244f; p.ringFreq = 340.0f;
            p.ringAmount = 0.12f; p.wireAmount = 0.132881f;
            p.noiseTightness = 0.50f; p.bodyMix = 0.30f;
            p.noiseLP = 6500.0f;
            break;
        case GenreStyle::Techno:
            p.bodyFreq = 236.570929f; p.bodyDecay = 0.032483f;
            p.noiseDecay = 0.052063f; p.noiseColor = 0.016155f;
            p.snapAmount = 0.771333f; p.ringFreq = 380.0f;
            p.ringAmount = 0.20f; p.wireAmount = 0.255737f;
            p.noiseTightness = 0.65f; p.bodyMix = 0.35f;
            p.noiseLP = 8000.0f;
            break;
        case GenreStyle::House:
            p.bodyFreq = 279.339405f; p.bodyDecay = 0.156872f;
            p.noiseDecay = 0.052426f; p.noiseColor = 0.691611f;
            p.snapAmount = 0.587697f; p.ringFreq = 350.0f;
            p.ringAmount = 0.12f; p.wireAmount = 0.487813f;
            p.noiseTightness = 0.55f; p.bodyMix = 0.30f;
            p.noiseLP = 7500.0f;
            break;
        case GenreStyle::Reggaeton:
            p.bodyFreq = 215.077607f; p.bodyDecay = 0.132835f;
            p.noiseDecay = 0.050791f; p.noiseColor = 0.298405f;
            p.snapAmount = 0.732569f; p.ringFreq = 340.0f;
            p.ringAmount = 0.10f; p.wireAmount = 0.569848f;
            p.noiseTightness = 0.70f; p.bodyMix = 0.35f;
            p.noiseLP = 8000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.bodyFreq = 276.114141f; p.bodyDecay = 0.046409f;
            p.noiseDecay = 0.050564f; p.noiseColor = 0.194888f;
            p.snapAmount = 0.618631f; p.ringFreq = 340.0f;
            p.ringAmount = 0.10f; p.wireAmount = 0.112958f;
            p.noiseTightness = 0.55f; p.bodyMix = 0.30f;
            p.noiseLP = 6500.0f;
            break;
        case GenreStyle::RnB:
            p.bodyFreq = 263.749892f; p.bodyDecay = 0.083141f;
            p.noiseDecay = 0.094073f; p.noiseColor = 0.560071f;
            p.snapAmount = 0.038715f; p.ringFreq = 320.0f;
            p.ringAmount = 0.15f; p.wireAmount = 0.404946f;
            p.noiseTightness = 0.45f; p.bodyMix = 0.35f;
            p.noiseLP = 6000.0f;
            break;
        case GenreStyle::EDM:
            p.bodyFreq = 279.359609f; p.bodyDecay = 0.192921f;
            p.noiseDecay = 0.051042f; p.noiseColor = 0.625066f;
            p.snapAmount = 0.081707f; p.ringFreq = 380.0f;
            p.ringAmount = 0.18f; p.wireAmount = 0.349356f;
            p.noiseTightness = 0.60f; p.bodyMix = 0.30f;
            p.noiseLP = 8500.0f;
            break;
        case GenreStyle::Ambient:
            p.bodyFreq = 273.153244f; p.bodyDecay = 0.160993f;
            p.noiseDecay = 0.050259f; p.noiseColor = 0.919017f;
            p.snapAmount = 0.72668f; p.ringFreq = 300.0f;
            p.ringAmount = 0.10f; p.wireAmount = 0.07645f;
            p.noiseTightness = 0.40f; p.bodyMix = 0.35f;
            p.noiseLP = 5500.0f;
            p.base.filterCutoff = 8000.0f;
            break;
    }
    return p;
}

// ============================================================
// HI-HAT — ML-calibrated
// ============================================================
inline HiHatParams hiHatBase (GenreStyle genre)
{
    HiHatParams p;
    p.base.volume    = 0.70f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.001f;
    p.base.oscType   = OscillatorType::Noise;
    p.base.filterType     = FilterType::HighPass;
    p.base.filterCutoff   = 6000.0f;
    p.base.filterResonance = 0.1f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.freqRange = 331.339875f * 10.0f; p.metallic = 0.449295f;
            p.noiseColor = 0.185151f; p.openDecay = 0.30f;
            p.closedDecay = 0.070898f; p.openAmount = 0.35f;
            p.ringAmount = 0.900095f; p.highPassFreq = 5500.0f;
            break;
        case GenreStyle::HipHop:
            p.freqRange = 331.339875f * 10.0f; p.metallic = 0.449295f;
            p.noiseColor = 0.185151f; p.openDecay = 0.30f;
            p.closedDecay = 0.070898f; p.openAmount = 0.35f;
            p.ringAmount = 0.900095f; p.highPassFreq = 5000.0f;
            break;
        case GenreStyle::Techno:
            p.freqRange = 551.885672f * 10.0f; p.metallic = 0.417723f;
            p.noiseColor = 0.98732f; p.openDecay = 0.25f;
            p.closedDecay = 0.01008f; p.openAmount = 0.30f;
            p.ringAmount = 0.56568f; p.highPassFreq = 6000.0f;
            break;
        case GenreStyle::House:
            p.freqRange = 331.339875f * 10.0f; p.metallic = 0.449295f;
            p.noiseColor = 0.185151f; p.openDecay = 0.30f;
            p.closedDecay = 0.070898f; p.openAmount = 0.35f;
            p.ringAmount = 0.900095f; p.highPassFreq = 5500.0f;
            break;
        case GenreStyle::Reggaeton:
            p.freqRange = 776.144806f * 10.0f; p.metallic = 0.97052f;
            p.noiseColor = 0.036211f; p.openDecay = 0.20f;
            p.closedDecay = 0.010009f; p.openAmount = 0.25f;
            p.ringAmount = 0.277866f; p.highPassFreq = 6000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.freqRange = 688.761217f * 10.0f; p.metallic = 0.97068f;
            p.noiseColor = 0.02489f; p.openDecay = 0.25f;
            p.closedDecay = 0.01036f; p.openAmount = 0.30f;
            p.ringAmount = 0.304621f; p.highPassFreq = 5500.0f;
            break;
        case GenreStyle::RnB:
            p.freqRange = 335.171363f * 10.0f; p.metallic = 0.258759f;
            p.noiseColor = 0.783555f; p.openDecay = 0.30f;
            p.closedDecay = 0.010435f; p.openAmount = 0.35f;
            p.ringAmount = 0.169118f; p.highPassFreq = 5000.0f;
            break;
        case GenreStyle::EDM:
            p.freqRange = 602.538231f * 10.0f; p.metallic = 0.129087f;
            p.noiseColor = 0.995567f; p.openDecay = 0.22f;
            p.closedDecay = 0.010716f; p.openAmount = 0.30f;
            p.ringAmount = 0.15146f; p.highPassFreq = 6000.0f;
            break;
        case GenreStyle::Ambient:
            p.freqRange = 791.522522f * 10.0f; p.metallic = 0.242891f;
            p.noiseColor = 0.964417f; p.openDecay = 0.40f;
            p.closedDecay = 0.013951f; p.openAmount = 0.45f;
            p.ringAmount = 0.395243f; p.highPassFreq = 4500.0f;
            p.base.filterCutoff = 5000.0f;
            break;
    }
    return p;
}

// ============================================================
// CLAP — ML-calibrated
// ============================================================
inline ClapParams clapBase (GenreStyle genre)
{
    ClapParams p;
    p.base.volume    = 0.75f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.001f;
    p.base.oscType   = OscillatorType::Noise;
    p.base.filterType     = FilterType::BandPass;
    p.base.filterCutoff   = 2500.0f;
    p.base.filterResonance = 0.15f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.numLayers = 4.080555f; p.layerSpacing = 0.012f;
            p.noiseDecay = 0.058486f; p.noiseColor = 0.996108f;
            p.toneAmount = 0.15f; p.toneFreq = 1000.0f;
            p.reverbAmount = 0.20f; p.thickness = 0.250375f;
            p.noiseTightness = 0.55f; p.noiseLP = 6000.0f;
            p.transientSnap = 0.367751f;
            break;
        case GenreStyle::HipHop:
            p.numLayers = 3.650233f; p.layerSpacing = 0.012f;
            p.noiseDecay = 0.05085f; p.noiseColor = 0.275365f;
            p.toneAmount = 0.15f; p.toneFreq = 1000.0f;
            p.reverbAmount = 0.15f; p.thickness = 0.28781f;
            p.noiseTightness = 0.55f; p.noiseLP = 6000.0f;
            p.transientSnap = 0.410997f;
            break;
        case GenreStyle::Techno:
            p.numLayers = 5.752215f; p.layerSpacing = 0.010f;
            p.noiseDecay = 0.050026f; p.noiseColor = 0.390251f;
            p.toneAmount = 0.12f; p.toneFreq = 1100.0f;
            p.reverbAmount = 0.18f; p.thickness = 0.12045f;
            p.noiseTightness = 0.60f; p.noiseLP = 7000.0f;
            p.transientSnap = 0.66526f;
            break;
        case GenreStyle::House:
            p.numLayers = 5.855968f; p.layerSpacing = 0.010f;
            p.noiseDecay = 0.050949f; p.noiseColor = 0.915077f;
            p.toneAmount = 0.12f; p.toneFreq = 1000.0f;
            p.reverbAmount = 0.25f; p.thickness = 0.290299f;
            p.noiseTightness = 0.50f; p.noiseLP = 7000.0f;
            p.transientSnap = 0.436991f;
            break;
        case GenreStyle::Reggaeton:
            p.numLayers = 4.218191f; p.layerSpacing = 0.008f;
            p.noiseDecay = 0.050332f; p.noiseColor = 0.236928f;
            p.toneAmount = 0.18f; p.toneFreq = 1100.0f;
            p.reverbAmount = 0.12f; p.thickness = 0.274205f;
            p.noiseTightness = 0.65f; p.noiseLP = 7500.0f;
            p.transientSnap = 0.382913f;
            break;
        case GenreStyle::Afrobeat:
            p.numLayers = 4.902383f; p.layerSpacing = 0.012f;
            p.noiseDecay = 0.050045f; p.noiseColor = 0.748869f;
            p.toneAmount = 0.12f; p.toneFreq = 1000.0f;
            p.reverbAmount = 0.18f; p.thickness = 0.282423f;
            p.noiseTightness = 0.55f; p.noiseLP = 6500.0f;
            p.transientSnap = 0.545603f;
            break;
        case GenreStyle::RnB:
            p.numLayers = 5.164185f; p.layerSpacing = 0.010f;
            p.noiseDecay = 0.051927f; p.noiseColor = 0.98181f;
            p.toneAmount = 0.10f; p.toneFreq = 950.0f;
            p.reverbAmount = 0.22f; p.thickness = 0.264466f;
            p.noiseTightness = 0.50f; p.noiseLP = 6000.0f;
            p.transientSnap = 0.913814f;
            break;
        case GenreStyle::EDM:
            p.numLayers = 4.908215f; p.layerSpacing = 0.010f;
            p.noiseDecay = 0.053887f; p.noiseColor = 0.91292f;
            p.toneAmount = 0.15f; p.toneFreq = 1100.0f;
            p.reverbAmount = 0.18f; p.thickness = 0.278583f;
            p.noiseTightness = 0.55f; p.noiseLP = 7500.0f;
            p.transientSnap = 0.782092f;
            break;
        case GenreStyle::Ambient:
            p.numLayers = 5.972815f; p.layerSpacing = 0.015f;
            p.noiseDecay = 0.069025f; p.noiseColor = 0.995764f;
            p.toneAmount = 0.08f; p.toneFreq = 900.0f;
            p.reverbAmount = 0.35f; p.thickness = 0.553892f;
            p.noiseTightness = 0.40f; p.noiseLP = 5500.0f;
            p.transientSnap = 0.403767f;
            break;
    }
    return p;
}

// ============================================================
// PERC — ML-calibrated
// ============================================================
inline PercParams percBase (GenreStyle genre)
{
    PercParams p;
    p.base.volume    = 0.75f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.001f;
    p.base.oscType   = OscillatorType::Triangle;
    p.base.filterType     = FilterType::BandPass;
    p.base.filterCutoff   = 3000.0f;
    p.base.filterResonance = 0.15f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.freq = 498.989293f; p.toneDecay = 0.032799f;
            p.metallic = 0.900095f; p.woodiness = 0.185151f;
            p.membrane = 0.30f; p.clickAmount = 0.35f;
            p.pitchDrop = 10.78307f; p.harmonicRatio = 1.5f;
            break;
        case GenreStyle::HipHop:
            p.freq = 1969.894209f; p.toneDecay = 0.028437f;
            p.metallic = 0.527854f; p.woodiness = 0.542276f;
            p.membrane = 0.35f; p.clickAmount = 0.30f;
            p.pitchDrop = 1.007049f; p.harmonicRatio = 1.4f;
            break;
        case GenreStyle::Techno:
            p.freq = 861.623657f; p.toneDecay = 0.066419f;
            p.metallic = 0.98401f; p.woodiness = 0.754815f;
            p.membrane = 0.25f; p.clickAmount = 0.30f;
            p.pitchDrop = 21.544663f; p.harmonicRatio = 1.8f;
            break;
        case GenreStyle::House:
            p.freq = 498.989293f; p.toneDecay = 0.032799f;
            p.metallic = 0.900095f; p.woodiness = 0.185151f;
            p.membrane = 0.30f; p.clickAmount = 0.30f;
            p.pitchDrop = 10.78307f; p.harmonicRatio = 1.5f;
            break;
        case GenreStyle::Reggaeton:
            p.freq = 510.621202f; p.toneDecay = 0.020816f;
            p.metallic = 0.174861f; p.woodiness = 0.72443f;
            p.membrane = 0.40f; p.clickAmount = 0.45f;
            p.pitchDrop = 6.260927f; p.harmonicRatio = 1.3f;
            break;
        case GenreStyle::Afrobeat:
            p.freq = 1985.388007f; p.toneDecay = 0.020858f;
            p.metallic = 0.292398f; p.woodiness = 0.95503f;
            p.membrane = 0.45f; p.clickAmount = 0.25f;
            p.pitchDrop = 12.690399f; p.harmonicRatio = 1.4f;
            break;
        case GenreStyle::RnB:
            p.freq = 498.989293f; p.toneDecay = 0.032799f;
            p.metallic = 0.900095f; p.woodiness = 0.185151f;
            p.membrane = 0.30f; p.clickAmount = 0.25f;
            p.pitchDrop = 10.78307f; p.harmonicRatio = 1.5f;
            break;
        case GenreStyle::EDM:
            p.freq = 498.989293f; p.toneDecay = 0.032799f;
            p.metallic = 0.900095f; p.woodiness = 0.185151f;
            p.membrane = 0.25f; p.clickAmount = 0.35f;
            p.pitchDrop = 10.78307f; p.harmonicRatio = 1.6f;
            break;
        case GenreStyle::Ambient:
            p.freq = 516.855915f; p.toneDecay = 0.054854f;
            p.metallic = 0.823319f; p.woodiness = 0.317614f;
            p.membrane = 0.30f; p.clickAmount = 0.15f;
            p.pitchDrop = 21.286696f; p.harmonicRatio = 1.5f;
            break;
    }
    return p;
}

// ============================================================
// BASS 808 — ML-calibrated
// ============================================================
inline Bass808Params bass808Base (GenreStyle genre)
{
    Bass808Params p;
    p.base.volume    = 0.85f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.001f;
    p.base.oscType   = OscillatorType::Sine;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 3000.0f;
    p.base.filterResonance = 0.15f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.subFreq = 35.789477f; p.sustainLevel = 0.160008f;
            p.glideTime = 0.05f; p.glideAmount = 3.0f;
            p.saturation = 0.0f; p.subOctave = 0.15f;
            p.harmonics = 0.15f; p.tailLength = 0.80f;
            p.reeseMix = 0.084322f; p.reeseDetune = 0.10f;
            p.punchiness = 0.869966f; p.filterEnvAmt = 0.033123f;
            break;
        case GenreStyle::HipHop:
            p.subFreq = 35.486493f; p.sustainLevel = 0.222954f;
            p.glideTime = 0.04f; p.glideAmount = 2.0f;
            p.saturation = 0.002077f; p.subOctave = 0.18f;
            p.harmonics = 0.12f; p.tailLength = 0.75f;
            p.reeseMix = 0.019379f; p.reeseDetune = 0.08f;
            p.punchiness = 0.819628f; p.filterEnvAmt = 0.01873f;
            break;
        case GenreStyle::Techno:
            p.subFreq = 64.203066f; p.sustainLevel = 0.100427f;
            p.glideTime = 0.02f; p.glideAmount = 1.0f;
            p.saturation = 0.351621f; p.subOctave = 0.10f;
            p.harmonics = 0.20f; p.tailLength = 0.50f;
            p.reeseMix = 0.148084f; p.reeseDetune = 0.15f;
            p.punchiness = 0.004997f; p.filterEnvAmt = 0.007323f;
            break;
        case GenreStyle::House:
            p.subFreq = 33.930342f; p.sustainLevel = 0.102527f;
            p.glideTime = 0.03f; p.glideAmount = 1.5f;
            p.saturation = 0.001799f; p.subOctave = 0.15f;
            p.harmonics = 0.10f; p.tailLength = 0.60f;
            p.reeseMix = 0.04029f; p.reeseDetune = 0.08f;
            p.punchiness = 0.965425f; p.filterEnvAmt = 0.055913f;
            break;
        case GenreStyle::Reggaeton:
            p.subFreq = 34.323921f; p.sustainLevel = 0.193862f;
            p.glideTime = 0.06f; p.glideAmount = 4.0f;
            p.saturation = 0.001118f; p.subOctave = 0.15f;
            p.harmonics = 0.12f; p.tailLength = 0.75f;
            p.reeseMix = 0.070475f; p.reeseDetune = 0.10f;
            p.punchiness = 0.875439f; p.filterEnvAmt = 0.090075f;
            break;
        case GenreStyle::Afrobeat:
            p.subFreq = 64.504942f; p.sustainLevel = 0.100022f;
            p.glideTime = 0.03f; p.glideAmount = 2.0f;
            p.saturation = 0.000008f; p.subOctave = 0.12f;
            p.harmonics = 0.10f; p.tailLength = 0.55f;
            p.reeseMix = 0.030195f; p.reeseDetune = 0.06f;
            p.punchiness = 0.999954f; p.filterEnvAmt = 0.01024f;
            break;
        case GenreStyle::RnB:
            p.subFreq = 42.933034f; p.sustainLevel = 0.101083f;
            p.glideTime = 0.04f; p.glideAmount = 2.0f;
            p.saturation = 0.62825f; p.subOctave = 0.15f;
            p.harmonics = 0.18f; p.tailLength = 0.70f;
            p.reeseMix = 0.028881f; p.reeseDetune = 0.10f;
            p.punchiness = 0.432593f; p.filterEnvAmt = 0.169882f;
            break;
        case GenreStyle::EDM:
            p.subFreq = 34.122864f; p.sustainLevel = 0.11087f;
            p.glideTime = 0.04f; p.glideAmount = 3.0f;
            p.saturation = 0.370432f; p.subOctave = 0.12f;
            p.harmonics = 0.22f; p.tailLength = 0.60f;
            p.reeseMix = 0.187499f; p.reeseDetune = 0.15f;
            p.punchiness = 0.697991f; p.filterEnvAmt = 0.709649f;
            break;
        case GenreStyle::Ambient:
            p.subFreq = 35.547441f; p.sustainLevel = 0.100632f;
            p.glideTime = 0.05f; p.glideAmount = 2.0f;
            p.saturation = 0.7804f; p.subOctave = 0.20f;
            p.harmonics = 0.15f; p.tailLength = 1.0f;
            p.reeseMix = 0.202146f; p.reeseDetune = 0.20f;
            p.punchiness = 0.002988f; p.filterEnvAmt = 0.777612f;
            p.base.filterCutoff = 1500.0f;
            break;
    }
    return p;
}

// ============================================================
// LEAD — ML-calibrated
// ============================================================
inline LeadParams leadBase (GenreStyle genre)
{
    LeadParams p;
    p.base.volume    = 0.75f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.005f;
    p.base.oscType   = OscillatorType::Saw;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 8000.0f;
    p.base.filterResonance = 0.2f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.oscDetune = 0.003506f; p.pulseWidth = 0.336564f;
            p.vibratoRate = 0.976825f; p.vibratoDepth = 0.08f;
            p.brightness = 0.981696f; p.waveformMix = 0.30f;
            p.unisonVoices = 3.0f; p.portamento = 0.02f;
            p.filterEnvAmt = 0.20f; p.subOscLevel = 0.10f;
            p.base.attack = 0.001414f;
            break;
        case GenreStyle::HipHop:
            p.oscDetune = 0.044696f; p.pulseWidth = 0.32479f;
            p.vibratoRate = 5.455955f; p.vibratoDepth = 0.08f;
            p.brightness = 0.969615f; p.waveformMix = 0.35f;
            p.unisonVoices = 3.0f; p.portamento = 0.02f;
            p.filterEnvAmt = 0.15f; p.subOscLevel = 0.12f;
            p.base.attack = 0.001f;
            break;
        case GenreStyle::Techno:
            p.oscDetune = 0.003697f; p.pulseWidth = 0.665588f;
            p.vibratoRate = 4.129925f; p.vibratoDepth = 0.05f;
            p.brightness = 0.004087f; p.waveformMix = 0.50f;
            p.unisonVoices = 2.0f; p.portamento = 0.01f;
            p.filterEnvAmt = 0.25f; p.subOscLevel = 0.08f;
            p.base.attack = 0.001111f;
            break;
        case GenreStyle::House:
            p.oscDetune = 0.003728f; p.pulseWidth = 0.398802f;
            p.vibratoRate = 0.958115f; p.vibratoDepth = 0.06f;
            p.brightness = 0.012202f; p.waveformMix = 0.40f;
            p.unisonVoices = 3.0f; p.portamento = 0.02f;
            p.filterEnvAmt = 0.18f; p.subOscLevel = 0.10f;
            p.base.attack = 0.003719f;
            break;
        case GenreStyle::Reggaeton:
            p.oscDetune = 0.044826f; p.pulseWidth = 0.663125f;
            p.vibratoRate = 6.034424f; p.vibratoDepth = 0.06f;
            p.brightness = 0.984114f; p.waveformMix = 0.35f;
            p.unisonVoices = 3.0f; p.portamento = 0.02f;
            p.filterEnvAmt = 0.20f; p.subOscLevel = 0.10f;
            p.base.attack = 0.001156f;
            break;
        case GenreStyle::Afrobeat:
            p.oscDetune = 0.040877f; p.pulseWidth = 0.482552f;
            p.vibratoRate = 6.191592f; p.vibratoDepth = 0.08f;
            p.brightness = 0.979307f; p.waveformMix = 0.35f;
            p.unisonVoices = 2.0f; p.portamento = 0.01f;
            p.filterEnvAmt = 0.15f; p.subOscLevel = 0.10f;
            p.base.attack = 0.001f;
            break;
        case GenreStyle::RnB:
            p.oscDetune = 0.049283f; p.pulseWidth = 0.553603f;
            p.vibratoRate = 0.672607f; p.vibratoDepth = 0.10f;
            p.brightness = 0.524738f; p.waveformMix = 0.40f;
            p.unisonVoices = 3.0f; p.portamento = 0.03f;
            p.filterEnvAmt = 0.15f; p.subOscLevel = 0.15f;
            p.base.attack = 0.0073f;
            break;
        case GenreStyle::EDM:
            p.oscDetune = 0.002676f; p.pulseWidth = 0.348328f;
            p.vibratoRate = 2.739574f; p.vibratoDepth = 0.05f;
            p.brightness = 0.178743f; p.waveformMix = 0.30f;
            p.unisonVoices = 4.0f; p.portamento = 0.01f;
            p.filterEnvAmt = 0.30f; p.subOscLevel = 0.08f;
            p.base.attack = 0.028973f;
            break;
        case GenreStyle::Ambient:
            p.oscDetune = 0.005035f; p.pulseWidth = 0.494188f;
            p.vibratoRate = 2.543297f; p.vibratoDepth = 0.12f;
            p.brightness = 0.986034f; p.waveformMix = 0.55f;
            p.unisonVoices = 4.0f; p.portamento = 0.04f;
            p.filterEnvAmt = 0.12f; p.subOscLevel = 0.15f;
            p.base.attack = 0.001003f;
            p.base.filterCutoff = 5000.0f;
            break;
    }
    return p;
}

// ============================================================
// PLUCK — ML-calibrated
// ============================================================
inline PluckParams pluckBase (GenreStyle genre)
{
    PluckParams p;
    p.base.volume    = 0.75f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.002f;
    p.base.oscType   = OscillatorType::Triangle;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 10000.0f;
    p.base.filterResonance = 0.2f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.brightness = 0.898883f; p.bodyResonance = 0.98797f;
            p.decayTime = 0.05004f * 10.0f; p.damping = 0.99871f;
            p.pickPosition = 0.50f; p.stringTension = 0.55f;
            p.harmonics = 0.20f; p.stereoWidth = 0.40f;
            break;
        case GenreStyle::HipHop:
            p.brightness = 0.851292f; p.bodyResonance = 0.948573f;
            p.decayTime = 0.050113f * 10.0f; p.damping = 0.988851f;
            p.pickPosition = 0.45f; p.stringTension = 0.50f;
            p.harmonics = 0.20f; p.stereoWidth = 0.35f;
            break;
        case GenreStyle::Techno:
            p.brightness = 0.855135f; p.bodyResonance = 0.993698f;
            p.decayTime = 0.05023f * 10.0f; p.damping = 0.988351f;
            p.pickPosition = 0.55f; p.stringTension = 0.60f;
            p.harmonics = 0.25f; p.stereoWidth = 0.40f;
            break;
        case GenreStyle::House:
            p.brightness = 0.895147f; p.bodyResonance = 0.967222f;
            p.decayTime = 0.050023f * 10.0f; p.damping = 0.99829f;
            p.pickPosition = 0.45f; p.stringTension = 0.50f;
            p.harmonics = 0.18f; p.stereoWidth = 0.40f;
            break;
        case GenreStyle::Reggaeton:
            p.brightness = 0.848197f; p.bodyResonance = 0.975222f;
            p.decayTime = 0.05002f * 10.0f; p.damping = 0.994298f;
            p.pickPosition = 0.50f; p.stringTension = 0.55f;
            p.harmonics = 0.18f; p.stereoWidth = 0.35f;
            break;
        case GenreStyle::Afrobeat:
            p.brightness = 0.791777f; p.bodyResonance = 0.837952f;
            p.decayTime = 0.050001f * 10.0f; p.damping = 0.990358f;
            p.pickPosition = 0.50f; p.stringTension = 0.50f;
            p.harmonics = 0.22f; p.stereoWidth = 0.35f;
            break;
        case GenreStyle::RnB:
            p.brightness = 0.650407f; p.bodyResonance = 1.0f;
            p.decayTime = 0.05f * 10.0f; p.damping = 1.0f;
            p.pickPosition = 0.40f; p.stringTension = 0.45f;
            p.harmonics = 0.15f; p.stereoWidth = 0.40f;
            break;
        case GenreStyle::EDM:
            p.brightness = 0.697481f; p.bodyResonance = 0.946964f;
            p.decayTime = 0.050063f * 10.0f; p.damping = 0.974657f;
            p.pickPosition = 0.55f; p.stringTension = 0.60f;
            p.harmonics = 0.25f; p.stereoWidth = 0.45f;
            break;
        case GenreStyle::Ambient:
            p.brightness = 0.899797f; p.bodyResonance = 0.9314f;
            p.decayTime = 0.050097f * 10.0f; p.damping = 0.978148f;
            p.pickPosition = 0.45f; p.stringTension = 0.45f;
            p.harmonics = 0.20f; p.stereoWidth = 0.50f;
            p.base.filterCutoff = 7000.0f;
            break;
    }
    return p;
}

// ============================================================
// PAD — ML-calibrated
// ============================================================
inline PadParams padBase (GenreStyle genre)
{
    PadParams p;
    p.base.volume    = 0.70f;
    p.base.pan       = 0.5f;
    p.base.oscType   = OscillatorType::Saw;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 5000.0f;
    p.base.filterResonance = 0.12f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.unisonVoices = 5.0f;  p.detuneSpread = 0.026865f;
            p.driftRate = 0.20f;    p.warmth = 0.997406f;
            p.evolutionRate = 0.08f; p.filterSweep = 0.94756f;
            p.stereoWidth = 0.75f;  p.chorusAmount = 0.578031f;
            p.airAmount = 0.08f;    p.subLevel = 0.30f;
            p.base.attack = 0.992282f;
            p.base.release = 1.543242f;
            p.base.filterCutoff = 4500.0f;
            break;
        case GenreStyle::HipHop:
            p.unisonVoices = 4.0f;  p.detuneSpread = 0.15f;
            p.driftRate = 0.15f;    p.warmth = 0.65f;
            p.evolutionRate = 0.05f; p.filterSweep = 0.08f;
            p.stereoWidth = 0.65f;  p.chorusAmount = 0.25f;
            p.airAmount = 0.06f;    p.subLevel = 0.30f;
            p.base.attack = 0.35f;
            p.base.filterCutoff = 4000.0f;
            break;
        case GenreStyle::Techno:
            p.unisonVoices = 5.0f;  p.detuneSpread = 0.00112f;
            p.driftRate = 0.22f;    p.warmth = 0.998569f;
            p.evolutionRate = 0.10f; p.filterSweep = 0.607224f;
            p.stereoWidth = 0.75f;  p.chorusAmount = 0.689238f;
            p.airAmount = 0.10f;    p.subLevel = 0.28f;
            p.base.attack = 0.991786f;
            p.base.release = 1.455127f;
            p.base.filterCutoff = 4200.0f;
            break;
        case GenreStyle::House:
            p.unisonVoices = 4.0f;  p.detuneSpread = 0.013183f;
            p.driftRate = 0.18f;    p.warmth = 0.992237f;
            p.evolutionRate = 0.07f; p.filterSweep = 0.009113f;
            p.stereoWidth = 0.70f;  p.chorusAmount = 0.371432f;
            p.airAmount = 0.08f;    p.subLevel = 0.28f;
            p.base.attack = 0.626013f;
            p.base.release = 1.097888f;
            p.base.filterCutoff = 4500.0f;
            break;
        case GenreStyle::Reggaeton:
            p.unisonVoices = 4.0f;  p.detuneSpread = 0.16f;
            p.driftRate = 0.18f;    p.warmth = 0.60f;
            p.evolutionRate = 0.06f; p.filterSweep = 0.06f;
            p.stereoWidth = 0.60f;  p.chorusAmount = 0.25f;
            p.airAmount = 0.05f;    p.subLevel = 0.32f;
            p.base.attack = 0.35f;
            p.base.filterCutoff = 4000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.unisonVoices = 4.0f;  p.detuneSpread = 0.013367f;
            p.driftRate = 0.22f;    p.warmth = 0.99878f;
            p.evolutionRate = 0.08f; p.filterSweep = 0.858507f;
            p.stereoWidth = 0.60f;  p.chorusAmount = 0.635405f;
            p.airAmount = 0.08f;    p.subLevel = 0.25f;
            p.base.attack = 0.99802f;
            p.base.release = 1.502721f;
            p.base.filterCutoff = 3800.0f;
            break;
        case GenreStyle::RnB:
            p.unisonVoices = 5.0f;  p.detuneSpread = 0.012324f;
            p.driftRate = 0.12f;    p.warmth = 0.997786f;
            p.evolutionRate = 0.04f; p.filterSweep = 0.617853f;
            p.stereoWidth = 0.70f;  p.chorusAmount = 0.983197f;
            p.airAmount = 0.10f;    p.subLevel = 0.32f;
            p.base.attack = 0.976857f;
            p.base.release = 1.729161f;
            p.base.filterCutoff = 3500.0f;
            break;
        case GenreStyle::EDM:
            p.unisonVoices = 6.0f;  p.detuneSpread = 0.012492f;
            p.driftRate = 0.18f;    p.warmth = 0.953768f;
            p.evolutionRate = 0.08f; p.filterSweep = 0.848993f;
            p.stereoWidth = 0.80f;  p.chorusAmount = 0.798657f;
            p.airAmount = 0.10f;    p.subLevel = 0.25f;
            p.base.attack = 0.987832f;
            p.base.release = 1.59922f;
            p.base.filterCutoff = 5000.0f;
            break;
        case GenreStyle::Ambient:
            p.unisonVoices = 6.0f;  p.detuneSpread = 0.20f;
            p.driftRate = 0.30f;    p.warmth = 0.78f;
            p.evolutionRate = 0.12f; p.filterSweep = 0.12f;
            p.stereoWidth = 0.85f;  p.chorusAmount = 0.35f;
            p.airAmount = 0.15f;    p.subLevel = 0.38f;
            p.base.attack = 1.0f;   p.base.release = 1.5f;
            p.base.filterCutoff = 3000.0f;
            break;
    }
    return p;
}

// ============================================================
// TEXTURE — ML-calibrated
// ============================================================
inline TextureParams textureBase (GenreStyle genre)
{
    TextureParams p;
    p.base.volume    = 0.65f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.2f;
    p.base.decay     = 0.5f;
    p.base.release   = 0.5f;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 8000.0f;
    p.base.filterResonance = 0.08f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.density = 0.102035f;  p.grainSize = 0.005323f;
            p.scatter = 0.30f;  p.spectralTilt = -0.693505f;
            p.movement = 0.678306f; p.noiseColor = 0.427241f;
            p.stereoWidth = 0.70f; p.evolutionRate = 0.15f;
            break;
        case GenreStyle::HipHop:
            p.density = 0.448818f;  p.grainSize = 0.005205f;
            p.scatter = 0.25f;  p.spectralTilt = 0.1568f;
            p.movement = 0.115394f; p.noiseColor = 0.080239f;
            p.stereoWidth = 0.60f; p.evolutionRate = 0.10f;
            break;
        case GenreStyle::Techno:
            p.density = 0.101539f;  p.grainSize = 0.005138f;
            p.scatter = 0.35f;  p.spectralTilt = -0.774084f;
            p.movement = 0.422089f; p.noiseColor = 0.00089f;
            p.stereoWidth = 0.75f; p.evolutionRate = 0.20f;
            break;
        case GenreStyle::House:
            p.density = 0.10172f;  p.grainSize = 0.005219f;
            p.scatter = 0.28f;  p.spectralTilt = -0.75303f;
            p.movement = 0.624845f; p.noiseColor = 0.999826f;
            p.stereoWidth = 0.65f; p.evolutionRate = 0.12f;
            break;
        case GenreStyle::Reggaeton:
            p.density = 0.193821f;  p.grainSize = 0.005008f;
            p.scatter = 0.25f;  p.spectralTilt = -0.95897f;
            p.movement = 0.45196f; p.noiseColor = 0.09835f;
            p.stereoWidth = 0.60f; p.evolutionRate = 0.12f;
            break;
        case GenreStyle::Afrobeat:
            p.density = 0.101263f;  p.grainSize = 0.005053f;
            p.scatter = 0.30f;  p.spectralTilt = 0.23921f;
            p.movement = 0.444134f; p.noiseColor = 0.022571f;
            p.stereoWidth = 0.65f; p.evolutionRate = 0.15f;
            break;
        case GenreStyle::RnB:
            p.density = 0.10191f;  p.grainSize = 0.005274f;
            p.scatter = 0.20f;  p.spectralTilt = 0.334659f;
            p.movement = 0.458019f; p.noiseColor = 0.534516f;
            p.stereoWidth = 0.65f; p.evolutionRate = 0.08f;
            break;
        case GenreStyle::EDM:
            p.density = 0.100002f;  p.grainSize = 0.005763f;
            p.scatter = 0.35f;  p.spectralTilt = -0.827592f;
            p.movement = 0.798175f; p.noiseColor = 0.930755f;
            p.stereoWidth = 0.80f; p.evolutionRate = 0.22f;
            break;
        case GenreStyle::Ambient:
            p.density = 0.100005f;  p.grainSize = 0.005667f;
            p.scatter = 0.40f;  p.spectralTilt = -0.668882f;
            p.movement = 0.240851f; p.noiseColor = 0.920666f;
            p.stereoWidth = 0.90f; p.evolutionRate = 0.25f;
            p.base.attack = 0.6f; p.base.release = 1.0f;
            p.base.filterCutoff = 5000.0f;
            break;
    }
    return p;
}

// ============================================================
// EFFECTS — Reglas de efectos por género e instrumento
// ============================================================
// Filosofía: los efectos complementan el sonido base.
// — Percusivos (Kick, Snare, HiHat, Clap, Perc): compresión + EQ, reverb sutil
// — Bass: compresión + distorsión sutil, NO reverb (ensucia el sub)
// — Melódicos (Lead, Pluck): delay + reverb + chorus
// — Atmosféricos (Pad, Texture): reverb + chorus + phaser
//
// Cada género modifica los defaults:
// — Trap/Reggaeton: más compresión, menos reverb (punch/pegada)
// — Techno/EDM: más distorsión, delay, phaser
// — Ambient: mucha reverb, chorus, delay largo
// — House: reverb media, chorus sutil
// — HipHop/RnB: compresión, EQ, reverb sutil
// — Afrobeat: reverb media, delay sutil

inline EffectChainParams effectsBase (GenreStyle genre, InstrumentType instrument)
{
    EffectChainParams fx;

    // ============ INSTRUMENT-SPECIFIC DEFAULTS ============

    // --- Percusivos: EQ + Compresión ---
    bool isPerc = (instrument == InstrumentType::Kick
                || instrument == InstrumentType::Snare
                || instrument == InstrumentType::HiHat
                || instrument == InstrumentType::Clap
                || instrument == InstrumentType::Perc);

    if (isPerc)
    {
        fx.compressor.enabled = true;
        fx.compressor.threshold = -10.0f;
        fx.compressor.ratio = 2.5f;
        fx.compressor.attackMs = 2.0f;
        fx.compressor.releaseMs = 40.0f;
        fx.compressor.makeupGain = 0.0f;
        fx.compressor.mix = 1.0f;

        fx.eq.enabled = true;
        fx.eq.lowGain = 0.0f;
        fx.eq.midGain = 0.0f;
        fx.eq.highGain = 0.0f;
    }

    // --- Bass 808: Compresión suave + EQ, SIN distorsión extra, SIN reverb ---
    if (instrument == InstrumentType::Bass808)
    {
        fx.compressor.enabled = true;
        fx.compressor.threshold = -6.0f;
        fx.compressor.ratio = 2.5f;
        fx.compressor.attackMs = 8.0f;
        fx.compressor.releaseMs = 80.0f;
        fx.compressor.makeupGain = 0.5f;

        fx.eq.enabled = true;
        fx.eq.lowGain = 1.5f;
        fx.eq.lowFreq = 80.0f;
        fx.eq.midGain = -1.0f;
        fx.eq.midFreq = 300.0f;
        fx.eq.highGain = 0.0f;

        // Sin distorsión en efectos — la saturación del synth es suficiente
    }

    // --- Lead: Delay + Reverb + Chorus sutil ---
    if (instrument == InstrumentType::Lead)
    {
        fx.reverb.enabled = true;
        fx.reverb.size = 0.4f;
        fx.reverb.decay = 0.4f;
        fx.reverb.damping = 0.5f;
        fx.reverb.mix = 0.15f;

        fx.delay.enabled = true;
        fx.delay.time = 0.18f;
        fx.delay.feedback = 0.25f;
        fx.delay.highCut = 6000.0f;
        fx.delay.mix = 0.12f;

        fx.chorus.enabled = true;
        fx.chorus.rate = 1.2f;
        fx.chorus.depth = 0.3f;
        fx.chorus.mix = 0.15f;

        fx.eq.enabled = true;
        fx.eq.midGain = 1.0f;
        fx.eq.midFreq = 2000.0f;
    }

    // --- Pluck: Delay + Reverb ---
    if (instrument == InstrumentType::Pluck)
    {
        fx.reverb.enabled = true;
        fx.reverb.size = 0.35f;
        fx.reverb.decay = 0.35f;
        fx.reverb.damping = 0.6f;
        fx.reverb.mix = 0.18f;

        fx.delay.enabled = true;
        fx.delay.time = 0.15f;
        fx.delay.feedback = 0.20f;
        fx.delay.highCut = 8000.0f;
        fx.delay.mix = 0.10f;

        fx.eq.enabled = true;
        fx.eq.highGain = 1.5f;
    }

    // --- Pad: Reverb grande + Chorus + Phaser sutil ---
    if (instrument == InstrumentType::Pad)
    {
        fx.reverb.enabled = true;
        fx.reverb.size = 0.7f;
        fx.reverb.decay = 0.65f;
        fx.reverb.damping = 0.4f;
        fx.reverb.predelay = 0.02f;
        fx.reverb.mix = 0.30f;

        fx.chorus.enabled = true;
        fx.chorus.rate = 0.8f;
        fx.chorus.depth = 0.4f;
        fx.chorus.mix = 0.25f;

        fx.phaser.enabled = true;
        fx.phaser.rate = 0.3f;
        fx.phaser.depth = 0.3f;
        fx.phaser.feedback = 0.2f;
        fx.phaser.stages = 4;
        fx.phaser.mix = 0.15f;
    }

    // --- Texture: Reverb grande + Delay + Chorus ---
    if (instrument == InstrumentType::Texture)
    {
        fx.reverb.enabled = true;
        fx.reverb.size = 0.75f;
        fx.reverb.decay = 0.7f;
        fx.reverb.damping = 0.35f;
        fx.reverb.predelay = 0.03f;
        fx.reverb.mix = 0.35f;

        fx.delay.enabled = true;
        fx.delay.time = 0.3f;
        fx.delay.feedback = 0.30f;
        fx.delay.highCut = 5000.0f;
        fx.delay.mix = 0.15f;
        fx.delay.stereo = true;

        fx.chorus.enabled = true;
        fx.chorus.rate = 0.6f;
        fx.chorus.depth = 0.5f;
        fx.chorus.mix = 0.20f;
    }

    // ============ GENRE-SPECIFIC MODULATIONS ============

    switch (genre)
    {
        case GenreStyle::Trap:
            // Más compresión y pegada, reverb mínima
            if (fx.compressor.enabled)
            {
                fx.compressor.ratio += 1.0f;
                fx.compressor.threshold -= 1.0f;
            }
            if (fx.reverb.enabled)
                fx.reverb.mix *= 0.6f;  // menos reverb
            // Kick: click presence, sub controlado
            if (instrument == InstrumentType::Kick)
            {
                fx.eq.lowGain = 1.0f;
                fx.eq.lowFreq = 65.0f;
                fx.eq.midGain = -1.5f;
                fx.eq.midFreq = 250.0f;  // limpia mud
                fx.eq.highGain = 2.5f;   // click presence
            }
            // Snare: presencia
            if (instrument == InstrumentType::Snare)
            {
                fx.eq.midGain = 2.0f;
                fx.eq.midFreq = 3000.0f;
                fx.eq.highGain = 1.5f;
            }
            break;

        case GenreStyle::HipHop:
            // Compresión moderada, EQ definido
            if (instrument == InstrumentType::Kick)
            {
                fx.eq.lowGain = 1.0f;
                fx.eq.lowFreq = 75.0f;
                fx.eq.midGain = -1.0f;
                fx.eq.midFreq = 250.0f;
            }
            if (instrument == InstrumentType::Snare)
            {
                fx.reverb.enabled = true;
                fx.reverb.size = 0.25f;
                fx.reverb.decay = 0.2f;
                fx.reverb.mix = 0.08f;
                fx.eq.midGain = 1.5f;
                fx.eq.midFreq = 2500.0f;
            }
            break;

        case GenreStyle::Techno:
            // Más distorsión, phaser en percusión, delay
            if (isPerc)
            {
                fx.distortion.enabled = true;
                fx.distortion.drive = 0.20f;
                fx.distortion.tone = 0.6f;
                fx.distortion.mix = 0.15f;
            }
            if (instrument == InstrumentType::HiHat)
            {
                fx.phaser.enabled = true;
                fx.phaser.rate = 0.4f;
                fx.phaser.depth = 0.25f;
                fx.phaser.mix = 0.10f;
            }
            if (fx.delay.enabled)
            {
                fx.delay.feedback += 0.10f;
                fx.delay.mix += 0.05f;
            }
            // Kick: más drive, sub controlado
            if (instrument == InstrumentType::Kick)
            {
                fx.distortion.enabled = true;
                fx.distortion.drive = 0.10f;
                fx.distortion.tone = 0.4f;
                fx.distortion.mix = 0.10f;
                fx.eq.lowGain = 0.5f;
                fx.eq.midGain = -1.0f;
                fx.eq.midFreq = 250.0f;
            }
            break;

        case GenreStyle::House:
            // Reverb media, chorus sutil, compresión suave
            if (fx.reverb.enabled)
                fx.reverb.mix *= 1.1f;
            if (instrument == InstrumentType::Clap)
            {
                fx.reverb.enabled = true;
                fx.reverb.size = 0.4f;
                fx.reverb.decay = 0.35f;
                fx.reverb.mix = 0.20f;
            }
            if (instrument == InstrumentType::Kick)
            {
                fx.eq.lowGain = 0.5f;
                fx.eq.lowFreq = 85.0f;
                fx.eq.midGain = -1.5f;
                fx.eq.midFreq = 300.0f;
                fx.eq.highGain = 1.0f;
            }
            break;

        case GenreStyle::Reggaeton:
            // Máxima pegada: compresión controlada, mínima reverb
            if (fx.compressor.enabled)
            {
                fx.compressor.ratio += 1.0f;
                fx.compressor.threshold -= 1.5f;
                fx.compressor.attackMs = 2.0f;
            }
            if (fx.reverb.enabled)
                fx.reverb.mix *= 0.5f;  // mínima reverb
            // Kick: pegada = click + punch, sub controlado
            if (instrument == InstrumentType::Kick)
            {
                fx.eq.lowGain = 0.5f;
                fx.eq.lowFreq = 60.0f;
                fx.eq.midGain = -2.0f;
                fx.eq.midFreq = 250.0f;   // limpia mud para más pegada
                fx.eq.highGain = 3.0f;     // click pronunciado
                fx.distortion.enabled = true;
                fx.distortion.drive = 0.12f;
                fx.distortion.tone = 0.6f;
                fx.distortion.mix = 0.10f;
            }
            if (instrument == InstrumentType::Snare)
            {
                fx.eq.midGain = 2.5f;
                fx.eq.midFreq = 3500.0f;
                fx.eq.highGain = 2.0f;
            }
            break;

        case GenreStyle::Afrobeat:
            // Reverb natural media, EQ cálido
            if (fx.reverb.enabled)
            {
                fx.reverb.size *= 1.1f;
                fx.reverb.damping = 0.55f;
            }
            if (isPerc)
            {
                fx.reverb.enabled = true;
                fx.reverb.size = 0.3f;
                fx.reverb.decay = 0.3f;
                fx.reverb.mix = 0.10f;
            }
            if (instrument == InstrumentType::Perc)
            {
                fx.delay.enabled = true;
                fx.delay.time = 0.12f;
                fx.delay.feedback = 0.15f;
                fx.delay.mix = 0.08f;
            }
            break;

        case GenreStyle::RnB:
            // Compresión suave, reverb sutil, chorus en pads/leads
            if (fx.compressor.enabled)
                fx.compressor.ratio = std::max (fx.compressor.ratio - 0.5f, 1.5f);
            if (instrument == InstrumentType::Snare)
            {
                fx.reverb.enabled = true;
                fx.reverb.size = 0.35f;
                fx.reverb.decay = 0.30f;
                fx.reverb.mix = 0.12f;
            }
            if (fx.chorus.enabled)
            {
                fx.chorus.mix += 0.05f;
                fx.chorus.depth += 0.05f;
            }
            // Bass: warm EQ
            if (instrument == InstrumentType::Bass808)
            {
                fx.eq.lowGain = 2.5f;
                fx.eq.midGain = -1.5f;
                fx.eq.midFreq = 250.0f;
            }
            break;

        case GenreStyle::EDM:
            // Más distorsión, delay, compresión agresiva
            if (fx.compressor.enabled)
            {
                fx.compressor.ratio += 1.0f;
                fx.compressor.threshold -= 2.0f;
            }
            if (fx.distortion.enabled)
            {
                fx.distortion.drive += 0.10f;
                fx.distortion.mix += 0.05f;
            }
            if (fx.delay.enabled)
            {
                fx.delay.feedback += 0.08f;
                fx.delay.mix += 0.05f;
            }
            // Kick: punch EQ, sub controlado
            if (instrument == InstrumentType::Kick)
            {
                fx.eq.lowGain = 0.5f;
                fx.eq.midGain = 1.5f;
                fx.eq.midFreq = 3000.0f;  // presencia del punch
                fx.distortion.enabled = true;
                fx.distortion.drive = 0.12f;
                fx.distortion.tone = 0.55f;
                fx.distortion.mix = 0.10f;
            }
            // Lead/Pluck: más reverb y delay
            if (instrument == InstrumentType::Lead || instrument == InstrumentType::Pluck)
            {
                fx.reverb.mix += 0.05f;
                fx.delay.mix += 0.05f;
            }
            break;

        case GenreStyle::Ambient:
            // Mucha reverb, delay largo, chorus generoso, compresión suave
            if (fx.reverb.enabled)
            {
                fx.reverb.size += 0.2f;
                fx.reverb.size = std::min (fx.reverb.size, 1.0f);
                fx.reverb.decay += 0.2f;
                fx.reverb.decay = std::min (fx.reverb.decay, 1.0f);
                fx.reverb.mix += 0.15f;
                fx.reverb.mix = std::min (fx.reverb.mix, 0.6f);
            }
            if (fx.delay.enabled)
            {
                fx.delay.time += 0.15f;
                fx.delay.feedback += 0.10f;
                fx.delay.mix += 0.05f;
            }
            if (fx.chorus.enabled)
            {
                fx.chorus.depth += 0.10f;
                fx.chorus.mix += 0.05f;
            }
            // Todos: reverb habilitada
            if (! fx.reverb.enabled)
            {
                fx.reverb.enabled = true;
                fx.reverb.size = 0.6f;
                fx.reverb.decay = 0.55f;
                fx.reverb.damping = 0.4f;
                fx.reverb.mix = 0.20f;
            }
            // Compresión muy suave si existe
            if (fx.compressor.enabled)
            {
                fx.compressor.ratio = std::max (fx.compressor.ratio - 1.0f, 1.5f);
                fx.compressor.threshold += 3.0f;
            }
            break;
    }

    return fx;
}

} // namespace genrerules
