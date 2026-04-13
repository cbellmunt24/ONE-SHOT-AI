#pragma once

#include "UniversalSynthParams.h"
#include "../Params/SynthEnums.h"

namespace universalsynth
{
namespace genrerules
{

// ============================================================================
// KICK — tonalLevel=0.9, noiseLevel=0, modalLevel=0, transientLevel=click
// Mapping: subFreq->basePitch, click->transientLevel + snapAmount*0.5,
//   bodyDecay->ampBodyDecay, pitchDrop->pitchEnvDepth,
//   pitchDropTime->pitchEnvFast*0.3 + pitchEnvSlow*1.2,
//   drive->satAmount (satType=1 tape), subLevel->subLevel,
//   tailLength->duration calc, clickFreq=4000, clickDecay=0.002, clickWidth=0.4
//   oscType=0, ampAttack=0.001, ampPunchDecay=bodyDecay*0.5, ampPunchLevel=0.6
//   pitchEnvBalance=0.65
// ============================================================================
inline UniversalSynthParams kickBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Layer gates
    p.tonalLevel     = 0.9f;
    p.noiseLevel     = 0.0f;
    p.modalLevel     = 0.0f;
    // transientLevel set per genre from click

    // Fixed kick params
    p.oscType        = 0;      // sine
    p.clickFreq      = 4000.0f;
    p.clickDecay     = 0.002f;
    p.clickWidth     = 0.4f;
    p.clickType      = 0;
    p.ampAttack      = 0.001f;
    p.ampPunchLevel  = 0.6f;
    p.pitchEnvBalance= 0.65f;
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.basePitch      = 61.25f;
            p.transientLevel = 0.45f;
            p.snapAmount     = 0.45f * 0.5f;
            p.ampBodyDecay   = 0.052f;
            p.pitchEnvDepth  = 52.93f;
            p.pitchEnvFast   = 0.057f * 0.3f;
            p.pitchEnvSlow   = 0.057f * 1.2f;
            p.satAmount      = 0.50f;  p.satType = 1;
            p.subLevel       = 0.99f;
            p.ampPunchDecay  = 0.052f * 0.5f;
            p.duration       = 0.001f + 0.052f + 0.07f + 0.1f;
            break;
        case GenreStyle::HipHop:
            p.basePitch      = 59.99f;
            p.transientLevel = 0.31f;
            p.snapAmount     = 0.31f * 0.5f;
            p.ampBodyDecay   = 0.050f;
            p.pitchEnvDepth  = 58.56f;
            p.pitchEnvFast   = 0.054f * 0.3f;
            p.pitchEnvSlow   = 0.054f * 1.2f;
            p.satAmount      = 0.39f;  p.satType = 1;
            p.subLevel       = 0.80f;
            p.ampPunchDecay  = 0.050f * 0.5f;
            p.duration       = 0.001f + 0.050f + 0.06f + 0.1f;
            break;
        case GenreStyle::Techno:
            p.basePitch      = 42.22f;
            p.transientLevel = 0.79f;
            p.snapAmount     = 0.79f * 0.5f;
            p.ampBodyDecay   = 0.050f;
            p.pitchEnvDepth  = 52.86f;
            p.pitchEnvFast   = 0.071f * 0.3f;
            p.pitchEnvSlow   = 0.071f * 1.2f;
            p.satAmount      = 0.30f;  p.satType = 1;
            p.subLevel       = 0.41f;
            p.ampPunchDecay  = 0.050f * 0.5f;
            p.duration       = 0.001f + 0.050f + 0.09f + 0.1f;
            break;
        case GenreStyle::House:
            p.basePitch      = 34.13f;
            p.transientLevel = 0.02f;
            p.snapAmount     = 0.02f * 0.5f;
            p.ampBodyDecay   = 0.050f;
            p.pitchEnvDepth  = 59.50f;
            p.pitchEnvFast   = 0.042f * 0.3f;
            p.pitchEnvSlow   = 0.042f * 1.2f;
            p.satAmount      = 0.49f;  p.satType = 1;
            p.subLevel       = 0.99f;
            p.ampPunchDecay  = 0.050f * 0.5f;
            p.duration       = 0.001f + 0.050f + 0.06f + 0.1f;
            break;
        case GenreStyle::Reggaeton:
            p.basePitch      = 32.47f;
            p.transientLevel = 0.94f;
            p.snapAmount     = 0.94f * 0.5f;
            p.ampBodyDecay   = 0.050f;
            p.pitchEnvDepth  = 46.19f;
            p.pitchEnvFast   = 0.023f * 0.3f;
            p.pitchEnvSlow   = 0.023f * 1.2f;
            p.satAmount      = 0.42f;  p.satType = 1;
            p.subLevel       = 0.56f;
            p.ampPunchDecay  = 0.050f * 0.5f;
            p.duration       = 0.001f + 0.050f + 0.07f + 0.1f;
            break;
        case GenreStyle::Afrobeat:
            p.basePitch      = 39.73f;
            p.transientLevel = 0.08f;
            p.snapAmount     = 0.08f * 0.5f;
            p.ampBodyDecay   = 0.051f;
            p.pitchEnvDepth  = 59.40f;
            p.pitchEnvFast   = 0.056f * 0.3f;
            p.pitchEnvSlow   = 0.056f * 1.2f;
            p.satAmount      = 0.46f;  p.satType = 1;
            p.subLevel       = 0.55f;
            p.ampPunchDecay  = 0.051f * 0.5f;
            p.duration       = 0.001f + 0.051f + 0.07f + 0.1f;
            break;
        case GenreStyle::RnB:
            p.basePitch      = 63.95f;
            p.transientLevel = 0.55f;
            p.snapAmount     = 0.55f * 0.5f;
            p.ampBodyDecay   = 0.051f;
            p.pitchEnvDepth  = 54.36f;
            p.pitchEnvFast   = 0.049f * 0.3f;
            p.pitchEnvSlow   = 0.049f * 1.2f;
            p.satAmount      = 0.21f;  p.satType = 1;
            p.subLevel       = 0.96f;
            p.ampPunchDecay  = 0.051f * 0.5f;
            p.duration       = 0.001f + 0.051f + 0.06f + 0.1f;
            break;
        case GenreStyle::EDM:
            p.basePitch      = 62.02f;
            p.transientLevel = 0.96f;
            p.snapAmount     = 0.96f * 0.5f;
            p.ampBodyDecay   = 0.098f;
            p.pitchEnvDepth  = 53.69f;
            p.pitchEnvFast   = 0.070f * 0.3f;
            p.pitchEnvSlow   = 0.070f * 1.2f;
            p.satAmount      = 0.49f;  p.satType = 1;
            p.subLevel       = 0.94f;
            p.ampPunchDecay  = 0.098f * 0.5f;
            p.duration       = 0.001f + 0.098f + 0.17f + 0.1f;
            break;
        case GenreStyle::Ambient:
            p.basePitch      = 44.0f;
            p.transientLevel = 0.05f;
            p.snapAmount     = 0.05f * 0.5f;
            p.ampBodyDecay   = 0.32f;
            p.pitchEnvDepth  = 18.0f;
            p.pitchEnvFast   = 0.065f * 0.3f;
            p.pitchEnvSlow   = 0.065f * 1.2f;
            p.satAmount      = 0.0f;   p.satType = 1;
            p.subLevel       = 0.55f;
            p.ampPunchDecay  = 0.32f * 0.5f;
            p.duration       = 0.001f + 0.32f + 0.45f + 0.1f;
            break;
    }
    return p;
}

