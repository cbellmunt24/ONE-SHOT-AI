#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <random>
#include <array>
#include "../DSP/DSPConstants.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Saturator.h"
#include "../DSP/DCBlocker.h"
#include "../SynthEngine/SynthUtils.h"

// Universal one-shot synthesizer for the OneShotMatch system.
//
// 95 parameters total:
//   22 CORE params — always active
//   73 EXTENSION params (v1..v5) — activated by gap analysis:
//     v1 (17): FM, resonance, wobble, snap, comb, multiband sat, phase distort
//     v2 (25): additive, multi-reson, noise shape, EQ, env complex, stereo
//     v3 (12): unison, formant, transient layer, reverb
//     v4 (10): mix levels, filter sweep, sub pitch, formant Q
//     v5 (9):  residual, harmonics 5-8, spectral match, sub wavetable, transient sample

namespace oneshotmatch
{

// ========== Side-channel data (reference-derived, not optimizable) ==========

struct WavetableData
{
    static constexpr int MAX_FRAMES = 32;
    static constexpr int FRAME_SIZE = 2048;
    std::array<std::array<float, FRAME_SIZE>, MAX_FRAMES> frames = {};
    int numFrames = 0;
    int cyclesPerFrame = 1;  // 1-4 cycles captured per frame
    float totalDuration = 0.0f;  // total duration covered by frames (seconds)
    bool valid = false;
};

struct ResidualNoiseData
{
    std::vector<float> residual;
    std::vector<float> envelope;  // per-sample amplitude envelope of the residual
    float sampleRate = 44100.0f;
    bool valid = false;
};

struct TransientSampleData
{
    std::vector<float> samples;  // first ~5ms of reference after peak
    float sampleRate = 44100.0f;
    bool valid = false;
};

struct HarmonicPhaseData
{
    static constexpr int MAX_HARMONICS = 8;
    std::array<float, MAX_HARMONICS> phases = {};  // initial phase offset (0..2π) for harmonics 1-8
    int numHarmonics = 0;
    bool valid = false;
};

struct SpectralEnvelopeData
{
    static constexpr int NUM_BANDS = 8;
    std::array<float, NUM_BANDS> targetBandEnergy = {};  // normalized band energies from reference body
    bool valid = false;
};

// Learned profile computed from K-NN match history — passed to optimizer
struct LearnedProfile
{
    // 1. Adaptive distance weights (multipliers on default weights)
    float envWeight = 1.0f;
    float pitchWeight = 1.0f;
    float spectralWeight = 1.0f;
    float subWeight = 1.0f;
    float transientWeight = 1.0f;
    float spectroTemporalWeight = 1.0f;

    // 2. Learned param bounds (narrowed from successful matches)
    // Allocated dynamically since NUM_PARAMS not yet known — filled by engine
    std::vector<float> learnedMins;
    std::vector<float> learnedMaxs;
    bool boundsValid = false;

    // 3. Pre-activated extensions (from similar matches)
    std::vector<int> preActivateExtensions;
    int preferredOscType = -1;  // -1 = no preference

    bool valid = false;
};

struct MatchSynthParams
{
    // === CORE (22) ===
    int   oscType          = 0;
    float basePitch        = 50.0f;
    float bodyHarmonics    = 0.0f;
    float pitchEnvDepth    = 48.0f;
    float pitchEnvFast     = 0.008f;
    float pitchEnvSlow     = 0.060f;
    float pitchEnvBalance  = 0.65f;
    float ampAttack        = 0.001f;
    float ampPunchDecay    = 0.10f;
    float ampBodyDecay     = 0.40f;
    float ampPunchLevel    = 0.55f;
    float subLevel         = 0.80f;
    float subTailDecay     = 0.30f;
    float subDetune        = 0.0f;
    float clickAmount      = 0.30f;
    float clickFreq        = 3500.0f;
    float clickWidth       = 0.30f;
    float clickDecay       = 0.0015f;
    float distortion       = 0.0f;
    float noiseAmount      = 0.0f;
    float filterCutoff     = 20000.0f;
    float harmonicEmphasis = 0.0f;

    // === EXT v1 (17) ===
    float fmDepth          = 0.0f;
    float fmRatio          = 2.0f;
    float fmDecay          = 0.05f;
    float bodyResonance    = 0.0f;
    float bodyResonFreq    = 200.0f;
    float pitchWobble      = 0.0f;
    float wobbleRate       = 8.0f;
    float wobbleDecay      = 0.05f;
    float transientSnap    = 0.0f;
    float transientHold    = 0.001f;
    float combAmount       = 0.0f;
    float combFreq         = 150.0f;
    float combFeedback     = 0.3f;
    float lowSaturation    = 0.0f;
    float highSaturation   = 0.0f;
    float phaseDistort     = 0.0f;
    float phaseDistDecay   = 0.05f;

    // === EXT v2 (25) ===
    float additiveAmt      = 0.0f;
    float harmonic2        = 0.0f;
    float harmonic3        = 0.0f;
    float harmonic4        = 0.0f;
    float harmonicDecayRate = 2.0f;
    float inharmonicity    = 0.0f;
    float reson2Amt        = 0.0f;
    float reson2Freq       = 300.0f;
    float reson3Amt        = 0.0f;
    float reson3Freq       = 600.0f;
    float noiseShapeAmt    = 0.0f;
    float noiseColor       = 0.5f;
    float noiseFilterFreq  = 4000.0f;
    float noiseFilterQ     = 0.3f;
    float eqAmount         = 0.0f;
    float eqLowGain        = 0.0f;
    float eqMidGain        = 0.0f;
    float eqMidFreq        = 1000.0f;
    float eqHighGain       = 0.0f;
    float envSustainLevel  = 0.0f;
    float envSustainTime   = 0.1f;
    float envRelease       = 0.1f;
    float envCurve         = 1.0f;
    float stereoWidth      = 0.0f;
    float stereoFreq       = 2000.0f;

    // === EXT v3 (12) ===
    float unisonAmt        = 0.0f;    // 0..1 (0=off)
    float unisonDetune     = 10.0f;   // 0.1..50 cents
    float unisonSpread     = 0.5f;    // 0..1, stereo spread
    float formantAmt       = 0.0f;    // 0..1 (0=off)
    float formantFreq1     = 700.0f;  // 200..3000 Hz
    float formantFreq2     = 1500.0f; // 500..5000 Hz
    float transLayerAmt    = 0.0f;    // 0..1 (0=off)
    float transLayerFreq   = 5000.0f; // 1000..10000 Hz
    float transLayerDecay  = 0.003f;  // 0.0005..0.01 s
    float reverbAmt        = 0.0f;    // 0..1 (0=off)
    float reverbDecay      = 0.3f;    // 0.05..2.0 s
    float reverbDamp       = 0.5f;    // 0..1

    // === EXT v4 (10) ===
    float bodyMix          = 0.50f;   // 0.05..1.0 — body component level
    float subMix           = 0.70f;   // 0.05..1.0 — sub component level
    float clickMix         = 0.35f;   // 0.0..1.0  — click component level
    float topMix           = 0.70f;   // 0.0..1.0  — top/transient level
    float filterSweepAmt   = 0.0f;    // 0..1 (0=off) — dynamic filter sweep
    float filterSweepStart = 8000.0f; // 200..18000 Hz — filter start freq
    float filterSweepEnd   = 500.0f;  // 50..5000 Hz — filter end freq
    float filterSweepReso  = 0.3f;    // 0..0.9 — filter sweep resonance
    float subPitch         = 0.0f;    // 0..200 Hz (0=follow basePitch+detune)
    float formantQ         = 0.5f;    // 0.1..2.0 — formant filter Q

