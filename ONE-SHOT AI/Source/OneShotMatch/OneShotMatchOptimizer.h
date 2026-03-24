#pragma once

#include <JuceHeader.h>
#include <random>
#include <functional>
#include <algorithm>
#include <vector>
#include <array>
#include <cmath>
#include <numeric>
#include "OneShotMatchDescriptors.h"
#include "OneShotMatchSynth.h"

// Self-improving optimizer for the OneShotMatch system.
//
// Five-phase adaptive strategy:
//   1. DE/best/1/bin on CORE params only (22 params) — fast global search  [50% budget]
//   2. Gap analysis #1 → activate extensions
//   3. DE on CORE + activated extensions                                   [30% budget]
//   4. Gap analysis #2 → re-analyze best, activate MORE extensions if needed
//   5. DE on all active params                                             [20% budget]
//   6. Sensitivity analysis + Nelder-Mead refinement on sensitive params   [120 evals]
//
// The optimizer automatically discovers which synth extensions are needed
// and only activates them when the core params can't match specific aspects
// of the reference sound.

namespace oneshotmatch
{

struct OptimizationResult
{
    MatchSynthParams bestParams;
    float            bestDistance        = 999.0f;
    int              iterations         = 0;
    bool             converged          = false;

    // Per-parameter sensitivity (0..1)
    std::array<float, MatchSynthParams::NUM_PARAMS> sensitivity = {};

    // Merged gap analysis from both passes
    GapAnalysis gaps;
    int         extensionsActivated = 0;
    int         phase1Iterations    = 0;
    float       phase1Distance      = 999.0f;
};

using ProgressCallback = std::function<bool (int, float, int)>;

class OneShotMatchOptimizer
{
public:
    void setMaxIterations (int n) { maxIterations = n; }
    void setTargetDistance (float d) { targetDistance = d; }
    void setPopulationSize (int n) { popSize = n; }

    // Set reference buffer for direct spectral comparison in NM refinement
    void setReferenceBuffer (const juce::AudioBuffer<float>* buf) { refBuffer = buf; }

    OptimizationResult optimize (const MatchDescriptors& refDescFull,
                                 double sampleRate,
                                 ProgressCallback progress = nullptr,
                                 const MatchSynthParams* seedParams = nullptr)
    {
        // Re-extract reference in fastMode (same features as candidates will use)
        MatchDescriptors refDesc = refDescFull;
        if (refBuffer != nullptr && refBuffer->getNumSamples() > 0)
        {
            extractor.setFastMode (true);
            refDesc = extractor.extract (*refBuffer, sampleRate);
            extractor.setFastMode (false);
        }

        // Prepare reference mono for STFT comparison in Nelder-Mead refinement
        prepareRefMono (sampleRate, sampleRate);

        const int N = MatchSynthParams::NUM_PARAMS;
        float mins[N], maxs[N];
        MatchSynthParams::getBounds (mins, maxs);

        // Apply learned bounds (narrowed search space from similar matches)
        if (learnedProfile != nullptr && learnedProfile->valid && learnedProfile->boundsValid
            && (int) learnedProfile->learnedMins.size() == N)
        {
            for (int i = 0; i < N; ++i)
            {
                // Narrow bounds but keep at least 30% of original range as safety margin
                float origRange = maxs[i] - mins[i];
                float newMin = std::max (mins[i], learnedProfile->learnedMins[i] - origRange * 0.15f);
                float newMax = std::min (maxs[i], learnedProfile->learnedMaxs[i] + origRange * 0.15f);
                if (newMax - newMin >= origRange * 0.3f) // only apply if >30% of range remains
                {
                    mins[i] = newMin;
                    maxs[i] = newMax;
                }
            }
        }

        std::mt19937 rng (std::random_device{}());
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);

        // === Active params — start with core + mix levels (always active) ===
        std::vector<int> activeParams;
        for (int i = 0; i < MatchSynthParams::NUM_CORE_PARAMS; ++i)
            activeParams.push_back (i);
        // v4 mix levels — always optimize from phase 1
        activeParams.push_back (76); // bodyMix
        activeParams.push_back (77); // subMix
        activeParams.push_back (78); // clickMix
        activeParams.push_back (79); // topMix
        activeParams.push_back (105); // subCrossover — body/sub crossover frequency
        // v5: subWavetable always active when wavetable is available
        if (wavetable != nullptr && wavetable->valid)
            activeParams.push_back (93); // subWavetable

        // Pre-activate extensions from learned profile (skip gap analysis wait)
        if (learnedProfile != nullptr && learnedProfile->valid)
        {
            for (int idx : learnedProfile->preActivateExtensions)
            {
                bool already = false;
                for (int a : activeParams) if (a == idx) { already = true; break; }
                if (! already) activeParams.push_back (idx);
            }
        }

        // === Population initialization ===
        std::vector<std::vector<float>> pop (popSize, std::vector<float> (N));
        std::vector<float> fitness (popSize, 999.0f);
        std::vector<float> F_vec (popSize, 0.7f);
        std::vector<float> CR_vec (popSize, 0.85f);

