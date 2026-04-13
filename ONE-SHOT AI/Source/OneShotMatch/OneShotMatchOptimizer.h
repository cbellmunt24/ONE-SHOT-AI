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
#include "../UniversalSynth/UniversalSynth.h"
#include "../UniversalSynth/UniversalGenreRules.h"

// ==========================================================================
// OneShotMatchOptimizer v6 — Direct Analysis + Timbral CMA-ES
//
// Key insight: MEASURE envelope/pitch from audio, don't OPTIMIZE them.
// Only CMA-ES on timbral params (~30D) with 7-resolution spectral loss.
//
//   Phase 0: Direct analysis → SET structural/envelope/pitch params (±5-20%)
//   Phase 1: OscType screen (14 evals)
//   Phase 2: Timbral CMA-ES (~30 params, 800 gens, spectral+attack loss)
//   Phase 3: Global NM polish (top-40 params, full composite loss)
//
// Total: ~18K evals in ~5 min with much better convergence.
// ==========================================================================

namespace oneshotmatch
{

#ifndef ONESHOTMATCH_TYPES_DEFINED
#define ONESHOTMATCH_TYPES_DEFINED
using MatchSynthParams = universalsynth::UniversalSynthParams;
using WavetableData = universalsynth::WavetableData;
using ResidualNoiseData = universalsynth::ResidualNoiseData;
using TransientSampleData = universalsynth::TransientSampleData;
using HarmonicPhaseData = universalsynth::HarmonicPhaseData;
using SpectralEnvelopeData = universalsynth::SpectralEnvelopeData;
using LearnedProfile = universalsynth::LearnedProfile;
#endif

} // close temporarily to keep the rest of file unchanged

namespace oneshotmatch
{

struct OptimizationResult
{
    MatchSynthParams bestParams;
    float            bestDistance        = 999.0f;
    float            descDistance        = 999.0f;
    float            stftDistance        = 999.0f;
    int              iterations         = 0;
    bool             converged          = false;

    std::array<float, MatchSynthParams::NUM_PARAMS> sensitivity = {};

    // Legacy compat (kept for UI)
    GapAnalysis gaps;
    int         extensionsActivated = 0;
    int         phase1Iterations    = 0;
    float       phase1Distance      = 999.0f;

    // New spectral match metrics
    float       melLoss             = 999.0f;
    float       envCorrelation      = 0.0f;
    float       pitchContourLoss    = 999.0f;
    int         cmaGenerations      = 0;
};

using ProgressCallback = std::function<bool (int, float, int)>;

// ==========================================================================
// Instrument type classifier — kept for UI display and initialization hints
// ==========================================================================
enum class InstrumentType { Kick, Snare, HiHat, Clap, Perc, Bass, Lead, Pad, Texture, Unknown };

inline InstrumentType classifyInstrument (const MatchDescriptors& d)
{
    float f0 = d.fundamentalFreq;
    float dur = d.totalDuration;
    float hnr = d.harmonicNoiseRatio;
    float bright = d.brightness;
    float sub = d.subEnergy;
    float high = d.highEnergy;
    float mid = d.midEnergy;
    float trans = d.transientStrength;
    float pitchDrop = d.pitchDropSemitones;

    if (f0 > 20.0f && f0 < 200.0f && sub > 0.15f && pitchDrop > 8.0f && dur < 1.0f)
        return InstrumentType::Kick;
    if (f0 > 20.0f && f0 < 200.0f && sub > 0.2f && dur > 0.3f && pitchDrop < 10.0f)
        return InstrumentType::Bass;
    if (bright > 0.15f && high > 0.3f && hnr < 0.6f && dur < 0.5f)
        return InstrumentType::HiHat;
    if (hnr < 0.5f && mid > 0.15f && dur < 0.5f && bright > 0.05f && bright < 0.4f)
        return InstrumentType::Clap;
    if (hnr > 0.2f && hnr < 1.2f && dur < 0.6f && (sub > 0.05f || mid > 0.1f))
        return InstrumentType::Snare;
    if (dur > 1.0f && trans < 4.0f)
        return (hnr > 0.8f) ? InstrumentType::Pad : InstrumentType::Texture;
    if (f0 > 200.0f && hnr > 0.8f && dur < 1.5f)
        return InstrumentType::Lead;
    if (dur < 0.8f)
        return InstrumentType::Perc;
    return InstrumentType::Unknown;
}

// ==========================================================================
// Get best preset seed for this instrument type + descriptors
// ==========================================================================
inline MatchSynthParams getPresetSeed (InstrumentType type, const MatchDescriptors& d)
{
    using namespace universalsynth::genrerules;

    auto tryGenres = [&] (auto genreFunc) -> MatchSynthParams
    {
        GenreStyle styles[] = {
            GenreStyle::Trap, GenreStyle::HipHop, GenreStyle::Techno,
            GenreStyle::House, GenreStyle::Reggaeton, GenreStyle::Afrobeat,
            GenreStyle::RnB, GenreStyle::EDM, GenreStyle::Ambient
        };

        MatchSynthParams bestPreset;
        float bestScore = 1e6f;

        for (auto style : styles)
        {
            auto preset = genreFunc (style);
            float pitchDiff = std::abs (preset.basePitch - d.fundamentalFreq) / std::max (30.0f, d.fundamentalFreq);
            float durDiff = std::abs (preset.duration - d.totalDuration) / std::max (0.05f, d.totalDuration);
            float score = pitchDiff + durDiff * 0.5f;
            if (score < bestScore)
            {
                bestScore = score;
                bestPreset = preset;
            }
        }
        return bestPreset;
    };

    switch (type)
    {
        case InstrumentType::Kick:   return tryGenres (kickBase);
        case InstrumentType::Snare:  return tryGenres (snareBase);
        case InstrumentType::HiHat:  return tryGenres (hihatBase);
        case InstrumentType::Clap:   return tryGenres (clapBase);
        case InstrumentType::Perc:   return tryGenres (percBase);
        case InstrumentType::Bass:   return tryGenres (bass808Base);
        case InstrumentType::Lead:   return tryGenres (leadBase);
        case InstrumentType::Pad:    return tryGenres (padBase);
        case InstrumentType::Texture: return tryGenres (textureBase);
        default:                     return tryGenres (kickBase);
    }
}

// ==========================================================================
// Core descriptor loss — KEPT for reporting only, NOT used in optimization
// ==========================================================================
inline float computeCoreLoss (const MatchDescriptors& ref, const MatchDescriptors& gen)
{
    if (! ref.valid || ! gen.valid) return 999.0f;

    float dist = 0.0f;

    auto nsd = [](float a, float b, float scale) -> float
    {
        float d = (a - b) / std::max (scale, 0.001f);
        return d * d;
    };

    dist += 3.0f * nsd (ref.fundamentalFreq, gen.fundamentalFreq, std::max (10.0f, ref.fundamentalFreq * 0.15f));

    {
        float peDist = 0.0f;
        for (int i = 0; i < MatchDescriptors::PITCH_ENV_POINTS; ++i)
        {
            float scale = std::max (20.0f, ref.pitchEnvelope[i]);
            peDist += nsd (ref.pitchEnvelope[i], gen.pitchEnvelope[i], scale);
        }
        dist += 2.5f * peDist / (float) MatchDescriptors::PITCH_ENV_POINTS;
    }

    dist += 2.5f * nsd (ref.rmsLoudness, gen.rmsLoudness, std::max (0.03f, ref.rmsLoudness * 0.2f));
    dist += 2.0f * nsd (ref.spectralCentroid, gen.spectralCentroid, std::max (30.0f, ref.spectralCentroid * 0.2f));

    {
        float stDist = 0.0f;
        for (int f = 0; f < SPECTRO_FRAMES; f += 2)
            for (int b = 0; b < SPECTRO_BANDS; b += 2)
            {
                int idx = f * SPECTRO_BANDS + b;
                float diff = ref.spectroTemporal[idx] - gen.spectroTemporal[idx];
                stDist += diff * diff;
            }
        dist += 3.0f * stDist / 16.0f;
    }

    {
        float envDist = 0.0f;
        for (int i = 0; i < MatchDescriptors::ENV_POINTS; ++i)
            envDist += (ref.ampEnvelope[i] - gen.ampEnvelope[i]) * (ref.ampEnvelope[i] - gen.ampEnvelope[i]);
        dist += 2.5f * envDist / (float) MatchDescriptors::ENV_POINTS;
    }

    dist += 2.0f * nsd (ref.subEnergy, gen.subEnergy, 0.06f);
    dist += 1.5f * nsd (ref.totalDuration, gen.totalDuration, std::max (0.03f, ref.totalDuration * 0.2f));
    dist += 0.5f * nsd (ref.lowMidEnergy, gen.lowMidEnergy, 0.07f);
    dist += 0.5f * nsd (ref.midEnergy, gen.midEnergy, 0.1f);
    dist += 0.5f * nsd (ref.highEnergy, gen.highEnergy, 0.05f);
    dist += 1.0f * nsd (ref.transientStrength, gen.transientStrength, std::max (1.0f, ref.transientStrength * 0.3f));

    return dist;
}

// ==========================================================================
// CMA-ES (Covariance Matrix Adaptation Evolution Strategy)
// Canonical implementation for continuous optimization in R^N
// ==========================================================================
struct CMAES
{
    int N = 0;         // dimension
    int lambda = 0;    // population size
    int mu = 0;        // parent count
    std::vector<float> weights; // recombination weights
    float muEff = 0.0f;

