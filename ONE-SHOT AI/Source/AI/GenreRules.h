#pragma once

#include "../Params/InstrumentParams.h"
#include "../Params/SynthEnums.h"
#include "../Effects/EffectParams.h"

// Reglas musicales por género para cada instrumento.
// Cada función devuelve un perfil base de parámetros
// que luego el ParameterGenerator modula con carácter, impacto, energía y randomización.

namespace genrerules
{

// ============================================================
// KICK
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
            p.subFreq = 46.0f;  p.clickAmount = 0.45f;  p.bodyDecay = 0.22f;
            p.pitchDrop = 50.0f; p.pitchDropTime = 0.028f;
            p.driveAmount = 0.30f; p.subLevel = 0.65f; p.tailLength = 0.30f;
            break;
        case GenreStyle::HipHop:
            p.subFreq = 54.0f;  p.clickAmount = 0.35f;  p.bodyDecay = 0.18f;
            p.pitchDrop = 30.0f; p.pitchDropTime = 0.038f;
            p.driveAmount = 0.15f; p.subLevel = 0.60f; p.tailLength = 0.25f;
            break;
        case GenreStyle::Techno:
            p.subFreq = 54.0f;  p.clickAmount = 0.20f;  p.bodyDecay = 0.28f;
            p.pitchDrop = 40.0f; p.pitchDropTime = 0.042f;
            p.driveAmount = 0.22f; p.subLevel = 0.55f; p.tailLength = 0.32f;
            break;
        case GenreStyle::House:
            p.subFreq = 58.0f;  p.clickAmount = 0.35f;  p.bodyDecay = 0.16f;
            p.pitchDrop = 28.0f; p.pitchDropTime = 0.032f;
            p.driveAmount = 0.10f; p.subLevel = 0.55f; p.tailLength = 0.18f;
            break;
        case GenreStyle::Reggaeton:
            p.subFreq = 50.0f;  p.clickAmount = 0.75f;  p.bodyDecay = 0.14f;
            p.pitchDrop = 44.0f; p.pitchDropTime = 0.016f;
            p.driveAmount = 0.38f; p.subLevel = 0.60f; p.tailLength = 0.18f;
            p.base.filterCutoff = 10000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.subFreq = 55.0f;  p.clickAmount = 0.20f;  p.bodyDecay = 0.20f;
            p.pitchDrop = 28.0f; p.pitchDropTime = 0.038f;
            p.driveAmount = 0.05f; p.subLevel = 0.55f; p.tailLength = 0.22f;
            break;
        case GenreStyle::RnB:
            p.subFreq = 48.0f;  p.clickAmount = 0.12f;  p.bodyDecay = 0.25f;
            p.pitchDrop = 24.0f; p.pitchDropTime = 0.048f;
            p.driveAmount = 0.05f; p.subLevel = 0.60f; p.tailLength = 0.32f;
            break;
        case GenreStyle::EDM:
            p.subFreq = 52.0f;  p.clickAmount = 0.50f;  p.bodyDecay = 0.20f;
            p.pitchDrop = 45.0f; p.pitchDropTime = 0.022f;
            p.driveAmount = 0.35f; p.subLevel = 0.60f; p.tailLength = 0.25f;
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
// SNARE
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
            p.bodyFreq = 190.0f; p.bodyDecay = 0.09f;
            p.noiseDecay = 0.12f; p.noiseColor = 0.70f;
            p.snapAmount = 0.65f; p.ringFreq = 340.0f;
            p.ringAmount = 0.15f; p.wireAmount = 0.20f;
            p.noiseTightness = 0.60f; p.bodyMix = 0.28f;
            p.noiseLP = 7500.0f;
            break;
        case GenreStyle::HipHop:
            p.bodyFreq = 175.0f; p.bodyDecay = 0.12f;
            p.noiseDecay = 0.13f; p.noiseColor = 0.50f;
            p.snapAmount = 0.45f; p.ringFreq = 320.0f;
            p.ringAmount = 0.20f; p.wireAmount = 0.15f;
            p.noiseTightness = 0.55f; p.bodyMix = 0.35f;
            p.noiseLP = 7000.0f;
            break;
        case GenreStyle::Techno:
            p.bodyFreq = 220.0f; p.bodyDecay = 0.07f;
            p.noiseDecay = 0.09f; p.noiseColor = 0.75f;
            p.snapAmount = 0.60f; p.ringFreq = 380.0f;
            p.ringAmount = 0.10f; p.wireAmount = 0.08f;
            p.noiseTightness = 0.65f; p.bodyMix = 0.25f;
            p.noiseLP = 8000.0f;
            break;
        case GenreStyle::House:
            p.bodyFreq = 210.0f; p.bodyDecay = 0.10f;
            p.noiseDecay = 0.11f; p.noiseColor = 0.60f;
            p.snapAmount = 0.50f; p.ringFreq = 350.0f;
            p.ringAmount = 0.15f; p.wireAmount = 0.22f;
            p.noiseTightness = 0.55f; p.bodyMix = 0.30f;
            p.noiseLP = 7000.0f;
            break;
        case GenreStyle::Reggaeton:
            p.bodyFreq = 200.0f; p.bodyDecay = 0.07f;
            p.noiseDecay = 0.09f; p.noiseColor = 0.75f;
            p.snapAmount = 0.75f; p.ringFreq = 360.0f;
            p.ringAmount = 0.10f; p.wireAmount = 0.10f;
            p.noiseTightness = 0.70f; p.bodyMix = 0.22f;
            p.noiseLP = 8500.0f;
            p.base.filterCutoff = 14000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.bodyFreq = 180.0f; p.bodyDecay = 0.14f;
            p.noiseDecay = 0.14f; p.noiseColor = 0.40f;
            p.snapAmount = 0.35f; p.ringFreq = 300.0f;
            p.ringAmount = 0.25f; p.wireAmount = 0.28f;
            p.noiseTightness = 0.45f; p.bodyMix = 0.40f;
            p.noiseLP = 6000.0f;
            break;
        case GenreStyle::RnB:
            p.bodyFreq = 185.0f; p.bodyDecay = 0.12f;
            p.noiseDecay = 0.12f; p.noiseColor = 0.45f;
            p.snapAmount = 0.40f; p.ringFreq = 310.0f;
            p.ringAmount = 0.18f; p.wireAmount = 0.15f;
            p.noiseTightness = 0.50f; p.bodyMix = 0.35f;
            p.noiseLP = 6500.0f;
            break;
        case GenreStyle::EDM:
            p.bodyFreq = 210.0f; p.bodyDecay = 0.10f;
            p.noiseDecay = 0.12f; p.noiseColor = 0.65f;
            p.snapAmount = 0.60f; p.ringFreq = 370.0f;
            p.ringAmount = 0.15f; p.wireAmount = 0.25f;
            p.noiseTightness = 0.60f; p.bodyMix = 0.28f;
            p.noiseLP = 8000.0f;
            break;
        case GenreStyle::Ambient:
            p.bodyFreq = 165.0f; p.bodyDecay = 0.18f;
            p.noiseDecay = 0.18f; p.noiseColor = 0.25f;
            p.snapAmount = 0.12f; p.ringFreq = 280.0f;
            p.ringAmount = 0.10f; p.wireAmount = 0.05f;
            p.noiseTightness = 0.40f; p.bodyMix = 0.45f;
            p.noiseLP = 5000.0f;
            p.base.filterCutoff = 6000.0f;
            break;
    }
    return p;
}