    // === EXT v5 (9) ===
    float residualAmt      = 0.0f;    // 0..1 (0=off) — reference noise residual
    float residualLevel    = 0.5f;    // 0..1 — residual volume
    float harmonic5        = 0.0f;    // 0..1 — 5th harmonic level
    float harmonic6        = 0.0f;    // 0..1 — 6th harmonic level
    float harmonic7        = 0.0f;    // 0..1 — 7th harmonic level
    float harmonic8        = 0.0f;    // 0..1 — 8th harmonic level
    float spectralMatchAmt = 0.0f;    // 0..1 (0=off) — spectral envelope matching
    float subWavetable     = 0.0f;    // 0..1 — crossfade sub sine→wavetable
    float transientSampleAmt = 0.0f;  // 0..1 (0=off) — reference transient sample mix

    // === Interface ===
    static constexpr int NUM_CORE_PARAMS = 22;
    static constexpr int NUM_EXT_PARAMS  = 73;
    static constexpr int NUM_PARAMS      = 95;

    void toArray (float* out) const
    {
        out[0]  = (float) oscType; out[1]  = basePitch; out[2]  = bodyHarmonics;
        out[3]  = pitchEnvDepth; out[4]  = pitchEnvFast; out[5]  = pitchEnvSlow; out[6]  = pitchEnvBalance;
        out[7]  = ampAttack; out[8]  = ampPunchDecay; out[9]  = ampBodyDecay; out[10] = ampPunchLevel;
        out[11] = subLevel; out[12] = subTailDecay; out[13] = subDetune;
        out[14] = clickAmount; out[15] = clickFreq; out[16] = clickWidth; out[17] = clickDecay;
        out[18] = distortion; out[19] = noiseAmount; out[20] = filterCutoff; out[21] = harmonicEmphasis;
        out[22] = fmDepth; out[23] = fmRatio; out[24] = fmDecay;
        out[25] = bodyResonance; out[26] = bodyResonFreq;
        out[27] = pitchWobble; out[28] = wobbleRate; out[29] = wobbleDecay;
        out[30] = transientSnap; out[31] = transientHold;
        out[32] = combAmount; out[33] = combFreq; out[34] = combFeedback;
        out[35] = lowSaturation; out[36] = highSaturation;
        out[37] = phaseDistort; out[38] = phaseDistDecay;
        out[39] = additiveAmt; out[40] = harmonic2; out[41] = harmonic3; out[42] = harmonic4;
        out[43] = harmonicDecayRate; out[44] = inharmonicity;
        out[45] = reson2Amt; out[46] = reson2Freq; out[47] = reson3Amt; out[48] = reson3Freq;
        out[49] = noiseShapeAmt; out[50] = noiseColor; out[51] = noiseFilterFreq; out[52] = noiseFilterQ;
        out[53] = eqAmount; out[54] = eqLowGain; out[55] = eqMidGain; out[56] = eqMidFreq; out[57] = eqHighGain;
        out[58] = envSustainLevel; out[59] = envSustainTime; out[60] = envRelease; out[61] = envCurve;
        out[62] = stereoWidth; out[63] = stereoFreq;
        out[64] = unisonAmt; out[65] = unisonDetune; out[66] = unisonSpread;
        out[67] = formantAmt; out[68] = formantFreq1; out[69] = formantFreq2;
        out[70] = transLayerAmt; out[71] = transLayerFreq; out[72] = transLayerDecay;
        out[73] = reverbAmt; out[74] = reverbDecay; out[75] = reverbDamp;
        out[76] = bodyMix; out[77] = subMix; out[78] = clickMix; out[79] = topMix;
        out[80] = filterSweepAmt; out[81] = filterSweepStart; out[82] = filterSweepEnd; out[83] = filterSweepReso;
        out[84] = subPitch; out[85] = formantQ;
        out[86] = residualAmt; out[87] = residualLevel;
        out[88] = harmonic5; out[89] = harmonic6; out[90] = harmonic7; out[91] = harmonic8;
        out[92] = spectralMatchAmt; out[93] = subWavetable; out[94] = transientSampleAmt;
    }

