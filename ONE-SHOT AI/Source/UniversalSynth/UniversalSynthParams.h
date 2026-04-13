#pragma once

#include <cmath>
#include <array>
#include <vector>
#include <algorithm>

namespace universalsynth
{

// ========== Side-channel data (reference-derived, not optimizable) ==========
// Preserved from OneShotMatch system — same structs, reusable by both modes.

struct WavetableData
{
    static constexpr int MAX_FRAMES = 32;
    static constexpr int FRAME_SIZE = 2048;
    std::array<std::array<float, FRAME_SIZE>, MAX_FRAMES> frames = {};
    int numFrames = 0;
    int cyclesPerFrame = 1;
    float totalDuration = 0.0f;
    bool valid = false;
};

struct ResidualNoiseData
{
    std::vector<float> residual;
    std::vector<float> envelope;
    float sampleRate = 44100.0f;
    bool valid = false;
};

struct TransientSampleData
{
    std::vector<float> samples;
    float sampleRate = 44100.0f;
    bool valid = false;
};

struct HarmonicPhaseData
{
    static constexpr int MAX_HARMONICS = 8;
    std::array<float, MAX_HARMONICS> phases = {};
    int numHarmonics = 0;
    bool valid = false;
};

struct SpectralEnvelopeData
{
    static constexpr int NUM_BANDS = 8;
    std::array<float, NUM_BANDS> targetBandEnergy = {};
    bool valid = false;
};

// Learned profile from K-NN match history
struct LearnedProfile
{
    float envWeight = 1.0f;
    float pitchWeight = 1.0f;
    float spectralWeight = 1.0f;
    float subWeight = 1.0f;
    float transientWeight = 1.0f;
    float spectroTemporalWeight = 1.0f;

    std::vector<float> learnedMins;
    std::vector<float> learnedMaxs;
    bool boundsValid = false;

    std::vector<int> preActivateExtensions;
    int preferredOscType = -1;

    bool valid = false;
};

// ==========================================================================
// UniversalSynthParams — 88 parameters for the universal one-shot synth
//
// Architecture: 4 parallel layers mixed into a shared filter/effects chain
//   Layer A (Tonal):     oscillators + FM + additive + unison + sub
//   Layer B (Noise):     colored noise + bursts + granular + residual
//   Layer C (Modal/KS):  modal resonators + Karplus-Strong
//   Layer D (Transient): clicks + snaps + impulses + transient samples
//
// Each layer has a gate param (xxxLevel): 0 = layer off (skipped for CPU).
// ==========================================================================

struct UniversalSynthParams
{
    // === GLOBAL (6) ===
    int   oscType          = 0;        // 0..13 waveform selector
    float basePitch        = 50.0f;    // 20..12000 Hz
    float duration         = 0.5f;     // 0.01..5.0 s
    float masterGain       = 0.9f;     // 0..1
    float stereoWidth      = 0.0f;     // 0..1
    float pan              = 0.5f;     // 0..1 (0.5=center)

    // === PITCH ENGINE (8) ===
    float pitchEnvDepth    = 0.0f;     // 0..96 semitones
    float pitchEnvFast     = 0.008f;   // 0.0003..0.05 s
    float pitchEnvSlow     = 0.060f;   // 0.003..0.5 s
    float pitchEnvBalance  = 0.65f;    // 0..1
    float pitchHoldTime    = 0.0f;     // 0..0.03 s
    float pitchBounce      = 0.0f;     // 0..1
    float pitchWobble      = 0.0f;     // 0..1
    float wobbleRate       = 8.0f;     // 1..40 Hz