    // Strategy parameters
    float cc = 0.0f, cs = 0.0f, c1 = 0.0f, cmu_ = 0.0f, damps = 0.0f, chiN = 0.0f;

    // State
    std::vector<float> mean;                 // N
    float sigma = 0.3f;                      // step size
    std::vector<float> pc, ps;               // evolution paths
    std::vector<std::vector<float>> C;       // covariance matrix NxN
    std::vector<std::vector<float>> B;       // eigenvectors NxN
    std::vector<float> D;                    // sqrt eigenvalues N
    int eigenInterval = 1;
    int eigenCounter = 0;
    int generation = 0;

    void init (int dim, const std::vector<float>& initialMean, float initialSigma, int lambdaOverride = 0)
    {
        N = dim;
        lambda = (lambdaOverride > 0) ? lambdaOverride : std::max (20, 4 + (int)(3.0f * std::log ((float) N)));
        mu = lambda / 2;
        sigma = initialSigma;
        generation = 0;

        // Weights
        weights.resize (mu);
        float wSum = 0.0f;
        for (int i = 0; i < mu; ++i)
        {
            weights[i] = std::log ((float) mu + 0.5f) - std::log ((float)(i + 1));
            wSum += weights[i];
        }
        for (auto& w : weights) w /= wSum;

        float wSqSum = 0.0f;
        for (auto w : weights) wSqSum += w * w;
        muEff = 1.0f / wSqSum;

        // Strategy parameters
        cc = (4.0f + muEff / (float) N) / ((float) N + 4.0f + 2.0f * muEff / (float) N);
        cs = (muEff + 2.0f) / ((float) N + muEff + 5.0f);
        c1 = 2.0f / (((float) N + 1.3f) * ((float) N + 1.3f) + muEff);
        cmu_ = std::min (1.0f - c1,
                         2.0f * (muEff - 2.0f + 1.0f / muEff) /
                         (((float) N + 2.0f) * ((float) N + 2.0f) + muEff));
        damps = 1.0f + 2.0f * std::max (0.0f, std::sqrt ((muEff - 1.0f) / ((float) N + 1.0f)) - 1.0f) + cs;
        chiN = std::sqrt ((float) N) * (1.0f - 1.0f / (4.0f * (float) N) + 1.0f / (21.0f * (float) N * (float) N));

        // State init
        mean = initialMean;
        pc.assign (N, 0.0f);
        ps.assign (N, 0.0f);

        // C = identity
        C.assign (N, std::vector<float> (N, 0.0f));
        B.assign (N, std::vector<float> (N, 0.0f));
        D.assign (N, 1.0f);
        for (int i = 0; i < N; ++i)
        {
            C[i][i] = 1.0f;
            B[i][i] = 1.0f;
        }

        eigenInterval = std::max (1, N / 10);
        eigenCounter = 0;
    }

    // Sample lambda candidates. Returns {samples, z_vectors}
    std::pair<std::vector<std::vector<float>>, std::vector<std::vector<float>>>
    samplePopulation (std::mt19937& rng, const float* mins, const float* maxs)
    {
        std::normal_distribution<float> normal (0.0f, 1.0f);

        std::vector<std::vector<float>> samples (lambda, std::vector<float> (N));
        std::vector<std::vector<float>> zVecs (lambda, std::vector<float> (N));

        for (int k = 0; k < lambda; ++k)
        {
            // z ~ N(0, I)
            for (int i = 0; i < N; ++i)
                zVecs[k][i] = normal (rng);

            // x = mean + sigma * B * D * z
            for (int i = 0; i < N; ++i)
            {
                float sum = 0.0f;
                for (int j = 0; j < N; ++j)
                    sum += B[i][j] * D[j] * zVecs[k][j];
                samples[k][i] = mean[i] + sigma * sum;
            }

            // Clamp to bounds
            for (int i = 0; i < N; ++i)
                samples[k][i] = std::max (mins[i], std::min (maxs[i], samples[k][i]));

            // Round discrete params
            samples[k][0] = std::round (std::max (0.0f, std::min (maxs[0], samples[k][0])));
            if (N > 59) samples[k][59] = std::round (std::max (0.0f, std::min (3.0f, samples[k][59])));
            if (N > 86) samples[k][86] = std::round (std::max (0.0f, std::min (2.0f, samples[k][86])));
        }

        return {samples, zVecs};
    }