    void fromArray (const float* in)
    {
        oscType         = std::max (0, std::min (12, (int) std::round (in[0])));
        basePitch       = cl (in[1],  20.0f, 200.0f);     bodyHarmonics  = cl (in[2],  0.0f,  1.0f);
        pitchEnvDepth   = cl (in[3],  0.0f,  72.0f);      pitchEnvFast   = cl (in[4],  0.0005f, 0.05f);
        pitchEnvSlow    = cl (in[5],  0.005f, 0.3f);      pitchEnvBalance= cl (in[6],  0.0f,  1.0f);
        ampAttack       = cl (in[7],  0.0001f, 0.01f);    ampPunchDecay  = cl (in[8],  0.01f, 0.5f);
        ampBodyDecay    = cl (in[9],  0.05f, 1.5f);       ampPunchLevel  = cl (in[10], 0.0f,  1.0f);
        subLevel        = cl (in[11], 0.0f,  1.0f);       subTailDecay   = cl (in[12], 0.05f, 2.0f);
        subDetune       = cl (in[13], -5.0f, 5.0f);       clickAmount    = cl (in[14], 0.0f,  1.0f);
        clickFreq       = cl (in[15], 800.0f, 14000.0f);  clickWidth     = cl (in[16], 0.05f, 1.0f);
        clickDecay      = cl (in[17], 0.0002f, 0.01f);    distortion     = cl (in[18], 0.0f,  1.0f);
        noiseAmount     = cl (in[19], 0.0f,  1.0f);       filterCutoff   = cl (in[20], 200.0f, 20000.0f);
        harmonicEmphasis= cl (in[21], 0.0f, 1.0f);
        fmDepth         = cl (in[22], 0.0f,  1.0f);       fmRatio        = cl (in[23], 0.5f,  8.0f);
        fmDecay         = cl (in[24], 0.005f, 0.2f);      bodyResonance  = cl (in[25], 0.0f,  1.0f);
        bodyResonFreq   = cl (in[26], 60.0f, 800.0f);     pitchWobble    = cl (in[27], 0.0f,  1.0f);
        wobbleRate      = cl (in[28], 2.0f,  30.0f);      wobbleDecay    = cl (in[29], 0.01f, 0.2f);
        transientSnap   = cl (in[30], 0.0f,  1.0f);       transientHold  = cl (in[31], 0.0005f, 0.005f);
        combAmount      = cl (in[32], 0.0f,  1.0f);       combFreq       = cl (in[33], 50.0f, 500.0f);
        combFeedback    = cl (in[34], 0.0f,  0.95f);      lowSaturation  = cl (in[35], 0.0f,  1.0f);
        highSaturation  = cl (in[36], 0.0f,  1.0f);       phaseDistort   = cl (in[37], 0.0f,  1.0f);
        phaseDistDecay  = cl (in[38], 0.005f, 0.15f);
        additiveAmt     = cl (in[39], 0.0f,  1.0f);       harmonic2      = cl (in[40], 0.0f,  1.0f);
        harmonic3       = cl (in[41], 0.0f,  1.0f);       harmonic4      = cl (in[42], 0.0f,  1.0f);
        harmonicDecayRate= cl (in[43], 0.5f, 8.0f);       inharmonicity  = cl (in[44], 0.0f,  0.05f);
        reson2Amt       = cl (in[45], 0.0f,  1.0f);       reson2Freq     = cl (in[46], 60.0f, 2000.0f);
        reson3Amt       = cl (in[47], 0.0f,  1.0f);       reson3Freq     = cl (in[48], 100.0f, 4000.0f);
        noiseShapeAmt   = cl (in[49], 0.0f,  1.0f);       noiseColor     = cl (in[50], 0.0f,  1.0f);
        noiseFilterFreq = cl (in[51], 200.0f, 12000.0f);  noiseFilterQ   = cl (in[52], 0.0f,  1.0f);
        eqAmount        = cl (in[53], 0.0f,  1.0f);       eqLowGain      = cl (in[54], -12.0f, 12.0f);
        eqMidGain       = cl (in[55], -12.0f, 12.0f);     eqMidFreq      = cl (in[56], 200.0f, 5000.0f);
        eqHighGain      = cl (in[57], -12.0f, 12.0f);     envSustainLevel= cl (in[58], 0.0f,  1.0f);
        envSustainTime  = cl (in[59], 0.0f,  2.0f);       envRelease     = cl (in[60], 0.01f, 5.0f);
        envCurve        = cl (in[61], 0.1f,  4.0f);       stereoWidth    = cl (in[62], 0.0f,  1.0f);
        stereoFreq      = cl (in[63], 200.0f, 10000.0f);
        unisonAmt       = cl (in[64], 0.0f,  1.0f);       unisonDetune   = cl (in[65], 0.1f,  50.0f);
        unisonSpread    = cl (in[66], 0.0f,  1.0f);       formantAmt     = cl (in[67], 0.0f,  1.0f);
        formantFreq1    = cl (in[68], 200.0f, 3000.0f);   formantFreq2   = cl (in[69], 500.0f, 5000.0f);
        transLayerAmt   = cl (in[70], 0.0f,  1.0f);       transLayerFreq = cl (in[71], 1000.0f, 10000.0f);
        transLayerDecay = cl (in[72], 0.0005f, 0.01f);    reverbAmt      = cl (in[73], 0.0f,  1.0f);
        reverbDecay     = cl (in[74], 0.05f, 2.0f);       reverbDamp     = cl (in[75], 0.0f,  1.0f);
        bodyMix         = cl (in[76], 0.05f, 1.0f);       subMix         = cl (in[77], 0.05f, 1.0f);
        clickMix        = cl (in[78], 0.0f,  1.0f);       topMix         = cl (in[79], 0.0f,  1.0f);
        filterSweepAmt  = cl (in[80], 0.0f,  1.0f);       filterSweepStart = cl (in[81], 200.0f, 18000.0f);
        filterSweepEnd  = cl (in[82], 50.0f, 5000.0f);    filterSweepReso  = cl (in[83], 0.0f,  0.9f);
        subPitch        = cl (in[84], 0.0f,  200.0f);     formantQ         = cl (in[85], 0.1f,  2.0f);
        residualAmt     = cl (in[86], 0.0f,  1.0f);       residualLevel    = cl (in[87], 0.0f,  1.0f);
        harmonic5       = cl (in[88], 0.0f,  1.0f);       harmonic6        = cl (in[89], 0.0f,  1.0f);
        harmonic7       = cl (in[90], 0.0f,  1.0f);       harmonic8        = cl (in[91], 0.0f,  1.0f);
        spectralMatchAmt= cl (in[92], 0.0f,  1.0f);       subWavetable     = cl (in[93], 0.0f,  1.0f);
        transientSampleAmt = cl (in[94], 0.0f, 1.0f);
    }

    static void getBounds (float* mins, float* maxs)
    {
        mins[0]  = 0.0f;     maxs[0]  = 12.0f;      mins[1]  = 20.0f;    maxs[1]  = 200.0f;
        mins[2]  = 0.0f;     maxs[2]  = 1.0f;      mins[3]  = 0.0f;     maxs[3]  = 72.0f;
        mins[4]  = 0.0005f;  maxs[4]  = 0.05f;     mins[5]  = 0.005f;   maxs[5]  = 0.3f;
        mins[6]  = 0.0f;     maxs[6]  = 1.0f;      mins[7]  = 0.0001f;  maxs[7]  = 0.01f;
        mins[8]  = 0.01f;    maxs[8]  = 0.5f;      mins[9]  = 0.05f;    maxs[9]  = 1.5f;
        mins[10] = 0.0f;     maxs[10] = 1.0f;      mins[11] = 0.0f;     maxs[11] = 1.0f;
        mins[12] = 0.05f;    maxs[12] = 2.0f;      mins[13] = -5.0f;    maxs[13] = 5.0f;
        mins[14] = 0.0f;     maxs[14] = 1.0f;      mins[15] = 800.0f;   maxs[15] = 14000.0f;
        mins[16] = 0.05f;    maxs[16] = 1.0f;      mins[17] = 0.0002f;  maxs[17] = 0.01f;
        mins[18] = 0.0f;     maxs[18] = 1.0f;      mins[19] = 0.0f;     maxs[19] = 1.0f;
        mins[20] = 200.0f;   maxs[20] = 20000.0f;  mins[21] = 0.0f;     maxs[21] = 1.0f;
        mins[22] = 0.0f;     maxs[22] = 1.0f;      mins[23] = 0.5f;     maxs[23] = 8.0f;
        mins[24] = 0.005f;   maxs[24] = 0.2f;      mins[25] = 0.0f;     maxs[25] = 1.0f;
        mins[26] = 60.0f;    maxs[26] = 800.0f;    mins[27] = 0.0f;     maxs[27] = 1.0f;
        mins[28] = 2.0f;     maxs[28] = 30.0f;     mins[29] = 0.01f;    maxs[29] = 0.2f;
        mins[30] = 0.0f;     maxs[30] = 1.0f;      mins[31] = 0.0005f;  maxs[31] = 0.005f;
        mins[32] = 0.0f;     maxs[32] = 1.0f;      mins[33] = 50.0f;    maxs[33] = 500.0f;
        mins[34] = 0.0f;     maxs[34] = 0.95f;     mins[35] = 0.0f;     maxs[35] = 1.0f;
        mins[36] = 0.0f;     maxs[36] = 1.0f;      mins[37] = 0.0f;     maxs[37] = 1.0f;
        mins[38] = 0.005f;   maxs[38] = 0.15f;     mins[39] = 0.0f;     maxs[39] = 1.0f;
        mins[40] = 0.0f;     maxs[40] = 1.0f;      mins[41] = 0.0f;     maxs[41] = 1.0f;
        mins[42] = 0.0f;     maxs[42] = 1.0f;      mins[43] = 0.5f;     maxs[43] = 8.0f;
        mins[44] = 0.0f;     maxs[44] = 0.05f;     mins[45] = 0.0f;     maxs[45] = 1.0f;
        mins[46] = 60.0f;    maxs[46] = 2000.0f;   mins[47] = 0.0f;     maxs[47] = 1.0f;
        mins[48] = 100.0f;   maxs[48] = 4000.0f;   mins[49] = 0.0f;     maxs[49] = 1.0f;
        mins[50] = 0.0f;     maxs[50] = 1.0f;      mins[51] = 200.0f;   maxs[51] = 12000.0f;
        mins[52] = 0.0f;     maxs[52] = 1.0f;      mins[53] = 0.0f;     maxs[53] = 1.0f;
        mins[54] = -12.0f;   maxs[54] = 12.0f;     mins[55] = -12.0f;   maxs[55] = 12.0f;
        mins[56] = 200.0f;   maxs[56] = 5000.0f;   mins[57] = -12.0f;   maxs[57] = 12.0f;
        mins[58] = 0.0f;     maxs[58] = 1.0f;      mins[59] = 0.0f;     maxs[59] = 2.0f;
        mins[60] = 0.01f;    maxs[60] = 5.0f;      mins[61] = 0.1f;     maxs[61] = 4.0f;
        mins[62] = 0.0f;     maxs[62] = 1.0f;      mins[63] = 200.0f;   maxs[63] = 10000.0f;
        mins[64] = 0.0f;     maxs[64] = 1.0f;      mins[65] = 0.1f;     maxs[65] = 50.0f;
        mins[66] = 0.0f;     maxs[66] = 1.0f;      mins[67] = 0.0f;     maxs[67] = 1.0f;
        mins[68] = 200.0f;   maxs[68] = 3000.0f;   mins[69] = 500.0f;   maxs[69] = 5000.0f;
        mins[70] = 0.0f;     maxs[70] = 1.0f;      mins[71] = 1000.0f;  maxs[71] = 10000.0f;
        mins[72] = 0.0005f;  maxs[72] = 0.01f;     mins[73] = 0.0f;     maxs[73] = 1.0f;
        mins[74] = 0.05f;    maxs[74] = 2.0f;      mins[75] = 0.0f;     maxs[75] = 1.0f;
        mins[76] = 0.05f;    maxs[76] = 1.0f;      mins[77] = 0.05f;    maxs[77] = 1.0f;
        mins[78] = 0.0f;     maxs[78] = 1.0f;      mins[79] = 0.0f;     maxs[79] = 1.0f;
        mins[80] = 0.0f;     maxs[80] = 1.0f;      mins[81] = 200.0f;   maxs[81] = 18000.0f;
        mins[82] = 50.0f;    maxs[82] = 5000.0f;   mins[83] = 0.0f;     maxs[83] = 0.9f;
        mins[84] = 0.0f;     maxs[84] = 200.0f;    mins[85] = 0.1f;     maxs[85] = 2.0f;
        mins[86] = 0.0f;     maxs[86] = 1.0f;      mins[87] = 0.0f;     maxs[87] = 1.0f;
        mins[88] = 0.0f;     maxs[88] = 1.0f;      mins[89] = 0.0f;     maxs[89] = 1.0f;
        mins[90] = 0.0f;     maxs[90] = 1.0f;      mins[91] = 0.0f;     maxs[91] = 1.0f;
        mins[92] = 0.0f;     maxs[92] = 1.0f;      mins[93] = 0.0f;     maxs[93] = 1.0f;
        mins[94] = 0.0f;     maxs[94] = 1.0f;
    }