// ============================================================
// HI-HAT
// ============================================================
inline HiHatParams hiHatBase (GenreStyle genre)
{
    HiHatParams p;
    p.base.volume  = 0.70f;
    p.base.pan     = 0.5f;
    p.base.attack  = 0.0005f;
    p.base.filterType     = FilterType::HighPass;
    p.base.filterCutoff   = 3000.0f;
    p.base.filterResonance = 0.05f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.freqRange = 8500.0f;  p.metallic = 0.75f;  p.noiseColor = 0.75f;
            p.openDecay = 0.38f;    p.closedDecay = 0.018f;
            p.openAmount = 0.0f;    p.ringAmount = 0.35f;  p.highPassFreq = 5000.0f;
            break;
        case GenreStyle::HipHop:
            p.freqRange = 7500.0f;  p.metallic = 0.60f;  p.noiseColor = 0.60f;
            p.openDecay = 0.35f;    p.closedDecay = 0.022f;
            p.openAmount = 0.0f;    p.ringAmount = 0.28f;  p.highPassFreq = 4500.0f;
            break;
        case GenreStyle::Techno:
            p.freqRange = 9500.0f;  p.metallic = 0.85f;  p.noiseColor = 0.78f;
            p.openDecay = 0.28f;    p.closedDecay = 0.010f;
            p.openAmount = 0.0f;    p.ringAmount = 0.40f;  p.highPassFreq = 5500.0f;
            break;
        case GenreStyle::House:
            p.freqRange = 8000.0f;  p.metallic = 0.65f;  p.noiseColor = 0.65f;
            p.openDecay = 0.42f;    p.closedDecay = 0.020f;
            p.openAmount = 0.12f;   p.ringAmount = 0.30f;  p.highPassFreq = 4500.0f;
            break;
        case GenreStyle::Reggaeton:
            p.freqRange = 9000.0f;  p.metallic = 0.70f;  p.noiseColor = 0.78f;
            p.openDecay = 0.28f;    p.closedDecay = 0.015f;
            p.openAmount = 0.0f;    p.ringAmount = 0.30f;  p.highPassFreq = 5000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.freqRange = 6500.0f;  p.metallic = 0.50f;  p.noiseColor = 0.55f;
            p.openDecay = 0.38f;    p.closedDecay = 0.028f;
            p.openAmount = 0.08f;   p.ringAmount = 0.25f;  p.highPassFreq = 3800.0f;
            break;
        case GenreStyle::RnB:
            p.freqRange = 6500.0f;  p.metallic = 0.50f;  p.noiseColor = 0.50f;
            p.openDecay = 0.32f;    p.closedDecay = 0.025f;
            p.openAmount = 0.0f;    p.ringAmount = 0.22f;  p.highPassFreq = 4000.0f;
            break;
        case GenreStyle::EDM:
            p.freqRange = 9500.0f;  p.metallic = 0.78f;  p.noiseColor = 0.80f;
            p.openDecay = 0.32f;    p.closedDecay = 0.012f;
            p.openAmount = 0.0f;    p.ringAmount = 0.35f;  p.highPassFreq = 5500.0f;
            break;
        case GenreStyle::Ambient:
            p.freqRange = 5500.0f;  p.metallic = 0.35f;  p.noiseColor = 0.40f;
            p.openDecay = 0.55f;    p.closedDecay = 0.035f;
            p.openAmount = 0.25f;   p.ringAmount = 0.18f;  p.highPassFreq = 3000.0f;
            p.base.volume = 0.55f;
            break;
    }
    return p;
}