    // Update CMA-ES state after evaluating population
    void update (const std::vector<std::vector<float>>& samples,
                 const std::vector<std::vector<float>>& zVecs,
                 const std::vector<int>& ranking)
    {
        // New mean = weighted recombination of top mu
        std::vector<float> oldMean = mean;
        std::fill (mean.begin(), mean.end(), 0.0f);
        for (int i = 0; i < mu; ++i)
            for (int j = 0; j < N; ++j)
                mean[j] += weights[i] * samples[ranking[i]][j];

        // Mean displacement (in sigma units)
        std::vector<float> meanShift (N);
        for (int j = 0; j < N; ++j)
            meanShift[j] = (mean[j] - oldMean[j]) / sigma;

        // C^{-1/2} * meanShift (using B * D^{-1} * B^T)
        std::vector<float> invsqrtCshift (N, 0.0f);
        {
            // BT * meanShift
            std::vector<float> tmp (N, 0.0f);
            for (int i = 0; i < N; ++i)
                for (int j = 0; j < N; ++j)
                    tmp[i] += B[j][i] * meanShift[j];
            // D^{-1} * tmp
            for (int i = 0; i < N; ++i)
                tmp[i] /= std::max (1e-20f, D[i]);
            // B * tmp
            for (int i = 0; i < N; ++i)
                for (int j = 0; j < N; ++j)
                    invsqrtCshift[i] += B[i][j] * tmp[j];
        }

        // Update sigma path
        float sqrtMuEff = std::sqrt (cs * (2.0f - cs) * muEff);
        for (int i = 0; i < N; ++i)
            ps[i] = (1.0f - cs) * ps[i] + sqrtMuEff * invsqrtCshift[i];

        // ||ps|| for sigma update and hsig
        float psNorm = 0.0f;
        for (auto v : ps) psNorm += v * v;
        psNorm = std::sqrt (psNorm);

        // hsig: stall detection for pc update
        float psThresh = (1.4f + 2.0f / ((float) N + 1.0f)) * chiN
                         * std::sqrt (1.0f - std::pow (1.0f - cs, 2.0f * (float)(generation + 1)));
        float hsig = (psNorm < psThresh) ? 1.0f : 0.0f;

        // Update covariance path
        float sqrtMuEffCC = std::sqrt (cc * (2.0f - cc) * muEff);
        for (int i = 0; i < N; ++i)
            pc[i] = (1.0f - cc) * pc[i] + hsig * sqrtMuEffCC * meanShift[i];

        // Update covariance matrix
        // C = (1 - c1 - cmu) * C + c1 * (pc*pc^T + (1-hsig)*cc*(2-cc)*C) + cmu * sum(w_i * z_i*z_i^T)
        float c1a = c1 * (1.0f - (1.0f - hsig * hsig) * cc * (2.0f - cc));
        float cOld = 1.0f - c1a - cmu_ * std::min (1.0f, (float) mu * 1.0f); // ensure non-negative

        for (int i = 0; i < N; ++i)
        {
            for (int j = 0; j <= i; ++j)
            {
                float val = cOld * C[i][j];
                val += c1 * pc[i] * pc[j];

                float rankMu = 0.0f;
                for (int k = 0; k < mu; ++k)
                {
                    // Use z vectors of selected parents
                    // But we need y_k = (x_k - oldMean) / sigma
                    float yi = (samples[ranking[k]][i] - oldMean[i]) / sigma;
                    float yj = (samples[ranking[k]][j] - oldMean[j]) / sigma;
                    rankMu += weights[k] * yi * yj;
                }
                val += cmu_ * rankMu;

                C[i][j] = val;
                C[j][i] = val; // symmetric
            }
        }

        // Update sigma
        sigma *= std::exp ((cs / damps) * (psNorm / chiN - 1.0f));
        sigma = std::max (1e-12f, std::min (1e4f, sigma)); // safety bounds

        // Eigendecomposition every eigenInterval generations
        ++eigenCounter;
        if (eigenCounter >= eigenInterval)
        {
            eigenDecompose();
            eigenCounter = 0;
        }

        ++generation;
    }

    // Jacobi eigendecomposition of symmetric matrix C
    void eigenDecompose()
    {
        // Work on a copy
        auto A = C;
        // V = identity (will accumulate eigenvectors)
        auto V = B;
        for (int i = 0; i < N; ++i)
            for (int j = 0; j < N; ++j)
                V[i][j] = (i == j) ? 1.0f : 0.0f;

        const int maxSweeps = 15;
        for (int sweep = 0; sweep < maxSweeps; ++sweep)
        {
            // Check convergence: sum of off-diagonal squared
            float offDiag = 0.0f;
            for (int i = 0; i < N; ++i)
                for (int j = i + 1; j < N; ++j)
                    offDiag += A[i][j] * A[i][j];

            if (offDiag < 1e-12f * (float)(N * N)) break;

            for (int p = 0; p < N; ++p)
            {
                for (int q = p + 1; q < N; ++q)
                {
                    if (std::abs (A[p][q]) < 1e-15f) continue;

                    float tau = (A[q][q] - A[p][p]) / (2.0f * A[p][q]);
                    float t = (tau >= 0.0f)
                        ? 1.0f / (tau + std::sqrt (1.0f + tau * tau))
                        : -1.0f / (-tau + std::sqrt (1.0f + tau * tau));
                    float c = 1.0f / std::sqrt (1.0f + t * t);
                    float s = t * c;

                    // Rotate A
                    float App = A[p][p], Aqq = A[q][q], Apq = A[p][q];
                    A[p][p] = c * c * App - 2.0f * s * c * Apq + s * s * Aqq;
                    A[q][q] = s * s * App + 2.0f * s * c * Apq + c * c * Aqq;
                    A[p][q] = 0.0f;
                    A[q][p] = 0.0f;

                    for (int i = 0; i < N; ++i)
                    {
                        if (i == p || i == q) continue;
                        float aip = A[i][p], aiq = A[i][q];
                        A[i][p] = c * aip - s * aiq;
                        A[p][i] = A[i][p];
                        A[i][q] = s * aip + c * aiq;
                        A[q][i] = A[i][q];
                    }

                    // Rotate V
                    for (int i = 0; i < N; ++i)
                    {
                        float vip = V[i][p], viq = V[i][q];
                        V[i][p] = c * vip - s * viq;
                        V[i][q] = s * vip + c * viq;
                    }
                }
            }
        }

        // Extract eigenvalues (diagonal of A) and eigenvectors (columns of V)
        B = V;
        for (int i = 0; i < N; ++i)
        {
            float eigenval = std::max (1e-20f, A[i][i]); // clamp positive
            D[i] = std::sqrt (eigenval);
        }
    }
};

// ==========================================================================
// Main optimizer class
// ==========================================================================
class OneShotMatchOptimizer
{
public:
    void setMaxIterations (int n) { maxGenerations = n; }
    void setTargetDistance (float d) { targetDistance = d; }
    void setPopulationSize (int n) { popSizeHint = n; }

    void setReferenceBuffer (const juce::AudioBuffer<float>* buf) { refBuffer = buf; }