    static const char* getParamName (int idx)
    {
        static const char* n[] = {
            "oscType","basePitch","bodyHarmonics","pitchEnvDepth","pitchEnvFast","pitchEnvSlow","pitchEnvBalance",
            "ampAttack","ampPunchDecay","ampBodyDecay","ampPunchLevel","subLevel","subTailDecay","subDetune",
            "clickAmount","clickFreq","clickWidth","clickDecay","distortion","noiseAmount","filterCutoff","harmonicEmphasis",
            "fmDepth","fmRatio","fmDecay","bodyResonance","bodyResonFreq","pitchWobble","wobbleRate","wobbleDecay",
            "transientSnap","transientHold","combAmount","combFreq","combFeedback","lowSaturation","highSaturation",
            "phaseDistort","phaseDistDecay",
            "additiveAmt","harmonic2","harmonic3","harmonic4","harmonicDecayRate","inharmonicity",
            "reson2Amt","reson2Freq","reson3Amt","reson3Freq",
            "noiseShapeAmt","noiseColor","noiseFilterFreq","noiseFilterQ",
            "eqAmount","eqLowGain","eqMidGain","eqMidFreq","eqHighGain",
            "envSustainLevel","envSustainTime","envRelease","envCurve","stereoWidth","stereoFreq",
            "unisonAmt","unisonDetune","unisonSpread",
            "formantAmt","formantFreq1","formantFreq2",
            "transLayerAmt","transLayerFreq","transLayerDecay",
            "reverbAmt","reverbDecay","reverbDamp",
            "bodyMix","subMix","clickMix","topMix",
            "filterSweepAmt","filterSweepStart","filterSweepEnd","filterSweepReso",
            "subPitch","formantQ",
            "residualAmt","residualLevel",
            "harmonic5","harmonic6","harmonic7","harmonic8",
            "spectralMatchAmt","subWavetable","transientSampleAmt"
        };
        return (idx >= 0 && idx < NUM_PARAMS) ? n[idx] : "?";
    }

    static const char* getParamUnit (int idx)
    {
        static const char* u[] = {
            "","Hz","","st","s","s","","s","s","s","","","s","Hz","","Hz","","s","","","Hz","",
            "","","s","","Hz","","Hz","s","","s","","Hz","","","","","s",
            "","","","","","","","Hz","","Hz","","","Hz","","","dB","dB","Hz","dB","","s","s","","","Hz",
            "","ct","","","Hz","Hz","","Hz","s","","s","",
            "","","","","","Hz","Hz","","Hz","",
            "","","","","","","","",""
        };
        return (idx >= 0 && idx < NUM_PARAMS) ? u[idx] : "";
    }

    static bool isExtension (int idx) { return idx >= NUM_CORE_PARAMS; }

    static int getExtensionGateIndex (int idx)
    {
        if (idx >= 22 && idx <= 24) return 22;
        if (idx >= 25 && idx <= 26) return 25;
        if (idx >= 27 && idx <= 29) return 27;
        if (idx >= 30 && idx <= 31) return 30;
        if (idx >= 32 && idx <= 34) return 32;
        if (idx >= 35 && idx <= 36) return 35;
        if (idx >= 37 && idx <= 38) return 37;
        if (idx >= 39 && idx <= 44) return 39;
        if (idx >= 45 && idx <= 46) return 45;
        if (idx >= 47 && idx <= 48) return 47;
        if (idx >= 49 && idx <= 52) return 49;
        if (idx >= 53 && idx <= 57) return 53;
        if (idx >= 58 && idx <= 61) return 58;
        if (idx >= 62 && idx <= 63) return 62;
        if (idx >= 64 && idx <= 66) return 64;
        if (idx >= 67 && idx <= 69) return 67;
        if (idx >= 70 && idx <= 72) return 70;
        if (idx >= 73 && idx <= 75) return 73;
        if (idx >= 76 && idx <= 79) return 76;  // mix group (always active)
        if (idx >= 80 && idx <= 83) return 80;  // filter sweep
        if (idx == 84) return 84;               // sub pitch
        if (idx == 85) return 85;               // formant Q (activated with formant)
        if (idx >= 86 && idx <= 87) return 86;  // residual noise
        if (idx >= 88 && idx <= 91) return 88;  // harmonics 5-8 (activated with additive)
        if (idx == 92) return 92;               // spectral match
        if (idx == 93) return 93;               // sub wavetable
        if (idx == 94) return 94;               // transient sample
        return -1;
    }

private:
    static float cl (float v, float lo, float hi) { return std::max (lo, std::min (hi, v)); }
};


class OneShotMatchSynth
{
public:
    void setWavetable (const WavetableData* wt) { wavetable = wt; }
    void setResidualNoise (const ResidualNoiseData* rn) { residualNoise = rn; }
    void setTransientSample (const TransientSampleData* ts) { transientSample = ts; }
    void setSpectralEnvelope (const SpectralEnvelopeData* se) { spectralEnvelope = se; }
    void setHarmonicPhases (const HarmonicPhaseData* hp) { harmonicPhases = hp; }