// ============================================================================
// SNARE — tonalLevel=bodyMix(~0.35), noiseLevel=0.7, modalLevel=wireAmount*0.2,
//   transientLevel=snapAmount*0.8
// Mapping: bodyFreq->basePitch, bodyDecay->ampPunchDecay,
//   noiseDecay->noiseDecay+ampBodyDecay, noiseColor->noiseColor,
//   snapAmount->snapAmount+transientLevel, wireAmount->modalLevel,
//   noiseTightness->noiseFilterQ, noiseLP->noiseFilterFreq,
//   numModes=2, modeSpread=0.3, modeRatioBase=2.0,
//   clickType=0, clickFreq=5000, clickDecay=0.001,
//   oscType=0, duration=0.3, ampAttack=0.001, ampPunchLevel=0.7,
//   pitchEnvDepth=5, pitchEnvFast=0.005
// ============================================================================
inline UniversalSynthParams snareBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Fixed snare params
    p.noiseLevel     = 0.7f;
    p.oscType        = 0;
    p.duration       = 0.3f;
    p.ampAttack      = 0.001f;
    p.ampPunchLevel  = 0.7f;
    p.pitchEnvDepth  = 5.0f;
    p.pitchEnvFast   = 0.005f;
    p.clickType      = 0;
    p.clickFreq      = 5000.0f;
    p.clickDecay     = 0.001f;
    p.numModes       = 2.0f;
    p.modeSpread     = 0.3f;
    p.modeRatioBase  = 2.0f;
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.basePitch      = 263.75f;
            p.tonalLevel     = 0.35f;
            p.ampPunchDecay  = 0.083f;
            p.noiseDecay     = 0.094f;
            p.ampBodyDecay   = 0.094f;
            p.noiseColor     = 0.56f;
            p.snapAmount     = 0.039f;
            p.transientLevel = 0.039f * 0.8f;
            p.modalLevel     = 0.40f * 0.2f;
            p.noiseFilterQ   = 0.55f;
            p.noiseFilterFreq= 7000.0f;
            break;
        case GenreStyle::HipHop:
            p.basePitch      = 277.09f;
            p.tonalLevel     = 0.30f;
            p.ampPunchDecay  = 0.053f;
            p.noiseDecay     = 0.059f;
            p.ampBodyDecay   = 0.059f;
            p.noiseColor     = 0.007f;
            p.snapAmount     = 0.63f;
            p.transientLevel = 0.63f * 0.8f;
            p.modalLevel     = 0.13f * 0.2f;
            p.noiseFilterQ   = 0.50f;
            p.noiseFilterFreq= 6500.0f;
            break;
        case GenreStyle::Techno:
            p.basePitch      = 236.57f;
            p.tonalLevel     = 0.35f;
            p.ampPunchDecay  = 0.032f;
            p.noiseDecay     = 0.052f;
            p.ampBodyDecay   = 0.052f;
            p.noiseColor     = 0.016f;
            p.snapAmount     = 0.77f;
            p.transientLevel = 0.77f * 0.8f;
            p.modalLevel     = 0.26f * 0.2f;
            p.noiseFilterQ   = 0.65f;
            p.noiseFilterFreq= 8000.0f;
            break;
        case GenreStyle::House:
            p.basePitch      = 279.34f;
            p.tonalLevel     = 0.30f;
            p.ampPunchDecay  = 0.157f;
            p.noiseDecay     = 0.052f;
            p.ampBodyDecay   = 0.052f;
            p.noiseColor     = 0.69f;
            p.snapAmount     = 0.59f;
            p.transientLevel = 0.59f * 0.8f;
            p.modalLevel     = 0.49f * 0.2f;
            p.noiseFilterQ   = 0.55f;
            p.noiseFilterFreq= 7500.0f;
            break;
        case GenreStyle::Reggaeton:
            p.basePitch      = 215.08f;
            p.tonalLevel     = 0.35f;
            p.ampPunchDecay  = 0.133f;
            p.noiseDecay     = 0.051f;
            p.ampBodyDecay   = 0.051f;
            p.noiseColor     = 0.30f;
            p.snapAmount     = 0.73f;
            p.transientLevel = 0.73f * 0.8f;
            p.modalLevel     = 0.57f * 0.2f;
            p.noiseFilterQ   = 0.70f;
            p.noiseFilterFreq= 8000.0f;
            break;
        case GenreStyle::Afrobeat:
            p.basePitch      = 276.11f;
            p.tonalLevel     = 0.30f;
            p.ampPunchDecay  = 0.046f;
            p.noiseDecay     = 0.051f;
            p.ampBodyDecay   = 0.051f;
            p.noiseColor     = 0.19f;
            p.snapAmount     = 0.62f;
            p.transientLevel = 0.62f * 0.8f;
            p.modalLevel     = 0.11f * 0.2f;
            p.noiseFilterQ   = 0.55f;
            p.noiseFilterFreq= 6500.0f;
            break;
        case GenreStyle::RnB:
            p.basePitch      = 263.75f;
            p.tonalLevel     = 0.35f;
            p.ampPunchDecay  = 0.083f;
            p.noiseDecay     = 0.094f;
            p.ampBodyDecay   = 0.094f;
            p.noiseColor     = 0.56f;
            p.snapAmount     = 0.039f;
            p.transientLevel = 0.039f * 0.8f;
            p.modalLevel     = 0.40f * 0.2f;
            p.noiseFilterQ   = 0.45f;
            p.noiseFilterFreq= 6000.0f;
            break;
        case GenreStyle::EDM:
            p.basePitch      = 279.36f;
            p.tonalLevel     = 0.30f;
            p.ampPunchDecay  = 0.193f;
            p.noiseDecay     = 0.051f;
            p.ampBodyDecay   = 0.051f;
            p.noiseColor     = 0.63f;
            p.snapAmount     = 0.082f;
            p.transientLevel = 0.082f * 0.8f;
            p.modalLevel     = 0.35f * 0.2f;
            p.noiseFilterQ   = 0.60f;
            p.noiseFilterFreq= 8500.0f;
            break;
        case GenreStyle::Ambient:
            p.basePitch      = 273.15f;
            p.tonalLevel     = 0.35f;
            p.ampPunchDecay  = 0.161f;
            p.noiseDecay     = 0.050f;
            p.ampBodyDecay   = 0.050f;
            p.noiseColor     = 0.92f;
            p.snapAmount     = 0.73f;
            p.transientLevel = 0.73f * 0.8f;
            p.modalLevel     = 0.08f * 0.2f;
            p.noiseFilterQ   = 0.40f;
            p.noiseFilterFreq= 5500.0f;
            break;
    }
    return p;
}