// ============================================================
// CLAP
// ============================================================
inline ClapParams clapBase (GenreStyle genre)
{
    ClapParams p;
    p.base.volume  = 0.75f;
    p.base.pan     = 0.5f;
    p.base.attack  = 0.001f;
    p.base.filterType     = FilterType::BandPass;
    p.base.filterCutoff   = 2500.0f;
    p.base.filterResonance = 0.15f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.numLayers = 5.0f;  p.layerSpacing = 0.011f;
            p.noiseDecay = 0.08f; p.noiseColor = 0.70f;
            p.toneAmount = 0.12f; p.toneFreq = 1100.0f;
            p.reverbAmount = 0.25f; p.thickness = 0.60f;
            p.noiseTightness = 0.55f; p.noiseLP = 8000.0f;
            p.transientSnap = 0.80f;
            break;
        case GenreStyle::HipHop:
            p.numLayers = 4.0f;  p.layerSpacing = 0.014f;
            p.noiseDecay = 0.10f; p.noiseColor = 0.55f;
            p.toneAmount = 0.18f; p.toneFreq = 1000.0f;
            p.reverbAmount = 0.25f; p.thickness = 0.55f;
            p.noiseTightness = 0.50f; p.noiseLP = 7000.0f;
            p.transientSnap = 0.70f;
            break;
        case GenreStyle::Techno:
            p.numLayers = 3.0f;  p.layerSpacing = 0.008f;
            p.noiseDecay = 0.06f; p.noiseColor = 0.75f;
            p.toneAmount = 0.08f; p.toneFreq = 1200.0f;
            p.reverbAmount = 0.30f; p.thickness = 0.50f;
            p.noiseTightness = 0.65f; p.noiseLP = 8500.0f;
            p.transientSnap = 0.75f;
            break;
        case GenreStyle::House:
            p.numLayers = 5.0f;  p.layerSpacing = 0.013f;
            p.noiseDecay = 0.10f; p.noiseColor = 0.60f;
            p.toneAmount = 0.15f; p.toneFreq = 1050.0f;
            p.reverbAmount = 0.35f; p.thickness = 0.55f;
            p.noiseTightness = 0.50f; p.noiseLP = 7000.0f;
            p.transientSnap = 0.65f;
            break;
        case GenreStyle::Reggaeton:
            p.numLayers = 4.0f;  p.layerSpacing = 0.008f;
            p.noiseDecay = 0.06f; p.noiseColor = 0.75f;
            p.toneAmount = 0.12f; p.toneFreq = 1150.0f;
            p.reverbAmount = 0.15f; p.thickness = 0.70f;
            p.noiseTightness = 0.70f; p.noiseLP = 9000.0f;
            p.transientSnap = 0.90f;
            break;
        case GenreStyle::Afrobeat:
            p.numLayers = 6.0f;  p.layerSpacing = 0.015f;
            p.noiseDecay = 0.11f; p.noiseColor = 0.50f;
            p.toneAmount = 0.20f; p.toneFreq = 950.0f;
            p.reverbAmount = 0.28f; p.thickness = 0.55f;
            p.noiseTightness = 0.45f; p.noiseLP = 6500.0f;
            p.transientSnap = 0.55f;
            break;
        case GenreStyle::RnB:
            p.numLayers = 4.0f;  p.layerSpacing = 0.013f;
            p.noiseDecay = 0.10f; p.noiseColor = 0.50f;
            p.toneAmount = 0.18f; p.toneFreq = 1000.0f;
            p.reverbAmount = 0.25f; p.thickness = 0.50f;
            p.noiseTightness = 0.50f; p.noiseLP = 6500.0f;
            p.transientSnap = 0.65f;
            break;
        case GenreStyle::EDM:
            p.numLayers = 5.0f;  p.layerSpacing = 0.010f;
            p.noiseDecay = 0.08f; p.noiseColor = 0.70f;
            p.toneAmount = 0.12f; p.toneFreq = 1100.0f;
            p.reverbAmount = 0.32f; p.thickness = 0.60f;
            p.noiseTightness = 0.60f; p.noiseLP = 8500.0f;
            p.transientSnap = 0.75f;
            break;
        case GenreStyle::Ambient:
            p.numLayers = 3.0f;  p.layerSpacing = 0.018f;
            p.noiseDecay = 0.20f; p.noiseColor = 0.35f;
            p.toneAmount = 0.22f; p.toneFreq = 850.0f;
            p.reverbAmount = 0.50f; p.thickness = 0.40f;
            p.noiseTightness = 0.35f; p.noiseLP = 5500.0f;
            p.transientSnap = 0.40f;
            p.base.volume = 0.60f;
            break;
    }
    return p;
}