    juce::AudioBuffer<float> generate (const MatchSynthParams& p, double sampleRate)
    {
        const float sr = (float) sampleRate;
        float duration = p.ampAttack + p.ampBodyDecay + p.subTailDecay + 0.12f;
        if (p.envSustainLevel > 0.01f) duration += p.envSustainTime + p.envRelease;
        if (p.reverbAmt > 0.01f) duration += p.reverbDecay * 0.5f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        dsputil::NoiseGenerator noise;
        noise.setSeed (42);

        double bodyPhase = 0.0, subPhase = 0.0, fmModPhase = 0.0, wobbleLFOPhase = 0.0;
        double harm2Phase = 0.0, harm3Phase = 0.0, harm4Phase = 0.0;
        double harm5Phase = 0.0, harm6Phase = 0.0, harm7Phase = 0.0, harm8Phase = 0.0;
        static constexpr int MAX_UNISON = 4;
        double unisonPhases[MAX_UNISON] = {};

        // === Filters ===
        dsputil::SVFilter clickBP;
        clickBP.setParameters (p.clickFreq, 0.15f + (1.0f - p.clickWidth) * 0.55f, FilterType::BandPass, sr);

        dsputil::SVFilter subLP;
        subLP.setParameters (std::min ((p.basePitch + p.subDetune) * 4.0f, sr * 0.48f), 0.0f, FilterType::LowPass, sr);

        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.filterCutoff, 0.0f, FilterType::LowPass, sr);

        dsputil::SVFilter harmonicBP;
        harmonicBP.setParameters (std::min (p.basePitch * 2.5f, sr * 0.4f), 0.3f, FilterType::BandPass, sr);

        dsputil::SVFilter resonBP;
        if (p.bodyResonance > 0.01f)
            resonBP.setParameters (std::min (p.bodyResonFreq, sr * 0.45f), 0.3f + p.bodyResonance * 0.5f, FilterType::BandPass, sr);

        static constexpr int MAX_COMB_DELAY = 1024;
        float combBuf[MAX_COMB_DELAY] = {};
        int combWritePos = 0;
        int combDelaySamples = (p.combAmount > 0.01f) ? std::max (2, std::min (MAX_COMB_DELAY - 1, (int)(sr / p.combFreq))) : 0;

        dsputil::SVFilter mbLowLP, mbHighHP;
        if (p.lowSaturation > 0.01f || p.highSaturation > 0.01f)
        {
            mbLowLP.setParameters (300.0f, 0.0f, FilterType::LowPass, sr);
            mbHighHP.setParameters (3000.0f, 0.0f, FilterType::HighPass, sr);
        }

        dsputil::SVFilter reson2BP, reson3BP;
        if (p.reson2Amt > 0.01f) reson2BP.setParameters (std::min (p.reson2Freq, sr * 0.45f), 0.3f + p.reson2Amt * 0.5f, FilterType::BandPass, sr);
        if (p.reson3Amt > 0.01f) reson3BP.setParameters (std::min (p.reson3Freq, sr * 0.45f), 0.3f + p.reson3Amt * 0.5f, FilterType::BandPass, sr);

        dsputil::SVFilter noiseShapeFilt;
        if (p.noiseShapeAmt > 0.01f) noiseShapeFilt.setParameters (std::min (p.noiseFilterFreq, sr * 0.45f), p.noiseFilterQ * 0.7f, FilterType::BandPass, sr);

        dsputil::SVFilter eqLowLP, eqMidBP, eqHighHP;
        float eqLG = 0.0f, eqMG = 0.0f, eqHG = 0.0f;
        if (p.eqAmount > 0.01f)
        {
            eqLowLP.setParameters (100.0f, 0.0f, FilterType::LowPass, sr);
            eqMidBP.setParameters (std::min (p.eqMidFreq, sr * 0.4f), 0.4f, FilterType::BandPass, sr);
            eqHighHP.setParameters (std::min (8000.0f, sr * 0.45f), 0.0f, FilterType::HighPass, sr);
            eqLG = std::pow (10.0f, p.eqLowGain / 20.0f) - 1.0f;
            eqMG = std::pow (10.0f, p.eqMidGain / 20.0f) - 1.0f;
            eqHG = std::pow (10.0f, p.eqHighGain / 20.0f) - 1.0f;
        }

        dsputil::SVFilter formant1BP, formant2BP;
        if (p.formantAmt > 0.01f)
        {
            formant1BP.setParameters (std::min (p.formantFreq1, sr * 0.45f), p.formantQ, FilterType::BandPass, sr);
            formant2BP.setParameters (std::min (p.formantFreq2, sr * 0.45f), p.formantQ, FilterType::BandPass, sr);
        }

        dsputil::SVFilter transLayerBP;
        if (p.transLayerAmt > 0.01f)
            transLayerBP.setParameters (std::min (p.transLayerFreq, sr * 0.45f), 0.3f, FilterType::BandPass, sr);

        // === Dynamic filter sweep ===
        dsputil::SVFilter sweepFilter;
        bool useSweep = p.filterSweepAmt > 0.01f;

        const float attackSec = std::max (0.0001f, p.ampAttack);
        const float targetFreq = p.basePitch;
        const float startFreq = targetFreq * std::pow (2.0f, p.pitchEnvDepth / 12.0f);
        const float subFreq = (p.subPitch > 1.0f) ? p.subPitch : (targetFreq + p.subDetune);

        dsputil::DCBlocker dcBlock;
        dsputil::Saturator saturator;

        // === Main loop ===
        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // Pitch envelope
            float fastEnv = std::exp (-t / std::max (0.0002f, p.pitchEnvFast));
            float slowEnv = std::exp (-t / std::max (0.001f, p.pitchEnvSlow));
            float pitchEnv = fastEnv * p.pitchEnvBalance + slowEnv * (1.0f - p.pitchEnvBalance);
            float bodyFreq = targetFreq + (startFreq - targetFreq) * pitchEnv;

            if (p.pitchWobble > 0.01f)
            {
                float wobbleEnv = std::exp (-t / std::max (0.005f, p.wobbleDecay));
                wobbleLFOPhase += (double) p.wobbleRate / (double) sr;
                if (wobbleLFOPhase >= 1.0) wobbleLFOPhase -= 1.0;
                bodyFreq *= 1.0f + std::sin ((float)(wobbleLFOPhase * dsputil::TWO_PI)) * p.pitchWobble * 0.08f * wobbleEnv;
            }

            bodyPhase += (double) bodyFreq / (double) sr;
            if (bodyPhase >= 1.0) bodyPhase -= 1.0;
            subPhase += (double) subFreq / (double) sr;
            if (subPhase >= 1.0) subPhase -= 1.0;

            // FM
            float fmMod = 0.0f;
            if (p.fmDepth > 0.01f)
            {
                fmModPhase += (double)(bodyFreq * p.fmRatio) / (double) sr;
                if (fmModPhase >= 1.0) fmModPhase -= 1.0;
                fmMod = std::sin ((float)(fmModPhase * dsputil::TWO_PI)) * p.fmDepth * std::exp (-t / std::max (0.002f, p.fmDecay)) * 3.0f;
            }