// ============================================================================
// HIHAT — tonalLevel=0, noiseLevel=0.4, modalLevel=0.8, transientLevel=0.5
// Mapping: freqRange->basePitch, metallic->modeSpread,
//   noiseColor->noiseColor, openAmount scales modeDecay between closedDecay/openDecay,
//   ringAmount->modalLevel, highPassFreq->noiseHP,
//   numModes=6, modeRatioBase=1.5, modeDamping=0.4,
//   noiseFilterFreq=8000, noiseFilterQ=0.3,
//   clickType=0, clickFreq=8000, clickDecay=0.001, clickWidth=0.6,
//   oscType=0, ampAttack=0.001, ampPunchDecay=0.01, ampPunchLevel=0.8
// ============================================================================
inline UniversalSynthParams hihatBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Layer gates
    p.tonalLevel     = 0.0f;
    p.noiseLevel     = 0.4f;
    p.modalLevel     = 0.8f;
    p.transientLevel = 0.5f;

    // Fixed hihat params
    p.oscType        = 0;
    p.ampAttack      = 0.001f;
    p.ampPunchDecay  = 0.01f;
    p.ampPunchLevel  = 0.8f;
    p.clickType      = 0;
    p.clickFreq      = 8000.0f;
    p.clickDecay     = 0.001f;
    p.clickWidth     = 0.6f;
    p.numModes       = 6.0f;
    p.modeRatioBase  = 1.5f;
    p.modeDamping    = 0.4f;
    p.noiseFilterFreq= 8000.0f;
    p.noiseFilterQ   = 0.3f;
    p.masterGain     = 0.9f;

    // Helper: interpolate modeDecay from closedDecay to openDecay based on openAmount
    auto calcModeDecay = [] (float closedDecay, float openDecay, float openAmount) -> float
    {
        return closedDecay + (openDecay - closedDecay) * openAmount;
    };

    switch (genre)
    {
        case GenreStyle::Trap:
            p.basePitch    = 3313.4f;
            p.modeSpread   = 0.45f;
            p.noiseColor   = 0.19f;
            p.modeDecay    = calcModeDecay (0.071f, 0.30f, 0.35f);
            p.modalLevel   = 0.90f;
            p.noiseHP      = 5500.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::HipHop:
            p.basePitch    = 3313.4f;
            p.modeSpread   = 0.45f;
            p.noiseColor   = 0.19f;
            p.modeDecay    = calcModeDecay (0.071f, 0.30f, 0.35f);
            p.modalLevel   = 0.90f;
            p.noiseHP      = 5000.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::Techno:
            p.basePitch    = 5518.9f;
            p.modeSpread   = 0.42f;
            p.noiseColor   = 0.99f;
            p.modeDecay    = calcModeDecay (0.010f, 0.25f, 0.30f);
            p.modalLevel   = 0.57f;
            p.noiseHP      = 6000.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::House:
            p.basePitch    = 3313.4f;
            p.modeSpread   = 0.45f;
            p.noiseColor   = 0.19f;
            p.modeDecay    = calcModeDecay (0.071f, 0.30f, 0.35f);
            p.modalLevel   = 0.90f;
            p.noiseHP      = 5500.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::Reggaeton:
            p.basePitch    = 7761.4f;
            p.modeSpread   = 0.97f;
            p.noiseColor   = 0.04f;
            p.modeDecay    = calcModeDecay (0.010f, 0.20f, 0.25f);
            p.modalLevel   = 0.28f;
            p.noiseHP      = 6000.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::Afrobeat:
            p.basePitch    = 6887.6f;
            p.modeSpread   = 0.97f;
            p.noiseColor   = 0.02f;
            p.modeDecay    = calcModeDecay (0.010f, 0.25f, 0.30f);
            p.modalLevel   = 0.30f;
            p.noiseHP      = 5500.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::RnB:
            p.basePitch    = 3351.7f;
            p.modeSpread   = 0.26f;
            p.noiseColor   = 0.78f;
            p.modeDecay    = calcModeDecay (0.010f, 0.30f, 0.35f);
            p.modalLevel   = 0.17f;
            p.noiseHP      = 5000.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::EDM:
            p.basePitch    = 6025.4f;
            p.modeSpread   = 0.13f;
            p.noiseColor   = 1.00f;
            p.modeDecay    = calcModeDecay (0.011f, 0.22f, 0.30f);
            p.modalLevel   = 0.15f;
            p.noiseHP      = 6000.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
        case GenreStyle::Ambient:
            p.basePitch    = 7915.2f;
            p.modeSpread   = 0.24f;
            p.noiseColor   = 0.96f;
            p.modeDecay    = calcModeDecay (0.014f, 0.40f, 0.45f);
            p.modalLevel   = 0.40f;
            p.noiseHP      = 4500.0f;
            p.ampBodyDecay = p.modeDecay;
            p.duration     = p.modeDecay + 0.05f;
            break;
    }
    return p;
}

// ============================================================================
// CLAP — tonalLevel=toneAmount(~0.1), noiseLevel=0.85, modalLevel=0,
//   transientLevel=transientSnap*0.8
// Mapping: numLayers->burstCount, spacing->burstSpacing,
//   noiseDecay->noiseDecay, noiseColor->noiseColor,
//   toneAmount->tonalLevel, toneFreq->basePitch(~500),
//   thickness+noiseTightness->noiseFilterQ, transientSnap->snapAmount+transientLevel,
//   noiseLP->noiseFilterFreq, reverbAmount->reverbAmt(small),
//   clickType=0, clickFreq=3000, clickDecay=0.003,
//   duration=burstCount*burstSpacing+noiseDecay+0.1,
//   ampAttack=0.001, ampPunchDecay=0.02, ampPunchLevel=0.5
// ============================================================================
inline UniversalSynthParams clapBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Fixed clap params
    p.noiseLevel     = 0.85f;
    p.modalLevel     = 0.0f;
    p.clickType      = 0;
    p.clickFreq      = 3000.0f;
    p.clickDecay     = 0.003f;
    p.ampAttack      = 0.001f;
    p.ampPunchDecay  = 0.02f;
    p.ampPunchLevel  = 0.5f;
    p.basePitch      = 500.0f;  // default tone freq for claps
    p.tonalLevel     = 0.1f;    // default small tonal amount
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.burstCount     = 4.08f;
            p.burstSpacing   = 0.012f;
            p.noiseDecay     = 0.058f;
            p.ampBodyDecay   = 0.058f;
            p.noiseColor     = 1.0f;
            p.snapAmount     = 0.37f;
            p.transientLevel = 0.37f * 0.8f;
            p.noiseFilterQ   = (0.25f + 0.55f) * 0.5f;
            p.noiseFilterFreq= 6000.0f;
            p.duration       = 4.08f * 0.012f + 0.058f + 0.1f;
            break;
        case GenreStyle::HipHop:
            p.burstCount     = 3.65f;
            p.burstSpacing   = 0.012f;
            p.noiseDecay     = 0.051f;
            p.ampBodyDecay   = 0.051f;
            p.noiseColor     = 0.28f;
            p.snapAmount     = 0.41f;
            p.transientLevel = 0.41f * 0.8f;
            p.noiseFilterQ   = (0.29f + 0.55f) * 0.5f;
            p.noiseFilterFreq= 6000.0f;
            p.duration       = 3.65f * 0.012f + 0.051f + 0.1f;
            break;
        case GenreStyle::Techno:
            p.burstCount     = 5.75f;
            p.burstSpacing   = 0.010f;
            p.noiseDecay     = 0.050f;
            p.ampBodyDecay   = 0.050f;
            p.noiseColor     = 0.39f;
            p.snapAmount     = 0.67f;
            p.transientLevel = 0.67f * 0.8f;
            p.noiseFilterQ   = (0.12f + 0.60f) * 0.5f;
            p.noiseFilterFreq= 7000.0f;
            p.duration       = 5.75f * 0.010f + 0.050f + 0.1f;
            break;
        case GenreStyle::House:
            p.burstCount     = 5.86f;
            p.burstSpacing   = 0.010f;
            p.noiseDecay     = 0.051f;
            p.ampBodyDecay   = 0.051f;
            p.noiseColor     = 0.92f;
            p.snapAmount     = 0.44f;
            p.transientLevel = 0.44f * 0.8f;
            p.noiseFilterQ   = (0.29f + 0.50f) * 0.5f;
            p.noiseFilterFreq= 7000.0f;
            p.duration       = 5.86f * 0.010f + 0.051f + 0.1f;
            break;
        case GenreStyle::Reggaeton:
            p.burstCount     = 4.22f;
            p.burstSpacing   = 0.008f;
            p.noiseDecay     = 0.050f;
            p.ampBodyDecay   = 0.050f;
            p.noiseColor     = 0.24f;
            p.snapAmount     = 0.38f;
            p.transientLevel = 0.38f * 0.8f;
            p.noiseFilterQ   = (0.27f + 0.65f) * 0.5f;
            p.noiseFilterFreq= 7500.0f;
            p.duration       = 4.22f * 0.008f + 0.050f + 0.1f;
            break;
        case GenreStyle::Afrobeat:
            p.burstCount     = 4.90f;
            p.burstSpacing   = 0.012f;
            p.noiseDecay     = 0.050f;
            p.ampBodyDecay   = 0.050f;
            p.noiseColor     = 0.75f;
            p.snapAmount     = 0.55f;
            p.transientLevel = 0.55f * 0.8f;
            p.noiseFilterQ   = (0.28f + 0.55f) * 0.5f;
            p.noiseFilterFreq= 6500.0f;
            p.duration       = 4.90f * 0.012f + 0.050f + 0.1f;
            break;
        case GenreStyle::RnB:
            p.burstCount     = 5.16f;
            p.burstSpacing   = 0.010f;
            p.noiseDecay     = 0.052f;
            p.ampBodyDecay   = 0.052f;
            p.noiseColor     = 0.98f;
            p.snapAmount     = 0.91f;
            p.transientLevel = 0.91f * 0.8f;
            p.noiseFilterQ   = (0.26f + 0.50f) * 0.5f;
            p.noiseFilterFreq= 6000.0f;
            p.duration       = 5.16f * 0.010f + 0.052f + 0.1f;
            break;
        case GenreStyle::EDM:
            p.burstCount     = 4.91f;
            p.burstSpacing   = 0.010f;
            p.noiseDecay     = 0.054f;
            p.ampBodyDecay   = 0.054f;
            p.noiseColor     = 0.91f;
            p.snapAmount     = 0.78f;
            p.transientLevel = 0.78f * 0.8f;
            p.noiseFilterQ   = (0.28f + 0.55f) * 0.5f;
            p.noiseFilterFreq= 7500.0f;
            p.duration       = 4.91f * 0.010f + 0.054f + 0.1f;
            break;
        case GenreStyle::Ambient:
            p.burstCount     = 5.97f;
            p.burstSpacing   = 0.015f;
            p.noiseDecay     = 0.069f;
            p.ampBodyDecay   = 0.069f;
            p.noiseColor     = 1.0f;
            p.snapAmount     = 0.40f;
            p.transientLevel = 0.40f * 0.8f;
            p.noiseFilterQ   = (0.55f + 0.40f) * 0.5f;
            p.noiseFilterFreq= 5500.0f;
            p.duration       = 5.97f * 0.015f + 0.069f + 0.1f;
            break;
    }
    return p;
}