    // ==========================================================================
    // Active parameter set: ALL 88 params are optimizable.
    // The synth can produce almost any sound — don't restrict the search space.
    // Descriptor-based initialization provides the warm start; CMA-ES explores freely.
    // ==========================================================================
    static std::vector<int> getActiveParamsForType (InstrumentType /*type*/)
    {
        std::vector<int> params (MatchSynthParams::NUM_PARAMS);
        std::iota (params.begin(), params.end(), 0);
        return params;
    }

    // Priority params per instrument type — used for NM polish (top-priority first)
    static std::vector<int> getPriorityParamsForType (InstrumentType type)
    {
        std::vector<int> params;

        // Common timbral core: oscType + layer levels + filter + saturation + effects
        auto addCore = [&]() {
            params.insert (params.end(), {0, 14, 15, 74, 75, 76, 77, 78, 82, 83, 84, 85, 86, 87});
        };

        // Envelope params (always high priority)
        auto addEnvelope = [&]() {
            params.insert (params.end(), {66, 67, 68, 69, 70, 71, 72, 73});
        };

        // Pitch params
        auto addPitch = [&]() {
            params.insert (params.end(), {1, 6, 7, 8, 9, 10, 11, 12, 13});
        };

        switch (type)
        {
            case InstrumentType::Kick:
            case InstrumentType::Bass:
                addCore(); addEnvelope(); addPitch();
                params.insert (params.end(), {16, 17, 18, 30, 31}); // FM, sub
                params.insert (params.end(), {58, 59, 60, 61, 63, 65}); // transient
                break;

            case InstrumentType::Snare:
                addCore(); addEnvelope(); addPitch();
                params.insert (params.end(), {16, 17, 18}); // FM
                params.insert (params.end(), {32, 33, 34, 35, 36, 42}); // noise
                params.insert (params.end(), {58, 59, 60, 61, 63, 65}); // transient
                break;

            case InstrumentType::HiHat:
                addCore(); addEnvelope();
                params.insert (params.end(), {46, 47, 48, 49, 50, 51, 52}); // modal
                params.insert (params.end(), {32, 33, 34, 35, 36, 42}); // noise
                params.insert (params.end(), {53, 54, 55, 56, 57}); // KS
                params.insert (params.end(), {58, 59, 60, 61, 65}); // transient
                break;

            case InstrumentType::Clap:
                addCore(); addEnvelope();
                params.insert (params.end(), {32, 33, 34, 35, 36, 37, 38, 39, 42}); // noise + bursts
                params.insert (params.end(), {58, 59, 60, 61, 63, 65}); // transient
                break;

            case InstrumentType::Lead:
                addCore(); addEnvelope(); addPitch();
                params.insert (params.end(), {16, 17, 18, 19, 20, 21, 22, 23}); // FM + additive
                params.insert (params.end(), {25, 26, 28, 29}); // unison, phase distort
                params.insert (params.end(), {79, 80}); // formant
                break;

            case InstrumentType::Pad:
            case InstrumentType::Texture:
                addCore(); addEnvelope(); addPitch();
                params.insert (params.end(), {19, 20, 21, 22, 23, 25, 26, 27}); // additive + unison
                params.insert (params.end(), {32, 33, 36, 44, 45}); // noise + granular
                params.insert (params.end(), {46, 48, 49, 50}); // modal
                break;

            default: // Perc, Unknown — include everything
                addCore(); addEnvelope(); addPitch();
                params.insert (params.end(), {16, 17, 18, 30, 31}); // FM, sub
                params.insert (params.end(), {32, 33, 34, 35, 36, 42}); // noise
                params.insert (params.end(), {46, 48, 49, 50, 51, 52}); // modal
                params.insert (params.end(), {53, 54, 55, 56, 57}); // KS
                params.insert (params.end(), {58, 59, 60, 61, 63, 65}); // transient
                break;
        }

        std::sort (params.begin(), params.end());
        params.erase (std::unique (params.begin(), params.end()), params.end());
        return params;
    }