        // Set ALL extension params to their "off" default (0)
        auto setExtensionDefaults = [&](std::vector<float>& vec)
        {
            for (int j = MatchSynthParams::NUM_CORE_PARAMS; j < N; ++j)
                vec[j] = 0.0f;
            // Non-zero defaults for ratio/freq/decay params (inactive since gate=0)
            // v1 extensions
            vec[23] = 2.0f;    // fmRatio
            vec[24] = 0.05f;   // fmDecay
            vec[26] = 200.0f;  // bodyResonFreq
            vec[28] = 8.0f;    // wobbleRate
            vec[29] = 0.05f;   // wobbleDecay
            vec[31] = 0.001f;  // transientHold
            vec[33] = 150.0f;  // combFreq
            vec[34] = 0.3f;    // combFeedback
            vec[38] = 0.05f;   // phaseDistDecay
            // v2 extensions
            vec[43] = 2.0f;    // harmonicDecayRate
            vec[46] = 300.0f;  // reson2Freq
            vec[48] = 600.0f;  // reson3Freq
            vec[50] = 0.5f;    // noiseColor
            vec[51] = 4000.0f; // noiseFilterFreq
            vec[52] = 0.3f;    // noiseFilterQ
            vec[56] = 1000.0f; // eqMidFreq
            vec[59] = 0.1f;    // envSustainTime
            vec[60] = 0.1f;    // envRelease
            vec[61] = 1.0f;    // envCurve
            vec[63] = 2000.0f; // stereoFreq
            // v3 extensions
            vec[65] = 10.0f;   // unisonDetune
            vec[66] = 0.5f;    // unisonSpread
            vec[68] = 700.0f;  // formantFreq1
            vec[69] = 1500.0f; // formantFreq2
            vec[71] = 5000.0f; // transLayerFreq
            vec[72] = 0.003f;  // transLayerDecay
            vec[74] = 0.3f;    // reverbDecay
            vec[75] = 0.5f;    // reverbDamp
            // v4 extensions — mix defaults (always active)
            vec[76] = 0.50f;   // bodyMix
            vec[77] = 0.50f;   // subMix
            vec[78] = 0.10f;   // clickMix
            vec[79] = 0.05f;   // topMix (low default — only raise if reference has HF)
            vec[80] = 0.0f;    // filterSweepAmt (off)
            vec[81] = 8000.0f; // filterSweepStart
            vec[82] = 500.0f;  // filterSweepEnd
            vec[83] = 0.3f;    // filterSweepReso
            vec[84] = 0.0f;    // subPitch (follow body)
            vec[85] = 0.5f;    // formantQ
            // v5 extensions
            vec[86] = 0.0f;    // residualAmt (off)
            vec[87] = 0.5f;    // residualLevel
            vec[88] = 0.0f;    // harmonic5
            vec[89] = 0.0f;    // harmonic6
            vec[90] = 0.0f;    // harmonic7
            vec[91] = 0.0f;    // harmonic8
            vec[92] = 0.0f;    // spectralMatchAmt (off)
            vec[93] = 0.0f;    // subWavetable (off)
            vec[94] = 0.0f;    // transientSampleAmt (off)
            // v6 extensions
            vec[95] = 0.0f;    // pitchBounce (off)
            vec[96] = 0.0f;    // pitchHoldTime
            vec[97] = 0.0f;    // clickType (noise)
            vec[98] = 0.0f;    // masterSatAmount (off)
            vec[99] = 0.0f;    // masterSatType (soft-clip)
            vec[100] = 0.0f;   // subPhaseOffset
            vec[101] = 0.0f;   // compAmount (off)
            vec[102] = 4.0f;   // compRatio
            vec[103] = 0.005f; // compAttack
            vec[104] = 0.05f;  // compRelease
            vec[105] = 0.0f;   // subCrossover (auto)
        };