// ============================================================================
// PERC — tonalLevel=0.5, noiseLevel=woodiness*0.5, modalLevel=metallic*0.5,
//   transientLevel=clickAmount*0.7
// Mapping: freq->basePitch, toneDecay->ampPunchDecay + ampBodyDecay=toneDecay*2,
//   metallic->modalLevel + modeSpread=0.5, woodiness->noiseLevel + noiseColor=0.3,
//   membrane->noiseFilterQ, clickAmount->transientLevel,
//   pitchDrop->pitchEnvDepth, harmonicRatio->modeRatioBase,
//   numModes=4, modeDamping=0.5, modeDecay=toneDecay,
//   clickType=0, clickFreq=5000, clickDecay=0.002,
//   ampAttack=0.001, ampPunchLevel=0.6
// ============================================================================
inline UniversalSynthParams percBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Fixed perc params
    p.tonalLevel     = 0.5f;
    p.oscType        = 0;
    p.clickType      = 0;
    p.clickFreq      = 5000.0f;
    p.clickDecay     = 0.002f;
    p.ampAttack      = 0.001f;
    p.ampPunchLevel  = 0.6f;
    p.numModes       = 4.0f;
    p.modeDamping    = 0.5f;
    p.modeSpread     = 0.5f;
    p.noiseColor     = 0.3f;   // pink for wood
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.basePitch      = 499.0f;
            p.ampPunchDecay  = 0.033f;
            p.ampBodyDecay   = 0.033f * 2.0f;
            p.modalLevel     = 0.90f * 0.5f;
            p.noiseLevel     = 0.19f * 0.5f;
            p.transientLevel = 0.35f * 0.7f;
            p.pitchEnvDepth  = 10.78f;
            p.modeRatioBase  = 1.5f;
            p.modeDecay      = 0.033f;
            p.duration       = 0.033f * 3.0f + 0.1f;
            break;
        case GenreStyle::HipHop:
            p.basePitch      = 1969.9f;
            p.ampPunchDecay  = 0.028f;
            p.ampBodyDecay   = 0.028f * 2.0f;
            p.modalLevel     = 0.53f * 0.5f;
            p.noiseLevel     = 0.54f * 0.5f;
            p.transientLevel = 0.30f * 0.7f;
            p.pitchEnvDepth  = 1.01f;
            p.modeRatioBase  = 1.4f;
            p.modeDecay      = 0.028f;
            p.duration       = 0.028f * 3.0f + 0.1f;
            break;
        case GenreStyle::Techno:
            p.basePitch      = 861.6f;
            p.ampPunchDecay  = 0.066f;
            p.ampBodyDecay   = 0.066f * 2.0f;
            p.modalLevel     = 0.98f * 0.5f;
            p.noiseLevel     = 0.75f * 0.5f;
            p.transientLevel = 0.30f * 0.7f;
            p.pitchEnvDepth  = 21.54f;
            p.modeRatioBase  = 1.8f;
            p.modeDecay      = 0.066f;
            p.duration       = 0.066f * 3.0f + 0.1f;
            break;
        case GenreStyle::House:
            p.basePitch      = 499.0f;
            p.ampPunchDecay  = 0.033f;
            p.ampBodyDecay   = 0.033f * 2.0f;
            p.modalLevel     = 0.90f * 0.5f;
            p.noiseLevel     = 0.19f * 0.5f;
            p.transientLevel = 0.30f * 0.7f;
            p.pitchEnvDepth  = 10.78f;
            p.modeRatioBase  = 1.5f;
            p.modeDecay      = 0.033f;
            p.duration       = 0.033f * 3.0f + 0.1f;
            break;
        case GenreStyle::Reggaeton:
            p.basePitch      = 510.6f;
            p.ampPunchDecay  = 0.021f;
            p.ampBodyDecay   = 0.021f * 2.0f;
            p.modalLevel     = 0.17f * 0.5f;
            p.noiseLevel     = 0.72f * 0.5f;
            p.transientLevel = 0.45f * 0.7f;
            p.pitchEnvDepth  = 6.26f;
            p.modeRatioBase  = 1.3f;
            p.modeDecay      = 0.021f;
            p.duration       = 0.021f * 3.0f + 0.1f;
            break;
        case GenreStyle::Afrobeat:
            p.basePitch      = 1985.4f;
            p.ampPunchDecay  = 0.021f;
            p.ampBodyDecay   = 0.021f * 2.0f;
            p.modalLevel     = 0.29f * 0.5f;
            p.noiseLevel     = 0.96f * 0.5f;
            p.transientLevel = 0.25f * 0.7f;
            p.pitchEnvDepth  = 12.69f;
            p.modeRatioBase  = 1.4f;
            p.modeDecay      = 0.021f;
            p.duration       = 0.021f * 3.0f + 0.1f;
            break;
        case GenreStyle::RnB:
            p.basePitch      = 499.0f;
            p.ampPunchDecay  = 0.033f;
            p.ampBodyDecay   = 0.033f * 2.0f;
            p.modalLevel     = 0.90f * 0.5f;
            p.noiseLevel     = 0.19f * 0.5f;
            p.transientLevel = 0.25f * 0.7f;
            p.pitchEnvDepth  = 10.78f;
            p.modeRatioBase  = 1.5f;
            p.modeDecay      = 0.033f;
            p.duration       = 0.033f * 3.0f + 0.1f;
            break;
        case GenreStyle::EDM:
            p.basePitch      = 499.0f;
            p.ampPunchDecay  = 0.033f;
            p.ampBodyDecay   = 0.033f * 2.0f;
            p.modalLevel     = 0.90f * 0.5f;
            p.noiseLevel     = 0.19f * 0.5f;
            p.transientLevel = 0.35f * 0.7f;
            p.pitchEnvDepth  = 10.78f;
            p.modeRatioBase  = 1.6f;
            p.modeDecay      = 0.033f;
            p.duration       = 0.033f * 3.0f + 0.1f;
            break;
        case GenreStyle::Ambient:
            p.basePitch      = 516.9f;
            p.ampPunchDecay  = 0.055f;
            p.ampBodyDecay   = 0.055f * 2.0f;
            p.modalLevel     = 0.82f * 0.5f;
            p.noiseLevel     = 0.32f * 0.5f;
            p.transientLevel = 0.15f * 0.7f;
            p.pitchEnvDepth  = 21.29f;
            p.modeRatioBase  = 1.5f;
            p.modeDecay      = 0.055f;
            p.duration       = 0.055f * 3.0f + 0.1f;
            break;
    }
    return p;
}