    OptimizationResult optimize (const MatchDescriptors& refDescFull,
                                 double sampleRate,
                                 ProgressCallback progress = nullptr,
                                 const MatchSynthParams* seedParams = nullptr)
    {
        const int N = MatchSynthParams::NUM_PARAMS;
        float mins[N], maxs[N];
        MatchSynthParams::getBounds (mins, maxs);

        refHNR = refDescFull.harmonicNoiseRatio;
        prepareRefMono (sampleRate, sampleRate);

        InstrumentType instrType = classifyInstrument (refDescFull);
        detectedType = instrType;
        std::mt19937 rng (std::random_device{}());

        // ═══════════════════════════════════════════════════════════════
        // PHASE 0: Direct Analysis → SET & LOCK structural/envelope/pitch
        // These params are MEASURED from audio — NOT optimized by CMA-ES.
        // ═══════════════════════════════════════════════════════════════
        std::vector<float> bestVec (N);
        {
            auto guess = descriptorGuess (refDescFull, instrType);
            guess.toArray (bestVec.data());
        }

        // Only blend TIMBRAL params with preset — keep structural from analysis
        {
            auto preset = getPresetSeed (instrType, refDescFull);
            std::vector<float> presetVec (N);
            preset.toArray (presetVec.data());
            auto timbreIdx = getPriorityParamsForType (instrType);
            for (int j : timbreIdx)
                bestVec[j] = 0.5f * bestVec[j] + 0.5f * presetVec[j];
        }

        // Blend with user seed if provided (timbral only)
        if (seedParams != nullptr)
        {
            std::vector<float> seedVec (N);
            seedParams->toArray (seedVec.data());
            auto timbreIdx = getPriorityParamsForType (instrType);
            for (int j : timbreIdx)
                bestVec[j] = 0.5f * bestVec[j] + 0.5f * seedVec[j];
        }

        // --- LOCKED bounds: measured ±5-20%, timbral full range ---
        {
            float f0 = refDescFull.fundamentalFreq;
            float dur = refDescFull.totalDuration;

            // Structural: pitch ±10%, duration ±2% (duration is measured precisely)
            if (f0 > 20.0f) { mins[1] = f0 * 0.9f; maxs[1] = f0 * 1.1f; }
            mins[2] = std::max (0.01f, dur * 0.98f);
            maxs[2] = std::min (5.0f, dur * 1.02f);
            mins[3] = 0.75f; maxs[3] = 1.0f;

            // Pitch envelope: proportional to detected (tight — measured value)
            if (refDescFull.pitchDropSemitones > 2.0f)
            {
                mins[6] = refDescFull.pitchDropSemitones * 0.5f;
                maxs[6] = std::min (96.0f, refDescFull.pitchDropSemitones * 1.5f);
            }
            if (refDescFull.pitchDropTime > 0.001f)
            {
                mins[7] = std::max (0.0003f, refDescFull.pitchDropTime * 0.1f);
                maxs[7] = std::min (0.05f, refDescFull.pitchDropTime * 0.5f);
                mins[8] = std::max (0.003f, refDescFull.pitchDropTime * 0.5f);
                maxs[8] = std::min (0.5f, refDescFull.pitchDropTime * 2.0f);
            }

            // subDetune: ±3 semitones
            mins[31] = -3.0f; maxs[31] = 3.0f;

            // Envelope: proportional to detected
            float att = std::max (0.0001f, refDescFull.attackTime);
            mins[66] = std::max (0.0001f, att * 0.2f);
            maxs[66] = std::min (0.05f, att * 5.0f);
            mins[67] = std::max (0.005f, std::max (0.005f, refDescFull.decayTime) * 0.2f);
            maxs[67] = std::min (0.5f, std::max (0.005f, refDescFull.decayTime) * 5.0f);
            mins[68] = std::max (0.05f, dur * 0.15f);  // at least 15% of duration
            maxs[68] = std::min (3.0f, dur * 2.0f);
            mins[72] = std::max (0.01f, dur * 0.05f);  // at least 5% of duration
            maxs[72] = std::min (2.0f, dur * 0.6f);
            float detCurve = std::max (0.3f, std::min (3.0f, refDescFull.envelopeShape * 2.5f + 0.3f));
            mins[73] = std::max (0.1f, detCurve * 0.3f);
            maxs[73] = std::min (4.0f, detCurve * 3.0f);

            // Filter: NEVER choke the sound
            mins[74] = std::max (800.0f, refDescFull.spectralCentroid * 1.5f);
            maxs[75] = 0.6f;

            // Effects caps
            maxs[82] = 0.5f; maxs[84] = 0.4f; maxs[87] = 0.6f;
        }

        // Clamp
        for (int i = 0; i < N; ++i)
            bestVec[i] = std::max (mins[i], std::min (maxs[i], bestVec[i]));

        // OscType screening
        {
            float bestFit = 1e6f; int bestOsc = 0;
            auto testVec = bestVec;
            for (int osc = 0; osc <= maxOscType; ++osc)
            {
                testVec[0] = (float) osc;
                float fit = evaluate (testVec.data(), sampleRate);
                if (fit < bestFit) { bestFit = fit; bestOsc = osc; }
            }
            bestVec[0] = (float) bestOsc;
        }

        OptimizationResult result;
        int totalEvals = maxOscType + 1;
        int progressGen = 0;
        int totalProgressGens = 800 + 100;

        // ═══════════════════════════════════════════════════════════════
        // PHASE 2: Timbral CMA-ES (~45 params)
        // Envelope/pitch LOCKED from Phase 0. Only timbre explored.
        // Loss: 7-resolution STFT + mel + attack waveform + RMS
        // ═══════════════════════════════════════════════════════════════
        {
            std::vector<int> timbreParams = {
                14, 32, 46, 58,                                         // layer levels
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 30, 31, // tonal
                33, 34, 35, 36, 42,                                     // noise
                47, 48, 49, 50, 51, 53, 54, 55,                         // modal/KS
                59, 60, 61, 62, 63, 65,                                 // transient
                74, 75, 76, 77, 78, 79, 80, 81,                         // filter
                82, 83, 85, 86, 87                                      // effects
            };

            float p2Best = 1e6f;
            bestVec = runCMAPhase (
                bestVec, timbreParams, mins, maxs,
                800, 0.3f,
                [this](const float* p, double sr) { return evaluateTimbre (p, sr); },
                sampleRate, p2Best, rng, progress, progressGen, totalProgressGens
            );
            totalEvals += 800 * 22;
            result.phase1Distance = p2Best;
            result.phase1Iterations = progressGen;
        }

        // ═══════════════════════════════════════════════════════════════
        // PHASE 3: Global Polish (NM on full composite loss)
        // ═══════════════════════════════════════════════════════════════
        float bestFitness = evaluate (bestVec.data(), sampleRate);
        {
            // Sensitivity analysis
            std::vector<std::pair<float, int>> sensRanking;
            for (int j = 0; j < N; ++j)
            {
                float range = maxs[j] - mins[j];
                if (range < 1e-10f) continue;
                float step = range * 0.02f;
                auto tv = bestVec;
                tv[j] = std::max (mins[j], std::min (maxs[j], bestVec[j] + step));
                float fp = evaluate (tv.data(), sampleRate);
                tv[j] = std::max (mins[j], std::min (maxs[j], bestVec[j] - step));
                float fm = evaluate (tv.data(), sampleRate);
                float s = std::abs (fp - bestFitness) + std::abs (fm - bestFitness);
                if (s > 1e-6f) sensRanking.push_back ({s, j});
                result.sensitivity[j] = s;
            }

            float maxSens = 0.0f;
            for (auto& s : result.sensitivity) maxSens = std::max (maxSens, s);
            if (maxSens > 0.0f) for (auto& s : result.sensitivity) s /= maxSens;

            std::sort (sensRanking.begin(), sensRanking.end(),
                       [](auto& a, auto& b) { return a.first > b.first; });

            std::vector<int> nmParams;
            for (int i = 0; i < std::min (40, (int) sensRanking.size()); ++i)
                nmParams.push_back (sensRanking[i].second);

            MatchDescriptors dummyRef;
            for (int round = 0; round < 10 && ! nmParams.empty(); ++round)
            {
                auto refined = nelderMeadRefine (bestVec, nmParams, mins, maxs, dummyRef, sampleRate);
                float refinedFit = evaluate (refined.data(), sampleRate);
                if (refinedFit < bestFitness - 1e-6f)
                { bestVec = refined; bestFitness = refinedFit; }
                else break;
            }
        }

        result.bestDistance = bestFitness;
        result.bestParams.fromArray (bestVec.data());
        result.iterations = totalEvals;
        result.cmaGenerations = progressGen;
        if (bestFitness < targetDistance) result.converged = true;

        // ═══════════════════════════════════════════════════════════════
        // FINAL: Compute detailed metrics for reporting
        // ═══════════════════════════════════════════════════════════════
        {
            MatchSynthParams finalP;
            finalP.fromArray (bestVec.data());
            configureSynth();
            auto finalBuf = synth.generate (finalP, sampleRate);

            // Descriptor distance for reporting
            extractor.setFastMode (true);
            auto finalDesc = extractor.extract (finalBuf, sampleRate);
            extractor.setFastMode (false);
            if (finalDesc.valid)
                result.descDistance = computeCoreLoss (refDescFull, finalDesc);

            // STFT distance for reporting
            if (refMonoReady)
                result.stftDistance = multiResSTFTLoss (finalBuf);

            // Detailed spectral metrics
            if (refMonoReady)
            {
                int genLen = finalBuf.getNumSamples();
                std::vector<float> genMono (genLen, 0.0f);
                for (int ch = 0; ch < finalBuf.getNumChannels(); ++ch)
                {
                    const float* data = finalBuf.getReadPointer (ch);
                    for (int i = 0; i < genLen; ++i) genMono[i] += data[i];
                }
                float chScale = 1.0f / (float) finalBuf.getNumChannels();
                for (auto& s : genMono) s *= chScale;

                result.melLoss = computeMelSpectrogramLoss (refMono, genMono, (float) sampleRate);
                result.envCorrelation = computeEnvelopeCorrelation (refMono, genMono, (float) sampleRate);
                result.pitchContourLoss = computePitchContourLoss (refMono, genMono, (float) sampleRate, refHNR);
            }
        }

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

    InstrumentType getDetectedType() const { return detectedType; }

private:
    int   maxGenerations  = 1500;   // enough budget for convergence (~30K evals, ~5-8 min)
    float targetDistance   = 0.05f;  // 90% score = exp(-0.05*2) = 0.905
    int   popSizeHint     = 20;
    int   maxOscType      = 11;

    universalsynth::UniversalSynth synth;
    DescriptorExtractor extractor;

    const juce::AudioBuffer<float>* refBuffer = nullptr;
    std::vector<float> refMono;
    bool refMonoReady = false;
    float refHNR = 0.0f;
    InstrumentType detectedType = InstrumentType::Unknown;

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

    // Generate mono audio from params (shared by all evaluate functions)
    std::vector<float> generateMono (const float* paramArray, double sampleRate)
    {
        MatchSynthParams p;
        p.fromArray (paramArray);
        configureSynth();
        auto buffer = synth.generate (p, sampleRate);

        float peak = buffer.getMagnitude (0, 0, buffer.getNumSamples());
        if (peak < 1e-8f || std::isnan (peak) || std::isinf (peak))
            return {};

        int genLen = buffer.getNumSamples();
        std::vector<float> genMono (genLen, 0.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int i = 0; i < genLen; ++i) genMono[i] += data[i];
        }
        float chScale = 1.0f / (float) buffer.getNumChannels();
        for (auto& s : genMono) s *= chScale;
        return genMono;
    }