        // === Warm-start or descriptor-based seeding ===
        if (seedParams != nullptr)
        {
            // Individual 0: exact seed
            seedParams->toArray (pop[0].data());
            setExtensionDefaults (pop[0]);

            // Individuals 1-4: perturbed versions of the seed
            for (int i = 1; i < std::min (5, popSize); ++i)
            {
                pop[i] = pop[0];
                for (int j : activeParams)
                {
                    float range = maxs[j] - mins[j];
                    float perturbation = (uni (rng) - 0.5f) * 0.3f * range;
                    pop[i][j] = std::max (mins[j], std::min (maxs[j], pop[i][j] + perturbation));
                }
                pop[i][0] = std::max (0.0f, std::min ((float) maxOscType, std::round (pop[i][0])));
                setExtensionDefaults (pop[i]);
            }

            // Individual 5: descriptor-based guess (same logic as no-seed path)
            if (popSize > 5)
            {
                MatchSynthParams guess;
                guess.basePitch = (refDesc.fundamentalFreq > 20.0f) ? refDesc.fundamentalFreq : 50.0f;
                guess.pitchEnvDepth = std::max (0.0f, refDesc.pitchDropSemitones);
                guess.pitchEnvFast = std::max (0.0005f, std::min (0.005f, refDesc.pitchDropTime * 0.1f));
                guess.pitchEnvSlow = std::max (0.005f, refDesc.pitchDropTime);
                guess.ampAttack = std::max (0.0001f, std::min (0.01f, refDesc.attackTime));
                guess.ampPunchDecay = std::max (0.01f, refDesc.decayTime * 0.6f);
                guess.ampBodyDecay = std::max (0.05f, refDesc.decayTime);
                guess.subTailDecay = std::max (0.05f, refDesc.decayTime40);
                guess.clickAmount = std::min (1.0f, std::max (0.1f, refDesc.transientStrength / 4.0f));
                guess.ampPunchLevel = (refDesc.transientStrength > 4.0f) ? 0.7f : 0.5f;
                guess.clickFreq = std::max (2000.0f, refDesc.transientRegion.spectralCentroid);
                guess.distortion = (refDesc.harmonicNoiseRatio > 0.4f) ? 0.3f : 0.0f;
                // Filter cutoff from spectral rolloff
                guess.filterCutoff = std::max (200.0f, refDesc.spectralRolloff * 2.5f);
                guess.subLevel = std::max (0.1f, std::min (0.8f, refDesc.subEnergy * 0.8f));
                guess.subDetune = (refDesc.subHarmonicRatio > 0.1f)
                    ? std::min (3.0f, refDesc.subHarmonicRatio * 5.0f) : 0.0f;
                if (refDesc.noiseSpectralCentroid > 100.0f && refDesc.harmonicNoiseRatio < 0.7f)
                    guess.noiseAmount = std::min (0.5f, (1.0f - refDesc.harmonicNoiseRatio) * 0.6f);
                else
                    guess.noiseAmount = std::min (0.3f, std::max (0.0f, refDesc.highEnergy * 2.0f));
                if (refDesc.fundamentalFreq > 20.0f)
                {
                    guess.bodyHarmonics = std::min (0.6f, std::max (0.0f,
                        (refDesc.harmonicProfile[1] + refDesc.harmonicProfile[2]) * 0.5f));
                    guess.harmonicEmphasis = std::min (0.5f, refDesc.harmonicProfile[1] * 0.5f);
                }
                else
                {
                    guess.bodyHarmonics = std::min (0.5f, std::max (0.0f, (1.0f - refDesc.bodyRegion.spectralFlatness) * 0.3f));
                    guess.harmonicEmphasis = std::min (0.5f, std::max (0.0f, refDesc.lowMidEnergy));
                }
                if (refDesc.spectralCrest < 3.0f && refDesc.harmonicNoiseRatio > 0.4f)
                    guess.distortion = std::min (0.5f, (1.0f - refDesc.spectralCrest / 10.0f) * 0.4f);
                guess.pitchEnvBalance = (refDesc.pitchDropSemitones > 12.0f) ? 0.85f : 0.5f;
                guess.clickWidth = std::max (0.2f, std::min (1.0f, refDesc.brightness * 2.0f + 0.3f));
                guess.clickDecay = std::max (0.0003f, std::min (0.003f, refDesc.attackTime * 0.3f));
                // Crossover: set at fundamentalFreq * 1.7 (above fundamental, below 2nd harmonic)
                guess.subCrossover = (refDesc.fundamentalFreq > 20.0f) ? refDesc.fundamentalFreq * 1.7f : 90.0f;
                // Prefer wavetable when available - it captures the exact timbre of the reference
                if (wavetable != nullptr && wavetable->valid)
                    guess.oscType = 12; // wavetable from reference
                // For dark kicks with lots of lowMid energy, saw or clipped sine work best
                else if (refDesc.lowMidEnergy > 0.2f && refDesc.brightness < 0.01f)
                    guess.oscType = 2; // saw
                // Body/sub mix balance from energy bands
                guess.bodyMix = std::max (0.3f, std::min (1.0f, refDesc.lowMidEnergy * 2.0f + 0.2f));
                guess.subMix = std::max (0.1f, std::min (0.8f, refDesc.subEnergy * 0.9f));
                guess.subLevel = std::max (0.1f, std::min (0.8f, refDesc.subEnergy * 0.8f));
                guess.clickMix = std::max (0.1f, std::min (1.0f, refDesc.transientStrength / 6.0f));
                guess.topMix = std::max (0.05f, std::min (1.0f, refDesc.highEnergy * 3.0f + 0.1f));
                // Suppress click/noise for dark kicks
                if (refDesc.brightness < 0.005f) {
                    guess.clickAmount = 0.0f;
                    guess.clickMix = 0.0f;
                    guess.topMix = 0.0f;
                    guess.noiseAmount = 0.0f;
                }
                guess.toArray (pop[5].data());
                setExtensionDefaults (pop[5]);
            }

            // Random for the rest
            for (int i = 6; i < popSize; ++i)
            {
                for (int j : activeParams)
                    pop[i][j] = mins[j] + uni (rng) * (maxs[j] - mins[j]);
                pop[i][0] = std::max (0.0f, std::min ((float) maxOscType, std::round (pop[i][0])));
                setExtensionDefaults (pop[i]);
            }
        }
        else
        {
            // No seed: descriptor-based guess at individual 0
            {
                MatchSynthParams guess;
                guess.basePitch = (refDesc.fundamentalFreq > 20.0f) ? refDesc.fundamentalFreq : 50.0f;
                guess.pitchEnvDepth = std::max (0.0f, refDesc.pitchDropSemitones);
                guess.pitchEnvFast = std::max (0.0005f, std::min (0.005f, refDesc.pitchDropTime * 0.1f));
                guess.pitchEnvSlow = std::max (0.005f, refDesc.pitchDropTime);
                guess.ampAttack = std::max (0.0001f, std::min (0.01f, refDesc.attackTime));
                guess.ampPunchDecay = std::max (0.01f, refDesc.decayTime * 0.6f);
                guess.ampBodyDecay = std::max (0.05f, refDesc.decayTime);
                guess.subTailDecay = std::max (0.05f, refDesc.decayTime40);
                guess.clickAmount = std::min (1.0f, std::max (0.1f, refDesc.transientStrength / 4.0f));
                guess.ampPunchLevel = (refDesc.transientStrength > 4.0f) ? 0.7f : 0.5f;
                guess.clickFreq = std::max (2000.0f, refDesc.transientRegion.spectralCentroid);
                guess.distortion = (refDesc.harmonicNoiseRatio > 0.4f) ? 0.3f : 0.0f;
                // Filter cutoff from spectral rolloff
                guess.filterCutoff = std::max (200.0f, refDesc.spectralRolloff * 2.5f);
                guess.subLevel = std::max (0.1f, std::min (0.8f, refDesc.subEnergy * 0.8f));
                guess.subDetune = (refDesc.subHarmonicRatio > 0.1f)
                    ? std::min (3.0f, refDesc.subHarmonicRatio * 5.0f) : 0.0f;
                if (refDesc.noiseSpectralCentroid > 100.0f && refDesc.harmonicNoiseRatio < 0.7f)
                    guess.noiseAmount = std::min (0.5f, (1.0f - refDesc.harmonicNoiseRatio) * 0.6f);
                else
                    guess.noiseAmount = std::min (0.3f, std::max (0.0f, refDesc.highEnergy * 2.0f));
                if (refDesc.fundamentalFreq > 20.0f)
                {
                    guess.bodyHarmonics = std::min (0.6f, std::max (0.0f,
                        (refDesc.harmonicProfile[1] + refDesc.harmonicProfile[2]) * 0.5f));
                    guess.harmonicEmphasis = std::min (0.5f, refDesc.harmonicProfile[1] * 0.5f);
                }
                else
                {
                    guess.bodyHarmonics = std::min (0.5f, std::max (0.0f, (1.0f - refDesc.bodyRegion.spectralFlatness) * 0.3f));
                    guess.harmonicEmphasis = std::min (0.5f, std::max (0.0f, refDesc.lowMidEnergy));
                }
                if (refDesc.spectralCrest < 3.0f && refDesc.harmonicNoiseRatio > 0.4f)
                    guess.distortion = std::min (0.5f, (1.0f - refDesc.spectralCrest / 10.0f) * 0.4f);
                guess.pitchEnvBalance = (refDesc.pitchDropSemitones > 12.0f) ? 0.85f : 0.5f;
                guess.clickWidth = std::max (0.2f, std::min (1.0f, refDesc.brightness * 2.0f + 0.3f));
                guess.clickDecay = std::max (0.0003f, std::min (0.003f, refDesc.attackTime * 0.3f));
                // Crossover: set at fundamentalFreq * 1.7 (above fundamental, below 2nd harmonic)
                guess.subCrossover = (refDesc.fundamentalFreq > 20.0f) ? refDesc.fundamentalFreq * 1.7f : 90.0f;
                // Prefer wavetable when available - it captures the exact timbre of the reference
                if (wavetable != nullptr && wavetable->valid)
                    guess.oscType = 12; // wavetable from reference
                // For dark kicks with lots of lowMid energy, saw or clipped sine work best
                else if (refDesc.lowMidEnergy > 0.2f && refDesc.brightness < 0.01f)
                    guess.oscType = 2; // saw
                // Body/sub mix balance from energy bands
                guess.bodyMix = std::max (0.3f, std::min (1.0f, refDesc.lowMidEnergy * 2.0f + 0.2f));
                guess.subMix = std::max (0.1f, std::min (0.8f, refDesc.subEnergy * 0.9f));
                guess.subLevel = std::max (0.1f, std::min (0.8f, refDesc.subEnergy * 0.8f));
                guess.clickMix = std::max (0.1f, std::min (1.0f, refDesc.transientStrength / 6.0f));
                guess.topMix = std::max (0.05f, std::min (1.0f, refDesc.highEnergy * 3.0f + 0.1f));
                // Suppress click/noise for dark kicks
                if (refDesc.brightness < 0.005f) {
                    guess.clickAmount = 0.0f;
                    guess.clickMix = 0.0f;
                    guess.topMix = 0.0f;
                    guess.noiseAmount = 0.0f;
                }
                guess.toArray (pop[0].data());
                setExtensionDefaults (pop[0]);
            }

            // Seed a few more with perturbed versions of the guess
            for (int i = 1; i < std::min (5, popSize); ++i)
            {
                pop[i] = pop[0];
                for (int j : activeParams)
                {
                    float range = maxs[j] - mins[j];
                    float perturbation = (uni (rng) - 0.5f) * 0.3f * range;
                    pop[i][j] = std::max (mins[j], std::min (maxs[j], pop[i][j] + perturbation));
                }
                pop[i][0] = std::max (0.0f, std::min ((float) maxOscType, std::round (pop[i][0])));
                setExtensionDefaults (pop[i]);
            }

            // Random for the rest
            for (int i = 5; i < popSize; ++i)
            {
                for (int j : activeParams)
                    pop[i][j] = mins[j] + uni (rng) * (maxs[j] - mins[j]);
                pop[i][0] = std::max (0.0f, std::min ((float) maxOscType, std::round (pop[i][0])));
                setExtensionDefaults (pop[i]);
            }
        }