// ============================================================
// PERC
// ============================================================
inline PercParams percBase (GenreStyle genre)
{
    PercParams p;
    p.base.volume  = 0.75f;
    p.base.pan     = 0.5f;
    p.base.attack  = 0.001f;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 10000.0f;
    p.base.filterResonance = 0.1f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.freq = 450.0f; p.toneDecay = 0.07f;
            p.metallic = 0.30f; p.woodiness = 0.10f; p.membrane = 0.20f;
            p.clickAmount = 0.50f; p.pitchDrop = 15.0f; p.harmonicRatio = 1.2f;
            break;
        case GenreStyle::HipHop:
            p.freq = 380.0f; p.toneDecay = 0.08f;
            p.metallic = 0.10f; p.woodiness = 0.30f; p.membrane = 0.40f;
            p.clickAmount = 0.40f; p.pitchDrop = 10.0f; p.harmonicRatio = 1.0f;
            break;
        case GenreStyle::Techno:
            p.freq = 500.0f; p.toneDecay = 0.06f;
            p.metallic = 0.60f; p.woodiness = 0.05f; p.membrane = 0.10f;
            p.clickAmount = 0.55f; p.pitchDrop = 18.0f; p.harmonicRatio = 1.6f;
            break;
        case GenreStyle::House:
            p.freq = 420.0f; p.toneDecay = 0.08f;
            p.metallic = 0.15f; p.woodiness = 0.20f; p.membrane = 0.35f;
            p.clickAmount = 0.40f; p.pitchDrop = 12.0f; p.harmonicRatio = 1.1f;
            break;
        case GenreStyle::Reggaeton:
            p.freq = 480.0f; p.toneDecay = 0.06f;
            p.metallic = 0.20f; p.woodiness = 0.15f; p.membrane = 0.25f;
            p.clickAmount = 0.65f; p.pitchDrop = 16.0f; p.harmonicRatio = 1.3f;
            break;
        case GenreStyle::Afrobeat:
            p.freq = 350.0f; p.toneDecay = 0.10f;
            p.metallic = 0.05f; p.woodiness = 0.40f; p.membrane = 0.50f;
            p.clickAmount = 0.35f; p.pitchDrop = 8.0f; p.harmonicRatio = 1.0f;
            break;
        case GenreStyle::RnB:
            p.freq = 400.0f; p.toneDecay = 0.08f;
            p.metallic = 0.10f; p.woodiness = 0.25f; p.membrane = 0.30f;
            p.clickAmount = 0.35f; p.pitchDrop = 10.0f; p.harmonicRatio = 1.0f;
            break;
        case GenreStyle::EDM:
            p.freq = 500.0f; p.toneDecay = 0.06f;
            p.metallic = 0.45f; p.woodiness = 0.10f; p.membrane = 0.15f;
            p.clickAmount = 0.55f; p.pitchDrop = 18.0f; p.harmonicRatio = 1.5f;
            break;
        case GenreStyle::Ambient:
            p.freq = 300.0f; p.toneDecay = 0.15f;
            p.metallic = 0.15f; p.woodiness = 0.35f; p.membrane = 0.30f;
            p.clickAmount = 0.15f; p.pitchDrop = 5.0f; p.harmonicRatio = 1.0f;
            p.base.filterCutoff = 5000.0f;
            break;
    }
    return p;
}