    // Phase 1 loss: point-by-point RMS envelope distance + pitch contour
    // NOT correlation (which only measures shape, not values)
    float evaluateEnvelope (const float* paramArray, double sampleRate)
    {
        auto genMono = generateMono (paramArray, sampleRate);
        if (genMono.empty() || ! refMonoReady) return 1e6f;

        // Point-by-point RMS envelope L1 distance (2ms frames)
        float envDist = 0.0f;
        {
            int hopSamples = std::max (1, (int)(sampleRate * 0.002f)); // 2ms
            int refLen = (int) refMono.size();
            int numFrames = refLen / hopSamples;
            if (numFrames < 2) return 1e6f;

            float totalDiff = 0.0f;
            float totalRef = 0.0f;

            for (int f = 0; f < numFrames; ++f)
            {
                int start = f * hopSamples;
                float refSq = 0.0f, genSq = 0.0f;
                for (int i = 0; i < hopSamples && (start + i) < refLen; ++i)
                {
                    float rv = refMono[start + i];
                    float gv = (start + i < (int) genMono.size()) ? genMono[start + i] : 0.0f;
                    refSq += rv * rv;
                    genSq += gv * gv;
                }
                float refRMS = std::sqrt (refSq / (float) hopSamples);
                float genRMS = std::sqrt (genSq / (float) hopSamples);
                totalDiff += std::abs (refRMS - genRMS);
                totalRef += refRMS;
            }

            envDist = (totalRef > 1e-6f) ? totalDiff / totalRef : totalDiff;
        }

        // Envelope correlation (bonus — rewards shape match too)
        float envCorr = computeEnvelopeCorrelation (refMono, genMono, (float) sampleRate);

        float pitchLoss = computePitchContourLoss (refMono, genMono, (float) sampleRate, refHNR);
        float rmsLoss = computeRMSLoss (refMono, genMono);

        return 0.40f * envDist               // actual envelope values must match
             + 0.15f * (1.0f - envCorr)       // envelope shape bonus
             + 0.25f * pitchLoss              // pitch contour
             + 0.20f * rmsLoss;               // overall loudness
    }

    // Phase 2 loss: mel + STFT spectral (NO envelope — already matched)
    float evaluateTimbre (const float* paramArray, double sampleRate)
    {
        auto genMono = generateMono (paramArray, sampleRate);
        if (genMono.empty() || ! refMonoReady) return 1e6f;

        float stftLoss = computeLinearSTFTLoss (refMono, genMono, (float) sampleRate);
        float melLoss = computeMelSpectrogramLoss (refMono, genMono, (float) sampleRate);
        float rmsLoss = computeRMSLoss (refMono, genMono);

        return 0.45f * stftLoss
             + 0.45f * melLoss
             + 0.10f * rmsLoss;
    }