            // Body oscillator
            float ph = (float) bodyPhase;
            if (p.phaseDistort > 0.01f)
            {
                float pdAmt = p.phaseDistort * std::exp (-t / std::max (0.002f, p.phaseDistDecay));
                ph = ph + pdAmt * 0.5f * std::sin (ph * dsputil::TWO_PI);
                ph -= std::floor (ph);
            }
            if (p.fmDepth > 0.01f)
                ph = (float) std::fmod ((double) ph + (double) fmMod + 100.0, 1.0);

            float body = 0.0f;
            switch (p.oscType)
            {
                case 0:  body = std::sin (ph * dsputil::TWO_PI); break;                          // Sine
                case 1:  body = 2.0f * std::abs (2.0f * ph - 1.0f) - 1.0f; break;               // Triangle
                case 2:  body = 2.0f * ph - 1.0f; break;                                         // Saw
                case 3:  body = (ph < 0.5f) ? 1.0f : -1.0f; break;                               // Square
                case 4:  body = (ph < 0.25f) ? 1.0f : -1.0f; break;                              // Pulse 25%
                case 5:  body = (ph < 0.125f) ? 1.0f : -1.0f; break;                             // Pulse 12.5%
                case 6:  body = (ph < 0.5f) ? std::sin (ph * dsputil::TWO_PI) : 0.0f; break;     // Half-rect sine
                case 7:  body = std::abs (std::sin (ph * dsputil::TWO_PI)) * 2.0f - 1.0f; break; // Abs sine
                case 8:  body = 4.0f * ph * (1.0f - ph) * 2.0f - 1.0f; break;                   // Parabolic
                case 9:  body = std::round (std::sin (ph * dsputil::TWO_PI) * 3.0f) / 3.0f; break; // Staircase
                case 10: body = std::sin (ph * dsputil::TWO_PI) * 0.7f
                              + std::sin (ph * dsputil::TWO_PI * 2.0f) * 0.3f; break;            // Double sine
                case 11: body = std::tanh (std::sin (ph * dsputil::TWO_PI) * 2.5f); break;       // Clipped sine
                case 12: // Wavetable (reference-derived, 32 frames, multi-cycle)
                    if (wavetable != nullptr && wavetable->valid && wavetable->numFrames > 0)
                    {
                        // Morph position based on actual duration covered by wavetable
                        float morphTime = std::max (0.01f, wavetable->totalDuration);
                        float morphPos = std::min (t / morphTime, 1.0f) * (float)(wavetable->numFrames - 1);
                        int f0 = std::min ((int) morphPos, wavetable->numFrames - 1);
                        int f1 = std::min (f0 + 1, wavetable->numFrames - 1);
                        float mFrac = morphPos - (float) f0;
                        // For multi-cycle frames: phase wraps within the frame
                        // cyclesPerFrame determines how many cycles fit in FRAME_SIZE
                        float cyclePhase = std::fmod (ph * (float) wavetable->cyclesPerFrame, 1.0f);
                        float tPos = cyclePhase * (float) WavetableData::FRAME_SIZE;
                        int ti0 = (int) tPos % WavetableData::FRAME_SIZE;
                        int ti1 = (ti0 + 1) % WavetableData::FRAME_SIZE;
                        float tFrac = tPos - std::floor (tPos);
                        float s0 = wavetable->frames[f0][ti0] * (1.0f - tFrac) + wavetable->frames[f0][ti1] * tFrac;
                        float s1 = wavetable->frames[f1][ti0] * (1.0f - tFrac) + wavetable->frames[f1][ti1] * tFrac;
                        body = s0 * (1.0f - mFrac) + s1 * mFrac;
                    }
                    else body = std::sin (ph * dsputil::TWO_PI);
                    break;
                default: body = std::sin (ph * dsputil::TWO_PI); break;
            }

            if (p.bodyHarmonics > 0.01f)
            {
                float shaped = std::tanh (body * (1.0f + p.bodyHarmonics * 4.0f));
                body = body * (1.0f - p.bodyHarmonics) + shaped * p.bodyHarmonics;
            }

            // Additive harmonics (up to 8, with phase-accurate offsets from reference)
            float additiveSig = 0.0f;
            if (p.additiveAmt > 0.01f)
            {
                float dBase = p.harmonicDecayRate / std::max (0.01f, p.ampBodyDecay);
                bool hasPhases = (harmonicPhases != nullptr && harmonicPhases->valid);
                auto advanceHarm = [&](double& phase, float n, int harmIdx) {
                    float hF = bodyFreq * n * (1.0f + p.inharmonicity * (n * n - 1.0f) * 0.5f);
                    if (hF < sr * 0.45f) { phase += (double) hF / (double) sr; if (phase >= 1.0) phase -= 1.0; }
                    float phaseOffset = (hasPhases && harmIdx < harmonicPhases->numHarmonics) ? harmonicPhases->phases[harmIdx] : 0.0f;
                    return (hF < sr * 0.45f) ? std::sin ((float)(phase * dsputil::TWO_PI) + phaseOffset) : 0.0f;
                };
                additiveSig  = advanceHarm (harm2Phase, 2.0f, 1) * p.harmonic2 * std::exp (-t * dBase);
                additiveSig += advanceHarm (harm3Phase, 3.0f, 2) * p.harmonic3 * std::exp (-t * dBase * 1.5f);
                additiveSig += advanceHarm (harm4Phase, 4.0f, 3) * p.harmonic4 * std::exp (-t * dBase * 2.0f);
                additiveSig += advanceHarm (harm5Phase, 5.0f, 4) * p.harmonic5 * std::exp (-t * dBase * 2.5f);
                additiveSig += advanceHarm (harm6Phase, 6.0f, 5) * p.harmonic6 * std::exp (-t * dBase * 3.0f);
                additiveSig += advanceHarm (harm7Phase, 7.0f, 6) * p.harmonic7 * std::exp (-t * dBase * 3.5f);
                additiveSig += advanceHarm (harm8Phase, 8.0f, 7) * p.harmonic8 * std::exp (-t * dBase * 4.0f);
                additiveSig *= p.additiveAmt * 0.3f;
            }

            // Unison
            float unisonSig = 0.0f;
            if (p.unisonAmt > 0.01f)
            {
                float detuneHz = bodyFreq * (std::pow (2.0f, p.unisonDetune / 1200.0f) - 1.0f);
                for (int v = 0; v < MAX_UNISON; ++v)
                {
                    float sign = (v % 2 == 0) ? 1.0f : -1.0f;
                    float vFreq = bodyFreq + sign * detuneHz * ((float)(v / 2 + 1) / 2.0f);
                    unisonPhases[v] += (double) vFreq / (double) sr;
                    if (unisonPhases[v] >= 1.0) unisonPhases[v] -= 1.0;
                    unisonSig += std::sin ((float)(unisonPhases[v] * dsputil::TWO_PI));
                }
                unisonSig = unisonSig / (float) MAX_UNISON * p.unisonAmt;
            }