        // === OscType screening: evaluate all 12 types with the guess params ===
        std::vector<std::pair<float, int>> oscScores;
        {
            auto testVec = pop[0]; // copy the best guess
            for (int osc = 0; osc <= maxOscType; ++osc)
            {
                testVec[0] = (float) osc;
                float fit = evaluate (testVec.data(), refDesc, sampleRate);
                oscScores.push_back ({fit, osc});
            }
            std::sort (oscScores.begin(), oscScores.end());

            int bestOsc = oscScores[0].second;
            int secondOsc = oscScores[1].second;
            int thirdOsc = oscScores[2].second;

            // If wavetable available, always include it in top 3
            if (wavetable != nullptr && wavetable->valid)
            {
                bool wtInTop3 = (bestOsc == 12 || secondOsc == 12 || thirdOsc == 12);
                if (!wtInTop3) thirdOsc = 12;
            }

            // If learned profile has a preferred oscType, boost it if it's in top 5
            if (learnedProfile != nullptr && learnedProfile->valid && learnedProfile->preferredOscType >= 0)
            {
                int pref = learnedProfile->preferredOscType;
                bool inTop3 = (pref == bestOsc || pref == secondOsc || pref == thirdOsc);
                if (! inTop3)
                {
                    // Check if it's at least in top 5
                    for (int k = 3; k < std::min (5, (int) oscScores.size()); ++k)
                    {
                        if (oscScores[k].second == pref)
                        {
                            thirdOsc = pref; // replace 3rd with learned preference
                            break;
                        }
                    }
                }
            }

            // Distribute population across top 3 oscTypes
            for (int i = 0; i < popSize; ++i)
            {
                if (i < popSize * 2 / 5)         pop[i][0] = (float) bestOsc;    // 40%
                else if (i < popSize * 13 / 20)  pop[i][0] = (float) secondOsc;  // 25%
                else if (i < popSize * 3 / 4)    pop[i][0] = (float) thirdOsc;   // 15%
                // rest: random oscType (already set)
            }
        }

        // === Narrow bounds for structural params based on reference descriptors ===
        // These params define the temporal/pitch structure and must stay close to the reference.
        // The optimizer should focus on timbral params (harmonics, mix, distortion, etc.)
        {
            auto narrow = [&](int idx, float center, float margin)
            {
                float lo = std::max (mins[idx], center * (1.0f - margin));
                float hi = std::min (maxs[idx], center * (1.0f + margin));
                if (hi > lo) { mins[idx] = lo; maxs[idx] = hi; }
            };
            auto narrowAbs = [&](int idx, float center, float halfRange)
            {
                float lo = std::max (mins[idx], center - halfRange);
                float hi = std::min (maxs[idx], center + halfRange);
                if (hi > lo) { mins[idx] = lo; maxs[idx] = hi; }
            };

            // === HARD LOCK structural params with DIRECT assignments ===
            // No narrow() — direct min/max to guarantee they apply.

            float f0 = std::max (30.0f, refDescFull.fundamentalFreq);

            // basePitch: ±10% of fundamental
            mins[1] = f0 * 0.9f;
            maxs[1] = f0 * 1.1f;

            // pitchEnvDepth: ±4st of detected, min 0
            mins[3] = std::max (0.0f, refDescFull.pitchDropSemitones - 4.0f);
            maxs[3] = refDescFull.pitchDropSemitones + 4.0f;

            // pitchEnvFast: 0.3-5ms
            mins[4] = 0.0003f;
            maxs[4] = 0.005f;

            // pitchEnvSlow: ±40% of drop time
            if (refDescFull.pitchDropTime > 0.002f)
            {
                mins[5] = refDescFull.pitchDropTime * 0.6f;
                maxs[5] = refDescFull.pitchDropTime * 1.4f;
            }

            // ampAttack: 0.1ms to max(50ms, attackTime*2)
            mins[7] = 0.0001f;
            maxs[7] = std::max (0.05f, refDescFull.attackTime * 2.0f);

            // subTailDecay: max = totalDuration * 0.8
            maxs[12] = std::min (maxs[12], std::max (0.1f, refDescFull.totalDuration * 0.8f));

            // filterCutoff: based on spectral content
            {
                float fc = std::max (200.0f, refDescFull.spectralRolloff * 2.5f);
                mins[20] = fc * 0.5f;
                maxs[20] = std::min (maxs[20], fc * 2.0f);
            }

            // subPitch: LOCK to 0 (follow body pitch) — prevent independent sub freq
            mins[84] = 0.0f;
            maxs[84] = 0.0f;

            // subCrossover: near fundamental * 1.7
            {
                float xover = f0 * 1.7f;
                mins[105] = xover * 0.6f;
                maxs[105] = xover * 1.5f;
            }

            // bodyMix: minimum 0.3 (body must be present)
            mins[76] = 0.3f;

            // distortion: cap proportional to harmonic content
            // Clean sounds (HNR close to 2.0 = pure tone) need little distortion
            maxs[18] = std::max (0.1f, std::min (1.0f, (2.0f - refDescFull.harmonicNoiseRatio) * 3.0f));
            // For this kick: HNR=1.988, so max = (2-1.988)*3 = 0.036. Very little distortion.

            // Suppress click/noise/top based on reference HF content
            // topMix scales with highEnergy; if no high energy, cap it
            maxs[79] = std::max (0.05f, std::min (1.0f, refDescFull.highEnergy * 20.0f + 0.05f)); // topMix
            if (refDescFull.brightness < 0.01f)
            {
                maxs[14] = 0.15f;   // clickAmount
                maxs[19] = 0.15f;   // noiseAmount
                maxs[78] = 0.15f;   // clickMix
            }
        }

        // Evaluate initial population
        for (int i = 0; i < popSize; ++i)
        {
            pop[i][0] = std::max (0.0f, std::min ((float) maxOscType, std::round (pop[i][0])));
            // Clamp to narrowed bounds
            for (int j : activeParams)
                pop[i][j] = std::max (mins[j], std::min (maxs[j], pop[i][j]));
            fitness[i] = evaluate (pop[i].data(), refDesc, sampleRate);
        }