    // === LAYER A — TONAL (18) ===
    float tonalLevel       = 0.8f;     // 0..1 (GATE)
    float bodyHarmonics    = 0.0f;     // 0..1
    float fmDepth          = 0.0f;     // 0..1
    float fmRatio          = 2.0f;     // 0.5..12
    float fmDecay          = 0.05f;    // 0.003..0.3 s
    float additiveAmt      = 0.0f;     // 0..1
    float harmonic2        = 0.0f;     // 0..1
    float harmonic3        = 0.0f;     // 0..1
    float harmonic4        = 0.0f;     // 0..1
    float harmonic5        = 0.0f;     // 0..1
    float inharmonicity    = 0.0f;     // 0..0.1
    float unisonVoices     = 0.0f;     // 0..1 (0=off, >0 = 2-8 voices)
    float unisonDetune     = 10.0f;    // 0.1..80 cents
    float unisonDrift      = 0.0f;     // 0..1
    float phaseDistort     = 0.0f;     // 0..1
    float phaseDistDecay   = 0.05f;    // 0.003..0.2 s
    float subLevel         = 0.0f;     // 0..1
    float subDetune        = 0.0f;     // -12..12 semitones

    // === LAYER B — NOISE (14) ===
    float noiseLevel       = 0.0f;     // 0..1 (GATE)
    float noiseColor       = 0.5f;     // 0..1 (0=brown, 0.5=pink, 1=white)
    float noiseFilterFreq  = 4000.0f;  // 100..16000 Hz
    float noiseFilterQ     = 0.3f;     // 0..1
    float noiseDecay       = 0.2f;     // 0.005..2.0 s
    float burstCount       = 1.0f;     // 1..8
    float burstSpacing     = 0.01f;    // 0.003..0.03 s
    float noiseAttack      = 0.0f;     // 0..0.02 s
    float residualAmt      = 0.0f;     // 0..1
    float residualLevel    = 0.5f;     // 0..1
    float noiseHP          = 20.0f;    // 20..2000 Hz
    float noiseStereo      = 0.0f;     // 0..1
    float noiseEvolution   = 0.0f;     // 0..1
    float granularDensity  = 0.0f;     // 0..1 (0=continuous, >0=granular)

    // === LAYER C — MODAL / KARPLUS-STRONG (12) ===
    float modalLevel       = 0.0f;     // 0..1 (GATE) — enabled, optimizer sets when needed
    float modalMode        = 0.0f;     // 0..1 (0=modal resonators, 1=Karplus-Strong)
    float numModes         = 6.0f;     // 1..12
    float modeDecay        = 0.3f;     // 0.01..2.0 s
    float modeSpread       = 0.0f;     // 0..1 (0=harmonic, 1=inharmonic/metallic)
    float modeRatioBase    = 1.0f;     // 1.0..4.0
    float modeDamping      = 0.3f;     // 0..1
    float ksFeedback       = 0.995f;   // 0.9..0.999
    float ksDamping        = 0.5f;     // 0..1
    float ksBrightness     = 0.5f;     // 0..1
    float ksPickPosition   = 0.5f;     // 0..1
    float ksBodyResonance  = 0.0f;     // 0..1

    // === LAYER D — TRANSIENT (8) ===
    float transientLevel   = 0.0f;     // 0..1 (GATE)
    int   clickType        = 0;        // 0..3 (noise/impulse/FM-burst/chirp)
    float clickFreq        = 4000.0f;  // 500..14000 Hz
    float clickDecay       = 0.002f;   // 0.0001..0.015 s
    float clickWidth       = 0.3f;     // 0..1
    float snapAmount       = 0.0f;     // 0..1
    float transientSampleAmt = 0.0f;   // 0..1
    float topNoise         = 0.0f;     // 0..1

    // === AMPLITUDE ENVELOPE (8) ===
    float ampAttack        = 0.001f;   // 0.0001..0.05 s
    float ampPunchDecay    = 0.10f;    // 0.005..0.5 s
    float ampBodyDecay     = 0.40f;    // 0.02..3.0 s
    float ampPunchLevel    = 0.55f;    // 0..1
    float envSustainLevel  = 0.0f;     // 0..1
    float envSustainTime   = 0.0f;     // 0..3.0 s
    float envRelease       = 0.1f;     // 0.005..5.0 s
    float envCurve         = 1.0f;     // 0.1..4.0

