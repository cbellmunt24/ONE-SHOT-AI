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
// OneShotMatchOptimizer v3 — Spectral Matching Architecture
//
// Key changes from v2:
//   - PRIMARY loss: Multi-resolution Mel Spectrogram (not descriptors)
//   - Optimizer: CMA-ES (not DE) — searches ALL 88 params
//   - No classifier dependency for search space
//   - No locked params — all 88 are optimizable with smart initialization
//   - Descriptors used ONLY for initialization + UI display
//   - Composite loss: 0.7*Mel + 0.15*Envelope + 0.15*PitchContour
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

    void init (int dim, const std::vector<float>& initialMean, float initialSigma)
    {
        N = dim;
        lambda = std::max (20, 4 + (int)(3.0f * std::log ((float) N)));
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
    // Active parameter subsets per instrument type — TIMBRAL params only
    // Structural params (pitch, duration, envelope) are LOCKED from descriptors.
    // This reduces CMA-ES to ~25-30 dims instead of 88 → converges reliably.
    // ==========================================================================
    static std::vector<int> getActiveParamsForType (InstrumentType type)
    {
        std::vector<int> params;

        // Common timbral core: oscType + layer levels + filter + saturation + effects
        auto addCore = [&]() {
            params.insert (params.end(), {0, 14, 15, 74, 75, 76, 77, 78, 82, 83, 84, 85, 86, 87});
        };

        switch (type)
        {
            case InstrumentType::Kick:
            case InstrumentType::Bass:
                addCore();
                params.insert (params.end(), {16, 17, 18, 30, 31}); // FM, sub
                params.insert (params.end(), {58, 59, 60, 61, 63, 65}); // transient
                params.insert (params.end(), {67, 69}); // punchDecay, punchLevel
                break;

            case InstrumentType::Snare:
                addCore();
                params.insert (params.end(), {16, 17, 18}); // FM
                params.insert (params.end(), {32, 33, 34, 35, 36, 42}); // noise
                params.insert (params.end(), {58, 59, 60, 61, 63, 65}); // transient
                params.insert (params.end(), {67, 69}); // punch
                break;

            case InstrumentType::HiHat:
                addCore();
                params.insert (params.end(), {46, 47, 48, 49, 50, 51, 52}); // modal
                params.insert (params.end(), {32, 33, 34, 35, 36, 42}); // noise
                params.insert (params.end(), {58, 59, 60, 61, 65}); // transient
                break;

            case InstrumentType::Clap:
                addCore();
                params.insert (params.end(), {32, 33, 34, 35, 36, 37, 38, 39, 42}); // noise + bursts
                params.insert (params.end(), {58, 63, 65}); // transient
                break;

            case InstrumentType::Lead:
                addCore();
                params.insert (params.end(), {16, 17, 18, 19, 20, 21, 22, 23}); // FM + additive
                params.insert (params.end(), {25, 26, 28, 29}); // unison, phase distort
                params.insert (params.end(), {79, 80}); // formant
                break;

            case InstrumentType::Pad:
            case InstrumentType::Texture:
                addCore();
                params.insert (params.end(), {19, 20, 21, 22, 23, 25, 26, 27}); // additive + unison
                params.insert (params.end(), {32, 33, 36, 44, 45}); // noise + granular
                params.insert (params.end(), {46, 48, 49, 50}); // modal
                params.insert (params.end(), {70, 71}); // sustain
                break;

            default: // Perc, Unknown
                addCore();
                params.insert (params.end(), {16, 17, 30}); // FM, sub
                params.insert (params.end(), {32, 33, 34, 36}); // noise
                params.insert (params.end(), {46, 48, 49, 50}); // modal
                params.insert (params.end(), {58, 59, 60, 61, 65}); // transient
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

        // === STEP 1: Classify + get active timbral params ===
        InstrumentType instrType = classifyInstrument (refDescFull);
        detectedType = instrType;
        std::vector<int> activeParams = getActiveParamsForType (instrType);
        const int A = (int) activeParams.size();

        // === STEP 2: Build base vector from descriptors (structural params LOCKED) ===
        std::vector<float> baseVec (N);
        {
            auto guess = descriptorGuess (refDescFull, instrType);
            guess.toArray (baseVec.data());
        }

        // Blend timbral params with preset seed
        std::vector<float> presetVec (N);
        {
            auto preset = getPresetSeed (instrType, refDescFull);
            preset.basePitch = (refDescFull.fundamentalFreq > 20.0f) ? refDescFull.fundamentalFreq : preset.basePitch;
            preset.duration = refDescFull.totalDuration;
            preset.toArray (presetVec.data());
        }

        // For active (timbral) params: blend guess + preset
        for (int j : activeParams)
            baseVec[j] = 0.5f * baseVec[j] + 0.5f * presetVec[j];

        // Blend with user seed if provided
        if (seedParams != nullptr)
        {
            std::vector<float> seedVec (N);
            seedParams->toArray (seedVec.data());
            for (int j : activeParams)
                baseVec[j] = 0.5f * baseVec[j] + 0.5f * seedVec[j];
        }

        // === STEP 2b: Tighten bounds on structural params ===
        {
            float f0 = refDescFull.fundamentalFreq;
            float dur = refDescFull.totalDuration;

            if (f0 > 20.0f) { mins[1] = f0 * 0.95f; maxs[1] = f0 * 1.05f; }
            mins[2] = std::max (0.01f, dur * 0.85f);
            maxs[2] = std::min (5.0f, dur * 1.15f);
            mins[3] = 0.8f; maxs[3] = 1.0f;

            if (refDescFull.pitchDropSemitones > 2.0f)
            {
                mins[6] = refDescFull.pitchDropSemitones * 0.75f;
                maxs[6] = refDescFull.pitchDropSemitones * 1.25f;
            }
            else { mins[6] = 0.0f; maxs[6] = 3.0f; }

            if (refDescFull.pitchDropTime > 0.001f)
            {
                mins[7] = std::max (0.0003f, refDescFull.pitchDropTime * 0.1f);
                maxs[7] = std::min (0.05f, refDescFull.pitchDropTime * 0.3f);
                mins[8] = std::max (0.003f, refDescFull.pitchDropTime * 0.7f);
                maxs[8] = std::min (0.5f, refDescFull.pitchDropTime * 1.3f);
            }

            float att = std::max (0.0001f, refDescFull.attackTime);
            mins[66] = std::max (0.0001f, att * 0.5f);
            maxs[66] = std::min (0.05f, att * 2.0f);

            float dec = std::max (0.005f, refDescFull.decayTime);
            mins[67] = std::max (0.005f, dec * 0.3f);
            maxs[67] = std::min (0.5f, dec * 3.0f);
            mins[68] = std::max (0.02f, refDescFull.decayTime40 * 0.5f);
            maxs[68] = std::min (3.0f, refDescFull.decayTime40 * 2.0f);

            mins[72] = 0.005f;
            maxs[72] = std::min (1.0f, dur * 0.4f);

            float detectedCurve = std::max (0.3f, std::min (3.0f, refDescFull.envelopeShape * 2.5f + 0.3f));
            mins[73] = std::max (0.1f, detectedCurve * 0.4f);
            maxs[73] = std::min (4.0f, detectedCurve * 2.5f);

            // filterCutoff: must preserve spectral content
            mins[74] = std::max (200.0f, refDescFull.spectralRolloff * 2.0f);
            maxs[75] = 0.5f; // filterReso

            maxs[82] = 0.4f; maxs[84] = 0.3f; maxs[87] = 0.5f;
        }

        // Clamp base vector
        for (int i = 0; i < N; ++i)
            baseVec[i] = std::max (mins[i], std::min (maxs[i], baseVec[i]));

        // === STEP 3: OscType screening ===
        {
            auto testVec = baseVec;
            std::vector<std::pair<float, int>> oscScores;
            for (int osc = 0; osc <= maxOscType; ++osc)
            {
                testVec[0] = (float) osc;
                float fit = evaluate (testVec.data(), sampleRate);
                oscScores.push_back ({fit, osc});
            }
            std::sort (oscScores.begin(), oscScores.end());
            baseVec[0] = (float) oscScores[0].second;
        }

        // === STEP 4: CMA-ES in active param subspace (A dims, not 88) ===
        // Extract active params into sub-vector for CMA-ES
        std::vector<float> subMean (A);
        std::vector<float> subMins (A), subMaxs (A);
        for (int i = 0; i < A; ++i)
        {
            int j = activeParams[i];
            subMean[i] = baseVec[j];
            subMins[i] = mins[j];
            subMaxs[i] = maxs[j];
        }

        // Normalize to [0,1]
        std::vector<float> normMean (A);
        for (int i = 0; i < A; ++i)
        {
            float range = subMaxs[i] - subMins[i];
            normMean[i] = (range > 1e-10f) ? (subMean[i] - subMins[i]) / range : 0.5f;
        }
        std::vector<float> normMins (A, 0.0f), normMaxs (A, 1.0f);

        CMAES cma;
        cma.init (A, normMean, 0.25f); // wider sigma OK because fewer dims

        std::mt19937 rng (std::random_device{}());

        OptimizationResult result;
        float bestFitness = 1e6f;
        std::vector<float> bestVec = baseVec;

        int totalGens = maxGenerations;
        int stagnationCount = 0;
        const int stagnationLimit = 60;
        int restartsUsed = 0;

        // Helper: inject sub-vector into full 88-param vector
        auto injectParams = [&](const std::vector<float>& sub) -> std::vector<float>
        {
            auto full = baseVec;
            for (int i = 0; i < A; ++i)
            {
                int j = activeParams[i];
                float range = subMaxs[i] - subMins[i];
                full[j] = subMins[i] + sub[i] * range;
                full[j] = std::max (mins[j], std::min (maxs[j], full[j]));
            }
            full[0] = std::round (std::max (0.0f, std::min ((float) maxOscType, full[0])));
            if (N > 59) full[59] = std::round (std::max (0.0f, std::min (3.0f, full[59])));
            if (N > 86) full[86] = std::round (std::max (0.0f, std::min (2.0f, full[86])));
            return full;
        };

        // === STEP 5: CMA-ES Main Loop ===
        for (int gen = 0; gen < totalGens; ++gen)
        {
            auto [normSamples, zVecs] = cma.samplePopulation (rng, normMins.data(), normMaxs.data());

            // Evaluate in full 88-param space
            std::vector<std::vector<float>> fullSamples (cma.lambda);
            std::vector<float> fitnesses (cma.lambda);
            for (int k = 0; k < cma.lambda; ++k)
            {
                fullSamples[k] = injectParams (normSamples[k]);
                fitnesses[k] = evaluate (fullSamples[k].data(), sampleRate);
            }

            // Ranking
            std::vector<int> ranking (cma.lambda);
            std::iota (ranking.begin(), ranking.end(), 0);
            std::sort (ranking.begin(), ranking.end(), [&](int a, int b) {
                return fitnesses[a] < fitnesses[b];
            });

            if (fitnesses[ranking[0]] < bestFitness)
            {
                bestFitness = fitnesses[ranking[0]];
                bestVec = fullSamples[ranking[0]];
                stagnationCount = 0;
            }
            else
            {
                ++stagnationCount;
            }

            cma.update (normSamples, zVecs, ranking);

            result.iterations = (gen + 1) * cma.lambda;
            result.bestDistance = bestFitness;
            result.cmaGenerations = gen + 1;

            if (progress && ! progress (gen + 1, bestFitness, totalGens))
                break;

            if (bestFitness < targetDistance) { result.converged = true; break; }

            // Stagnation or sigma collapse → ALWAYS restart (never stop early)
            if (cma.sigma < 1e-8f || stagnationCount >= stagnationLimit)
            {
                ++restartsUsed;
                stagnationCount = 0;

                for (int i = 0; i < A; ++i)
                {
                    int j = activeParams[i];
                    float range = subMaxs[i] - subMins[i];
                    normMean[i] = (range > 1e-10f) ? (bestVec[j] - subMins[i]) / range : 0.5f;
                }
                float restartSigma = std::max (0.03f, 0.20f / (float)(restartsUsed));
                cma.init (A, normMean, restartSigma);
            }
        }

        result.phase1Iterations = result.cmaGenerations;
        result.phase1Distance = bestFitness;

        // === STEP 6: Sensitivity analysis on active params ===
        for (int j : activeParams)
        {
            float range = maxs[j] - mins[j];
            if (range < 1e-10f) { result.sensitivity[j] = 0.0f; continue; }

            float step = range * 0.02f;
            auto testVec = bestVec;

            testVec[j] = std::max (mins[j], std::min (maxs[j], bestVec[j] + step));
            if (j == 0) testVec[0] = std::round (std::max (0.0f, std::min ((float) maxOscType, testVec[0])));
            float fitPlus = evaluate (testVec.data(), sampleRate);

            testVec[j] = std::max (mins[j], std::min (maxs[j], bestVec[j] - step));
            if (j == 0) testVec[0] = std::round (std::max (0.0f, std::min ((float) maxOscType, testVec[0])));
            float fitMinus = evaluate (testVec.data(), sampleRate);

            result.sensitivity[j] = std::abs (fitPlus - bestFitness) + std::abs (fitMinus - bestFitness);
        }

        float maxSens = *std::max_element (result.sensitivity.begin(), result.sensitivity.end());
        if (maxSens > 0.0f)
            for (auto& s : result.sensitivity) s /= maxSens;

        // === STEP 7: Multiple rounds of NM polish ===
        {
            std::vector<int> nmParams;
            for (int j : activeParams)
                if (result.sensitivity[j] > 0.01f)
                    nmParams.push_back (j);

            // 5 rounds of NM polish — squeeze every last bit of improvement
            MatchDescriptors dummyRef;
            for (int round = 0; round < 5 && ! nmParams.empty(); ++round)
            {
                auto refined = nelderMeadRefine (bestVec, nmParams, mins, maxs, dummyRef, sampleRate);
                float refinedFit = evaluate (refined.data(), sampleRate);
                if (refinedFit < bestFitness)
                {
                    bestVec = refined;
                    bestFitness = refinedFit;
                }
                else break; // no improvement, stop polishing
            }
        }

        result.bestDistance = bestFitness;
        result.bestParams.fromArray (bestVec.data());

        // === STEP 8: Compute detailed metrics for reporting ===
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
        g.ampBodyDecay = std::max (0.02f, d.decayTime);
        g.ampPunchLevel = std::max (0.3f, std::min (0.9f, d.transientStrength / 8.0f + 0.3f));
        g.envRelease = std::max (0.01f, std::min (1.0f, (d.totalDuration - d.decayTime) * 0.5f));
        g.envCurve = std::max (0.5f, std::min (3.0f, d.envelopeShape * 2.0f + 0.5f));
        g.envSustainLevel = 0.0f;
        g.envSustainTime = 0.0f;

        // Filter
        g.filterCutoff = std::max (500.0f, std::min (20000.0f, d.spectralRolloff * 2.5f));
        g.filterReso = 0.0f;

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

    // Evaluate a candidate using spectral match loss (PRIMARY)
    float evaluate (const float* paramArray, double sampleRate)
    {
        MatchSynthParams p;
        p.fromArray (paramArray);
        configureSynth();

        auto buffer = synth.generate (p, sampleRate);

        // Check for silent/NaN buffer
        {
            float peak = buffer.getMagnitude (0, 0, buffer.getNumSamples());
            if (peak < 1e-8f || std::isnan (peak) || std::isinf (peak))
                return 1e6f;
        }

        if (! refMonoReady) return 1e6f;

        // Convert to mono
        int genLen = buffer.getNumSamples();
        std::vector<float> genMono (genLen, 0.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int i = 0; i < genLen; ++i) genMono[i] += data[i];
        }
        float chScale = 1.0f / (float) buffer.getNumChannels();
        for (auto& s : genMono) s *= chScale;

        return computeSpectralMatchLoss (refMono, genMono, (float) sampleRate, refHNR);
    }

    // Nelder-Mead on a subset of active parameters
    std::vector<float> nelderMeadRefine (const std::vector<float>& start,
                                          const std::vector<int>& activeParams,
                                          const float* mins, const float* maxs,
                                          const MatchDescriptors& /*ref*/, double sampleRate)
    {
        const int M = (int) activeParams.size();
        const int maxNMIter = 80;

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
            simplex[i][i - 1] += range * 0.05f;
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
