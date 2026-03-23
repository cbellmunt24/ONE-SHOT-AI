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

    OptimizationResult optimize (const MatchDescriptors& refDesc,
                                 double sampleRate,
                                 ProgressCallback progress = nullptr,
                                 const MatchSynthParams* seedParams = nullptr)
    {
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
            vec[77] = 0.70f;   // subMix
            vec[78] = 0.35f;   // clickMix
            vec[79] = 0.70f;   // topMix
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

            // Individual 5: descriptor-based guess
            if (popSize > 5)
            {
                MatchSynthParams guess;
                guess.basePitch = (refDesc.fundamentalFreq > 20.0f) ? refDesc.fundamentalFreq : 50.0f;
                guess.pitchEnvDepth = std::max (0.0f, refDesc.pitchDropSemitones);
                guess.pitchEnvFast = std::max (0.001f, refDesc.pitchDropTime * 0.2f);
                guess.pitchEnvSlow = std::max (0.01f, refDesc.pitchDropTime * 1.5f);
                guess.ampAttack = std::max (0.0001f, std::min (0.01f, refDesc.attackTime));
                guess.ampPunchDecay = std::max (0.01f, refDesc.decayTime * 0.6f);
                guess.ampBodyDecay = std::max (0.05f, refDesc.decayTime);
                guess.subTailDecay = std::max (0.05f, refDesc.decayTime40);
                guess.clickAmount = std::min (1.0f, std::max (0.0f, refDesc.transientStrength / 5.0f));
                guess.ampPunchLevel = (refDesc.transientStrength > 4.0f) ? 0.7f : 0.4f;
                guess.clickFreq = std::max (1000.0f, refDesc.transientRegion.spectralCentroid);
                guess.distortion = (refDesc.harmonicNoiseRatio > 0.4f) ? 0.3f : 0.0f;
                guess.filterCutoff = std::max (1000.0f, refDesc.spectralRolloff * 1.3f);
                guess.subLevel = std::min (1.0f, std::max (0.0f, refDesc.subEnergy * 4.0f));
                guess.noiseAmount = std::min (0.5f, std::max (0.0f, refDesc.highEnergy * 2.0f));
                guess.bodyHarmonics = std::min (0.5f, std::max (0.0f, (1.0f - refDesc.bodyRegion.spectralFlatness) * 0.3f));
                guess.harmonicEmphasis = std::min (0.5f, std::max (0.0f, refDesc.lowMidEnergy));
                // v4 mix: seed from descriptor energy distribution
                guess.bodyMix = std::max (0.1f, std::min (1.0f, refDesc.lowMidEnergy * 2.0f + 0.3f));
                guess.subMix = std::max (0.1f, std::min (1.0f, refDesc.subEnergy * 3.0f + 0.2f));
                guess.clickMix = std::max (0.0f, std::min (1.0f, refDesc.transientStrength / 8.0f));
                guess.topMix = std::max (0.0f, std::min (1.0f, refDesc.highEnergy * 3.0f));
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
                guess.pitchEnvFast = std::max (0.001f, refDesc.pitchDropTime * 0.2f);
                guess.pitchEnvSlow = std::max (0.01f, refDesc.pitchDropTime * 1.5f);
                guess.ampAttack = std::max (0.0001f, std::min (0.01f, refDesc.attackTime));
                guess.ampPunchDecay = std::max (0.01f, refDesc.decayTime * 0.6f);
                guess.ampBodyDecay = std::max (0.05f, refDesc.decayTime);
                guess.subTailDecay = std::max (0.05f, refDesc.decayTime40);
                guess.clickAmount = std::min (1.0f, std::max (0.0f, refDesc.transientStrength / 5.0f));
                guess.ampPunchLevel = (refDesc.transientStrength > 4.0f) ? 0.7f : 0.4f;
                guess.clickFreq = std::max (1000.0f, refDesc.transientRegion.spectralCentroid);
                guess.distortion = (refDesc.harmonicNoiseRatio > 0.4f) ? 0.3f : 0.0f;
                guess.filterCutoff = std::max (1000.0f, refDesc.spectralRolloff * 1.3f);
                guess.subLevel = std::min (1.0f, std::max (0.0f, refDesc.subEnergy * 4.0f));
                guess.noiseAmount = std::min (0.5f, std::max (0.0f, refDesc.highEnergy * 2.0f));
                guess.bodyHarmonics = std::min (0.5f, std::max (0.0f, (1.0f - refDesc.bodyRegion.spectralFlatness) * 0.3f));
                guess.harmonicEmphasis = std::min (0.5f, std::max (0.0f, refDesc.lowMidEnergy));
                // v4 mix: seed from descriptor energy distribution
                guess.bodyMix = std::max (0.1f, std::min (1.0f, refDesc.lowMidEnergy * 2.0f + 0.3f));
                guess.subMix = std::max (0.1f, std::min (1.0f, refDesc.subEnergy * 3.0f + 0.2f));
                guess.clickMix = std::max (0.0f, std::min (1.0f, refDesc.transientStrength / 8.0f));
                guess.topMix = std::max (0.0f, std::min (1.0f, refDesc.highEnergy * 3.0f));
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

        // Evaluate initial population
        for (int i = 0; i < popSize; ++i)
        {
            pop[i][0] = std::max (0.0f, std::min ((float) maxOscType, std::round (pop[i][0])));
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
        const int stagnationLimit = 30;
        const float improvementEpsilon = 0.0003f;
        int globalIter = 0;

        // === PHASE 1: DE on core params only (50% budget) ===
        for (int iter = 0; iter < phase1Iters; ++iter)
        {
            runDEGeneration (pop, fitness, F_vec, CR_vec, activeParams,
                             mins, maxs, bestIdx, refDesc, sampleRate, rng, uni);

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

            // Convergence detection
            float improvement = prevBest - fitness[bestIdx];
            if (improvement < improvementEpsilon) ++stagnationCount;
            else stagnationCount = 0;
            prevBest = fitness[bestIdx];

            if (stagnationCount >= stagnationLimit)
                break;
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
            auto matchedDesc = extractor.extract (bestBuffer, sampleRate);

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
                for (int iter = 0; iter < phase2Iters; ++iter)
                {
                    runDEGeneration (pop, fitness, F_vec, CR_vec, activeParams,
                                     mins, maxs, bestIdx, refDesc, sampleRate, rng, uni);

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
                        break;
                }
            }
        }

        // === GAP ANALYSIS #2 — re-analyze on current best, activate more extensions ===
        if (! result.converged)
        {
            MatchSynthParams bestP2;
            bestP2.fromArray (pop[bestIdx].data());
            auto bestBuffer2 = synth.generate (bestP2, sampleRate);
            auto matchedDesc2 = extractor.extract (bestBuffer2, sampleRate);

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
            for (int iter = 0; iter < phase3Iters; ++iter)
            {
                runDEGeneration (pop, fitness, F_vec, CR_vec, activeParams,
                                 mins, maxs, bestIdx, refDesc, sampleRate, rng, uni);

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
                    break;
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
            float fitPlus = evaluate (testVec.data(), refDesc, sampleRate);

            testVec[j] = std::max (mins[j], std::min (maxs[j], bestVec[j] - step));
            if (j == 0) testVec[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (testVec[0])));
            float fitMinus = evaluate (testVec.data(), refDesc, sampleRate);

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
            float refinedFit = evaluate (refined.data(), refDesc, sampleRate);

            if (refinedFit < bestFit)
            {
                bestVec = refined;
                bestFit = refinedFit;
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
    int   maxIterations  = 250;
    float targetDistance  = 0.4f;
    int   popSize        = 60;
    int   maxOscType     = 11;

    OneShotMatchSynth synth;
    DescriptorExtractor extractor;

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

    float evaluate (const float* paramArray, const MatchDescriptors& ref, double sampleRate)
    {
        MatchSynthParams p;
        p.fromArray (paramArray);
        configureSynth();
        auto buffer = synth.generate (p, sampleRate);
        auto genDesc = extractor.extract (buffer, sampleRate);
        // Use adaptive distance weights if learned profile is available
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
        return computeDistance (ref, genDesc, dw);
    }

    // Run one generation of DE on the active param subset
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
                          std::uniform_real_distribution<float>& uni)
    {
        const int PS = (int) pop.size();
        const int A = (int) activeParams.size();

        for (int i = 0; i < PS; ++i)
        {
            // jDE self-adaptation
            float Fi  = (uni (rng) < 0.1f) ? 0.1f + 0.9f * uni (rng) : F_vec[i];
            float CRi = (uni (rng) < 0.1f) ? uni (rng) : CR_vec[i];

            // DE/best/1
            int r1, r2;
            do { r1 = rng() % PS; } while (r1 == i);
            do { r2 = rng() % PS; } while (r2 == i || r2 == r1);

            auto trial = pop[i]; // copy all (keeps inactive params)
            int jrand = rng() % A;

            for (int k = 0; k < A; ++k)
            {
                int j = activeParams[k];
                if (uni (rng) < CRi || k == jrand)
                    trial[j] = pop[bestIdx][j] + Fi * (pop[r1][j] - pop[r2][j]);

                trial[j] = std::max (mins[j], std::min (maxs[j], trial[j]));
            }
            // Discrete oscType: occasionally try a random type instead of continuous mutation
            if (uni (rng) < 0.1f)
                trial[0] = (float) (rng() % (maxOscType + 1));
            trial[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (trial[0])));

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
        const int maxNMIter = 60;

        auto inject = [&](const std::vector<float>& base, const std::vector<float>& subVec) -> std::vector<float>
        {
            auto full = base;
            for (int i = 0; i < M; ++i)
                full[activeParams[i]] = subVec[i];
            full[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (full[0])));
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
            fvals[i] = evaluate (full.data(), ref, sampleRate);
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
            float fr = evaluate (fullR.data(), ref, sampleRate);

            if (fr < fvals[0])
            {
                std::vector<float> expanded (M);
                for (int j = 0; j < M; ++j)
                    expanded[j] = centroid[j] + 2.0f * (reflected[j] - centroid[j]);
                clampSub (expanded);
                auto fullE = inject (start, expanded);
                float fe = evaluate (fullE.data(), ref, sampleRate);
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
                float fc = evaluate (fullC.data(), ref, sampleRate);

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
                        fvals[i] = evaluate (fullS.data(), ref, sampleRate);
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