    // === FILTER CHAIN (8) ===
    float filterCutoff     = 20000.0f; // 100..20000 Hz
    float filterReso       = 0.0f;     // 0..0.95
    float filterSweepAmt   = 0.0f;     // 0..1
    float filterSweepStart = 8000.0f;  // 200..18000 Hz
    float filterSweepEnd   = 500.0f;   // 50..5000 Hz
    float formantAmt       = 0.0f;     // 0..1
    float formantFreq1     = 700.0f;   // 200..3500 Hz
    float formantFreq2     = 1500.0f;  // 400..5000 Hz

    // === EFFECTS / DYNAMICS (6) ===
    float reverbAmt        = 0.0f;     // 0..1
    float reverbDecay      = 0.3f;     // 0.03..2.0 s
    float chorusAmt        = 0.0f;     // 0..1
    float satAmount        = 0.0f;     // 0..1
    int   satType          = 0;        // 0..2 (soft-clip/tape/tube)
    float compAmount       = 0.0f;     // 0..1

    // === Backward-compatible fields (used by optimizer seeding, not part of the 88-param array) ===
    // These exist so OneShotMatchOptimizer.h compiles without rewriting its seeding logic.
    // They are silently ignored by toArray/fromArray — the seeding will be updated separately.
    float subTailDecay     = 0.1f;
    float clickAmount      = 0.0f;     // maps conceptually to transientLevel * snapAmount
    float distortion       = 0.0f;     // maps conceptually to satAmount
    float noiseAmount      = 0.0f;     // maps conceptually to noiseLevel
    float harmonicEmphasis = 0.0f;     // no direct equivalent
    float bodyMix          = 0.5f;     // maps conceptually to tonalLevel
    float subMix           = 0.5f;     // maps conceptually to subLevel
    float clickMix         = 0.1f;     // maps conceptually to transientLevel
    float topMix           = 0.05f;    // maps conceptually to topNoise
    float subCrossover     = 0.0f;     // no direct equivalent
    float compRatio        = 4.0f;     // compat stub (internal to UniversalSynth)
    float compAttack       = 0.005f;   // compat stub
    float compRelease      = 0.05f;    // compat stub

    // === Interface ===
    static constexpr int NUM_PARAMS = 88;

    // Compatibility: optimizer uses NUM_CORE_PARAMS for phased activation.
    // In the new flat layout all params are "core" — set to NUM_PARAMS so the
    // extension loop (j >= NUM_CORE_PARAMS) becomes a no-op.
    static constexpr int NUM_CORE_PARAMS = NUM_PARAMS;

    // Compatibility: gap analysis calls getExtensionGateIndex.
    // With the flat layout there are no extension gates, so return -1.
    static int getExtensionGateIndex (int /*idx*/) { return -1; }

    // Compatibility: isExtension — always false in flat layout.
    static bool isExtension (int /*idx*/) { return false; }

    // Compatibility: old code uses getParamName, new code uses paramName.
    static const char* getParamName (int index) { return paramName (index); }

    // Param units for display
    static const char* getParamUnit (int index)
    {
        // Frequency params
        if (index == 1 || index == 34 || index == 60 || index == 74 ||
            index == 77 || index == 78 || index == 80 || index == 81 || index == 42)
            return "Hz";
        // Time params
        if (index == 2 || index == 7 || index == 8 || index == 10 ||
            index == 18 || index == 29 || index == 36 || index == 38 || index == 39 ||
            index == 49 || index == 61 || index == 66 || index == 67 || index == 68 ||
            index == 71 || index == 72 || index == 83)
            return "s";
        // Semitone params
        if (index == 6 || index == 31) return "st";
        // Cents
        if (index == 26) return "cents";
        // Rate
        if (index == 13) return "Hz";
        return "";
    }