// ============================================================================
// BASS808 — tonalLevel=0.95, noiseLevel=0, modalLevel=0,
//   transientLevel=punchiness*0.3
// Mapping: subFreq->basePitch, sat->satAmount (satType=1 tape),
//   tail->duration, reese->unisonVoices, reeseDetune->unisonDetune(*80),
//   punch->ampPunchLevel+transientLevel, filterEnv->filterSweepAmt,
//   subOct->subLevel, sustain->envSustainLevel, glideTime->pitchEnvSlow,
//   glideAmt->pitchEnvDepth, oscType=0, subDetune=-12,
//   ampAttack=0.001, ampPunchDecay=0.05, ampBodyDecay=tail*0.7,
//   clickType=0, clickFreq=2000, clickDecay=0.005
// ============================================================================
inline UniversalSynthParams bass808Base (GenreStyle genre)
{
    UniversalSynthParams p;

    // Layer gates
    p.tonalLevel     = 0.95f;
    p.noiseLevel     = 0.0f;
    p.modalLevel     = 0.0f;

    // Fixed 808 params
    p.oscType        = 0;     // sine
    p.subDetune      = -12.0f; // octave below
    p.ampAttack      = 0.001f;
    p.ampPunchDecay  = 0.05f;
    p.clickType      = 0;
    p.clickFreq      = 2000.0f;
    p.clickDecay     = 0.005f;
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.basePitch       = 35.79f;
            p.satAmount       = 0.0f;    p.satType = 1;
            p.duration        = 0.80f;
            p.unisonVoices    = 0.08f;
            p.unisonDetune    = 0.10f * 80.0f;
            p.ampPunchLevel   = 0.87f;
            p.transientLevel  = 0.87f * 0.3f;
            p.filterSweepAmt  = 0.03f;
            p.subLevel        = 0.15f;
            p.envSustainLevel = 0.16f;
            p.pitchEnvSlow    = 0.05f;
            p.pitchEnvDepth   = 3.0f;
            p.ampBodyDecay    = 0.80f * 0.7f;
            break;
        case GenreStyle::HipHop:
            p.basePitch       = 35.49f;
            p.satAmount       = 0.002f;  p.satType = 1;
            p.duration        = 0.75f;
            p.unisonVoices    = 0.02f;
            p.unisonDetune    = 0.08f * 80.0f;
            p.ampPunchLevel   = 0.82f;
            p.transientLevel  = 0.82f * 0.3f;
            p.filterSweepAmt  = 0.02f;
            p.subLevel        = 0.18f;
            p.envSustainLevel = 0.22f;
            p.pitchEnvSlow    = 0.04f;
            p.pitchEnvDepth   = 2.0f;
            p.ampBodyDecay    = 0.75f * 0.7f;
            break;
        case GenreStyle::Techno:
            p.basePitch       = 64.20f;
            p.satAmount       = 0.35f;   p.satType = 1;
            p.duration        = 0.50f;
            p.unisonVoices    = 0.15f;
            p.unisonDetune    = 0.15f * 80.0f;
            p.ampPunchLevel   = 0.005f;
            p.transientLevel  = 0.005f * 0.3f;
            p.filterSweepAmt  = 0.007f;
            p.subLevel        = 0.10f;
            p.envSustainLevel = 0.10f;
            p.pitchEnvSlow    = 0.02f;
            p.pitchEnvDepth   = 1.0f;
            p.ampBodyDecay    = 0.50f * 0.7f;
            break;
        case GenreStyle::House:
            p.basePitch       = 33.93f;
            p.satAmount       = 0.002f;  p.satType = 1;
            p.duration        = 0.60f;
            p.unisonVoices    = 0.04f;
            p.unisonDetune    = 0.08f * 80.0f;
            p.ampPunchLevel   = 0.97f;
            p.transientLevel  = 0.97f * 0.3f;
            p.filterSweepAmt  = 0.06f;
            p.subLevel        = 0.15f;
            p.envSustainLevel = 0.10f;
            p.pitchEnvSlow    = 0.03f;
            p.pitchEnvDepth   = 1.5f;
            p.ampBodyDecay    = 0.60f * 0.7f;
            break;
        case GenreStyle::Reggaeton:
            p.basePitch       = 34.32f;
            p.satAmount       = 0.001f;  p.satType = 1;
            p.duration        = 0.75f;
            p.unisonVoices    = 0.07f;
            p.unisonDetune    = 0.10f * 80.0f;
            p.ampPunchLevel   = 0.88f;
            p.transientLevel  = 0.88f * 0.3f;
            p.filterSweepAmt  = 0.09f;
            p.subLevel        = 0.15f;
            p.envSustainLevel = 0.19f;
            p.pitchEnvSlow    = 0.06f;
            p.pitchEnvDepth   = 4.0f;
            p.ampBodyDecay    = 0.75f * 0.7f;
            break;
        case GenreStyle::Afrobeat:
            p.basePitch       = 64.50f;
            p.satAmount       = 0.0f;    p.satType = 1;
            p.duration        = 0.55f;
            p.unisonVoices    = 0.03f;
            p.unisonDetune    = 0.06f * 80.0f;
            p.ampPunchLevel   = 1.0f;
            p.transientLevel  = 1.0f * 0.3f;
            p.filterSweepAmt  = 0.01f;
            p.subLevel        = 0.12f;
            p.envSustainLevel = 0.10f;
            p.pitchEnvSlow    = 0.03f;
            p.pitchEnvDepth   = 2.0f;
            p.ampBodyDecay    = 0.55f * 0.7f;
            break;
        case GenreStyle::RnB:
            p.basePitch       = 42.93f;
            p.satAmount       = 0.63f;   p.satType = 1;
            p.duration        = 0.70f;
            p.unisonVoices    = 0.03f;
            p.unisonDetune    = 0.10f * 80.0f;
            p.ampPunchLevel   = 0.43f;
            p.transientLevel  = 0.43f * 0.3f;
            p.filterSweepAmt  = 0.17f;
            p.subLevel        = 0.15f;
            p.envSustainLevel = 0.10f;
            p.pitchEnvSlow    = 0.04f;
            p.pitchEnvDepth   = 2.0f;
            p.ampBodyDecay    = 0.70f * 0.7f;
            break;
        case GenreStyle::EDM:
            p.basePitch       = 34.12f;
            p.satAmount       = 0.37f;   p.satType = 1;
            p.duration        = 0.60f;
            p.unisonVoices    = 0.19f;
            p.unisonDetune    = 0.15f * 80.0f;
            p.ampPunchLevel   = 0.70f;
            p.transientLevel  = 0.70f * 0.3f;
            p.filterSweepAmt  = 0.71f;
            p.subLevel        = 0.12f;
            p.envSustainLevel = 0.11f;
            p.pitchEnvSlow    = 0.04f;
            p.pitchEnvDepth   = 3.0f;
            p.ampBodyDecay    = 0.60f * 0.7f;
            break;
        case GenreStyle::Ambient:
            p.basePitch       = 35.55f;
            p.satAmount       = 0.78f;   p.satType = 1;
            p.duration        = 1.0f;
            p.unisonVoices    = 0.20f;
            p.unisonDetune    = 0.20f * 80.0f;
            p.ampPunchLevel   = 0.003f;
            p.transientLevel  = 0.003f * 0.3f;
            p.filterSweepAmt  = 0.78f;
            p.subLevel        = 0.20f;
            p.envSustainLevel = 0.10f;
            p.pitchEnvSlow    = 0.05f;
            p.pitchEnvDepth   = 2.0f;
            p.ampBodyDecay    = 1.0f * 0.7f;
            break;
    }
    return p;
}