        int bestIdx = 0;
        for (int i = 1; i < popSize; ++i)
            if (fitness[i] < fitness[bestIdx]) bestIdx = i;

        OptimizationResult result;

        // Multi-pass iteration budget
        int phase1Iters = maxIterations / 2;              // 50% — core only (125)
        int phase2Iters = (maxIterations * 3) / 10;       // 30% — core + ext1 (75)
        int phase3Iters = maxIterations - phase1Iters - phase2Iters; // 20% — all active (50)
        int totalReportIters = maxIterations;

        // Convergence tracking
        float prevBest = fitness[bestIdx];
        int stagnationCount = 0;
        const int stagnationLimit = 15;
        const float improvementEpsilon = 0.0003f;
        int globalIter = 0;
        int restartsUsed = 0;
        const int maxRestarts = 2;

        // Partial restart: keep top 30%, re-randomize bottom 70% near best
        auto partialRestart = [&]()
        {
            // Sort population by fitness
            std::vector<int> sortedIdx (popSize);
            std::iota (sortedIdx.begin(), sortedIdx.end(), 0);
            std::sort (sortedIdx.begin(), sortedIdx.end(),
                       [&](int a, int b) { return fitness[a] < fitness[b]; });

            int keepCount = std::max (2, popSize * 3 / 10); // keep top 30%
            for (int rank = keepCount; rank < popSize; ++rank)
            {
                int i = sortedIdx[rank];
                // Perturb around best individual with moderate noise
                pop[i] = pop[bestIdx];
                for (int j : activeParams)
                {
                    float range = maxs[j] - mins[j];
                    float noise = (uni (rng) - 0.5f) * 0.5f * range;
                    pop[i][j] = std::max (mins[j], std::min (maxs[j], pop[i][j] + noise));
                }
                pop[i][0] = std::max (0.0f, std::min ((float) maxOscType, std::round (pop[i][0])));
                fitness[i] = evaluate (pop[i].data(), refDesc, sampleRate);
                if (fitness[i] < fitness[bestIdx]) bestIdx = i;
            }
            stagnationCount = 0;
            prevBest = fitness[bestIdx];
        };

        // === PHASE 1: DE on core params only (50% budget) ===
        for (int iter = 0; iter < phase1Iters; ++iter)
        {
            runDEGeneration (pop, fitness, F_vec, CR_vec, activeParams,
                             mins, maxs, bestIdx, refDesc, sampleRate, rng, uni,
                             (float) globalIter / (float) totalReportIters);

            ++globalIter;
            result.iterations = globalIter;
            result.bestDistance = fitness[bestIdx];

            if (progress && ! progress (globalIter, fitness[bestIdx], totalReportIters))
                break;

            if (fitness[bestIdx] < targetDistance)
            {
                result.converged = true;
                break;
            }

            // Convergence detection with partial restart
            float improvement = prevBest - fitness[bestIdx];
            if (improvement < improvementEpsilon) ++stagnationCount;
            else stagnationCount = 0;
            prevBest = fitness[bestIdx];

            if (stagnationCount >= stagnationLimit)
            {
                if (restartsUsed < maxRestarts)
                {
                    partialRestart();
                    ++restartsUsed;
                }
                else
                    break;
            }
        }

        result.phase1Iterations = globalIter;
        result.phase1Distance = fitness[bestIdx];

        // === GAP ANALYSIS #1 ===
        if (! result.converged)
        {
            // Generate best sound and extract descriptors for gap analysis
            MatchSynthParams bestP;
            bestP.fromArray (pop[bestIdx].data());
            auto bestBuffer = synth.generate (bestP, sampleRate);
            extractor.setFastMode (true);
            auto matchedDesc = extractor.extract (bestBuffer, sampleRate);
            extractor.setFastMode (false);

            auto gaps1 = analyzeGaps (refDesc, matchedDesc);
            result.gaps = gaps1;
            result.extensionsActivated = gaps1.extensionCount();

            if (result.extensionsActivated > 0)
            {
                // Activate extension params
                auto extIndices = gaps1.getExtensionIndices();
                for (int idx : extIndices)
                    activeParams.push_back (idx);

                // Initialize extension params in population
                // Best individual: seed extension gate params at moderate value
                for (int idx : extIndices)
                {
                    int gateIdx = MatchSynthParams::getExtensionGateIndex (idx);
                    if (idx == gateIdx)
                    {
                        // Gate param: start at moderate value
                        pop[bestIdx][idx] = 0.3f;
                    }
                    // Other extension params: keep at sensible defaults (already set)
                }

                // Perturb rest of population's extension params
                for (int i = 0; i < popSize; ++i)
                {
                    if (i == bestIdx) continue;
                    for (int idx : extIndices)
                    {
                        float range = maxs[idx] - mins[idx];
                        pop[i][idx] = mins[idx] + uni (rng) * range;
                    }
                    fitness[i] = evaluate (pop[i].data(), refDesc, sampleRate);
                }

                // Update best
                for (int i = 0; i < popSize; ++i)
                    if (fitness[i] < fitness[bestIdx]) bestIdx = i;

                // Reset stagnation for phase 2
                stagnationCount = 0;
                prevBest = fitness[bestIdx];

                // === PHASE 2: DE on core + ext1 (30% budget) ===
                restartsUsed = 0;
                for (int iter = 0; iter < phase2Iters; ++iter)
                {
                    runDEGeneration (pop, fitness, F_vec, CR_vec, activeParams,
                                     mins, maxs, bestIdx, refDesc, sampleRate, rng, uni,
                                     (float) globalIter / (float) totalReportIters);

                    ++globalIter;
                    result.iterations = globalIter;
                    result.bestDistance = fitness[bestIdx];

                    if (progress && ! progress (globalIter, fitness[bestIdx], totalReportIters))
                        break;

                    if (fitness[bestIdx] < targetDistance)
                    {
                        result.converged = true;
                        break;
                    }

                    float improvement = prevBest - fitness[bestIdx];
                    if (improvement < improvementEpsilon) ++stagnationCount;
                    else stagnationCount = 0;
                    prevBest = fitness[bestIdx];

                    if (stagnationCount >= stagnationLimit)
                    {
                        if (restartsUsed < maxRestarts)
                        {
                            partialRestart();
                            ++restartsUsed;
                        }
                        else
                            break;
                    }
                }
            }
        }