    void toArray (float* out) const
    {
        // Global (6)
        out[0]  = (float) oscType;  out[1]  = basePitch;       out[2]  = duration;
        out[3]  = masterGain;       out[4]  = stereoWidth;     out[5]  = pan;
        // Pitch Engine (8)
        out[6]  = pitchEnvDepth;    out[7]  = pitchEnvFast;    out[8]  = pitchEnvSlow;
        out[9]  = pitchEnvBalance;  out[10] = pitchHoldTime;   out[11] = pitchBounce;
        out[12] = pitchWobble;      out[13] = wobbleRate;
        // Layer A — Tonal (18)
        out[14] = tonalLevel;       out[15] = bodyHarmonics;   out[16] = fmDepth;
        out[17] = fmRatio;          out[18] = fmDecay;         out[19] = additiveAmt;
        out[20] = harmonic2;        out[21] = harmonic3;       out[22] = harmonic4;
        out[23] = harmonic5;        out[24] = inharmonicity;   out[25] = unisonVoices;
        out[26] = unisonDetune;     out[27] = unisonDrift;     out[28] = phaseDistort;
        out[29] = phaseDistDecay;   out[30] = subLevel;        out[31] = subDetune;
        // Layer B — Noise (14)
        out[32] = noiseLevel;       out[33] = noiseColor;      out[34] = noiseFilterFreq;
        out[35] = noiseFilterQ;     out[36] = noiseDecay;      out[37] = burstCount;
        out[38] = burstSpacing;     out[39] = noiseAttack;     out[40] = residualAmt;
        out[41] = residualLevel;    out[42] = noiseHP;         out[43] = noiseStereo;
        out[44] = noiseEvolution;   out[45] = granularDensity;
        // Layer C — Modal/KS (12)
        out[46] = modalLevel;       out[47] = modalMode;       out[48] = numModes;
        out[49] = modeDecay;        out[50] = modeSpread;      out[51] = modeRatioBase;
        out[52] = modeDamping;      out[53] = ksFeedback;      out[54] = ksDamping;
        out[55] = ksBrightness;     out[56] = ksPickPosition;  out[57] = ksBodyResonance;
        // Layer D — Transient (8)
        out[58] = transientLevel;   out[59] = (float) clickType; out[60] = clickFreq;
        out[61] = clickDecay;       out[62] = clickWidth;      out[63] = snapAmount;
        out[64] = transientSampleAmt; out[65] = topNoise;
        // Amplitude Envelope (8)
        out[66] = ampAttack;        out[67] = ampPunchDecay;   out[68] = ampBodyDecay;
        out[69] = ampPunchLevel;    out[70] = envSustainLevel; out[71] = envSustainTime;
        out[72] = envRelease;       out[73] = envCurve;
        // Filter Chain (8)
        out[74] = filterCutoff;     out[75] = filterReso;      out[76] = filterSweepAmt;
        out[77] = filterSweepStart; out[78] = filterSweepEnd;  out[79] = formantAmt;
        out[80] = formantFreq1;     out[81] = formantFreq2;
        // Effects / Dynamics (6)
        out[82] = reverbAmt;        out[83] = reverbDecay;     out[84] = chorusAmt;
        out[85] = satAmount;        out[86] = (float) satType; out[87] = compAmount;
    }