// ============================================================
// BASS 808
// ============================================================
inline Bass808Params bass808Base (GenreStyle genre)
{
    Bass808Params p;
    p.base.volume    = 0.85f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.002f;
    p.base.decay     = 0.3f;
    p.base.pitchBase = 36.0f;  // C2
    p.base.oscType   = OscillatorType::Sine;
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 5000.0f;
    p.base.filterResonance = 0.1f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.subFreq = 38.0f;  p.sustainLevel = 0.70f; p.glideTime = 0.05f;
            p.glideAmount = 5.0f; p.saturation = 0.15f;
            p.subOctave = 0.15f; p.harmonics = 0.10f; p.tailLength = 1.0f;
            p.reeseMix = 0.0f; p.reeseDetune = 0.0f;
            p.punchiness = 0.40f; p.filterEnvAmt = 0.0f;
            break;
        case GenreStyle::HipHop:
            p.subFreq = 42.0f;  p.sustainLevel = 0.60f; p.glideTime = 0.0f;
            p.glideAmount = 0.0f; p.saturation = 0.10f;
            p.subOctave = 0.10f; p.harmonics = 0.08f; p.tailLength = 0.8f;
            p.reeseMix = 0.0f; p.reeseDetune = 0.0f;
            p.punchiness = 0.30f; p.filterEnvAmt = 0.0f;
            break;
        case GenreStyle::Techno:
            p.subFreq = 45.0f;  p.sustainLevel = 0.45f; p.glideTime = 0.0f;
            p.glideAmount = 0.0f; p.saturation = 0.15f;
            p.subOctave = 0.0f;  p.harmonics = 0.12f; p.tailLength = 0.5f;
            p.reeseMix = 0.0f; p.reeseDetune = 0.0f;
            p.punchiness = 0.35f; p.filterEnvAmt = 0.15f;
            break;
        case GenreStyle::House:
            // DONK BASS: corto, punchy, filtro resonante con envelope
            p.subFreq = 52.0f;  p.sustainLevel = 0.30f; p.glideTime = 0.0f;
            p.glideAmount = 0.0f; p.saturation = 0.10f;
            p.subOctave = 0.0f;  p.harmonics = 0.08f; p.tailLength = 0.35f;
            p.reeseMix = 0.0f; p.reeseDetune = 0.0f;
            p.punchiness = 0.65f; p.filterEnvAmt = 0.70f;
            p.base.filterCutoff = 3000.0f;
            p.base.filterResonance = 0.40f;
            break;
        case GenreStyle::Reggaeton:
            // REESE BASS: 3 saws detuned, wobble LFO, filter growl, stereo
            p.subFreq = 42.0f;  p.sustainLevel = 0.70f; p.glideTime = 0.04f;
            p.glideAmount = 4.0f; p.saturation = 0.20f;
            p.subOctave = 0.05f; p.harmonics = 0.15f; p.tailLength = 0.9f;
            p.reeseMix = 0.85f; p.reeseDetune = 0.65f;
            p.punchiness = 0.45f; p.filterEnvAmt = 0.05f;
            p.base.filterCutoff = 3500.0f;
            p.base.filterResonance = 0.18f;
            break;
        case GenreStyle::Afrobeat:
            p.subFreq = 48.0f;  p.sustainLevel = 0.50f; p.glideTime = 0.0f;
            p.glideAmount = 0.0f; p.saturation = 0.08f;
            p.subOctave = 0.05f; p.harmonics = 0.05f; p.tailLength = 0.6f;
            p.reeseMix = 0.0f; p.reeseDetune = 0.0f;
            p.punchiness = 0.25f; p.filterEnvAmt = 0.0f;
            break;
        case GenreStyle::RnB:
            p.subFreq = 40.0f;  p.sustainLevel = 0.65f; p.glideTime = 0.04f;
            p.glideAmount = 4.0f; p.saturation = 0.10f;
            p.subOctave = 0.10f; p.harmonics = 0.08f; p.tailLength = 0.9f;
            p.reeseMix = 0.20f; p.reeseDetune = 0.15f;
            p.punchiness = 0.20f; p.filterEnvAmt = 0.0f;
            break;
        case GenreStyle::EDM:
            p.subFreq = 44.0f;  p.sustainLevel = 0.55f; p.glideTime = 0.03f;
            p.glideAmount = 3.0f; p.saturation = 0.18f;
            p.subOctave = 0.10f; p.harmonics = 0.12f; p.tailLength = 0.7f;
            p.reeseMix = 0.30f; p.reeseDetune = 0.20f;
            p.punchiness = 0.45f; p.filterEnvAmt = 0.20f;
            break;
        case GenreStyle::Ambient:
            p.subFreq = 38.0f;  p.sustainLevel = 0.75f; p.glideTime = 0.08f;
            p.glideAmount = 7.0f; p.saturation = 0.03f;
            p.subOctave = 0.18f; p.harmonics = 0.03f; p.tailLength = 1.5f;
            p.reeseMix = 0.0f; p.reeseDetune = 0.0f;
            p.punchiness = 0.10f; p.filterEnvAmt = 0.0f;
            p.base.filterCutoff = 2000.0f;
            break;
    }
    return p;
}