            // Body envelope
            float bodyEnv;
            if (t < attackSec)
            {
                bodyEnv = t / attackSec;
                if (p.envSustainLevel > 0.01f && std::abs (p.envCurve - 1.0f) > 0.01f)
                    bodyEnv = std::pow (std::max (0.0001f, bodyEnv), p.envCurve);
            }
            else
            {
                float dt = t - attackSec;
                if (p.transientSnap > 0.01f && dt < p.transientHold)
                    bodyEnv = 1.0f;
                else
                {
                    float adjDt = (p.transientSnap > 0.01f) ? std::max (0.0f, dt - p.transientHold) : dt;
                    float punch = std::exp (-adjDt / std::max (0.001f, p.ampPunchDecay));
                    float sus = std::exp (-adjDt / std::max (0.005f, p.ampBodyDecay));
                    bodyEnv = punch * p.ampPunchLevel + sus * (1.0f - p.ampPunchLevel);
                    if (p.envSustainLevel > 0.01f && std::abs (p.envCurve - 1.0f) > 0.01f)
                        bodyEnv = std::pow (std::max (0.0f, bodyEnv), p.envCurve);
                }
                if (p.envSustainLevel > 0.01f)
                {
                    float plateau = p.envSustainLevel;
                    if (dt > p.envSustainTime) plateau *= std::exp (-(dt - p.envSustainTime) / std::max (0.01f, p.envRelease));
                    bodyEnv = std::max (bodyEnv, plateau);
                }
            }

            // Resonances
            float resonSig = 0.0f;
            if (p.bodyResonance > 0.01f) resonSig = resonBP.process (body * bodyEnv) * p.bodyResonance * 0.4f;
            float r2Sig = 0.0f, r3Sig = 0.0f;
            if (p.reson2Amt > 0.01f) r2Sig = reson2BP.process (body * bodyEnv) * p.reson2Amt * 0.3f;
            if (p.reson3Amt > 0.01f) r3Sig = reson3BP.process (body * bodyEnv) * p.reson3Amt * 0.25f;

            // Formant
            float formantSig = 0.0f;
            if (p.formantAmt > 0.01f)
                formantSig = (formant1BP.process (body * bodyEnv) + formant2BP.process (body * bodyEnv) * 0.7f) * p.formantAmt * 0.35f;

            // Sub (with optional wavetable crossfade)
            float subSine = std::sin ((float)(subPhase * dsputil::TWO_PI));
            float sub = subSine;
            if (p.subWavetable > 0.01f && wavetable != nullptr && wavetable->valid && wavetable->numFrames > 0)
            {
                float subPh = (float) std::fmod (subPhase, 1.0);
                float tPos = subPh * (float) WavetableData::FRAME_SIZE;
                int ti0 = (int) tPos % WavetableData::FRAME_SIZE;
                int ti1 = (ti0 + 1) % WavetableData::FRAME_SIZE;
                float tFrac = tPos - std::floor (tPos);
                float wtSub = wavetable->frames[0][ti0] * (1.0f - tFrac) + wavetable->frames[0][ti1] * tFrac;
                sub = subSine * (1.0f - p.subWavetable) + wtSub * p.subWavetable;
            }
            sub = subLP.process (sub);
            float subAttack = attackSec * 1.2f;
            float subEnv;
            if (t < subAttack) subEnv = t / subAttack;
            else
            {
                float dt = t - subAttack;
                subEnv = std::exp (-dt / std::max (0.008f, p.subTailDecay * 0.25f)) * 0.35f
                       + std::exp (-dt / std::max (0.01f, p.subTailDecay)) * 0.65f;
                if (p.envSustainLevel > 0.01f)
                {
                    float sp = p.envSustainLevel * 0.7f;
                    if (dt > p.envSustainTime) sp *= std::exp (-(dt - p.envSustainTime) / std::max (0.01f, p.envRelease));
                    subEnv = std::max (subEnv, sp);
                }
            }

            // Click
            float noiseRaw = noise.nextPink();
            float click = clickBP.process (noiseRaw) * std::exp (-t / std::max (0.0001f, p.clickDecay)) * p.clickAmount;
            float top = noise.next() * std::exp (-t / 0.0003f) * 0.12f * p.clickAmount;

            // Transient layer
            float transLayerSig = 0.0f;
            if (p.transLayerAmt > 0.01f)
                transLayerSig = transLayerBP.process (noise.next()) * std::exp (-t / std::max (0.0001f, p.transLayerDecay)) * p.transLayerAmt * 0.4f;

            // Noise (synthetic + residual)
            float noiseSample = 0.0f;
            if (p.noiseAmount > 0.01f)
            {
                float noiseEnv = std::exp (-t / std::max (0.01f, p.ampBodyDecay * 0.4f));
                if (p.noiseShapeAmt > 0.01f)
                    noiseSample = noiseShapeFilt.process (noise.next() * (1.0f - p.noiseColor) + noise.nextPink() * p.noiseColor) * noiseEnv * p.noiseAmount * 0.4f;
                else
                    noiseSample = noise.next() * noiseEnv * p.noiseAmount * 0.25f;
            }

            // Residual noise from reference (with its own extracted envelope)
            float residualSig = 0.0f;
            if (p.residualAmt > 0.01f && residualNoise != nullptr && residualNoise->valid && ! residualNoise->residual.empty())
            {
                int resIdx = std::min (i, (int) residualNoise->residual.size() - 1);
                // Use the residual's own envelope if available, otherwise fall back to body decay
                float resEnv;
                if (! residualNoise->envelope.empty() && resIdx < (int) residualNoise->envelope.size())
                    resEnv = residualNoise->envelope[resIdx];
                else
                    resEnv = std::exp (-t / std::max (0.01f, p.ampBodyDecay * 0.5f));
                residualSig = residualNoise->residual[resIdx] * p.residualLevel * resEnv * p.residualAmt;
            }

            // Transient sample from reference
            float transSampleSig = 0.0f;
            if (p.transientSampleAmt > 0.01f && transientSample != nullptr && transientSample->valid && i < (int) transientSample->samples.size())
                transSampleSig = transientSample->samples[i] * p.transientSampleAmt;

            // Mix (optimizable levels)
            float sample = body * bodyEnv * p.bodyMix + additiveSig * bodyEnv + unisonSig * bodyEnv * 0.4f
                         + sub * subEnv * p.subLevel * p.subMix + click * p.clickMix + top * p.topMix + transLayerSig
                         + noiseSample + residualSig + transSampleSig + resonSig + r2Sig + r3Sig + formantSig;

            if (p.harmonicEmphasis > 0.01f)
                sample += harmonicBP.process (body * bodyEnv) * p.harmonicEmphasis * 0.3f;

            if (p.distortion > 0.01f)
                sample = saturator.process (sample, p.distortion, dsputil::SaturationMode::Tape);

            if (p.lowSaturation > 0.01f || p.highSaturation > 0.01f)
            {
                if (p.lowSaturation > 0.01f)
                    sample += (saturator.process (mbLowLP.process (sample), p.lowSaturation, dsputil::SaturationMode::Tube) - mbLowLP.process (sample)) * 0.5f;
                if (p.highSaturation > 0.01f)
                    sample += (saturator.process (mbHighHP.process (sample), p.highSaturation, dsputil::SaturationMode::SoftClip) - mbHighHP.process (sample)) * 0.4f;
            }

            if (p.combAmount > 0.01f && combDelaySamples > 0)
            {
                int rp = (combWritePos - combDelaySamples + MAX_COMB_DELAY) % MAX_COMB_DELAY;
                float combOut = sample + combBuf[rp] * p.combFeedback;
                combBuf[combWritePos] = combOut;
                combWritePos = (combWritePos + 1) % MAX_COMB_DELAY;
                sample = sample * (1.0f - p.combAmount) + combOut * p.combAmount;
            }