        // === GAP ANALYSIS #2 — re-analyze on current best, activate more extensions ===
        if (! result.converged)
        {
            MatchSynthParams bestP2;
            bestP2.fromArray (pop[bestIdx].data());
            auto bestBuffer2 = synth.generate (bestP2, sampleRate);
            extractor.setFastMode (true);
            auto matchedDesc2 = extractor.extract (bestBuffer2, sampleRate);
            extractor.setFastMode (false);

            auto gaps2 = analyzeGaps (refDesc, matchedDesc2);

            // Merge gap flags (OR with previous)
            result.gaps.merge (gaps2);

            // Find NEW extension indices not already in activeParams
            auto newExtIndices = gaps2.getExtensionIndices();
            std::vector<int> newlyAdded;

            for (int idx : newExtIndices)
            {
                bool alreadyActive = false;
                for (int a : activeParams)
                    if (a == idx) { alreadyActive = true; break; }
                if (! alreadyActive)
                {
                    activeParams.push_back (idx);
                    newlyAdded.push_back (idx);
                }
            }

            result.extensionsActivated = result.gaps.extensionCount();

            if (! newlyAdded.empty())
            {
                // Initialize newly added extension params in population
                for (int idx : newlyAdded)
                {
                    int gateIdx = MatchSynthParams::getExtensionGateIndex (idx);
                    if (idx == gateIdx)
                        pop[bestIdx][idx] = 0.3f;
                }

                // Perturb rest of population's newly added extension params
                for (int i = 0; i < popSize; ++i)
                {
                    if (i == bestIdx) continue;
                    for (int idx : newlyAdded)
                    {
                        float range = maxs[idx] - mins[idx];
                        pop[i][idx] = mins[idx] + uni (rng) * range;
                    }
                    fitness[i] = evaluate (pop[i].data(), refDesc, sampleRate);
                }

                // Update best
                for (int i = 0; i < popSize; ++i)
                    if (fitness[i] < fitness[bestIdx]) bestIdx = i;
            }

            // Reset stagnation for phase 3
            stagnationCount = 0;
            prevBest = fitness[bestIdx];

            // === PHASE 3: DE on all active params (20% budget) ===
            restartsUsed = 0;
            for (int iter = 0; iter < phase3Iters; ++iter)
            {
                runDEGeneration (pop, fitness, F_vec, CR_vec, activeParams,
                                 mins, maxs, bestIdx, refDesc, sampleRate, rng, uni,
                                 (float) globalIter / (float) totalReportIters);

                ++globalIter;
                result.iterations = globalIter;
                result.bestDistance = fitness[bestIdx];

                if (progress && ! progress (globalIter, fitness[bestIdx], totalReportIters))
                    break;

                if (fitness[bestIdx] < targetDistance)
                {
                    result.converged = true;
                    break;
                }

                float improvement = prevBest - fitness[bestIdx];
                if (improvement < improvementEpsilon) ++stagnationCount;
                else stagnationCount = 0;
                prevBest = fitness[bestIdx];

                if (stagnationCount >= stagnationLimit)
                {
                    if (restartsUsed < maxRestarts)
                    {
                        partialRestart();
                        ++restartsUsed;
                    }
                    else
                        break;
                }
            }
        }

        // === PHASE 4: Sensitivity analysis ===
        auto bestVec = pop[bestIdx];
        float bestFit = fitness[bestIdx];

        for (int j = 0; j < N; ++j)
        {
            // Only analyze active params
            bool isActive = false;
            for (int a : activeParams)
                if (a == j) { isActive = true; break; }
            if (! isActive) { result.sensitivity[j] = 0.0f; continue; }

            float range = maxs[j] - mins[j];
            float step = range * 0.02f;

            std::vector<float> testVec = bestVec;
            testVec[j] = std::max (mins[j], std::min (maxs[j], bestVec[j] + step));
            if (j == 0) testVec[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (testVec[0])));
            float fitPlus = evaluate (testVec.data(), refDesc, sampleRate, true);

            testVec[j] = std::max (mins[j], std::min (maxs[j], bestVec[j] - step));
            if (j == 0) testVec[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (testVec[0])));
            float fitMinus = evaluate (testVec.data(), refDesc, sampleRate, true);

            float sensitivity = std::abs (fitPlus - bestFit) + std::abs (fitMinus - bestFit);
            result.sensitivity[j] = sensitivity;
        }

        // Normalize sensitivity
        float maxSens = *std::max_element (result.sensitivity.begin(), result.sensitivity.end());
        if (maxSens > 0.0f)
            for (auto& s : result.sensitivity) s /= maxSens;

        // === Nelder-Mead on sensitive active params ===
        std::vector<int> nmParams;
        for (int j : activeParams)
            if (result.sensitivity[j] > 0.1f)
                nmParams.push_back (j);

        if (! nmParams.empty())
        {
            auto refined = nelderMeadRefine (bestVec, nmParams, mins, maxs, refDesc, sampleRate);
            // Compare using same metric (descriptor-only) to decide if NM improved
            float refinedDescDist = evaluate (refined.data(), refDesc, sampleRate, false);

            if (refinedDescDist < bestFit)
            {
                bestVec = refined;
                bestFit = refinedDescDist;
            }
        }

        result.bestDistance = bestFit;
        result.bestParams.fromArray (bestVec.data());
        if (result.bestDistance < targetDistance)
            result.converged = true;

        return result;
    }

    // Side-channel data setters
    void setWavetable (const WavetableData* wt) { wavetable = wt; maxOscType = (wt && wt->valid) ? 12 : 11; }
    void setResidualNoise (const ResidualNoiseData* rn) { residualNoise = rn; }
    void setTransientSample (const TransientSampleData* ts) { transientSample = ts; }
    void setSpectralEnvelope (const SpectralEnvelopeData* se) { spectralEnvelope = se; }
    void setHarmonicPhases (const HarmonicPhaseData* hp) { harmonicPhases = hp; }
    void setLearnedProfile (const LearnedProfile* lp) { learnedProfile = lp; }