// ============================================================================
// LEAD — tonalLevel=0.9, noiseLevel=0, modalLevel=0, transientLevel=0
// Mapping: detune->unisonDetune(*800), vibRate->wobbleRate, vibDepth->pitchWobble,
//   bright->filterCutoff(bright*15000+2000),
//   waveMix->oscType(<0.3:2=saw, else 3=square),
//   unison->unisonVoices(/8.0), filterEnv->filterSweepAmt,
//   subOsc->subLevel, portamento->pitchEnvSlow(small),
//   duration=0.5, ampPunchDecay=0.1, ampBodyDecay=0.3, ampPunchLevel=0.4
// ============================================================================
inline UniversalSynthParams leadBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Layer gates
    p.tonalLevel     = 0.9f;
    p.noiseLevel     = 0.0f;
    p.modalLevel     = 0.0f;
    p.transientLevel = 0.0f;

    // Fixed lead params
    p.duration       = 0.5f;
    p.ampPunchDecay  = 0.1f;
    p.ampBodyDecay   = 0.3f;
    p.ampPunchLevel  = 0.4f;
    p.ampAttack      = 0.001f;
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.unisonDetune   = 0.004f * 800.0f;
            p.wobbleRate     = 0.98f;
            p.pitchWobble    = 0.08f;
            p.filterCutoff   = 0.98f * 15000.0f + 2000.0f;
            p.oscType        = (0.30f < 0.3f) ? 2 : 3;   // waveMix=0.30 -> saw
            p.unisonVoices   = 3.0f / 8.0f;
            p.filterSweepAmt = 0.20f;
            p.subLevel       = 0.10f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::HipHop:
            p.unisonDetune   = 0.045f * 800.0f;
            p.wobbleRate     = 5.46f;
            p.pitchWobble    = 0.08f;
            p.filterCutoff   = 0.97f * 15000.0f + 2000.0f;
            p.oscType        = (0.35f < 0.3f) ? 2 : 3;   // waveMix=0.35 -> square
            p.unisonVoices   = 3.0f / 8.0f;
            p.filterSweepAmt = 0.15f;
            p.subLevel       = 0.12f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::Techno:
            p.unisonDetune   = 0.004f * 800.0f;
            p.wobbleRate     = 4.13f;
            p.pitchWobble    = 0.05f;
            p.filterCutoff   = 0.004f * 15000.0f + 2000.0f;
            p.oscType        = (0.50f < 0.3f) ? 2 : 3;   // waveMix=0.50 -> square
            p.unisonVoices   = 2.0f / 8.0f;
            p.filterSweepAmt = 0.25f;
            p.subLevel       = 0.08f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::House:
            p.unisonDetune   = 0.004f * 800.0f;
            p.wobbleRate     = 0.96f;
            p.pitchWobble    = 0.06f;
            p.filterCutoff   = 0.01f * 15000.0f + 2000.0f;
            p.oscType        = (0.40f < 0.3f) ? 2 : 3;   // waveMix=0.40 -> square
            p.unisonVoices   = 3.0f / 8.0f;
            p.filterSweepAmt = 0.18f;
            p.subLevel       = 0.10f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::Reggaeton:
            p.unisonDetune   = 0.045f * 800.0f;
            p.wobbleRate     = 6.03f;
            p.pitchWobble    = 0.06f;
            p.filterCutoff   = 0.98f * 15000.0f + 2000.0f;
            p.oscType        = (0.35f < 0.3f) ? 2 : 3;   // waveMix=0.35 -> square
            p.unisonVoices   = 3.0f / 8.0f;
            p.filterSweepAmt = 0.20f;
            p.subLevel       = 0.10f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::Afrobeat:
            p.unisonDetune   = 0.041f * 800.0f;
            p.wobbleRate     = 6.19f;
            p.pitchWobble    = 0.08f;
            p.filterCutoff   = 0.98f * 15000.0f + 2000.0f;
            p.oscType        = (0.35f < 0.3f) ? 2 : 3;   // waveMix=0.35 -> square
            p.unisonVoices   = 2.0f / 8.0f;
            p.filterSweepAmt = 0.15f;
            p.subLevel       = 0.10f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::RnB:
            p.unisonDetune   = 0.049f * 800.0f;
            p.wobbleRate     = 0.67f;
            p.pitchWobble    = 0.10f;
            p.filterCutoff   = 0.52f * 15000.0f + 2000.0f;
            p.oscType        = (0.40f < 0.3f) ? 2 : 3;   // waveMix=0.40 -> square
            p.unisonVoices   = 3.0f / 8.0f;
            p.filterSweepAmt = 0.15f;
            p.subLevel       = 0.15f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::EDM:
            p.unisonDetune   = 0.003f * 800.0f;
            p.wobbleRate     = 2.74f;
            p.pitchWobble    = 0.05f;
            p.filterCutoff   = 0.18f * 15000.0f + 2000.0f;
            p.oscType        = (0.30f < 0.3f) ? 2 : 3;   // waveMix=0.30 -> saw
            p.unisonVoices   = 4.0f / 8.0f;
            p.filterSweepAmt = 0.30f;
            p.subLevel       = 0.08f;
            p.pitchEnvSlow   = 0.01f;
            break;
        case GenreStyle::Ambient:
            p.unisonDetune   = 0.005f * 800.0f;
            p.wobbleRate     = 2.54f;
            p.pitchWobble    = 0.12f;
            p.filterCutoff   = 0.99f * 15000.0f + 2000.0f;
            p.oscType        = (0.55f < 0.3f) ? 2 : 3;   // waveMix=0.55 -> square
            p.unisonVoices   = 4.0f / 8.0f;
            p.filterSweepAmt = 0.12f;
            p.subLevel       = 0.15f;
            p.pitchEnvSlow   = 0.01f;
            break;
    }
    return p;
}