            sample = mainFilter.process (sample);

            // Dynamic filter sweep
            if (useSweep)
            {
                float sweepEnv = std::exp (-t / std::max (0.005f, p.ampBodyDecay * 0.8f));
                float sweepFreq = p.filterSweepEnd + (p.filterSweepStart - p.filterSweepEnd) * sweepEnv;
                sweepFreq = std::max (30.0f, std::min (sr * 0.45f, sweepFreq));
                sweepFilter.setParameters (sweepFreq, p.filterSweepReso, FilterType::LowPass, sr);
                float wet = sweepFilter.process (sample);
                sample = sample * (1.0f - p.filterSweepAmt) + wet * p.filterSweepAmt;
            }

            if (p.eqAmount > 0.01f)
            {
                float dry = sample;
                float eqd = sample + eqLowLP.process (sample) * eqLG + eqMidBP.process (sample) * eqMG + eqHighHP.process (sample) * eqHG;
                sample = dry + (eqd - dry) * p.eqAmount;
            }

            sample = dcBlock.process (sample) * 0.85f;
            buffer.setSample (0, i, sample);
            buffer.setSample (1, i, sample);
        }

        // === Post-loop: Spectral envelope matching ===
        if (p.spectralMatchAmt > 0.01f && spectralEnvelope != nullptr && spectralEnvelope->valid)
        {
            applySpectralEnvelopeMatch (buffer, p.spectralMatchAmt, sr);
        }

        // === Post-loop: Reverb ===
        if (p.reverbAmt > 0.01f)
        {
            static constexpr int MAX_REV = 4096;
            float revBuf[MAX_REV] = {};
            int revPos = 0;
            float dampSt = 0.0f;
            int t1 = std::min ((int)(0.0297f * sr), MAX_REV - 1);
            int t2 = std::min ((int)(0.0371f * sr), MAX_REV - 1);
            int t3 = std::min ((int)(0.0411f * sr), MAX_REV - 1);
            int t4 = std::min ((int)(0.0437f * sr), MAX_REV - 1);
            float fbk = std::min (0.95f, p.reverbDecay * 0.5f);
            for (int i = 0; i < numSamples; ++i)
            {
                float dry = buffer.getSample (0, i);
                float wet = (revBuf[(revPos - t1 + MAX_REV) % MAX_REV] + revBuf[(revPos - t2 + MAX_REV) % MAX_REV]
                           + revBuf[(revPos - t3 + MAX_REV) % MAX_REV] + revBuf[(revPos - t4 + MAX_REV) % MAX_REV]) * 0.25f;
                dampSt = dampSt * p.reverbDamp + wet * (1.0f - p.reverbDamp);
                revBuf[revPos] = dry * 0.5f + dampSt * fbk;
                revPos = (revPos + 1) % MAX_REV;
                float out = dry * (1.0f - p.reverbAmt * 0.5f) + wet * p.reverbAmt;
                buffer.setSample (0, i, out);
                buffer.setSample (1, i, out);
            }
        }

        // === Post-loop: Stereo ===
        if (p.stereoWidth > 0.01f)
        {
            dsputil::SVFilter stHP;
            stHP.setParameters (std::min (p.stereoFreq, sr * 0.45f), 0.0f, FilterType::HighPass, sr);
            for (int i = 0; i < numSamples; ++i)
            {
                float m = buffer.getSample (0, i);
                float d = stHP.process (m) * p.stereoWidth;
                buffer.setSample (0, i, m + d * 0.5f);
                buffer.setSample (1, i, m - d * 0.5f);
            }
        }

        // Unison stereo spread (Haas)
        if (p.unisonAmt > 0.01f && p.unisonSpread > 0.01f)
        {
            int del = std::max (1, std::min (22, (int)(p.unisonSpread * 0.5f * sr / 1000.0f)));
            for (int i = numSamples - 1; i >= del; --i)
                buffer.setSample (1, i, buffer.getSample (0, i - del));
            for (int i = 0; i < del; ++i) buffer.setSample (1, i, 0.0f);
        }

        synthutil::applyFadeOut (buffer, (int)(0.01f * sr));
        synthutil::normalizeBuffer (buffer, 0.91f);
        return buffer;
    }

private:
    const WavetableData* wavetable = nullptr;
    const ResidualNoiseData* residualNoise = nullptr;
    const TransientSampleData* transientSample = nullptr;
    const SpectralEnvelopeData* spectralEnvelope = nullptr;
    const HarmonicPhaseData* harmonicPhases = nullptr;

    // Spectral envelope matching: FFT the output, adjust band gains to match reference, IFFT back
    void applySpectralEnvelopeMatch (juce::AudioBuffer<float>& buffer, float amount, float sr)
    {
        const int numSamples = buffer.getNumSamples();
        const int fftOrder = 11;
        const int fftSize = 1 << fftOrder;
        if (numSamples < fftSize) return;

        // Process mono (channel 0), copy result to both channels
        std::vector<float> fftData (fftSize * 2, 0.0f);
        int available = std::min (fftSize, numSamples);
        for (int i = 0; i < available; ++i)
            fftData[i] = buffer.getSample (0, i);

        juce::dsp::FFT fft (fftOrder);
        fft.performRealOnlyForwardTransform (fftData.data());

        // Compute current band energies (same Mel bands as descriptors)
        int numBins = fftSize / 2;
        auto hzToMel = [](float hz) { return 2595.0f * std::log10 (1.0f + hz / 700.0f); };
        auto melToHz = [](float mel) { return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f); };

        float melLow = hzToMel (20.0f);
        float melHigh = hzToMel (std::min (20000.0f, sr * 0.45f));
        float melStep = (melHigh - melLow) / (float) SpectralEnvelopeData::NUM_BANDS;

        for (int b = 0; b < SpectralEnvelopeData::NUM_BANDS; ++b)
        {
            float edgeLo = melToHz (melLow + (float) b * melStep);
            float edgeHi = melToHz (melLow + (float)(b + 1) * melStep);
            int lo = std::max (1, std::min ((int)(edgeLo / sr * (float) fftSize), numBins - 1));
            int hi = std::max (lo + 1, std::min ((int)(edgeHi / sr * (float) fftSize), numBins));

            float currentE = 0.0f;
            for (int i = lo; i < hi; ++i)
            {
                float re = fftData[i * 2], im = fftData[i * 2 + 1];
                currentE += re * re + im * im;
            }
            currentE = std::sqrt (currentE / std::max (1, hi - lo));

            float targetE = std::sqrt (spectralEnvelope->targetBandEnergy[b]);
            if (currentE > 1e-8f)
            {
                float gain = 1.0f + (targetE / currentE - 1.0f) * amount;
                gain = std::max (0.1f, std::min (10.0f, gain)); // limit gain range
                for (int i = lo; i < hi; ++i)
                {
                    fftData[i * 2]     *= gain;
                    fftData[i * 2 + 1] *= gain;
                }
            }
        }

        fft.performRealOnlyInverseTransform (fftData.data());

        // Write back with crossfade
        for (int i = 0; i < available; ++i)
        {
            float original = buffer.getSample (0, i);
            float matched = fftData[i] / (float) fftSize;
            float out = original * (1.0f - amount) + matched * amount;
            buffer.setSample (0, i, out);
            buffer.setSample (1, i, out);
        }
    }
};

} // namespace oneshotmatch