private:
    int   maxIterations  = 150;
    float targetDistance  = 1.5f;
    int   popSize        = 40;
    int   maxOscType     = 11;

    OneShotMatchSynth synth;
    DescriptorExtractor extractor;

    const juce::AudioBuffer<float>* refBuffer = nullptr;
    std::vector<float> refMono;
    bool refMonoReady = false;
    const WavetableData* wavetable = nullptr;
    const ResidualNoiseData* residualNoise = nullptr;
    const TransientSampleData* transientSample = nullptr;
    const SpectralEnvelopeData* spectralEnvelope = nullptr;
    const HarmonicPhaseData* harmonicPhases = nullptr;
    const LearnedProfile* learnedProfile = nullptr;

    void configureSynth()
    {
        synth.setWavetable (wavetable);
        synth.setResidualNoise (residualNoise);
        synth.setTransientSample (transientSample);
        synth.setSpectralEnvelope (spectralEnvelope);
        synth.setHarmonicPhases (harmonicPhases);
    }

    // Prepare reference mono at optSR — called once before optimization loop
    void prepareRefMono (double optSR, double origSR)
    {
        if (refBuffer == nullptr || refBuffer->getNumSamples() == 0) return;

        // Mix to mono
        int origLen = refBuffer->getNumSamples();
        std::vector<float> monoOrig (origLen, 0.0f);
        for (int ch = 0; ch < refBuffer->getNumChannels(); ++ch)
        {
            const float* data = refBuffer->getReadPointer (ch);
            for (int i = 0; i < origLen; ++i)
                monoOrig[i] += data[i];
        }
        float scale = 1.0f / (float) refBuffer->getNumChannels();
        for (auto& s : monoOrig) s *= scale;

        // Resample to optSR
        double ratio = optSR / origSR;
        int newLen = (int)(origLen * ratio);
        refMono.resize (newLen);
        for (int i = 0; i < newLen; ++i)
        {
            float srcPos = (float) i / (float) ratio;
            int s0 = std::min ((int) srcPos, origLen - 1);
            int s1 = std::min (s0 + 1, origLen - 1);
            float frac = srcPos - (float) s0;
            refMono[i] = monoOrig[s0] * (1.0f - frac) + monoOrig[s1] * frac;
        }
        refMonoReady = true;
    }

    // Multi-resolution STFT loss: compare log-magnitude spectra at 3 resolutions
    // This is the gold standard for audio matching (used in DDSP, neural synths, etc.)
    float multiResSTFTLoss (const juce::AudioBuffer<float>& gen) const
    {
        if (! refMonoReady || refMono.empty()) return 0.0f;

        // Mix generated to mono
        int genLen = gen.getNumSamples();
        int compareLen = std::min ((int) refMono.size(), genLen);
        if (compareLen < 256) return 100.0f;

        std::vector<float> genMono (genLen, 0.0f);
        for (int ch = 0; ch < gen.getNumChannels(); ++ch)
        {
            const float* data = gen.getReadPointer (ch);
            for (int i = 0; i < genLen; ++i)
                genMono[i] += data[i];
        }
        float chScale = 1.0f / (float) gen.getNumChannels();
        for (auto& s : genMono) s *= chScale;

        float totalLoss = 0.0f;

        // 3 resolutions: 256, 1024, 2048 samples
        static constexpr int NUM_RES = 3;
        static constexpr int fftOrders[NUM_RES] = { 8, 10, 11 };  // 256, 1024, 2048
        static constexpr float resWeights[NUM_RES] = { 0.25f, 0.5f, 0.25f };

        for (int r = 0; r < NUM_RES; ++r)
        {
            int fftSize = 1 << fftOrders[r];
            int hopSize = fftSize / 4;
            int numBins = fftSize / 2;

            if (compareLen < fftSize) continue;

            juce::dsp::FFT fft (fftOrders[r]);

            int numFrames = std::max (1, (compareLen - fftSize) / hopSize + 1);
            numFrames = std::min (numFrames, 16); // cap at 16 frames for speed

            float specLoss = 0.0f;  // spectral convergence (log-mag L1)
            float magLoss = 0.0f;   // log-mag L1 distance
            int totalBins = 0;

            for (int frame = 0; frame < numFrames; ++frame)
            {
                int start = frame * hopSize;
                if (start + fftSize > compareLen) break;

                // Hann window + FFT for both signals
                std::vector<float> refFFT (fftSize * 2, 0.0f);
                std::vector<float> genFFT (fftSize * 2, 0.0f);

                for (int i = 0; i < fftSize; ++i)
                {
                    float win = 0.5f * (1.0f - std::cos (2.0f * 3.14159265f * (float) i / (float) fftSize));
                    refFFT[i] = refMono[start + i] * win;
                    genFFT[i] = ((start + i) < genLen ? genMono[start + i] : 0.0f) * win;
                }

                fft.performRealOnlyForwardTransform (refFFT.data());
                fft.performRealOnlyForwardTransform (genFFT.data());

                // Compare log magnitudes
                for (int b = 1; b < numBins; ++b) // skip DC
                {
                    float refMag = std::sqrt (refFFT[b * 2] * refFFT[b * 2] + refFFT[b * 2 + 1] * refFFT[b * 2 + 1]);
                    float genMag = std::sqrt (genFFT[b * 2] * genFFT[b * 2] + genFFT[b * 2 + 1] * genFFT[b * 2 + 1]);

                    float refLog = std::log (std::max (1e-7f, refMag));
                    float genLog = std::log (std::max (1e-7f, genMag));

                    magLoss += std::abs (refLog - genLog);
                    ++totalBins;
                }
            }

            if (totalBins > 0)
                totalLoss += resWeights[r] * magLoss / (float) totalBins;
        }

        return totalLoss;
    }

    float evaluate (const float* paramArray, const MatchDescriptors& ref, double sampleRate,
                    bool /*useSTFT*/ = false)
    {
        MatchSynthParams p;
        p.fromArray (paramArray);
        configureSynth();
        auto buffer = synth.generate (p, sampleRate);
        // Check for empty/NaN buffer
        {
            float peak = buffer.getMagnitude (0, 0, buffer.getNumSamples());
            if (peak < 1e-8f || std::isnan (peak) || std::isinf (peak))
                return 1e6f;
        }

        // PRIMARY: direct spectral comparison — this is the ground truth
        float stftLoss = 0.0f;
        if (refMonoReady)
            stftLoss = multiResSTFTLoss (buffer);

        // SECONDARY: fast descriptor distance for structural guidance (pitch, envelope)
        extractor.setFastMode (true);
        auto genDesc = extractor.extract (buffer, sampleRate);
        extractor.setFastMode (false);
        if (! genDesc.valid) return 1e6f;

        DistanceWeights dw;
        if (learnedProfile != nullptr && learnedProfile->valid)
        {
            dw.envelope = learnedProfile->envWeight;
            dw.pitch = learnedProfile->pitchWeight;
            dw.spectral = learnedProfile->spectralWeight;
            dw.sub = learnedProfile->subWeight;
            dw.transient = learnedProfile->transientWeight;
            dw.spectroTemporal = learnedProfile->spectroTemporalWeight;
        }
        float descDist = computeDistance (ref, genDesc, dw);

        // Hybrid: STFT is primary (matches actual audio), descriptors guide structure
        if (refMonoReady)
            return stftLoss * 2.0f + descDist * 0.3f;

        return descDist;
    }

    // Run one generation of DE on the active param subset
    // progressRatio: 0..1 indicating how far through the total budget we are
    void runDEGeneration (std::vector<std::vector<float>>& pop,
                          std::vector<float>& fitness,
                          std::vector<float>& F_vec,
                          std::vector<float>& CR_vec,
                          const std::vector<int>& activeParams,
                          const float* mins, const float* maxs,
                          int& bestIdx,
                          const MatchDescriptors& refDesc,
                          double sampleRate,
                          std::mt19937& rng,
                          std::uniform_real_distribution<float>& uni,
                          float progressRatio = 0.5f)
    {
        const int PS = (int) pop.size();
        const int A = (int) activeParams.size();

        // Greediness ramps from 0.3 (exploration) to 1.0 (exploitation)
        float greediness = 0.3f + 0.7f * progressRatio;

        for (int i = 0; i < PS; ++i)
        {
            // jDE self-adaptation
            float Fi  = (uni (rng) < 0.1f) ? 0.1f + 0.9f * uni (rng) : F_vec[i];
            float CRi = (uni (rng) < 0.1f) ? uni (rng) : CR_vec[i];

            // DE/current-to-best/1 hybrid
            // Early: more exploration (current + small step toward best)
            // Late: more exploitation (nearly pure best/1)
            int r1, r2;
            do { r1 = rng() % PS; } while (r1 == i);
            do { r2 = rng() % PS; } while (r2 == i || r2 == r1);

            auto trial = pop[i]; // copy all (keeps inactive params)
            int jrand = rng() % A;

            for (int k = 0; k < A; ++k)
            {
                int j = activeParams[k];
                if (uni (rng) < CRi || k == jrand)
                {
                    // Blend: current + greediness * (best - current) + F * (r1 - r2)
                    trial[j] = pop[i][j]
                             + greediness * Fi * (pop[bestIdx][j] - pop[i][j])
                             + Fi * (pop[r1][j] - pop[r2][j]);
                }

                trial[j] = std::max (mins[j], std::min (maxs[j], trial[j]));
            }
            // Discrete params: occasionally try a random type instead of continuous mutation
            if (uni (rng) < 0.1f)
                trial[0] = (float) (rng() % (maxOscType + 1));
            trial[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (trial[0])));
            // clickType (0-3), masterSatType (0-2) — discrete
            trial[97] = std::max (0.0f, std::min (3.0f, std::round (trial[97])));
            trial[99] = std::max (0.0f, std::min (2.0f, std::round (trial[99])));

            float trialFit = evaluate (trial.data(), refDesc, sampleRate);

            if (trialFit <= fitness[i])
            {
                pop[i] = trial;
                fitness[i] = trialFit;
                F_vec[i] = Fi;
                CR_vec[i] = CRi;

                if (trialFit < fitness[bestIdx])
                    bestIdx = i;
            }
        }
    }

    // Nelder-Mead on a subset of active parameters
    std::vector<float> nelderMeadRefine (const std::vector<float>& start,
                                          const std::vector<int>& activeParams,
                                          const float* mins, const float* maxs,
                                          const MatchDescriptors& ref, double sampleRate)
    {
        const int M = (int) activeParams.size();
        const int maxNMIter = 30;

        auto inject = [&](const std::vector<float>& base, const std::vector<float>& subVec) -> std::vector<float>
        {
            auto full = base;
            for (int i = 0; i < M; ++i)
                full[activeParams[i]] = subVec[i];
            full[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (full[0])));
            full[97] = std::max (0.0f, std::min (3.0f, std::round (full[97])));
            full[99] = std::max (0.0f, std::min (2.0f, std::round (full[99])));
            return full;
        };

        auto project = [&](const std::vector<float>& full) -> std::vector<float>
        {
            std::vector<float> sub (M);
            for (int i = 0; i < M; ++i)
                sub[i] = full[activeParams[i]];
            return sub;
        };

        auto clampSub = [&](std::vector<float>& sub)
        {
            for (int i = 0; i < M; ++i)
            {
                int pi = activeParams[i];
                sub[i] = std::max (mins[pi], std::min (maxs[pi], sub[i]));
            }
        };

        std::vector<std::vector<float>> simplex (M + 1, project (start));
        std::vector<float> fvals (M + 1);

        for (int i = 1; i <= M; ++i)
        {
            int pi = activeParams[i - 1];
            float range = maxs[pi] - mins[pi];
            simplex[i][i - 1] += range * 0.03f;
            clampSub (simplex[i]);
        }

        for (int i = 0; i <= M; ++i)
        {
            auto full = inject (start, simplex[i]);
            fvals[i] = evaluate (full.data(), ref, sampleRate, true);
        }

        for (int iter = 0; iter < maxNMIter; ++iter)
        {
            std::vector<int> idx (M + 1);
            std::iota (idx.begin(), idx.end(), 0);
            std::sort (idx.begin(), idx.end(), [&](int a, int b) { return fvals[a] < fvals[b]; });

            auto sortedS = simplex;
            auto sortedF = fvals;
            for (int i = 0; i <= M; ++i) { sortedS[i] = simplex[idx[i]]; sortedF[i] = fvals[idx[i]]; }
            simplex = sortedS;
            fvals = sortedF;

            std::vector<float> centroid (M, 0.0f);
            for (int i = 0; i < M; ++i)
                for (int j = 0; j < M; ++j)
                    centroid[j] += simplex[i][j];
            for (int j = 0; j < M; ++j)
                centroid[j] /= (float) M;

            std::vector<float> reflected (M);
            for (int j = 0; j < M; ++j)
                reflected[j] = centroid[j] + 1.0f * (centroid[j] - simplex[M][j]);
            clampSub (reflected);

            auto fullR = inject (start, reflected);
            float fr = evaluate (fullR.data(), ref, sampleRate, true);

            if (fr < fvals[0])
            {
                std::vector<float> expanded (M);
                for (int j = 0; j < M; ++j)
                    expanded[j] = centroid[j] + 2.0f * (reflected[j] - centroid[j]);
                clampSub (expanded);
                auto fullE = inject (start, expanded);
                float fe = evaluate (fullE.data(), ref, sampleRate, true);
                simplex[M] = (fe < fr) ? expanded : reflected;
                fvals[M] = std::min (fe, fr);
            }
            else if (fr < fvals[M - 1])
            {
                simplex[M] = reflected;
                fvals[M] = fr;
            }
            else
            {
                std::vector<float> contracted (M);
                for (int j = 0; j < M; ++j)
                    contracted[j] = centroid[j] + 0.5f * (simplex[M][j] - centroid[j]);
                clampSub (contracted);
                auto fullC = inject (start, contracted);
                float fc = evaluate (fullC.data(), ref, sampleRate, true);

                if (fc < fvals[M])
                {
                    simplex[M] = contracted;
                    fvals[M] = fc;
                }
                else
                {
                    for (int i = 1; i <= M; ++i)
                    {
                        for (int j = 0; j < M; ++j)
                            simplex[i][j] = simplex[0][j] + 0.5f * (simplex[i][j] - simplex[0][j]);
                        clampSub (simplex[i]);
                        auto fullS = inject (start, simplex[i]);
                        fvals[i] = evaluate (fullS.data(), ref, sampleRate, true);
                    }
                }
            }
        }

        int best = 0;
        for (int i = 1; i <= M; ++i)
            if (fvals[i] < fvals[best]) best = i;

        return inject (start, simplex[best]);
    }
};

} // namespace oneshotmatch