// ============================================================================
// PLUCK — tonalLevel=0.1, noiseLevel=0, modalLevel=0.9, transientLevel=0.3
// Uses KS mode: modalMode=1.0
// Mapping: bright->ksBrightness, bodyRes->ksBodyResonance,
//   decay->modeDecay+ampBodyDecay, damp->ksDamping,
//   pick->ksPickPosition, tension->ksFeedback(0.9+tension*0.099),
//   harm->additiveAmt, stereo->stereoWidth,
//   basePitch=440, clickType=1(impulse), clickFreq=6000, clickDecay=0.001,
//   ampAttack=0.001, ampPunchDecay=0.02, ampPunchLevel=0.8
// ============================================================================
inline UniversalSynthParams pluckBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Layer gates
    p.tonalLevel     = 0.1f;
    p.noiseLevel     = 0.0f;
    p.modalLevel     = 0.9f;
    p.transientLevel = 0.3f;

    // Fixed pluck params — KS mode
    p.modalMode      = 1.0f;
    p.basePitch      = 440.0f;
    p.clickType      = 1;     // impulse
    p.clickFreq      = 6000.0f;
    p.clickDecay     = 0.001f;
    p.ampAttack      = 0.001f;
    p.ampPunchDecay  = 0.02f;
    p.ampPunchLevel  = 0.8f;
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.ksBrightness   = 0.90f;
            p.ksBodyResonance= 0.99f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 1.0f;
            p.ksPickPosition = 0.50f;
            p.ksFeedback     = 0.9f + 0.55f * 0.099f;
            p.additiveAmt    = 0.20f;
            p.stereoWidth    = 0.40f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::HipHop:
            p.ksBrightness   = 0.85f;
            p.ksBodyResonance= 0.95f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 0.99f;
            p.ksPickPosition = 0.45f;
            p.ksFeedback     = 0.9f + 0.50f * 0.099f;
            p.additiveAmt    = 0.20f;
            p.stereoWidth    = 0.35f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::Techno:
            p.ksBrightness   = 0.86f;
            p.ksBodyResonance= 0.99f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 0.99f;
            p.ksPickPosition = 0.55f;
            p.ksFeedback     = 0.9f + 0.60f * 0.099f;
            p.additiveAmt    = 0.25f;
            p.stereoWidth    = 0.40f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::House:
            p.ksBrightness   = 0.90f;
            p.ksBodyResonance= 0.97f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 1.0f;
            p.ksPickPosition = 0.45f;
            p.ksFeedback     = 0.9f + 0.50f * 0.099f;
            p.additiveAmt    = 0.18f;
            p.stereoWidth    = 0.40f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::Reggaeton:
            p.ksBrightness   = 0.85f;
            p.ksBodyResonance= 0.98f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 0.99f;
            p.ksPickPosition = 0.50f;
            p.ksFeedback     = 0.9f + 0.55f * 0.099f;
            p.additiveAmt    = 0.18f;
            p.stereoWidth    = 0.35f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::Afrobeat:
            p.ksBrightness   = 0.79f;
            p.ksBodyResonance= 0.84f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 0.99f;
            p.ksPickPosition = 0.50f;
            p.ksFeedback     = 0.9f + 0.50f * 0.099f;
            p.additiveAmt    = 0.22f;
            p.stereoWidth    = 0.35f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::RnB:
            p.ksBrightness   = 0.65f;
            p.ksBodyResonance= 1.0f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 1.0f;
            p.ksPickPosition = 0.40f;
            p.ksFeedback     = 0.9f + 0.45f * 0.099f;
            p.additiveAmt    = 0.15f;
            p.stereoWidth    = 0.40f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::EDM:
            p.ksBrightness   = 0.70f;
            p.ksBodyResonance= 0.95f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 0.97f;
            p.ksPickPosition = 0.55f;
            p.ksFeedback     = 0.9f + 0.60f * 0.099f;
            p.additiveAmt    = 0.25f;
            p.stereoWidth    = 0.45f;
            p.duration       = 0.50f + 0.2f;
            break;
        case GenreStyle::Ambient:
            p.ksBrightness   = 0.90f;
            p.ksBodyResonance= 0.93f;
            p.modeDecay      = 0.50f;
            p.ampBodyDecay   = 0.50f;
            p.ksDamping      = 0.98f;
            p.ksPickPosition = 0.45f;
            p.ksFeedback     = 0.9f + 0.45f * 0.099f;
            p.additiveAmt    = 0.20f;
            p.stereoWidth    = 0.50f;
            p.duration       = 0.50f + 0.2f;
            break;
    }
    return p;
}

// ============================================================================
// PAD — tonalLevel=0.85, noiseLevel=airAmount, modalLevel=0, transientLevel=0
// Mapping: unison->unisonVoices(/8.0), detune->unisonDetune(*200),
//   drift->unisonDrift(*3), warmth->filterCutoff(warmth*4000+500),
//   filterSweep->filterSweepAmt, stereo->stereoWidth, chorus->chorusAmt,
//   air->noiseLevel+noiseColor=0.7+noiseFilterFreq=6000,
//   sub->subLevel, attack->ampAttack, release->envRelease,
//   oscType=2(saw), basePitch=220,
//   ampPunchDecay=0.5, ampBodyDecay=1.5, envSustainLevel=0.7, envSustainTime=1.0,
//   duration=attack+1.5+release
// ============================================================================
inline UniversalSynthParams padBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Layer gates
    p.tonalLevel     = 0.85f;
    p.modalLevel     = 0.0f;
    p.transientLevel = 0.0f;

    // Fixed pad params
    p.oscType        = 2;      // saw
    p.basePitch      = 220.0f;
    p.ampPunchDecay  = 0.5f;
    p.ampBodyDecay   = 1.5f;
    p.envSustainLevel= 0.7f;
    p.envSustainTime = 1.0f;
    p.noiseColor     = 0.7f;
    p.noiseFilterFreq= 6000.0f;
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.unisonVoices   = 5.0f / 8.0f;
            p.unisonDetune   = 0.027f * 200.0f;
            p.unisonDrift    = 0.20f * 3.0f;
            p.filterCutoff   = 1.0f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.95f;
            p.stereoWidth    = 0.75f;
            p.chorusAmt      = 0.58f;
            p.noiseLevel     = 0.08f;
            p.subLevel       = 0.30f;
            p.ampAttack      = 0.99f;
            p.envRelease     = 1.54f;
            p.duration       = 0.99f + 1.5f + 1.54f;
            break;
        case GenreStyle::HipHop:
            p.unisonVoices   = 4.0f / 8.0f;
            p.unisonDetune   = 0.15f * 200.0f;
            p.unisonDrift    = 0.15f * 3.0f;
            p.filterCutoff   = 0.65f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.08f;
            p.stereoWidth    = 0.65f;
            p.chorusAmt      = 0.25f;
            p.noiseLevel     = 0.06f;
            p.subLevel       = 0.30f;
            p.ampAttack      = 0.35f;
            p.envRelease     = 1.0f;   // default
            p.duration       = 0.35f + 1.5f + 1.0f;
            break;
        case GenreStyle::Techno:
            p.unisonVoices   = 5.0f / 8.0f;
            p.unisonDetune   = 0.001f * 200.0f;
            p.unisonDrift    = 0.22f * 3.0f;
            p.filterCutoff   = 1.0f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.61f;
            p.stereoWidth    = 0.75f;
            p.chorusAmt      = 0.69f;
            p.noiseLevel     = 0.10f;
            p.subLevel       = 0.28f;
            p.ampAttack      = 0.99f;
            p.envRelease     = 1.46f;
            p.duration       = 0.99f + 1.5f + 1.46f;
            break;
        case GenreStyle::House:
            p.unisonVoices   = 4.0f / 8.0f;
            p.unisonDetune   = 0.013f * 200.0f;
            p.unisonDrift    = 0.18f * 3.0f;
            p.filterCutoff   = 0.99f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.009f;
            p.stereoWidth    = 0.70f;
            p.chorusAmt      = 0.37f;
            p.noiseLevel     = 0.08f;
            p.subLevel       = 0.28f;
            p.ampAttack      = 0.63f;
            p.envRelease     = 1.10f;
            p.duration       = 0.63f + 1.5f + 1.10f;
            break;
        case GenreStyle::Reggaeton:
            p.unisonVoices   = 4.0f / 8.0f;
            p.unisonDetune   = 0.16f * 200.0f;
            p.unisonDrift    = 0.18f * 3.0f;
            p.filterCutoff   = 0.60f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.06f;
            p.stereoWidth    = 0.60f;
            p.chorusAmt      = 0.25f;
            p.noiseLevel     = 0.05f;
            p.subLevel       = 0.32f;
            p.ampAttack      = 0.35f;
            p.envRelease     = 1.0f;   // default
            p.duration       = 0.35f + 1.5f + 1.0f;
            break;
        case GenreStyle::Afrobeat:
            p.unisonVoices   = 4.0f / 8.0f;
            p.unisonDetune   = 0.013f * 200.0f;
            p.unisonDrift    = 0.22f * 3.0f;
            p.filterCutoff   = 1.0f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.86f;
            p.stereoWidth    = 0.60f;
            p.chorusAmt      = 0.64f;
            p.noiseLevel     = 0.08f;
            p.subLevel       = 0.25f;
            p.ampAttack      = 1.0f;
            p.envRelease     = 1.50f;
            p.duration       = 1.0f + 1.5f + 1.50f;
            break;
        case GenreStyle::RnB:
            p.unisonVoices   = 5.0f / 8.0f;
            p.unisonDetune   = 0.012f * 200.0f;
            p.unisonDrift    = 0.12f * 3.0f;
            p.filterCutoff   = 1.0f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.62f;
            p.stereoWidth    = 0.70f;
            p.chorusAmt      = 0.98f;
            p.noiseLevel     = 0.10f;
            p.subLevel       = 0.32f;
            p.ampAttack      = 0.98f;
            p.envRelease     = 1.73f;
            p.duration       = 0.98f + 1.5f + 1.73f;
            break;
        case GenreStyle::EDM:
            p.unisonVoices   = 6.0f / 8.0f;
            p.unisonDetune   = 0.012f * 200.0f;
            p.unisonDrift    = 0.18f * 3.0f;
            p.filterCutoff   = 0.95f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.85f;
            p.stereoWidth    = 0.80f;
            p.chorusAmt      = 0.80f;
            p.noiseLevel     = 0.10f;
            p.subLevel       = 0.25f;
            p.ampAttack      = 0.99f;
            p.envRelease     = 1.60f;
            p.duration       = 0.99f + 1.5f + 1.60f;
            break;
        case GenreStyle::Ambient:
            p.unisonVoices   = 6.0f / 8.0f;
            p.unisonDetune   = 0.20f * 200.0f;
            p.unisonDrift    = 0.30f * 3.0f;
            p.filterCutoff   = 0.78f * 4000.0f + 500.0f;
            p.filterSweepAmt = 0.12f;
            p.stereoWidth    = 0.85f;
            p.chorusAmt      = 0.35f;
            p.noiseLevel     = 0.15f;
            p.subLevel       = 0.38f;
            p.ampAttack      = 1.0f;
            p.envRelease     = 1.5f;
            p.duration       = 1.0f + 1.5f + 1.5f;
            break;
    }
    return p;
}