    // BIPOP-CMA-ES phase runner for a subset of params
    // Alternates large-population restarts (exploration) with small-population
    // restarts (exploitation) for better global search within the same budget.
    std::vector<float> runCMAPhase (
        std::vector<float>& baseVec,
        const std::vector<int>& phaseParams,
        const float* mins, const float* maxs,
        int maxGens, float initSigma,
        std::function<float (const float*, double)> evalFn,
        double sampleRate, float& bestFitness,
        std::mt19937& rng,
        ProgressCallback progress, int& progressGen, int totalProgressGens)
    {
        const int M = (int) phaseParams.size();
        const int N = MatchSynthParams::NUM_PARAMS;

        // Build sub-bounds
        std::vector<float> subMins (M), subMaxs (M);
        for (int i = 0; i < M; ++i)
        {
            subMins[i] = mins[phaseParams[i]];
            subMaxs[i] = maxs[phaseParams[i]];
        }

        std::vector<float> normMins (M, 0.0f), normMaxs (M, 1.0f);

        // Inject sub-params into full vector
        auto inject = [&](const std::vector<float>& sub) -> std::vector<float>
        {
            auto full = baseVec;
            for (int i = 0; i < M; ++i)
            {
                int j = phaseParams[i];
                float range = subMaxs[i] - subMins[i];
                full[j] = subMins[i] + sub[i] * range;
                full[j] = std::max (mins[j], std::min (maxs[j], full[j]));
            }
            full[0] = std::round (std::max (0.0f, std::min ((float) maxOscType, full[0])));
            if (N > 59) full[59] = std::round (std::max (0.0f, std::min (3.0f, full[59])));
            if (N > 86) full[86] = std::round (std::max (0.0f, std::min (2.0f, full[86])));
            return full;
        };

        auto toNormMean = [&](const std::vector<float>& fullVec) -> std::vector<float>
        {
            std::vector<float> nm (M);
            for (int i = 0; i < M; ++i)
            {
                float range = subMaxs[i] - subMins[i];
                nm[i] = (range > 1e-10f) ? (fullVec[phaseParams[i]] - subMins[i]) / range : 0.5f;
                nm[i] = std::max (0.0f, std::min (1.0f, nm[i]));
            }
            return nm;
        };

        bestFitness = evalFn (baseVec.data(), sampleRate);
        auto bestVec = baseVec;

        // BIPOP budget: total evaluations = maxGens * default_lambda
        int lambdaDefault = std::max (20, 4 + (int)(3.0f * std::log ((float) M)));
        int totalBudget = maxGens * lambdaDefault;
        int usedBudget = 0;
        int restartIdx = 0;
        bool cancelled = false;

        while (usedBudget < totalBudget && ! cancelled)
        {
            // BIPOP: alternate large (exploration) and small (exploitation) restarts
            int lambdaR;
            float sigmaR;

            if (restartIdx == 0)
            {
                // First run: default population
                lambdaR = lambdaDefault;
                sigmaR = initSigma;
            }
            else if (restartIdx % 2 == 1)
            {
                // Large restart: double population for exploration
                int largeFactor = 1 + restartIdx / 2;
                lambdaR = std::min (lambdaDefault * (1 << largeFactor), 200);
                sigmaR = 0.5f; // wide sigma for exploration
            }
            else
            {
                // Small restart: half population for fast exploitation around best
                lambdaR = std::max (10, lambdaDefault / 2);
                sigmaR = 0.1f; // focused sigma near best solution
            }

            int remainingBudget = totalBudget - usedBudget;
            int gensR = std::min (remainingBudget / std::max (1, lambdaR), 400);
            if (gensR < 5) break;

            auto normMean = toNormMean (bestVec);

            CMAES cma;
            cma.init (M, normMean, sigmaR, lambdaR);

            int stagnation = 0;
            const int stagLimit = std::max (20, M * 2);

            for (int gen = 0; gen < gensR; ++gen)
            {
                auto [samples, zVecs] = cma.samplePopulation (rng, normMins.data(), normMaxs.data());

                std::vector<std::vector<float>> fullSamples (cma.lambda);
                std::vector<float> fitnesses (cma.lambda);
                for (int k = 0; k < cma.lambda; ++k)
                {
                    fullSamples[k] = inject (samples[k]);
                    fitnesses[k] = evalFn (fullSamples[k].data(), sampleRate);
                }
                usedBudget += cma.lambda;

                std::vector<int> ranking (cma.lambda);
                std::iota (ranking.begin(), ranking.end(), 0);
                std::sort (ranking.begin(), ranking.end(),
                           [&](int a, int b) { return fitnesses[a] < fitnesses[b]; });

                if (fitnesses[ranking[0]] < bestFitness)
                {
                    bestFitness = fitnesses[ranking[0]];
                    bestVec = fullSamples[ranking[0]];
                    baseVec = bestVec;
                    stagnation = 0;
                }
                else { ++stagnation; }

                cma.update (samples, zVecs, ranking);

                if (progress)
                {
                    ++progressGen;
                    if (! progress (progressGen, bestFitness, totalProgressGens))
                    { cancelled = true; break; }
                }

                if (bestFitness < targetDistance) { cancelled = true; break; }

                // Break this restart on stagnation (let BIPOP handle the next restart)
                if (cma.sigma < 1e-8f || stagnation >= stagLimit)
                    break;
            }

            ++restartIdx;
        }

        return bestVec;
    }