    void fromArray (const float* in)
    {
        auto cl = [] (float v, float lo, float hi) { return std::max (lo, std::min (v, hi)); };

        // Global
        oscType        = std::max (0, std::min (13, (int) std::round (in[0])));
        basePitch      = cl (in[1],  20.0f, 12000.0f);
        duration       = cl (in[2],  0.01f, 5.0f);
        masterGain     = cl (in[3],  0.0f,  1.0f);
        stereoWidth    = cl (in[4],  0.0f,  1.0f);
        pan            = cl (in[5],  0.0f,  1.0f);
        // Pitch Engine
        pitchEnvDepth  = cl (in[6],  0.0f,  96.0f);
        pitchEnvFast   = cl (in[7],  0.0003f, 0.05f);
        pitchEnvSlow   = cl (in[8],  0.003f, 0.5f);
        pitchEnvBalance= cl (in[9],  0.0f,  1.0f);
        pitchHoldTime  = cl (in[10], 0.0f,  0.03f);
        pitchBounce    = cl (in[11], 0.0f,  1.0f);
        pitchWobble    = cl (in[12], 0.0f,  1.0f);
        wobbleRate     = cl (in[13], 1.0f,  40.0f);
        // Layer A — Tonal
        tonalLevel     = cl (in[14], 0.0f,  1.0f);
        bodyHarmonics  = cl (in[15], 0.0f,  1.0f);
        fmDepth        = cl (in[16], 0.0f,  1.0f);
        fmRatio        = cl (in[17], 0.5f,  12.0f);
        fmDecay        = cl (in[18], 0.003f, 0.3f);
        additiveAmt    = cl (in[19], 0.0f,  1.0f);
        harmonic2      = cl (in[20], 0.0f,  1.0f);
        harmonic3      = cl (in[21], 0.0f,  1.0f);
        harmonic4      = cl (in[22], 0.0f,  1.0f);
        harmonic5      = cl (in[23], 0.0f,  1.0f);
        inharmonicity  = cl (in[24], 0.0f,  0.1f);
        unisonVoices   = cl (in[25], 0.0f,  1.0f);
        unisonDetune   = cl (in[26], 0.1f,  80.0f);
        unisonDrift    = cl (in[27], 0.0f,  1.0f);
        phaseDistort   = cl (in[28], 0.0f,  1.0f);
        phaseDistDecay = cl (in[29], 0.003f, 0.2f);
        subLevel       = cl (in[30], 0.0f,  1.0f);
        subDetune      = cl (in[31], -12.0f, 12.0f);
        // Layer B — Noise
        noiseLevel     = cl (in[32], 0.0f,  1.0f);
        noiseColor     = cl (in[33], 0.0f,  1.0f);
        noiseFilterFreq= cl (in[34], 100.0f, 16000.0f);
        noiseFilterQ   = cl (in[35], 0.0f,  1.0f);
        noiseDecay     = cl (in[36], 0.005f, 2.0f);
        burstCount     = cl (in[37], 1.0f,  8.0f);
        burstSpacing   = cl (in[38], 0.003f, 0.03f);
        noiseAttack    = cl (in[39], 0.0f,  0.02f);
        residualAmt    = cl (in[40], 0.0f,  1.0f);
        residualLevel  = cl (in[41], 0.0f,  1.0f);
        noiseHP        = cl (in[42], 20.0f, 2000.0f);
        noiseStereo    = cl (in[43], 0.0f,  1.0f);
        noiseEvolution = cl (in[44], 0.0f,  1.0f);
        granularDensity= cl (in[45], 0.0f,  1.0f);
        // Layer C — Modal/KS
        modalLevel     = cl (in[46], 0.0f,  1.0f);
        modalMode      = cl (in[47], 0.0f,  1.0f);
        numModes       = cl (in[48], 1.0f,  12.0f);
        modeDecay      = cl (in[49], 0.01f, 2.0f);
        modeSpread     = cl (in[50], 0.0f,  1.0f);
        modeRatioBase  = cl (in[51], 1.0f,  4.0f);
        modeDamping    = cl (in[52], 0.0f,  1.0f);
        ksFeedback     = cl (in[53], 0.9f,  0.999f);
        ksDamping      = cl (in[54], 0.0f,  1.0f);
        ksBrightness   = cl (in[55], 0.0f,  1.0f);
        ksPickPosition = cl (in[56], 0.0f,  1.0f);
        ksBodyResonance= cl (in[57], 0.0f,  1.0f);
        // Layer D — Transient
        transientLevel = cl (in[58], 0.0f,  1.0f);
        clickType      = std::max (0, std::min (3, (int) std::round (in[59])));
        clickFreq      = cl (in[60], 500.0f, 14000.0f);
        clickDecay     = cl (in[61], 0.0001f, 0.015f);
        clickWidth     = cl (in[62], 0.0f,  1.0f);
        snapAmount     = cl (in[63], 0.0f,  1.0f);
        transientSampleAmt = cl (in[64], 0.0f, 1.0f);
        topNoise       = cl (in[65], 0.0f,  1.0f);
        // Amplitude Envelope
        ampAttack      = cl (in[66], 0.0001f, 0.05f);
        ampPunchDecay  = cl (in[67], 0.005f, 0.5f);
        ampBodyDecay   = cl (in[68], 0.02f, 3.0f);
        ampPunchLevel  = cl (in[69], 0.0f,  1.0f);
        envSustainLevel= cl (in[70], 0.0f,  1.0f);
        envSustainTime = cl (in[71], 0.0f,  3.0f);
        envRelease     = cl (in[72], 0.005f, 5.0f);
        envCurve       = cl (in[73], 0.1f,  4.0f);
        // Filter Chain
        filterCutoff   = cl (in[74], 100.0f, 20000.0f);
        filterReso     = cl (in[75], 0.0f,  0.95f);
        filterSweepAmt = cl (in[76], 0.0f,  1.0f);
        filterSweepStart= cl (in[77], 200.0f, 18000.0f);
        filterSweepEnd = cl (in[78], 50.0f, 5000.0f);
        formantAmt     = cl (in[79], 0.0f,  1.0f);
        formantFreq1   = cl (in[80], 200.0f, 3500.0f);
        formantFreq2   = cl (in[81], 400.0f, 5000.0f);
        // Effects / Dynamics
        reverbAmt      = cl (in[82], 0.0f,  1.0f);
        reverbDecay    = cl (in[83], 0.03f, 2.0f);
        chorusAmt      = cl (in[84], 0.0f,  1.0f);
        satAmount      = cl (in[85], 0.0f,  1.0f);
        satType        = std::max (0, std::min (2, (int) std::round (in[86])));
        compAmount     = cl (in[87], 0.0f,  1.0f);
    }