// ============================================================
// LEAD
// ============================================================
inline LeadParams leadBase (GenreStyle genre)
{
    LeadParams p;
    p.base.volume    = 0.75f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.01f;
    p.base.decay     = 0.4f;
    p.base.sustain   = 0.6f;
    p.base.release   = 0.15f;
    p.base.pitchBase = 65.0f;  // F4
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 8000.0f;
    p.base.filterResonance = 0.15f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.oscDetune = 0.12f; p.pulseWidth = 0.50f;
            p.vibratoRate = 5.5f; p.vibratoDepth = 0.10f;
            p.brightness = 0.55f; p.waveformMix = 0.0f;
            p.unisonVoices = 3.0f; p.portamento = 0.03f;
            p.filterEnvAmt = 0.30f; p.subOscLevel = 0.15f;
            break;
        case GenreStyle::HipHop:
            p.oscDetune = 0.08f; p.pulseWidth = 0.45f;
            p.vibratoRate = 4.5f; p.vibratoDepth = 0.08f;
            p.brightness = 0.45f; p.waveformMix = 0.3f;
            p.unisonVoices = 2.0f; p.portamento = 0.0f;
            p.filterEnvAmt = 0.20f; p.subOscLevel = 0.10f;
            break;
        case GenreStyle::Techno:
            p.oscDetune = 0.05f; p.pulseWidth = 0.50f;
            p.vibratoRate = 6.0f; p.vibratoDepth = 0.05f;
            p.brightness = 0.65f; p.waveformMix = 0.7f;
            p.unisonVoices = 2.0f; p.portamento = 0.0f;
            p.filterEnvAmt = 0.45f; p.subOscLevel = 0.0f;
            p.base.filterResonance = 0.30f;
            break;
        case GenreStyle::House:
            p.oscDetune = 0.10f; p.pulseWidth = 0.50f;
            p.vibratoRate = 5.0f; p.vibratoDepth = 0.06f;
            p.brightness = 0.55f; p.waveformMix = 0.2f;
            p.unisonVoices = 3.0f; p.portamento = 0.0f;
            p.filterEnvAmt = 0.35f; p.subOscLevel = 0.10f;
            break;
        case GenreStyle::Reggaeton:
            p.oscDetune = 0.10f; p.pulseWidth = 0.48f;
            p.vibratoRate = 5.0f; p.vibratoDepth = 0.08f;
            p.brightness = 0.60f; p.waveformMix = 0.0f;
            p.unisonVoices = 3.0f; p.portamento = 0.02f;
            p.filterEnvAmt = 0.25f; p.subOscLevel = 0.20f;
            break;
        case GenreStyle::Afrobeat:
            p.oscDetune = 0.08f; p.pulseWidth = 0.45f;
            p.vibratoRate = 5.0f; p.vibratoDepth = 0.10f;
            p.brightness = 0.45f; p.waveformMix = 0.4f;
            p.unisonVoices = 2.0f; p.portamento = 0.0f;
            p.filterEnvAmt = 0.15f; p.subOscLevel = 0.08f;
            break;
        case GenreStyle::RnB:
            p.oscDetune = 0.06f; p.pulseWidth = 0.42f;
            p.vibratoRate = 4.0f; p.vibratoDepth = 0.12f;
            p.brightness = 0.40f; p.waveformMix = 0.5f;
            p.unisonVoices = 2.0f; p.portamento = 0.04f;
            p.filterEnvAmt = 0.20f; p.subOscLevel = 0.12f;
            break;
        case GenreStyle::EDM:
            p.oscDetune = 0.18f; p.pulseWidth = 0.50f;
            p.vibratoRate = 5.5f; p.vibratoDepth = 0.05f;
            p.brightness = 0.75f; p.waveformMix = 0.0f;
            p.unisonVoices = 5.0f; p.portamento = 0.0f;
            p.filterEnvAmt = 0.40f; p.subOscLevel = 0.15f;
            break;
        case GenreStyle::Ambient:
            p.oscDetune = 0.15f; p.pulseWidth = 0.50f;
            p.vibratoRate = 3.0f; p.vibratoDepth = 0.15f;
            p.brightness = 0.30f; p.waveformMix = 0.6f;
            p.unisonVoices = 4.0f; p.portamento = 0.08f;
            p.filterEnvAmt = 0.10f; p.subOscLevel = 0.20f;
            p.base.attack = 0.15f; p.base.release = 0.5f;
            break;
    }
    return p;
}