// ============================================================================
// TEXTURE — tonalLevel=0.1, noiseLevel=0.8, modalLevel=0.1, transientLevel=0
// Mapping: density->granularDensity, grainSize->noiseDecay(*20),
//   scatter->noiseEvolution, tilt->noiseColor((tilt+1)/2),
//   movement->pitchWobble(*0.5), stereo->stereoWidth+noiseStereo(*0.8),
//   evolution->noiseEvolution(combine with scatter),
//   basePitch=300, oscType=0,
//   duration=1.5, ampAttack=0.2, ampPunchDecay=0.5, ampBodyDecay=1.0,
//   ampPunchLevel=0.3, envSustainLevel=0.5, envSustainTime=0.5,
//   noiseFilterFreq=4000, noiseFilterQ=0.2,
//   numModes=3, modeDecay=1.0, modeSpread=0.3
// ============================================================================
inline UniversalSynthParams textureBase (GenreStyle genre)
{
    UniversalSynthParams p;

    // Layer gates
    p.tonalLevel     = 0.1f;
    p.noiseLevel     = 0.8f;
    p.modalLevel     = 0.1f;
    p.transientLevel = 0.0f;

    // Fixed texture params
    p.basePitch      = 300.0f;
    p.oscType        = 0;     // sine
    p.duration       = 1.5f;
    p.ampAttack      = 0.2f;
    p.ampPunchDecay  = 0.5f;
    p.ampBodyDecay   = 1.0f;
    p.ampPunchLevel  = 0.3f;
    p.envSustainLevel= 0.5f;
    p.envSustainTime = 0.5f;
    p.noiseFilterFreq= 4000.0f;
    p.noiseFilterQ   = 0.2f;
    p.numModes       = 3.0f;
    p.modeDecay      = 1.0f;
    p.modeSpread     = 0.3f;
    p.masterGain     = 0.9f;

    switch (genre)
    {
        case GenreStyle::Trap:
            p.granularDensity= 0.10f;
            p.noiseDecay     = 0.005f * 20.0f;
            p.noiseEvolution = (0.30f + 0.15f) * 0.5f;
            p.noiseColor     = (-0.69f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.68f * 0.5f;
            p.stereoWidth    = 0.70f;
            p.noiseStereo    = 0.70f * 0.8f;
            break;
        case GenreStyle::HipHop:
            p.granularDensity= 0.45f;
            p.noiseDecay     = 0.005f * 20.0f;
            p.noiseEvolution = (0.25f + 0.10f) * 0.5f;
            p.noiseColor     = (0.16f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.12f * 0.5f;
            p.stereoWidth    = 0.60f;
            p.noiseStereo    = 0.60f * 0.8f;
            break;
        case GenreStyle::Techno:
            p.granularDensity= 0.10f;
            p.noiseDecay     = 0.005f * 20.0f;
            p.noiseEvolution = (0.35f + 0.20f) * 0.5f;
            p.noiseColor     = (-0.77f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.42f * 0.5f;
            p.stereoWidth    = 0.75f;
            p.noiseStereo    = 0.75f * 0.8f;
            break;
        case GenreStyle::House:
            p.granularDensity= 0.10f;
            p.noiseDecay     = 0.005f * 20.0f;
            p.noiseEvolution = (0.28f + 0.12f) * 0.5f;
            p.noiseColor     = (-0.75f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.62f * 0.5f;
            p.stereoWidth    = 0.65f;
            p.noiseStereo    = 0.65f * 0.8f;
            break;
        case GenreStyle::Reggaeton:
            p.granularDensity= 0.19f;
            p.noiseDecay     = 0.005f * 20.0f;
            p.noiseEvolution = (0.25f + 0.12f) * 0.5f;
            p.noiseColor     = (-0.96f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.45f * 0.5f;
            p.stereoWidth    = 0.60f;
            p.noiseStereo    = 0.60f * 0.8f;
            break;
        case GenreStyle::Afrobeat:
            p.granularDensity= 0.10f;
            p.noiseDecay     = 0.005f * 20.0f;
            p.noiseEvolution = (0.30f + 0.15f) * 0.5f;
            p.noiseColor     = (0.24f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.44f * 0.5f;
            p.stereoWidth    = 0.65f;
            p.noiseStereo    = 0.65f * 0.8f;
            break;
        case GenreStyle::RnB:
            p.granularDensity= 0.10f;
            p.noiseDecay     = 0.005f * 20.0f;
            p.noiseEvolution = (0.20f + 0.08f) * 0.5f;
            p.noiseColor     = (0.33f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.46f * 0.5f;
            p.stereoWidth    = 0.65f;
            p.noiseStereo    = 0.65f * 0.8f;
            break;
        case GenreStyle::EDM:
            p.granularDensity= 0.10f;
            p.noiseDecay     = 0.006f * 20.0f;
            p.noiseEvolution = (0.35f + 0.22f) * 0.5f;
            p.noiseColor     = (-0.83f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.80f * 0.5f;
            p.stereoWidth    = 0.80f;
            p.noiseStereo    = 0.80f * 0.8f;
            break;
        case GenreStyle::Ambient:
            p.granularDensity= 0.10f;
            p.noiseDecay     = 0.006f * 20.0f;
            p.noiseEvolution = (0.40f + 0.25f) * 0.5f;
            p.noiseColor     = (-0.67f + 1.0f) / 2.0f;
            p.pitchWobble    = 0.24f * 0.5f;
            p.stereoWidth    = 0.90f;
            p.noiseStereo    = 0.90f * 0.8f;
            break;
    }
    return p;
}

// ============================================================================
// Master dispatch — returns base params for any instrument x genre combination
// ============================================================================
inline UniversalSynthParams getBase (InstrumentType instrument, GenreStyle genre)
{
    switch (instrument)
    {
        case InstrumentType::Kick:    return kickBase (genre);
        case InstrumentType::Snare:   return snareBase (genre);
        case InstrumentType::HiHat:   return hihatBase (genre);
        case InstrumentType::Clap:    return clapBase (genre);
        case InstrumentType::Perc:    return percBase (genre);
        case InstrumentType::Bass808: return bass808Base (genre);
        case InstrumentType::Lead:    return leadBase (genre);
        case InstrumentType::Pluck:   return pluckBase (genre);
        case InstrumentType::Pad:     return padBase (genre);
        case InstrumentType::Texture: return textureBase (genre);
    }
    return UniversalSynthParams {};  // fallback
}

} // namespace genrerules
} // namespace universalsynth