    static void getBounds (float* mins, float* maxs)
    {
        // Global
        mins[0] = 0.0f;     maxs[0] = 13.0f;     // oscType
        mins[1] = 20.0f;    maxs[1] = 12000.0f;   // basePitch
        mins[2] = 0.01f;    maxs[2] = 5.0f;       // duration
        mins[3] = 0.0f;     maxs[3] = 1.0f;       // masterGain
        mins[4] = 0.0f;     maxs[4] = 1.0f;       // stereoWidth
        mins[5] = 0.0f;     maxs[5] = 1.0f;       // pan
        // Pitch Engine
        mins[6] = 0.0f;     maxs[6] = 96.0f;      // pitchEnvDepth
        mins[7] = 0.0003f;  maxs[7] = 0.05f;      // pitchEnvFast
        mins[8] = 0.003f;   maxs[8] = 0.5f;       // pitchEnvSlow
        mins[9] = 0.0f;     maxs[9] = 1.0f;       // pitchEnvBalance
        mins[10]= 0.0f;     maxs[10]= 0.03f;      // pitchHoldTime
        mins[11]= 0.0f;     maxs[11]= 1.0f;       // pitchBounce
        mins[12]= 0.0f;     maxs[12]= 1.0f;       // pitchWobble
        mins[13]= 1.0f;     maxs[13]= 40.0f;      // wobbleRate
        // Layer A — Tonal
        mins[14]= 0.0f;     maxs[14]= 1.0f;       // tonalLevel
        mins[15]= 0.0f;     maxs[15]= 1.0f;       // bodyHarmonics
        mins[16]= 0.0f;     maxs[16]= 1.0f;       // fmDepth
        mins[17]= 0.5f;     maxs[17]= 12.0f;      // fmRatio
        mins[18]= 0.003f;   maxs[18]= 0.3f;       // fmDecay
        mins[19]= 0.0f;     maxs[19]= 1.0f;       // additiveAmt
        mins[20]= 0.0f;     maxs[20]= 1.0f;       // harmonic2
        mins[21]= 0.0f;     maxs[21]= 1.0f;       // harmonic3
        mins[22]= 0.0f;     maxs[22]= 1.0f;       // harmonic4
        mins[23]= 0.0f;     maxs[23]= 1.0f;       // harmonic5
        mins[24]= 0.0f;     maxs[24]= 0.1f;       // inharmonicity
        mins[25]= 0.0f;     maxs[25]= 1.0f;       // unisonVoices
        mins[26]= 0.1f;     maxs[26]= 80.0f;      // unisonDetune
        mins[27]= 0.0f;     maxs[27]= 1.0f;       // unisonDrift
        mins[28]= 0.0f;     maxs[28]= 1.0f;       // phaseDistort
        mins[29]= 0.003f;   maxs[29]= 0.2f;       // phaseDistDecay
        mins[30]= 0.0f;     maxs[30]= 1.0f;       // subLevel
        mins[31]= -12.0f;   maxs[31]= 12.0f;      // subDetune
        // Layer B — Noise
        mins[32]= 0.0f;     maxs[32]= 1.0f;       // noiseLevel
        mins[33]= 0.0f;     maxs[33]= 1.0f;       // noiseColor
        mins[34]= 100.0f;   maxs[34]= 16000.0f;   // noiseFilterFreq
        mins[35]= 0.0f;     maxs[35]= 1.0f;       // noiseFilterQ
        mins[36]= 0.005f;   maxs[36]= 2.0f;       // noiseDecay
        mins[37]= 1.0f;     maxs[37]= 8.0f;       // burstCount
        mins[38]= 0.003f;   maxs[38]= 0.03f;      // burstSpacing
        mins[39]= 0.0f;     maxs[39]= 0.02f;      // noiseAttack
        mins[40]= 0.0f;     maxs[40]= 1.0f;       // residualAmt
        mins[41]= 0.0f;     maxs[41]= 1.0f;       // residualLevel
        mins[42]= 20.0f;    maxs[42]= 2000.0f;    // noiseHP
        mins[43]= 0.0f;     maxs[43]= 1.0f;       // noiseStereo
        mins[44]= 0.0f;     maxs[44]= 1.0f;       // noiseEvolution
        mins[45]= 0.0f;     maxs[45]= 1.0f;       // granularDensity
        // Layer C — Modal/KS
        mins[46]= 0.0f;     maxs[46]= 1.0f;       // modalLevel
        mins[47]= 0.0f;     maxs[47]= 1.0f;       // modalMode
        mins[48]= 1.0f;     maxs[48]= 12.0f;      // numModes
        mins[49]= 0.01f;    maxs[49]= 2.0f;       // modeDecay
        mins[50]= 0.0f;     maxs[50]= 1.0f;       // modeSpread
        mins[51]= 1.0f;     maxs[51]= 4.0f;       // modeRatioBase
        mins[52]= 0.0f;     maxs[52]= 1.0f;       // modeDamping
        mins[53]= 0.9f;     maxs[53]= 0.999f;     // ksFeedback
        mins[54]= 0.0f;     maxs[54]= 1.0f;       // ksDamping
        mins[55]= 0.0f;     maxs[55]= 1.0f;       // ksBrightness
        mins[56]= 0.0f;     maxs[56]= 1.0f;       // ksPickPosition
        mins[57]= 0.0f;     maxs[57]= 1.0f;       // ksBodyResonance
        // Layer D — Transient
        mins[58]= 0.0f;     maxs[58]= 1.0f;       // transientLevel
        mins[59]= 0.0f;     maxs[59]= 3.0f;       // clickType
        mins[60]= 500.0f;   maxs[60]= 14000.0f;   // clickFreq
        mins[61]= 0.0001f;  maxs[61]= 0.015f;     // clickDecay
        mins[62]= 0.0f;     maxs[62]= 1.0f;       // clickWidth
        mins[63]= 0.0f;     maxs[63]= 1.0f;       // snapAmount
        mins[64]= 0.0f;     maxs[64]= 1.0f;       // transientSampleAmt
        mins[65]= 0.0f;     maxs[65]= 1.0f;       // topNoise
        // Amplitude Envelope
        mins[66]= 0.0001f;  maxs[66]= 0.05f;      // ampAttack
        mins[67]= 0.005f;   maxs[67]= 0.5f;       // ampPunchDecay
        mins[68]= 0.02f;    maxs[68]= 3.0f;       // ampBodyDecay
        mins[69]= 0.0f;     maxs[69]= 1.0f;       // ampPunchLevel
        mins[70]= 0.0f;     maxs[70]= 1.0f;       // envSustainLevel
        mins[71]= 0.0f;     maxs[71]= 3.0f;       // envSustainTime
        mins[72]= 0.005f;   maxs[72]= 5.0f;       // envRelease
        mins[73]= 0.1f;     maxs[73]= 4.0f;       // envCurve
        // Filter Chain
        mins[74]= 100.0f;   maxs[74]= 20000.0f;   // filterCutoff
        mins[75]= 0.0f;     maxs[75]= 0.95f;      // filterReso
        mins[76]= 0.0f;     maxs[76]= 1.0f;       // filterSweepAmt
        mins[77]= 200.0f;   maxs[77]= 18000.0f;   // filterSweepStart
        mins[78]= 50.0f;    maxs[78]= 5000.0f;    // filterSweepEnd
        mins[79]= 0.0f;     maxs[79]= 1.0f;       // formantAmt
        mins[80]= 200.0f;   maxs[80]= 3500.0f;    // formantFreq1
        mins[81]= 400.0f;   maxs[81]= 5000.0f;    // formantFreq2
        // Effects / Dynamics
        mins[82]= 0.0f;     maxs[82]= 1.0f;       // reverbAmt
        mins[83]= 0.03f;    maxs[83]= 2.0f;       // reverbDecay
        mins[84]= 0.0f;     maxs[84]= 1.0f;       // chorusAmt
        mins[85]= 0.0f;     maxs[85]= 1.0f;       // satAmount
        mins[86]= 0.0f;     maxs[86]= 2.0f;       // satType
        mins[87]= 0.0f;     maxs[87]= 1.0f;       // compAmount
    }