// ============================================================
// PLUCK
// ============================================================
inline PluckParams pluckBase (GenreStyle genre)
{
    PluckParams p;
    p.base.volume    = 0.75f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.001f;
    p.base.pitchBase = 62.0f;  // D4
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 12000.0f;
    p.base.filterResonance = 0.1f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.brightness = 0.65f; p.bodyResonance = 0.25f;
            p.decayTime = 0.35f;  p.damping = 0.45f;
            p.pickPosition = 0.55f; p.stringTension = 0.55f;
            p.harmonics = 0.30f; p.stereoWidth = 0.35f;
            break;
        case GenreStyle::HipHop:
            p.brightness = 0.50f; p.bodyResonance = 0.30f;
            p.decayTime = 0.40f;  p.damping = 0.50f;
            p.pickPosition = 0.50f; p.stringTension = 0.50f;
            p.harmonics = 0.25f; p.stereoWidth = 0.30f;
            break;
        case GenreStyle::Techno:
            p.brightness = 0.70f; p.bodyResonance = 0.15f;
            p.decayTime = 0.25f;  p.damping = 0.40f;
            p.pickPosition = 0.60f; p.stringTension = 0.60f;
            p.harmonics = 0.35f; p.stereoWidth = 0.30f;
            break;
        case GenreStyle::House:
            p.brightness = 0.60f; p.bodyResonance = 0.25f;
            p.decayTime = 0.35f;  p.damping = 0.45f;
            p.pickPosition = 0.55f; p.stringTension = 0.50f;
            p.harmonics = 0.25f; p.stereoWidth = 0.35f;
            break;
        case GenreStyle::Reggaeton:
            p.brightness = 0.65f; p.bodyResonance = 0.20f;
            p.decayTime = 0.30f;  p.damping = 0.40f;
            p.pickPosition = 0.60f; p.stringTension = 0.55f;
            p.harmonics = 0.30f; p.stereoWidth = 0.30f;
            break;
        case GenreStyle::Afrobeat:
            p.brightness = 0.50f; p.bodyResonance = 0.35f;
            p.decayTime = 0.45f;  p.damping = 0.55f;
            p.pickPosition = 0.45f; p.stringTension = 0.45f;
            p.harmonics = 0.20f; p.stereoWidth = 0.35f;
            break;
        case GenreStyle::RnB:
            p.brightness = 0.45f; p.bodyResonance = 0.30f;
            p.decayTime = 0.45f;  p.damping = 0.55f;
            p.pickPosition = 0.45f; p.stringTension = 0.45f;
            p.harmonics = 0.20f; p.stereoWidth = 0.35f;
            break;
        case GenreStyle::EDM:
            p.brightness = 0.75f; p.bodyResonance = 0.20f;
            p.decayTime = 0.30f;  p.damping = 0.35f;
            p.pickPosition = 0.60f; p.stringTension = 0.60f;
            p.harmonics = 0.35f; p.stereoWidth = 0.40f;
            break;
        case GenreStyle::Ambient:
            p.brightness = 0.35f; p.bodyResonance = 0.40f;
            p.decayTime = 0.70f;  p.damping = 0.65f;
            p.pickPosition = 0.40f; p.stringTension = 0.40f;
            p.harmonics = 0.15f; p.stereoWidth = 0.50f;
            break;
    }
    return p;
}