    void prepareRefMono (double optSR, double origSR)
    {
        if (refBuffer == nullptr || refBuffer->getNumSamples() == 0) return;

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

    // Multi-resolution STFT loss (kept for reporting)
    float multiResSTFTLoss (const juce::AudioBuffer<float>& gen) const
    {
        if (! refMonoReady || refMono.empty()) return 0.0f;

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

        {
            float refRMS = 0.0f, genRMS = 0.0f;
            for (int i = 0; i < compareLen; ++i)
            {
                refRMS += refMono[i] * refMono[i];
                genRMS += genMono[i] * genMono[i];
            }
            refRMS = std::sqrt (refRMS / (float) compareLen);
            genRMS = std::sqrt (genRMS / (float) compareLen);
            if (genRMS > 1e-8f && refRMS > 1e-8f)
            {
                float gainCorrection = refRMS / genRMS;
                for (int i = 0; i < genLen; ++i)
                    genMono[i] *= gainCorrection;
            }
        }

        float totalLoss = 0.0f;
        static constexpr int NUM_RES = 3;
        static constexpr int fftOrders[NUM_RES] = { 8, 10, 11 };
        static constexpr float resWeights[NUM_RES] = { 0.25f, 0.5f, 0.25f };

        for (int r = 0; r < NUM_RES; ++r)
        {
            int fftSize = 1 << fftOrders[r];
            int hopSize = fftSize / 4;
            int numBins = fftSize / 2;

            if (compareLen < fftSize) continue;

            juce::dsp::FFT fft (fftOrders[r]);

            int numFrames = std::max (1, (compareLen - fftSize) / hopSize + 1);
            numFrames = std::min (numFrames, 12);

            float magLoss = 0.0f;
            int totalBins = 0;

            for (int frame = 0; frame < numFrames; ++frame)
            {
                int start = frame * hopSize;
                if (start + fftSize > compareLen) break;

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

                for (int b = 1; b < numBins; ++b)
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

    // Build an initial guess from descriptors + instrument type
    // All params are starting points for CMA-ES (not locked)
    MatchSynthParams descriptorGuess (const MatchDescriptors& d, InstrumentType type) const
    {
        MatchSynthParams g;

        // Structural params — best guess from descriptors
        g.basePitch = (d.fundamentalFreq > 20.0f) ? d.fundamentalFreq : 50.0f;
        g.duration = std::max (0.05f, std::min (3.0f, d.totalDuration));
        g.masterGain = std::max (0.7f, std::min (1.0f, d.rmsLoudness / 0.45f));

        if (wavetable != nullptr && wavetable->valid)
            g.oscType = 12;
        else
            g.oscType = 0;

        // Pitch envelope
        g.pitchEnvDepth = std::max (0.0f, d.pitchDropSemitones);
        g.pitchEnvFast = std::max (0.0003f, std::min (0.01f, d.pitchDropTime * 0.15f));
        g.pitchEnvSlow = std::max (0.003f, d.pitchDropTime);
        g.pitchEnvBalance = (d.pitchDropSemitones > 12.0f) ? 0.85f : 0.5f;

        // Amplitude envelope
        g.ampAttack = std::max (0.0001f, std::min (0.05f, d.attackTime));
        g.ampPunchDecay = std::max (0.005f, std::min (0.5f, d.decayTime * 0.5f));
        // Body decay: use decayTime40 (to -40dB) as better estimate, and never less than 10% of duration
        g.ampBodyDecay = std::max (d.totalDuration * 0.1f, std::max (0.05f, d.decayTime40));
        g.ampPunchLevel = std::max (0.3f, std::min (0.9f, d.transientStrength / 8.0f + 0.3f));
        g.envRelease = std::max (0.02f, std::min (1.5f, (d.totalDuration - d.decayTime40) * 0.4f));
        g.envCurve = std::max (0.5f, std::min (3.0f, d.envelopeShape * 2.0f + 0.5f));

        // Sustain: if sound is long and has energy past the body phase, add sustain
        if (d.totalDuration > 0.3f)
        {
            g.envSustainLevel = std::max (0.0f, std::min (0.5f, d.rmsLoudness * 0.5f));
            g.envSustainTime = std::max (0.0f, d.totalDuration * 0.2f);
        }
        else
        {
            g.envSustainLevel = 0.0f;
            g.envSustainTime = 0.0f;
        }

        // Filter: start OPEN (high cutoff) — let the optimizer close it if needed
        // Never start with cutoff below 1kHz — that chokes the sound
        g.filterCutoff = std::max (1000.0f, std::min (20000.0f, d.spectralCentroid * 8.0f));
        g.filterReso = 0.0f;

        // subDetune: start at unison
        g.subDetune = 0.0f;

        // Layer gates
        switch (type)
        {
            case InstrumentType::Kick:
            case InstrumentType::Bass:
                g.tonalLevel = 0.9f;
                g.noiseLevel = 0.0f;
                g.modalLevel = 0.0f;
                g.transientLevel = (type == InstrumentType::Kick) ? std::min (0.6f, d.transientStrength / 5.0f) : 0.0f;
                g.subLevel = std::max (0.3f, d.subEnergy * 1.5f);
                break;

            case InstrumentType::Snare:
                g.tonalLevel = 0.5f;
                g.noiseLevel = 0.5f;
                g.modalLevel = 0.0f;
                g.transientLevel = std::min (0.5f, d.transientStrength / 6.0f);
                break;

            case InstrumentType::HiHat:
                g.tonalLevel = 0.0f;
                g.noiseLevel = 0.4f;
                g.modalLevel = 0.6f;
                g.transientLevel = 0.3f;
                g.modeSpread = 0.6f;
                g.numModes = 8.0f;
                break;

            case InstrumentType::Clap:
                g.tonalLevel = 0.1f;
                g.noiseLevel = 0.7f;
                g.modalLevel = 0.0f;
                g.transientLevel = 0.2f;
                g.burstCount = 4.0f;
                g.burstSpacing = 0.01f;
                break;

            case InstrumentType::Perc:
                g.tonalLevel = 0.4f;
                g.noiseLevel = 0.2f;
                g.modalLevel = 0.3f;
                g.transientLevel = 0.3f;
                break;

            case InstrumentType::Lead:
                g.tonalLevel = 0.8f;
                g.noiseLevel = 0.0f;
                g.modalLevel = 0.0f;
                g.transientLevel = 0.1f;
                break;

            case InstrumentType::Pad:
            case InstrumentType::Texture:
                g.tonalLevel = (type == InstrumentType::Pad) ? 0.6f : 0.3f;
                g.noiseLevel = 0.3f;
                g.modalLevel = 0.1f;
                g.transientLevel = 0.0f;
                g.envSustainLevel = 0.3f;
                g.envSustainTime = std::max (0.1f, d.totalDuration * 0.3f);
                break;

            default:
                g.tonalLevel = std::max (0.3f, std::min (1.0f, (d.subEnergy + d.lowMidEnergy) * 2.0f));
                g.noiseLevel = (d.harmonicNoiseRatio < 0.5f) ? 0.3f : 0.0f;
                g.transientLevel = std::min (0.5f, d.transientStrength / 6.0f);
                break;
        }

        // Timbral starting points
        g.bodyHarmonics = (d.fundamentalFreq > 20.0f)
            ? std::min (0.5f, (d.harmonicProfile[1] + d.harmonicProfile[2]) * 0.4f) : 0.0f;

        g.clickFreq = std::max (2000.0f, d.transientRegion.spectralCentroid);
        g.clickDecay = std::max (0.0003f, std::min (0.005f, d.attackTime * 0.3f));
        g.topNoise = std::min (0.3f, d.highEnergy * 3.0f);

        if (g.noiseLevel > 0.01f)
        {
            g.noiseColor = 0.5f;
            g.noiseDecay = std::max (0.01f, d.decayTime * 0.8f);
            g.noiseFilterFreq = std::min (12000.0f, std::max (500.0f, d.noiseSpectralCentroid));
        }

        g.satAmount = (d.harmonicNoiseRatio > 0.5f) ? 0.15f : 0.0f;

        if (residualNoise != nullptr && residualNoise->valid) g.residualAmt = 0.3f;
        if (transientSample != nullptr && transientSample->valid) g.transientSampleAmt = 0.4f;

        return g;
    }

    // Evaluate using full composite loss (used for Phase 3 polish + final scoring)
    float evaluate (const float* paramArray, double sampleRate)
    {
        auto genMono = generateMono (paramArray, sampleRate);
        if (genMono.empty() || ! refMonoReady) return 1e6f;

        // Sanity check: penalize dead/choked sounds
        int genLen = (int) genMono.size();
        int compareLen = std::min ((int) refMono.size(), genLen);
        if (compareLen > 0)
        {
            float refSq = 0.0f, genSq = 0.0f;
            for (int i = 0; i < compareLen; ++i)
            {
                refSq += refMono[i] * refMono[i];
                genSq += genMono[i] * genMono[i];
            }
            float refRMS = std::sqrt (refSq / (float) compareLen);
            float genRMS = std::sqrt (genSq / (float) compareLen);

            if (refRMS > 0.01f && genRMS < refRMS * 0.1f) return 50.0f;
            if (refRMS > 0.001f && genRMS > refRMS * 10.0f) return 20.0f;
        }

        return computeSpectralMatchLoss (refMono, genMono, (float) sampleRate, refHNR);
    }

    // Nelder-Mead on a subset of active parameters
    std::vector<float> nelderMeadRefine (const std::vector<float>& start,
                                          const std::vector<int>& activeParams,
                                          const float* mins, const float* maxs,
                                          const MatchDescriptors& /*ref*/, double sampleRate)
    {
        const int M = (int) activeParams.size();
        const int maxNMIter = 150; // was 80 — more iterations for better convergence

        auto inject = [&](const std::vector<float>& base, const std::vector<float>& subVec) -> std::vector<float>
        {
            auto full = base;
            for (int i = 0; i < M; ++i)
                full[activeParams[i]] = subVec[i];
            full[0] = std::max (0.0f, std::min ((float) maxOscType, std::round (full[0])));
            full[59] = std::max (0.0f, std::min (3.0f, std::round (full[59])));
            full[86] = std::max (0.0f, std::min (2.0f, std::round (full[86])));
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
            simplex[i][i - 1] += range * 0.12f; // was 0.05 — larger simplex for better exploration
            clampSub (simplex[i]);
        }

        for (int i = 0; i <= M; ++i)
        {
            auto full = inject (start, simplex[i]);
            fvals[i] = evaluate (full.data(), sampleRate);
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
            float fr = evaluate (fullR.data(), sampleRate);

            if (fr < fvals[0])
            {
                std::vector<float> expanded (M);
                for (int j = 0; j < M; ++j)
                    expanded[j] = centroid[j] + 2.0f * (reflected[j] - centroid[j]);
                clampSub (expanded);
                auto fullE = inject (start, expanded);
                float fe = evaluate (fullE.data(), sampleRate);
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
                float fc = evaluate (fullC.data(), sampleRate);

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
                        fvals[i] = evaluate (fullS.data(), sampleRate);
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