    // Parameter names for UI/debug
    static const char* paramName (int index)
    {
        static const char* names[NUM_PARAMS] = {
            "oscType", "basePitch", "duration", "masterGain", "stereoWidth", "pan",
            "pitchEnvDepth", "pitchEnvFast", "pitchEnvSlow", "pitchEnvBalance",
            "pitchHoldTime", "pitchBounce", "pitchWobble", "wobbleRate",
            "tonalLevel", "bodyHarmonics", "fmDepth", "fmRatio", "fmDecay",
            "additiveAmt", "harmonic2", "harmonic3", "harmonic4", "harmonic5",
            "inharmonicity", "unisonVoices", "unisonDetune", "unisonDrift",
            "phaseDistort", "phaseDistDecay", "subLevel", "subDetune",
            "noiseLevel", "noiseColor", "noiseFilterFreq", "noiseFilterQ",
            "noiseDecay", "burstCount", "burstSpacing", "noiseAttack",
            "residualAmt", "residualLevel", "noiseHP", "noiseStereo",
            "noiseEvolution", "granularDensity",
            "modalLevel", "modalMode", "numModes", "modeDecay", "modeSpread",
            "modeRatioBase", "modeDamping", "ksFeedback", "ksDamping",
            "ksBrightness", "ksPickPosition", "ksBodyResonance",
            "transientLevel", "clickType", "clickFreq", "clickDecay",
            "clickWidth", "snapAmount", "transientSampleAmt", "topNoise",
            "ampAttack", "ampPunchDecay", "ampBodyDecay", "ampPunchLevel",
            "envSustainLevel", "envSustainTime", "envRelease", "envCurve",
            "filterCutoff", "filterReso", "filterSweepAmt", "filterSweepStart",
            "filterSweepEnd", "formantAmt", "formantFreq1", "formantFreq2",
            "reverbAmt", "reverbDecay", "chorusAmt", "satAmount", "satType", "compAmount"
        };
        if (index >= 0 && index < NUM_PARAMS) return names[index];
        return "unknown";
    }
};

} // namespace universalsynth