// ============================================================
// PAD
// ============================================================
inline PadParams padBase (GenreStyle genre)
{
    PadParams p;
    p.base.volume    = 0.70f;
    p.base.pan       = 0.5f;
    p.base.attack    = 0.3f;
    p.base.decay     = 0.5f;
    p.base.sustain   = 0.7f;
    p.base.release   = 0.6f;
    p.base.pitchBase = 55.0f;  // G3
    p.base.filterType     = FilterType::LowPass;
    p.base.filterCutoff   = 6000.0f;
    p.base.filterResonance = 0.1f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.unisonVoices = 4.0f;  p.detuneSpread = 0.18f;
            p.driftRate = 0.20f;    p.warmth = 0.60f;
            p.evolutionRate = 0.07f; p.filterSweep = 0.08f;
            p.stereoWidth = 0.65f;  p.chorusAmount = 0.25f;
            p.airAmount = 0.06f;    p.subLevel = 0.25f;
            p.base.attack = 0.4f;
            p.base.filterCutoff = 4500.0f;
            break;
        case GenreStyle::HipHop:
            p.unisonVoices = 3.0f;  p.detuneSpread = 0.14f;
            p.driftRate = 0.18f;    p.warmth = 0.65f;
            p.evolutionRate = 0.05f; p.filterSweep = 0.05f;
            p.stereoWidth = 0.55f;  p.chorusAmount = 0.22f;
            p.airAmount = 0.05f;    p.subLevel = 0.30f;
            p.base.attack = 0.4f;
            p.base.filterCutoff = 4000.0f;
            break;
        case GenreStyle::Techno:
            p.unisonVoices = 3.0f;  p.detuneSpread = 0.16f;
            p.driftRate = 0.25f;    p.warmth = 0.50f;
            p.evolutionRate = 0.10f; p.filterSweep = 0.18f;
            p.stereoWidth = 0.60f;  p.chorusAmount = 0.18f;
            p.airAmount = 0.08f;    p.subLevel = 0.20f;
            p.base.attack = 0.35f;
            p.base.filterCutoff = 4500.0f;
            p.base.filterResonance = 0.15f;
            break;
        case GenreStyle::House:
            p.unisonVoices = 5.0f;  p.detuneSpread = 0.18f;
            p.driftRate = 0.18f;    p.warmth = 0.62f;
            p.evolutionRate = 0.07f; p.filterSweep = 0.10f;
            p.stereoWidth = 0.70f;  p.chorusAmount = 0.30f;
            p.airAmount = 0.08f;    p.subLevel = 0.28f;
            p.base.attack = 0.4f;
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
            p.unisonVoices = 4.0f;  p.detuneSpread = 0.14f;
            p.driftRate = 0.22f;    p.warmth = 0.68f;
            p.evolutionRate = 0.08f; p.filterSweep = 0.08f;
            p.stereoWidth = 0.60f;  p.chorusAmount = 0.25f;
            p.airAmount = 0.08f;    p.subLevel = 0.25f;
            p.base.attack = 0.45f;
            p.base.filterCutoff = 3800.0f;
            break;
        case GenreStyle::RnB:
            p.unisonVoices = 5.0f;  p.detuneSpread = 0.15f;
            p.driftRate = 0.12f;    p.warmth = 0.72f;
            p.evolutionRate = 0.04f; p.filterSweep = 0.04f;
            p.stereoWidth = 0.70f;  p.chorusAmount = 0.30f;
            p.airAmount = 0.10f;    p.subLevel = 0.32f;
            p.base.attack = 0.5f;
            p.base.filterCutoff = 3500.0f;
            break;
        case GenreStyle::EDM:
            p.unisonVoices = 6.0f;  p.detuneSpread = 0.22f;
            p.driftRate = 0.18f;    p.warmth = 0.48f;
            p.evolutionRate = 0.08f; p.filterSweep = 0.15f;
            p.stereoWidth = 0.80f;  p.chorusAmount = 0.30f;
            p.airAmount = 0.10f;    p.subLevel = 0.25f;
            p.base.attack = 0.35f;
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
// TEXTURE
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
            p.density = 0.50f;  p.grainSize = 0.04f;
            p.scatter = 0.30f;  p.spectralTilt = 0.10f;
            p.movement = 0.40f; p.noiseColor = 0.55f;
            p.stereoWidth = 0.70f; p.evolutionRate = 0.15f;
            break;
        case GenreStyle::HipHop:
            p.density = 0.40f;  p.grainSize = 0.05f;
            p.scatter = 0.25f;  p.spectralTilt = -0.10f;
            p.movement = 0.30f; p.noiseColor = 0.45f;
            p.stereoWidth = 0.60f; p.evolutionRate = 0.10f;
            break;
        case GenreStyle::Techno:
            p.density = 0.60f;  p.grainSize = 0.03f;
            p.scatter = 0.35f;  p.spectralTilt = 0.15f;
            p.movement = 0.55f; p.noiseColor = 0.60f;
            p.stereoWidth = 0.75f; p.evolutionRate = 0.20f;
            break;
        case GenreStyle::House:
            p.density = 0.45f;  p.grainSize = 0.05f;
            p.scatter = 0.28f;  p.spectralTilt = 0.05f;
            p.movement = 0.40f; p.noiseColor = 0.50f;
            p.stereoWidth = 0.65f; p.evolutionRate = 0.12f;
            break;
        case GenreStyle::Reggaeton:
            p.density = 0.45f;  p.grainSize = 0.04f;
            p.scatter = 0.25f;  p.spectralTilt = 0.10f;
            p.movement = 0.35f; p.noiseColor = 0.55f;
            p.stereoWidth = 0.60f; p.evolutionRate = 0.12f;
            break;
        case GenreStyle::Afrobeat:
            p.density = 0.50f;  p.grainSize = 0.05f;
            p.scatter = 0.30f;  p.spectralTilt = -0.05f;
            p.movement = 0.45f; p.noiseColor = 0.45f;
            p.stereoWidth = 0.65f; p.evolutionRate = 0.15f;
            break;
        case GenreStyle::RnB:
            p.density = 0.35f;  p.grainSize = 0.06f;
            p.scatter = 0.20f;  p.spectralTilt = -0.15f;
            p.movement = 0.30f; p.noiseColor = 0.40f;
            p.stereoWidth = 0.65f; p.evolutionRate = 0.08f;
            break;
        case GenreStyle::EDM:
            p.density = 0.60f;  p.grainSize = 0.03f;
            p.scatter = 0.35f;  p.spectralTilt = 0.20f;
            p.movement = 0.55f; p.noiseColor = 0.65f;
            p.stereoWidth = 0.80f; p.evolutionRate = 0.22f;
            break;
        case GenreStyle::Ambient:
            p.density = 0.55f;  p.grainSize = 0.08f;
            p.scatter = 0.40f;  p.spectralTilt = -0.10f;
            p.movement = 0.65f; p.noiseColor = 0.40f;
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
