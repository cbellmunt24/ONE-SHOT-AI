#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>
#include <array>
#include "../DSP/DSPConstants.h"

// Comprehensive audio descriptor extraction for the One-Shot Match system.
//
// Design: Multi-region analysis — the sound is split into three temporal zones:
//   1. TRANSIENT (0 to ~5ms after peak) — click, snap, attack character
//   2. BODY     (~5ms to ~80ms)         — tonal punch, pitch, harmonic content
//   3. TAIL     (~80ms to end)          — sub decay, resonance, release shape
//
// Each region gets its own spectral and temporal descriptors.
// Global descriptors capture the overall shape and perceptual character.
// Spectrotemporal matrix captures spectral evolution over time.
// MicroTransient captures fine transient detail in first 10ms after peak.
//
// Total: ~45 scalar descriptors + 64 spectrotemporal values + 8 microTransient + stereo correlation.

namespace oneshotmatch
{

// Number of spectral bands per region / spectrotemporal frame
static constexpr int NUM_SPECTRAL_BANDS = 8;
// High-resolution body bands (50-2000 Hz focused) for finer spectral matching
static constexpr int NUM_BODY_HIRES_BANDS = 16;
// Spectrotemporal matrix dimensions
static constexpr int SPECTRO_FRAMES = 8;
static constexpr int SPECTRO_BANDS  = 8;
static constexpr int SPECTRO_SIZE   = SPECTRO_FRAMES * SPECTRO_BANDS; // 64

struct RegionDescriptors
{
    // Spectral
    float spectralCentroid  = 0.0f;  // Hz
    float spectralSpread    = 0.0f;  // Hz (std dev around centroid)
    float spectralRolloff   = 0.0f;  // Hz (85% energy)
    float spectralFlatness  = 0.0f;  // 0..1 (1 = noise-like, 0 = tonal)
    float spectralFlux      = 0.0f;  // change from previous region

    // Energy distribution across bands (like coarse MFCC)
    std::array<float, NUM_SPECTRAL_BANDS> bandEnergy = {};

    // Temporal
    float rmsEnergy         = 0.0f;
    float peakLevel         = 0.0f;
    float zeroCrossingRate  = 0.0f;  // crossings/second
};

struct MatchDescriptors
{
    // === Global temporal ===
    float totalDuration     = 0.0f;  // seconds
    float attackTime        = 0.0f;  // seconds to reach peak
    float decayTime         = 0.0f;  // seconds from peak to -20 dB
    float decayTime40       = 0.0f;  // seconds from peak to -40 dB (full tail)
    float transientStrength = 0.0f;  // peak / RMS (crest factor)
    float envelopeShape     = 0.0f;  // 0=linear, 1=very exponential (curvature)

    // === Amplitude envelope shape (sampled at 16 points) ===
    static constexpr int ENV_POINTS = 16;
    std::array<float, ENV_POINTS> ampEnvelope = {};

    // === Global pitch ===
    float fundamentalFreq   = 0.0f;  // Hz, body steady-state fundamental
    float pitchStart        = 0.0f;  // Hz, frequency at transient
    float pitchEnd          = 0.0f;  // Hz, body steady frequency
    float pitchDropSemitones = 0.0f; // pitch difference start->end in semitones
    float pitchDropTime     = 0.0f;  // seconds, time to settle

    // === Pitch envelope (sampled at 8 points over first 100ms) ===
    static constexpr int PITCH_ENV_POINTS = 8;
    std::array<float, PITCH_ENV_POINTS> pitchEnvelope = {};

    // === Global spectral ===
    float spectralCentroid  = 0.0f;
    float spectralRolloff   = 0.0f;
    float brightness        = 0.0f;  // energy above 2kHz / total
    float harmonicNoiseRatio = 0.0f;
    float spectralTilt      = 0.0f;  // slope of spectrum (dB/octave)

    // === Sub energy ===
    float subEnergy         = 0.0f;  // energy below 100 Hz / total
    float lowMidEnergy      = 0.0f;  // energy 100-500 Hz / total
    float midEnergy         = 0.0f;  // energy 500-2000 Hz / total
    float highEnergy        = 0.0f;  // energy above 2000 Hz / total

    // === Perceptual ===
    float rmsLoudness       = 0.0f;
    float lufs              = 0.0f;  // estimated loudness

    // === Per-region descriptors ===
    RegionDescriptors transientRegion;
    RegionDescriptors bodyRegion;
    RegionDescriptors tailRegion;

    // === High-resolution body bands (16 bands, 50-2000 Hz) ===
    // Finer spectral resolution in the critical body region
    std::array<float, NUM_BODY_HIRES_BANDS> bodyHiResBands = {};

    // === Spectrotemporal matrix (8 frames x 8 bands) ===
    // Captures how the spectrum evolves over time — critical for precise matching
    std::array<float, SPECTRO_SIZE> spectroTemporal = {};

    // === Micro transient (8 points in first 10ms after peak) ===
    // Fine transient detail for precise attack matching
    static constexpr int MICRO_POINTS = 8;
    std::array<float, MICRO_POINTS> microTransient = {};

    // === Stereo correlation ===
    float stereoCorrelation = 1.0f;  // 1.0 = mono, 0.0 = fully decorrelated

    // === Harmonic profile (amplitudes of harmonics 1-6 relative to fundamental) ===
    static constexpr int NUM_HARMONICS = 6;
    std::array<float, NUM_HARMONICS> harmonicProfile = {};  // h1=1.0 (normalized), h2..h6

    // === Sub harmonic ratio (energy at f0/2 vs f0) ===
    float subHarmonicRatio = 0.0f;

    // === Noise spectral centroid (Hz, centroid of non-harmonic content) ===
    float noiseSpectralCentroid = 0.0f;

    // === Spectral crest factor (peak/mean of body spectrum, tonality indicator) ===
    float spectralCrest = 1.0f;

    bool valid = false;
};

class DescriptorExtractor
{
public:
    // fastMode: skip expensive spectrotemporal (8 FFTs) + per-region (3 FFTs) + body hi-res
    // Used during optimization to speed up evaluation ~4x
    void setFastMode (bool fast) { fastMode = fast; }

    MatchDescriptors extract (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        MatchDescriptors d;
        const int numSamples = buffer.getNumSamples();
        if (numSamples < 128) return d;

        // === Stereo correlation (before mono mix) ===
        if (buffer.getNumChannels() >= 2)
        {
            const float* L = buffer.getReadPointer (0);
            const float* R = buffer.getReadPointer (1);
            float corrNum = 0.0f, normL = 0.0f, normR = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                corrNum += L[i] * R[i];
                normL += L[i] * L[i];
                normR += R[i] * R[i];
            }
            float denom = std::sqrt (normL * normR);
            d.stereoCorrelation = (denom > 1e-10f) ? (corrNum / denom) : 1.0f;
        }

        // Mix to mono
        std::vector<float> mono (numSamples, 0.0f);
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float* data = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                mono[i] += data[i];
        }
        float chScale = 1.0f / (float) buffer.getNumChannels();
        for (auto& s : mono) s *= chScale;

        const float sr = (float) sampleRate;
        d.totalDuration = (float) numSamples / sr;

        // === Global RMS ===
        float sumSq = 0.0f;
        for (auto s : mono) sumSq += s * s;
        d.rmsLoudness = std::sqrt (sumSq / (float) numSamples);
        d.lufs = (d.rmsLoudness > 0.00001f) ? 20.0f * std::log10 (d.rmsLoudness) : -100.0f;

        // === Peak & attack ===
        float peak = 0.0f;
        int peakIdx = 0;
        for (int i = 0; i < numSamples; ++i)
        {
            float v = std::abs (mono[i]);
            if (v > peak) { peak = v; peakIdx = i; }
        }
        d.attackTime = (float) peakIdx / sr;
        d.transientStrength = (d.rmsLoudness > 0.0001f) ? (peak / d.rmsLoudness) : 0.0f;

        // === Decay times (-20 dB and -40 dB) ===
        float thresh20 = peak * 0.1f;
        float thresh40 = peak * 0.01f;
        d.decayTime = findDecayTime (mono, peakIdx, thresh20, sr);
        d.decayTime40 = findDecayTime (mono, peakIdx, thresh40, sr);

        // === Envelope shape (curvature) ===
        d.envelopeShape = measureEnvelopeCurvature (mono, peakIdx, sr);

        // === Sampled amplitude envelope (16 points) ===
        extractAmplitudeEnvelope (mono, d.ampEnvelope, sr);

        // === Define temporal regions ===
        int transientEnd = std::min (peakIdx + (int)(0.005f * sr), numSamples);
        int bodyEnd      = std::min (peakIdx + (int)(0.080f * sr), numSamples);
        int tailStart    = bodyEnd;

        // === Pitch analysis ===
        // Body fundamental: 30-100ms after peak (large window for low-freq detection)
        int pitchBodyStart = std::min (peakIdx + (int)(0.030f * sr), numSamples - 1);
        int pitchBodyEnd   = std::min (peakIdx + (int)(0.100f * sr), numSamples - 1);
        d.fundamentalFreq = estimateF0Autocorrelation (mono, pitchBodyStart, pitchBodyEnd, sr);

        // === F0 sanity check: wider window for very low-frequency sounds ===
        // YIN is robust against octave errors, but a wider window helps for sub-bass
        {
            float f0Wide = estimateF0Autocorrelation (mono, 0, std::min (numSamples, (int)(sr * 0.15f)), sr);

            // If body window gave no result but wider window did, use wider
            if (d.fundamentalFreq < 15.0f && f0Wide > 15.0f)
                d.fundamentalFreq = f0Wide;

            // If both disagree significantly, prefer the lower (likely fundamental)
            if (f0Wide > 15.0f && d.fundamentalFreq > f0Wide * 1.8f)
                d.fundamentalFreq = f0Wide;
        }

        d.pitchEnd = d.fundamentalFreq;

        // Onset pitch: use zero-crossing rate in first 3ms for instantaneous frequency
        // This captures the very start of a pitch sweep (autocorrelation needs too long a window)
        {
            // ZCR in first 3ms (captures frequencies up to ~sr/2, good for 100-2000Hz sweeps)
            int zcrEnd = std::min (peakIdx + (int)(0.003f * sr), numSamples - 1);
            int zcrSamples = zcrEnd - peakIdx;
            if (zcrSamples > 4)
            {
                int crossings = 0;
                for (int z = peakIdx + 1; z < zcrEnd; ++z)
                    if ((mono[z - 1] >= 0.0f) != (mono[z] >= 0.0f)) ++crossings;
                float zcr3ms = (float) crossings * sr / (2.0f * (float) zcrSamples);
                if (zcr3ms > 40.0f)
                    d.pitchStart = zcr3ms;
            }

            // Fallback: ZCR in first 5ms
            if (d.pitchStart < 40.0f)
            {
                int zcrEnd5 = std::min (peakIdx + (int)(0.005f * sr), numSamples - 1);
                int zcrSamples5 = zcrEnd5 - peakIdx;
                if (zcrSamples5 > 8)
                {
                    int crossings5 = 0;
                    for (int z = peakIdx + 1; z < zcrEnd5; ++z)
                        if ((mono[z - 1] >= 0.0f) != (mono[z] >= 0.0f)) ++crossings5;
                    float zcr5ms = (float) crossings5 * sr / (2.0f * (float) zcrSamples5);
                    if (zcr5ms > 30.0f) d.pitchStart = zcr5ms;
                }
            }

            // Last fallback: autocorrelation in 10ms window
            if (d.pitchStart < 30.0f)
            {
                int onsetEnd = std::min (peakIdx + (int)(0.010f * sr), numSamples - 1);
                d.pitchStart = estimateF0Autocorrelation (mono, peakIdx, onsetEnd, sr);
            }

            if (d.pitchStart < 15.0f) d.pitchStart = d.fundamentalFreq * 2.0f;
        }

        if (d.fundamentalFreq > 10.0f && d.pitchStart > 10.0f)
            d.pitchDropSemitones = 12.0f * std::log2 (d.pitchStart / d.fundamentalFreq);
        else
            d.pitchDropSemitones = 0.0f;

        d.pitchDropTime = estimatePitchDropTime (mono, peakIdx, sr, d.fundamentalFreq);

        // Pitch envelope: adaptive window size based on fundamental frequency
        extractPitchEnvelope (mono, peakIdx, d.pitchEnvelope, sr, d.fundamentalFreq);

        // === Full-sample spectral analysis ===
        int fftOrder = 11; // 2048
        int fftSize = 1 << fftOrder;

        if (numSamples >= fftSize)
        {
            auto fullSpec = computeSpectrum (mono, 0, fftSize, sr, fftOrder);
            d.spectralCentroid = fullSpec.spectralCentroid;
            d.spectralRolloff = fullSpec.spectralRolloff;
            d.harmonicNoiseRatio = computeHNR (fullSpec.magnitudes, d.fundamentalFreq, sr, fftSize);

            computeEnergyBands (fullSpec.magnitudes, sr, fftSize,
                                d.subEnergy, d.lowMidEnergy, d.midEnergy, d.highEnergy);
            d.brightness = d.highEnergy;

            d.spectralTilt = computeSpectralTilt (fullSpec.magnitudes, sr, fftSize);
        }

        // === Per-region spectral analysis (always computed — critical for distance function) ===
        {
            int regionFFTOrder = 9; // 512
            int regionFFTSize = 1 << regionFFTOrder;

            if (transientEnd - peakIdx >= regionFFTSize / 2)
                d.transientRegion = extractRegionDescriptors (mono, std::max (0, peakIdx - 64), transientEnd, sr, regionFFTOrder);
            else
                d.transientRegion = extractRegionDescriptors (mono, std::max (0, peakIdx - 64), std::min (numSamples, peakIdx + regionFFTSize), sr, regionFFTOrder);

            if (bodyEnd > transientEnd && bodyEnd - transientEnd >= regionFFTSize / 2)
                d.bodyRegion = extractRegionDescriptors (mono, transientEnd, bodyEnd, sr, regionFFTOrder);

            if (numSamples > tailStart + regionFFTSize / 2)
                d.tailRegion = extractRegionDescriptors (mono, tailStart, std::min (numSamples, tailStart + fftSize), sr, regionFFTOrder);

            d.transientRegion.spectralFlux = 0.0f;
            d.bodyRegion.spectralFlux = computeBandFlux (d.transientRegion.bandEnergy, d.bodyRegion.bandEnergy);
            d.tailRegion.spectralFlux = computeBandFlux (d.bodyRegion.bandEnergy, d.tailRegion.bandEnergy);
        }

        // === High-resolution body bands (16 bands, 50-2000 Hz) ===
        if (numSamples >= fftSize)
        {
            auto bodySpec = computeSpectrum (mono, transientEnd, fftSize, sr, fftOrder);
            computeBodyHiResBands (bodySpec.magnitudes, sr, fftSize, d.bodyHiResBands);

            // === Harmonic profile, sub ratio, noise centroid, spectral crest ===
            extractHarmonicProfile (bodySpec.magnitudes, d.fundamentalFreq, sr, fftSize, d.harmonicProfile);
            d.subHarmonicRatio = computeSubHarmonicRatio (bodySpec.magnitudes, d.fundamentalFreq, sr, fftSize);
            d.noiseSpectralCentroid = computeNoiseSpectralCentroid (bodySpec.magnitudes, d.fundamentalFreq, sr, fftSize);
            d.spectralCrest = computeSpectralCrest (bodySpec.magnitudes);
        }

        // === Spectrotemporal matrix (always computed — weight 8 in distance function) ===
        extractSpectroTemporal (mono, peakIdx, d.spectroTemporal, sr);

        // === Micro transient (8 points in first 10ms after peak) ===
        extractMicroTransient (mono, peakIdx, d.microTransient, sr);

        d.valid = true;
        return d;
    }

private:
    bool fastMode = false;

    // ========== Spectrum computation ==========

    struct SpectrumResult
    {
        std::vector<float> magnitudes;
        float spectralCentroid = 0.0f;
        float spectralRolloff = 0.0f;
        float totalEnergy = 0.0f;
    };

    SpectrumResult computeSpectrum (const std::vector<float>& mono, int start, int fftSize, float sr, int fftOrder)
    {
        SpectrumResult r;
        int numBins = fftSize / 2;
        r.magnitudes.resize (numBins, 0.0f);

        std::vector<float> fftData (fftSize * 2, 0.0f);
        int available = std::min (fftSize, (int) mono.size() - start);
        for (int i = 0; i < available; ++i)
        {
            float w = 0.5f * (1.0f - std::cos (dsputil::TWO_PI * (float) i / (float) fftSize));
            fftData[i] = mono[start + i] * w;
        }

        juce::dsp::FFT fft (fftOrder);
        fft.performRealOnlyForwardTransform (fftData.data());

        float centroidNum = 0.0f;
        float cumEnergy = 0.0f;
        bool rolloffFound = false;

        for (int i = 0; i < numBins; ++i)
        {
            float re = fftData[i * 2];
            float im = fftData[i * 2 + 1];
            float mag = std::sqrt (re * re + im * im);
            r.magnitudes[i] = mag;
            float freq = (float) i * sr / (float) fftSize;
            float energy = mag * mag;
            r.totalEnergy += energy;
            centroidNum += freq * energy;
        }

        if (r.totalEnergy > 0.0f)
        {
            r.spectralCentroid = centroidNum / r.totalEnergy;
            float target85 = r.totalEnergy * 0.85f;
            for (int i = 0; i < numBins && ! rolloffFound; ++i)
            {
                cumEnergy += r.magnitudes[i] * r.magnitudes[i];
                if (cumEnergy >= target85)
                {
                    r.spectralRolloff = (float) i * sr / (float) fftSize;
                    rolloffFound = true;
                }
            }
        }

        return r;
    }

    // ========== Region descriptor extraction ==========

    RegionDescriptors extractRegionDescriptors (const std::vector<float>& mono, int start, int end, float sr, int fftOrder)
    {
        RegionDescriptors rd;
        int len = end - start;
        if (len < 32) return rd;

        // Temporal
        float sumSq = 0.0f, peakV = 0.0f;
        int crossings = 0;
        for (int i = start; i < end; ++i)
        {
            float v = mono[i];
            sumSq += v * v;
            float av = std::abs (v);
            if (av > peakV) peakV = av;
            if (i > start && ((mono[i - 1] >= 0.0f && v < 0.0f) || (mono[i - 1] < 0.0f && v >= 0.0f)))
                ++crossings;
        }
        rd.rmsEnergy = std::sqrt (sumSq / (float) len);
        rd.peakLevel = peakV;
        rd.zeroCrossingRate = (float) crossings / ((float) len / sr);

        // Spectral
        int fftSize = 1 << fftOrder;
        if (len >= fftSize / 2)
        {
            auto spec = computeSpectrum (mono, start, fftSize, sr, fftOrder);
            rd.spectralCentroid = spec.spectralCentroid;
            rd.spectralRolloff = spec.spectralRolloff;

            int numBins = fftSize / 2;

            // Spectral spread
            if (spec.totalEnergy > 0.0f)
            {
                float spreadNum = 0.0f;
                for (int i = 0; i < numBins; ++i)
                {
                    float freq = (float) i * sr / (float) fftSize;
                    float diff = freq - rd.spectralCentroid;
                    spreadNum += diff * diff * spec.magnitudes[i] * spec.magnitudes[i];
                }
                rd.spectralSpread = std::sqrt (spreadNum / spec.totalEnergy);
            }

            // Spectral flatness (geometric mean / arithmetic mean)
            float logSum = 0.0f, linearSum = 0.0f;
            int validBins = 0;
            for (int i = 1; i < numBins; ++i)
            {
                if (spec.magnitudes[i] > 1e-10f)
                {
                    logSum += std::log (spec.magnitudes[i]);
                    linearSum += spec.magnitudes[i];
                    ++validBins;
                }
            }
            if (validBins > 0 && linearSum > 0.0f)
            {
                float geoMean = std::exp (logSum / (float) validBins);
                float ariMean = linearSum / (float) validBins;
                rd.spectralFlatness = geoMean / ariMean;
            }

            // Band energy (8 bands, logarithmically spaced)
            computeBandEnergies (spec.magnitudes, sr, fftSize, rd.bandEnergy);
        }

        return rd;
    }

    // ========== Spectrotemporal extraction ==========

    void extractSpectroTemporal (const std::vector<float>& mono, int peakIdx,
                                  std::array<float, SPECTRO_SIZE>& st, float sr)
    {
        st.fill (0.0f);
        int numSamples = (int) mono.size();

        // Analyze first 200ms or total duration, whichever is shorter
        float analysisDuration = std::min (0.2f, (float)(numSamples - peakIdx) / sr);
        if (analysisDuration < 0.005f) return;

        int stFFTOrder = 9; // 512
        int stFFTSize = 1 << stFFTOrder;

        for (int f = 0; f < SPECTRO_FRAMES; ++f)
        {
            float t = (float) f / (float)(SPECTRO_FRAMES - 1);
            int center = peakIdx + (int)(t * analysisDuration * sr);
            int frameStart = std::max (0, center - stFFTSize / 2);

            if (frameStart + stFFTSize > numSamples)
                frameStart = std::max (0, numSamples - stFFTSize);

            if (frameStart + stFFTSize / 2 > numSamples) continue;

            auto spec = computeSpectrum (mono, frameStart, stFFTSize, sr, stFFTOrder);

            std::array<float, NUM_SPECTRAL_BANDS> bands = {};
            computeBandEnergies (spec.magnitudes, sr, stFFTSize, bands);

            for (int b = 0; b < SPECTRO_BANDS; ++b)
                st[f * SPECTRO_BANDS + b] = bands[b];
        }
    }

    // ========== Micro transient extraction ==========
    // Samples abs(mono) at 8 equally-spaced points in the first 10ms after peak,
    // normalized so max = 1.

    void extractMicroTransient (const std::vector<float>& mono, int peakIdx,
                                std::array<float, MatchDescriptors::MICRO_POINTS>& mt, float sr)
    {
        mt.fill (0.0f);
        int numSamples = (int) mono.size();

        // 10ms window after peak
        int windowSamples = (int)(0.010f * sr);
        int windowEnd = std::min (numSamples, peakIdx + windowSamples);
        int windowLen = windowEnd - peakIdx;
        if (windowLen < MatchDescriptors::MICRO_POINTS) return;

        float step = (float) windowLen / (float) MatchDescriptors::MICRO_POINTS;

        for (int p = 0; p < MatchDescriptors::MICRO_POINTS; ++p)
        {
            int idx = peakIdx + (int)((float) p * step);
            if (idx < numSamples)
                mt[p] = std::abs (mono[idx]);
        }

        // Normalize to max = 1
        float maxVal = *std::max_element (mt.begin(), mt.end());
        if (maxVal > 0.0f)
            for (auto& v : mt) v /= maxVal;
    }

    // ========== Band energy computation ==========

    // Hz to Mel conversion
    static float hzToMel (float hz) { return 2595.0f * std::log10 (1.0f + hz / 700.0f); }
    static float melToHz (float mel) { return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f); }

    void computeBandEnergies (const std::vector<float>& mags, float sr, int fftSize,
                              std::array<float, NUM_SPECTRAL_BANDS>& bands)
    {
        // 8 Mel-spaced bands (perceptually uniform)
        float melLow = hzToMel (20.0f);
        float melHigh = hzToMel (std::min (20000.0f, sr * 0.45f));
        float melStep = (melHigh - melLow) / (float) NUM_SPECTRAL_BANDS;

        int numBins = (int) mags.size();
        float totalE = 0.0f;

        for (int b = 0; b < NUM_SPECTRAL_BANDS; ++b)
        {
            float edgeLo = melToHz (melLow + (float) b * melStep);
            float edgeHi = melToHz (melLow + (float)(b + 1) * melStep);
            int lo = std::max (0, std::min ((int)(edgeLo / sr * (float) fftSize), numBins - 1));
            int hi = std::max (lo + 1, std::min ((int)(edgeHi / sr * (float) fftSize), numBins));

            float e = 0.0f;
            for (int i = lo; i < hi; ++i) e += mags[i] * mags[i];
            bands[b] = e;
            totalE += e;
        }

        if (totalE > 0.0f)
            for (auto& b : bands) b /= totalE;
    }

    void computeEnergyBands (const std::vector<float>& mags, float sr, int fftSize,
                             float& sub, float& lowMid, float& mid, float& high)
    {
        int numBins = (int) mags.size();
        float totalE = 0.0f, subE = 0.0f, lmE = 0.0f, mE = 0.0f, hE = 0.0f;

        for (int i = 0; i < numBins; ++i)
        {
            float freq = (float) i * sr / (float) fftSize;
            float e = mags[i] * mags[i];
            totalE += e;
            if (freq < 100.0f)       subE += e;
            else if (freq < 500.0f)  lmE += e;
            else if (freq < 2000.0f) mE += e;
            else                      hE += e;
        }

        if (totalE > 0.0f)
        {
            sub = subE / totalE;
            lowMid = lmE / totalE;
            mid = mE / totalE;
            high = hE / totalE;
        }
    }

    // ========== High-resolution body band computation (50-2000 Hz, 16 bands) ==========

    void computeBodyHiResBands (const std::vector<float>& mags, float sr, int fftSize,
                                std::array<float, NUM_BODY_HIRES_BANDS>& bands)
    {
        float melLow = hzToMel (50.0f);
        float melHigh = hzToMel (2000.0f);
        float melStep = (melHigh - melLow) / (float) NUM_BODY_HIRES_BANDS;

        int numBins = (int) mags.size();
        float totalE = 0.0f;

        for (int b = 0; b < NUM_BODY_HIRES_BANDS; ++b)
        {
            float edgeLo = melToHz (melLow + (float) b * melStep);
            float edgeHi = melToHz (melLow + (float)(b + 1) * melStep);
            int lo = std::max (0, std::min ((int)(edgeLo / sr * (float) fftSize), numBins - 1));
            int hi = std::max (lo + 1, std::min ((int)(edgeHi / sr * (float) fftSize), numBins));

            float e = 0.0f;
            for (int i = lo; i < hi; ++i) e += mags[i] * mags[i];
            bands[b] = e;
            totalE += e;
        }

        if (totalE > 0.0f)
            for (auto& b : bands) b /= totalE;
    }

    // ========== Harmonic profile extraction ==========
    // Measures amplitudes of harmonics 1-6 relative to fundamental

    void extractHarmonicProfile (const std::vector<float>& mags, float f0, float sr, int fftSize,
                                 std::array<float, MatchDescriptors::NUM_HARMONICS>& profile)
    {
        profile.fill (0.0f);
        if (f0 < 20.0f) return; // no valid fundamental

        int numBins = (int) mags.size();
        float binRes = sr / (float) fftSize;

        // Find peak magnitude in a window around each harmonic
        auto peakNear = [&](float freq) -> float
        {
            int centerBin = (int)(freq / binRes + 0.5f);
            int window = std::max (1, (int)(freq * 0.05f / binRes)); // +/-5% of harmonic freq
            float best = 0.0f;
            for (int b = std::max (0, centerBin - window); b <= std::min (numBins - 1, centerBin + window); ++b)
                best = std::max (best, mags[b]);
            return best;
        };

        float h1 = peakNear (f0);
        profile[0] = 1.0f; // fundamental normalized to 1

        if (h1 < 1e-8f) return;

        for (int h = 2; h <= MatchDescriptors::NUM_HARMONICS; ++h)
        {
            float hFreq = f0 * (float) h;
            if (hFreq > sr * 0.45f) break; // above Nyquist margin
            profile[h - 1] = peakNear (hFreq) / h1;
        }
    }

    // ========== Sub harmonic ratio ==========
    // Ratio of energy at f0/2 vs energy at f0

    float computeSubHarmonicRatio (const std::vector<float>& mags, float f0, float sr, int fftSize)
    {
        if (f0 < 40.0f) return 0.0f; // sub-octave would be below 20 Hz

        float binRes = sr / (float) fftSize;
        int numBins = (int) mags.size();

        auto bandEnergy = [&](float freq) -> float
        {
            int centerBin = (int)(freq / binRes + 0.5f);
            int window = std::max (1, (int)(freq * 0.10f / binRes)); // +/-10%
            float e = 0.0f;
            for (int b = std::max (0, centerBin - window); b <= std::min (numBins - 1, centerBin + window); ++b)
                e += mags[b] * mags[b];
            return e;
        };

        float fundEnergy = bandEnergy (f0);
        float subEnergy  = bandEnergy (f0 * 0.5f);

        return (fundEnergy > 1e-10f) ? (subEnergy / fundEnergy) : 0.0f;
    }

    // ========== Noise spectral centroid ==========
    // Centroid of non-harmonic (noise) content: masks out bins near harmonics 1-10

    float computeNoiseSpectralCentroid (const std::vector<float>& mags, float f0, float sr, int fftSize)
    {
        if (f0 < 20.0f) return 0.0f;

        int numBins = (int) mags.size();
        float binRes = sr / (float) fftSize;

        // Build harmonic mask — mark bins within +/-2 of each harmonic
        std::vector<bool> isHarmonic (numBins, false);
        for (int h = 1; h <= 10; ++h)
        {
            float hFreq = f0 * (float) h;
            if (hFreq > sr * 0.45f) break;
            int centerBin = (int)(hFreq / binRes + 0.5f);
            for (int b = std::max (0, centerBin - 2); b <= std::min (numBins - 1, centerBin + 2); ++b)
                isHarmonic[b] = true;
        }

        // Compute centroid of remaining bins
        float centroidNum = 0.0f, totalE = 0.0f;
        for (int i = 1; i < numBins; ++i)
        {
            if (isHarmonic[i]) continue;
            float freq = (float) i * binRes;
            float e = mags[i] * mags[i];
            centroidNum += freq * e;
            totalE += e;
        }

        return (totalE > 1e-10f) ? (centroidNum / totalE) : 0.0f;
    }

    // ========== Spectral crest factor ==========
    // peak / mean of magnitude spectrum — high = tonal, low = noise-like

    float computeSpectralCrest (const std::vector<float>& mags)
    {
        if (mags.empty()) return 1.0f;

        float maxVal = 0.0f, sum = 0.0f;
        for (auto m : mags)
        {
            maxVal = std::max (maxVal, m);
            sum += m;
        }

        float mean = sum / (float) mags.size();
        return (mean > 1e-10f) ? (maxVal / mean) : 1.0f;
    }

    // ========== Pitch estimation: YIN algorithm (Cheveigné & Kawahara 2002) ==========
    // Key advantage over autocorrelation: cumulative mean normalized difference
    // finds the FIRST dip below threshold (fundamental), not the global maximum
    // (which may be a harmonic). This eliminates octave-locking errors.

    float estimateF0Autocorrelation (const std::vector<float>& mono, int start, int end, float sr)
    {
        int len = end - start;
        if (len < 32) return 0.0f;

        int minLag = std::max (2, (int)(sr / 500.0f));  // up to 500 Hz
        int maxLag = std::min (len / 2, (int)(sr / 20.0f)); // down to 20 Hz
        if (maxLag <= minLag) return 0.0f;

        // Step 1: Difference function d(tau) = sum((x[i] - x[i+tau])^2)
        std::vector<float> diff (maxLag + 1, 0.0f);
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float sum = 0.0f;
            int corrLen = len - tau;
            for (int i = 0; i < corrLen; ++i)
            {
                float d = mono[start + i] - mono[start + i + tau];
                sum += d * d;
            }
            diff[tau] = sum;
        }

        // Step 2: Cumulative Mean Normalized Difference (CMND)
        // d'(tau) = d(tau) / ((1/tau) * sum(d(j), j=1..tau))
        // d'(1) = 1 by definition
        std::vector<float> cmnd (maxLag + 1, 0.0f);
        cmnd[0] = 1.0f;
        float runningSum = 0.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            runningSum += diff[tau];
            cmnd[tau] = (runningSum > 1e-10f) ? diff[tau] * (float) tau / runningSum : 1.0f;
        }

        // Step 3: Absolute threshold — find FIRST tau where cmnd < threshold
        // YIN's key insight: prefer fundamental over harmonics
        const float threshold = 0.15f;
        int bestLag = -1;

        for (int tau = minLag; tau <= maxLag; ++tau)
        {
            if (cmnd[tau] < threshold)
            {
                // Find the local minimum in this dip
                while (tau + 1 <= maxLag && cmnd[tau + 1] < cmnd[tau])
                    ++tau;
                bestLag = tau;
                break;
            }
        }

        // Fallback: if no dip below threshold, find global minimum
        if (bestLag < 0)
        {
            float bestVal = 1e6f;
            for (int tau = minLag; tau <= maxLag; ++tau)
            {
                if (cmnd[tau] < bestVal)
                {
                    bestVal = cmnd[tau];
                    bestLag = tau;
                }
            }
            // If global min is still too high, no pitch detected
            if (bestVal > 0.5f) return 0.0f;
        }

        // Step 4: Parabolic interpolation for sub-sample accuracy
        if (bestLag > minLag && bestLag < maxLag)
        {
            float y0 = cmnd[bestLag - 1];
            float y1 = cmnd[bestLag];
            float y2 = cmnd[bestLag + 1];
            float denom = 2.0f * (2.0f * y1 - y0 - y2);
            if (std::abs (denom) > 1e-10f)
            {
                float shift = (y0 - y2) / denom;
                float refinedLag = (float) bestLag + std::max (-0.5f, std::min (0.5f, shift));
                return sr / refinedLag;
            }
        }

        return sr / (float) bestLag;
    }

    // ========== Pitch envelope ==========

    void extractPitchEnvelope (const std::vector<float>& mono, int peakIdx,
                               std::array<float, MatchDescriptors::PITCH_ENV_POINTS>& env, float sr,
                               float fundamentalFreq = 50.0f)
    {
        // Window must contain at least 2.5 cycles of the fundamental for reliable autocorrelation
        // Old: fixed 4ms = 176 samples → maxLag=88 → can only detect >500Hz! (useless for kicks)
        // New: adaptive, at least 2.5 cycles of fundamental or 20ms minimum
        float minPeriod = (fundamentalFreq > 20.0f) ? (1.0f / fundamentalFreq) : 0.02f;
        int windowSize = std::max ((int)(0.020f * sr), (int)(2.5f * minPeriod * sr));
        windowSize = std::min (windowSize, (int)(0.050f * sr)); // cap at 50ms

        float maxTime = 0.100f;
        float step = maxTime / (float) MatchDescriptors::PITCH_ENV_POINTS;

        for (int p = 0; p < MatchDescriptors::PITCH_ENV_POINTS; ++p)
        {
            float t = (float) p * step;
            int start = peakIdx + (int)(t * sr);
            int end = std::min (start + windowSize, (int) mono.size());
            if (end - start >= windowSize / 2)
                env[p] = estimateF0Autocorrelation (mono, start, end, sr);
            else
                env[p] = env[std::max (0, p - 1)];
        }
    }

    // ========== Amplitude envelope ==========

    void extractAmplitudeEnvelope (const std::vector<float>& mono,
                                   std::array<float, MatchDescriptors::ENV_POINTS>& env, float sr)
    {
        int numSamples = (int) mono.size();
        float step = (float) numSamples / (float) MatchDescriptors::ENV_POINTS;
        int windowSamples = std::max (1, (int)(0.002f * sr));

        for (int p = 0; p < MatchDescriptors::ENV_POINTS; ++p)
        {
            int center = (int)((float) p * step);
            int start = std::max (0, center - windowSamples / 2);
            int end = std::min (numSamples, center + windowSamples / 2);

            float maxAbs = 0.0f;
            for (int i = start; i < end; ++i)
            {
                float v = std::abs (mono[i]);
                if (v > maxAbs) maxAbs = v;
            }
            env[p] = maxAbs;
        }

        float maxEnv = *std::max_element (env.begin(), env.end());
        if (maxEnv > 0.0f)
            for (auto& e : env) e /= maxEnv;
    }

    // ========== Helpers ==========

    float findDecayTime (const std::vector<float>& mono, int peakIdx, float threshold, float sr)
    {
        for (int i = peakIdx; i < (int) mono.size(); ++i)
        {
            if (std::abs (mono[i]) < threshold)
                return (float)(i - peakIdx) / sr;
        }
        return (float)((int) mono.size() - peakIdx) / sr;
    }

    float measureEnvelopeCurvature (const std::vector<float>& mono, int peakIdx, float sr)
    {
        int checkLen = std::min ((int)(0.1f * sr), (int) mono.size() - peakIdx);
        if (checkLen < 10) return 0.5f;

        float peak = std::abs (mono[peakIdx]);
        if (peak < 0.001f) return 0.5f;

        int mid = peakIdx + checkLen / 2;
        float midLevel = std::abs (mono[mid]) / peak;
        float endLevel = std::abs (mono[peakIdx + checkLen - 1]) / peak;

        float linearMid = 0.5f * (1.0f + endLevel);
        float expMid = std::sqrt (endLevel);

        if (std::abs (linearMid - expMid) < 0.001f) return 0.5f;
        return std::max (0.0f, std::min (1.0f, (midLevel - linearMid) / (expMid - linearMid)));
    }

    float estimatePitchDropTime (const std::vector<float>& mono, int peakIdx, float sr, float targetFreq)
    {
        if (targetFreq < 20.0f) return 0.02f;

        // Window must be large enough to detect targetFreq (need at least 2 cycles)
        float minPeriod = 1.0f / targetFreq;
        int windowSize = std::max ((int)(0.020f * sr), (int)(2.5f * minPeriod * sr));
        windowSize = std::min (windowSize, (int)(0.050f * sr));
        int hopSize = windowSize / 3; // overlap for better temporal resolution
        int maxWindows = 25;

        for (int w = 0; w < maxWindows; ++w)
        {
            int start = peakIdx + w * hopSize;
            int end = std::min (start + windowSize, (int) mono.size());
            if (end - start < windowSize / 2) break;

            float freq = estimateF0Autocorrelation (mono, start, end, sr);
            if (freq > 0.0f && std::abs (freq - targetFreq) / targetFreq < 0.15f)
                return (float)(start - peakIdx) / sr;
        }
        return 0.04f;
    }

    float computeHNR (const std::vector<float>& mags, float fundamental, float sr, int fftSize)
    {
        if (fundamental < 20.0f) return 0.0f;

        float harmonicE = 0.0f, totalE = 0.0f;
        int numBins = (int) mags.size();
        float binW = sr / (float) fftSize;

        for (int i = 0; i < numBins; ++i)
            totalE += mags[i] * mags[i];

        if (totalE < 1e-10f) return 0.0f;

        for (int h = 1; h <= 10; ++h)
        {
            float hFreq = fundamental * (float) h;
            if (hFreq > sr * 0.45f) break;
            int hBin = (int)(hFreq / binW);
            for (int b = std::max (0, hBin - 2); b <= std::min (numBins - 1, hBin + 2); ++b)
                harmonicE += mags[b] * mags[b];
        }

        return harmonicE / totalE;
    }

    float computeSpectralTilt (const std::vector<float>& mags, float sr, int fftSize)
    {
        int numBins = (int) mags.size();
        float sumX = 0.0f, sumY = 0.0f, sumXY = 0.0f, sumX2 = 0.0f;
        int n = 0;

        for (int i = 2; i < numBins; ++i)
        {
            float freq = (float) i * sr / (float) fftSize;
            if (freq < 30.0f || freq > 15000.0f) continue;
            if (mags[i] < 1e-10f) continue;

            float x = std::log2 (freq);
            float y = 20.0f * std::log10 (mags[i]);
            sumX += x; sumY += y; sumXY += x * y; sumX2 += x * x;
            ++n;
        }

        if (n < 4) return 0.0f;
        return (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX + 1e-10f);
    }

    float computeBandFlux (const std::array<float, NUM_SPECTRAL_BANDS>& a,
                           const std::array<float, NUM_SPECTRAL_BANDS>& b)
    {
        float flux = 0.0f;
        for (int i = 0; i < NUM_SPECTRAL_BANDS; ++i)
            flux += (b[i] - a[i]) * (b[i] - a[i]);
        return std::sqrt (flux);
    }
};

// ==========================================================================
// Mel Spectrogram Loss — PRIMARY loss function for spectral matching
//
// Multi-resolution mel spectrogram comparison: compares the ACTUAL audio
// signals directly in perceptual frequency space. Far more accurate than
// scalar descriptors for sound reconstruction.
//
// Loss = 0.7 * MelSpectrogramLoss + 0.15 * (1 - EnvelopeCorrelation) + 0.15 * PitchContourLoss
// ==========================================================================

class MelFilterbank
{
public:
    MelFilterbank() = default;

    void build (int fftSz, float sr, int nBands = 128)
    {
        fftSize = fftSz;
        sampleRate = sr;
        numBands = nBands;
        int numBins = fftSize / 2;

        // Mel edges: numBands + 2 points from 20 Hz to Nyquist
        float melLow = hzToMel (20.0f);
        float melHigh = hzToMel (std::min (20000.0f, sr * 0.49f));
        std::vector<float> melEdges (numBands + 2);
        for (int i = 0; i < numBands + 2; ++i)
            melEdges[i] = melToHz (melLow + (float) i / (float)(numBands + 1) * (melHigh - melLow));

        // Build sparse triangular filters
        filters.resize (numBands);
        for (int m = 0; m < numBands; ++m)
        {
            filters[m].clear();
            float fLow  = melEdges[m];
            float fMid  = melEdges[m + 1];
            float fHigh = melEdges[m + 2];

            int binLow  = std::max (0, (int) std::floor (fLow  / sr * (float) fftSize));
            int binHigh = std::min (numBins - 1, (int) std::ceil (fHigh / sr * (float) fftSize));

            for (int b = binLow; b <= binHigh; ++b)
            {
                float freq = (float) b * sr / (float) fftSize;
                float weight = 0.0f;
                if (freq >= fLow && freq <= fMid && fMid > fLow)
                    weight = (freq - fLow) / (fMid - fLow);
                else if (freq > fMid && freq <= fHigh && fHigh > fMid)
                    weight = (fHigh - freq) / (fHigh - fMid);
                if (weight > 1e-6f)
                    filters[m].push_back ({b, weight});
            }
        }
        built = true;
    }

    void apply (const float* magnitudes, int numBins, float* melBands) const
    {
        for (int m = 0; m < numBands; ++m)
        {
            float sum = 0.0f;
            for (const auto& [bin, weight] : filters[m])
            {
                if (bin < numBins)
                    sum += magnitudes[bin] * magnitudes[bin] * weight;
            }
            melBands[m] = sum;
        }
    }

    bool isBuilt() const { return built; }
    int getNumBands() const { return numBands; }

private:
    static float hzToMel (float hz) { return 2595.0f * std::log10 (1.0f + hz / 700.0f); }
    static float melToHz (float mel) { return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f); }

    int fftSize = 0;
    float sampleRate = 0.0f;
    int numBands = 128;
    bool built = false;
    std::vector<std::vector<std::pair<int, float>>> filters; // [band][{bin, weight}]
};

// Compute log-mel spectrogram from mono audio
// Returns vector of frames, each is numMelBands log-energy values
inline std::vector<std::vector<float>> computeMelSpectrogram (
    const std::vector<float>& mono, float sampleRate,
    int fftSize, int hopSize, const MelFilterbank& fb)
{
    const int numBands = fb.getNumBands();
    const int numSamples = (int) mono.size();
    const int numFrames = std::min (64, std::max (1, (numSamples - fftSize) / hopSize + 1));

    juce::dsp::FFT fft ((int) std::round (std::log2 ((double) fftSize)));

    std::vector<std::vector<float>> result;
    result.reserve (numFrames);

    std::vector<float> fftData (fftSize * 2, 0.0f);
    std::vector<float> magnitudes (fftSize / 2, 0.0f);
    std::vector<float> melBands (numBands, 0.0f);

    for (int frame = 0; frame < numFrames; ++frame)
    {
        int start = frame * hopSize;
        if (start + fftSize > numSamples) break;

        // Windowed FFT (Hann)
        for (int i = 0; i < fftSize; ++i)
        {
            float win = 0.5f * (1.0f - std::cos (2.0f * 3.14159265358979f * (float) i / (float) fftSize));
            fftData[i] = mono[start + i] * win;
        }
        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);

        fft.performRealOnlyForwardTransform (fftData.data());

        int numBins = fftSize / 2;
        for (int b = 0; b < numBins; ++b)
        {
            float re = fftData[b * 2];
            float im = fftData[b * 2 + 1];
            magnitudes[b] = std::sqrt (re * re + im * im);
        }

        // Apply mel filterbank
        fb.apply (magnitudes.data(), numBins, melBands.data());

        // Log scale
        std::vector<float> logMel (numBands);
        for (int m = 0; m < numBands; ++m)
            logMel[m] = std::log (std::max (1e-7f, melBands[m]));

        result.push_back (std::move (logMel));
    }

    return result;
}

// A-weighting from frequency in Hz — used for per-bin spectral weighting
inline float aWeightForFreq (float fc)
{
    if (fc < 10.0f) return 0.0f;
    float f2 = fc * fc;
    float num = 12194.0f * 12194.0f * f2 * f2;
    float den = (f2 + 20.6f * 20.6f) * std::sqrt ((f2 + 107.7f * 107.7f) * (f2 + 737.9f * 737.9f)) * (f2 + 12194.0f * 12194.0f);
    float aW = (den > 0.0f) ? num / den : 0.0f;
    return std::min (1.0f, aW * 1.25f); // normalize so max ~= 1.0 at ~2.5 kHz
}

// Multi-resolution mel spectrogram loss
// Compares ref and gen audio using Spectral Convergence + Log Magnitude distance
// CRITICAL: Pads shorter signal with zeros so duration mismatch is penalized
inline float computeMelSpectrogramLoss (const std::vector<float>& refMono,
                                         const std::vector<float>& genMono,
                                         float sampleRate)
{
    if (refMono.empty() || genMono.empty()) return 100.0f;

    // Pad shorter signal to match longer — silence where audio should exist gets penalized
    int maxLen = std::max ((int) refMono.size(), (int) genMono.size());
    std::vector<float> refPad = refMono;
    std::vector<float> genPad = genMono;
    refPad.resize (maxLen, 0.0f);
    genPad.resize (maxLen, 0.0f);

    // Loudness normalize both signals so spectral SHAPE is compared, not volume
    // RMS loss handles absolute loudness separately
    {
        float refRMS = 0.0f, genRMS = 0.0f;
        for (int i = 0; i < maxLen; ++i)
        {
            refRMS += refPad[i] * refPad[i];
            genRMS += genPad[i] * genPad[i];
        }
        refRMS = std::sqrt (refRMS / (float) maxLen);
        genRMS = std::sqrt (genRMS / (float) maxLen);
        if (refRMS > 1e-8f && genRMS > 1e-8f)
        {
            float gain = refRMS / genRMS;
            for (auto& s : genPad) s *= gain;
        }
    }

    static constexpr int NUM_RES = 3;
    static constexpr int fftSizes[NUM_RES] = { 512, 1024, 2048 };
    static constexpr float resWeights[NUM_RES] = { 0.25f, 0.5f, 0.25f };

    // Thread-local mel filterbanks (built once per FFT size + sample rate)
    thread_local MelFilterbank filterbanks[NUM_RES];
    thread_local float cachedSR = 0.0f;

    if (cachedSR != sampleRate)
    {
        for (int r = 0; r < NUM_RES; ++r)
            filterbanks[r].build (fftSizes[r], sampleRate, 128);
        cachedSR = sampleRate;
    }

    float totalLoss = 0.0f;

    for (int r = 0; r < NUM_RES; ++r)
    {
        int fftSize = fftSizes[r];
        int hopSize = fftSize / 4;
        if (maxLen < fftSize) continue;

        auto refMel = computeMelSpectrogram (refPad, sampleRate, fftSize, hopSize, filterbanks[r]);
        auto genMel = computeMelSpectrogram (genPad, sampleRate, fftSize, hopSize, filterbanks[r]);

        // Use ALL frames from the reference (padded gen will have silence frames)
        int numFrames = std::max ((int) refMel.size(), (int) genMel.size());
        if (numFrames == 0) continue;
        int numBands = filterbanks[r].getNumBands();

        // Create silence frame for missing frames
        std::vector<float> silenceFrame (numBands, std::log (1e-7f));

        float diffNorm = 0.0f, refNorm = 0.0f;
        float logMagSum = 0.0f;
        int totalBins = 0;

        for (int f = 0; f < numFrames; ++f)
        {
            const auto& rv = (f < (int) refMel.size()) ? refMel[f] : silenceFrame;
            const auto& gv = (f < (int) genMel.size()) ? genMel[f] : silenceFrame;

            for (int b = 0; b < numBands; ++b)
            {
                float diff = rv[b] - gv[b];
                diffNorm += diff * diff;
                refNorm += rv[b] * rv[b];
                logMagSum += std::abs (diff);
                ++totalBins;
            }
        }

        float sc = (refNorm > 1e-10f) ? std::sqrt (diffNorm / refNorm) : std::sqrt (diffNorm);
        float lm = (totalBins > 0) ? logMagSum / (float) totalBins : 0.0f;

        totalLoss += resWeights[r] * (sc + lm);
    }

    return totalLoss;
}

// Envelope correlation: Pearson correlation of RMS envelopes at ~1ms resolution
// Uses the REFERENCE length — if gen is shorter, missing frames are treated as silence (0)
inline float computeEnvelopeCorrelation (const std::vector<float>& refMono,
                                          const std::vector<float>& genMono,
                                          float sampleRate)
{
    int hopSamples = std::max (1, (int)(sampleRate * 0.001f)); // 1ms
    // Use reference length as the frame count (gen treated as 0 beyond its end)
    int numFrames = (int) refMono.size() / hopSamples;
    if (numFrames < 4) return 0.0f;

    std::vector<float> refEnv (numFrames), genEnv (numFrames);

    for (int f = 0; f < numFrames; ++f)
    {
        int start = f * hopSamples;
        float refSq = 0.0f, genSq = 0.0f;
        for (int i = 0; i < hopSamples; ++i)
        {
            int idx = start + i;
            float rv = (idx < (int) refMono.size()) ? refMono[idx] : 0.0f;
            float gv = (idx < (int) genMono.size()) ? genMono[idx] : 0.0f;
            refSq += rv * rv;
            genSq += gv * gv;
        }
        refEnv[f] = std::sqrt (refSq / (float) hopSamples);
        genEnv[f] = std::sqrt (genSq / (float) hopSamples);
    }

    // Pearson correlation
    float meanR = 0.0f, meanG = 0.0f;
    for (int f = 0; f < numFrames; ++f) { meanR += refEnv[f]; meanG += genEnv[f]; }
    meanR /= (float) numFrames;
    meanG /= (float) numFrames;

    float cov = 0.0f, varR = 0.0f, varG = 0.0f;
    for (int f = 0; f < numFrames; ++f)
    {
        float dr = refEnv[f] - meanR;
        float dg = genEnv[f] - meanG;
        cov += dr * dg;
        varR += dr * dr;
        varG += dg * dg;
    }

    float denom = std::sqrt (varR * varG);
    return (denom > 1e-10f) ? std::max (0.0f, cov / denom) : 0.0f;
}

// Pitch contour loss: frame-by-frame f0 comparison (only for tonal sounds)
inline float computePitchContourLoss (const std::vector<float>& refMono,
                                       const std::vector<float>& genMono,
                                       float sampleRate, float hnr)
{
    if (hnr < 0.5f) return 0.0f; // not applicable for noisy sounds

    int frameSamples = (int)(sampleRate * 0.010f); // 10ms frames
    int numFrames = std::max (0, (int) refMono.size() / frameSamples - 1);
    numFrames = std::min (numFrames, 20); // cap at 200ms
    if (numFrames < 2) return 0.0f;

    // YIN-based f0 per frame (same algorithm as DescriptorExtractor::estimateF0Autocorrelation)
    auto estimateFrameF0 = [&](const std::vector<float>& mono, int frameStart, int frameLen, float sr) -> float
    {
        int minLag = std::max (2, (int)(sr / 500.0f));
        int maxLag = std::min (frameLen / 2, (int)(sr / 20.0f));
        if (maxLag <= minLag) return 0.0f;

        // YIN difference function + CMND
        std::vector<float> diff (maxLag + 1, 0.0f);
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            float sum = 0.0f;
            for (int i = 0; i < frameLen - tau; ++i)
            {
                float d = mono[frameStart + i] - mono[frameStart + i + tau];
                sum += d * d;
            }
            diff[tau] = sum;
        }

        std::vector<float> cmnd (maxLag + 1, 1.0f);
        float runSum = 0.0f;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            runSum += diff[tau];
            cmnd[tau] = (runSum > 1e-10f) ? diff[tau] * (float) tau / runSum : 1.0f;
        }

        // Find first dip below threshold
        int bestLag = -1;
        for (int tau = minLag; tau <= maxLag; ++tau)
        {
            if (cmnd[tau] < 0.2f)
            {
                while (tau + 1 <= maxLag && cmnd[tau + 1] < cmnd[tau]) ++tau;
                bestLag = tau;
                break;
            }
        }
        if (bestLag < 0) return 0.0f;

        return sr / (float) bestLag;
    };

    float totalError = 0.0f;
    int voicedFrames = 0;

    for (int f = 0; f < numFrames; ++f)
    {
        int start = f * frameSamples;
        float refF0 = estimateFrameF0 (refMono, start, frameSamples, sampleRate);
        // If gen is shorter than this frame, treat as unvoiced (penalty if ref is voiced)
        float genF0 = (start + frameSamples <= (int) genMono.size())
            ? estimateFrameF0 (genMono, start, frameSamples, sampleRate) : 0.0f;

        if (refF0 > 20.0f)
        {
            ++voicedFrames;
            if (genF0 > 20.0f)
            {
                float semitoneError = std::abs (12.0f * std::log2 (refF0 / genF0));
                totalError += std::min (12.0f, semitoneError);
            }
            else
            {
                totalError += 12.0f; // gen has no pitch where ref does = max penalty
            }
        }
    }

    if (voicedFrames == 0) return 0.0f;
    float meanError = totalError / (float) voicedFrames;
    return std::min (1.0f, meanError / 6.0f); // normalize: 6 semitones = 1.0
}

// RMS loudness match — penalizes wrong output level AND wrong duration
// Uses REFERENCE length: gen samples beyond gen length treated as 0
inline float computeRMSLoss (const std::vector<float>& refMono,
                              const std::vector<float>& genMono)
{
    int refLen = (int) refMono.size();
    if (refLen < 1) return 10.0f;

    // Compute RMS over full reference length (gen padded with zeros implicitly)
    float refSq = 0.0f, genSq = 0.0f;
    for (int i = 0; i < refLen; ++i)
    {
        refSq += refMono[i] * refMono[i];
        float gv = (i < (int) genMono.size()) ? genMono[i] : 0.0f;
        genSq += gv * gv;
    }
    float refRMS = std::sqrt (refSq / (float) refLen);
    float genRMS = std::sqrt (genSq / (float) refLen);

    if (refRMS < 1e-6f) return 0.0f;

    float ratio = genRMS / refRMS;
    if (ratio < 0.01f) return 10.0f;
    return std::min (5.0f, std::abs (std::log (std::max (0.01f, ratio))));
}

// 7-resolution LINEAR STFT loss — from transient detail (64) to bass precision (4096)
// Small FFT (64-256): captures transient/attack detail (critical for drums)
// Medium FFT (512-1024): captures pitch and formants
// Large FFT (2048-4096): captures fine spectral detail and bass harmonics
inline float computeLinearSTFTLoss (const std::vector<float>& refMono,
                                     const std::vector<float>& genMono,
                                     float sampleRate)
{
    if (refMono.empty() || genMono.empty()) return 100.0f;

    int maxLen = std::max ((int) refMono.size(), (int) genMono.size());
    std::vector<float> refPad = refMono;
    std::vector<float> genPad = genMono;
    refPad.resize (maxLen, 0.0f);
    genPad.resize (maxLen, 0.0f);

    // Loudness normalize so we compare spectral SHAPE, not volume
    {
        float refRMS = 0.0f, genRMS = 0.0f;
        for (int i = 0; i < maxLen; ++i)
        {
            refRMS += refPad[i] * refPad[i];
            genRMS += genPad[i] * genPad[i];
        }
        refRMS = std::sqrt (refRMS / (float) maxLen);
        genRMS = std::sqrt (genRMS / (float) maxLen);
        if (refRMS > 1e-8f && genRMS > 1e-8f)
        {
            float gain = refRMS / genRMS;
            for (auto& s : genPad) s *= gain;
        }
    }

    // 7 resolutions: transient → bass
    static constexpr int NUM_RES = 7;
    static constexpr int fftOrders[NUM_RES] = { 6, 7, 8, 9, 10, 11, 12 }; // 64,128,256,512,1024,2048,4096
    static constexpr float resWeights[NUM_RES] = { 0.08f, 0.10f, 0.12f, 0.15f, 0.20f, 0.20f, 0.15f };

    float totalLoss = 0.0f;
    float totalWeight = 0.0f;

    for (int r = 0; r < NUM_RES; ++r)
    {
        int fftSize = 1 << fftOrders[r];
        int hopSize = std::max (1, fftSize / 4);
        int numBins = fftSize / 2;

        if (maxLen < fftSize) continue;

        juce::dsp::FFT fft (fftOrders[r]);

        int numFrames = std::max (1, (maxLen - fftSize) / hopSize + 1);
        numFrames = std::min (numFrames, 48);

        float scDiffSq = 0.0f, scRefSq = 0.0f;
        float logMagSum = 0.0f;
        int totalBinCount = 0;

        std::vector<float> refFFT (fftSize * 2, 0.0f);
        std::vector<float> genFFT (fftSize * 2, 0.0f);

        for (int frame = 0; frame < numFrames; ++frame)
        {
            int start = frame * hopSize;
            if (start + fftSize > maxLen) break;

            for (int i = 0; i < fftSize; ++i)
            {
                float win = 0.5f * (1.0f - std::cos (2.0f * 3.14159265358979f * (float) i / (float) fftSize));
                refFFT[i] = refPad[start + i] * win;
                genFFT[i] = genPad[start + i] * win;
            }
            std::fill (refFFT.begin() + fftSize, refFFT.end(), 0.0f);
            std::fill (genFFT.begin() + fftSize, genFFT.end(), 0.0f);

            fft.performRealOnlyForwardTransform (refFFT.data());
            fft.performRealOnlyForwardTransform (genFFT.data());

            for (int b = 1; b < numBins; ++b)
            {
                float refMag = std::sqrt (refFFT[b * 2] * refFFT[b * 2] + refFFT[b * 2 + 1] * refFFT[b * 2 + 1]);
                float genMag = std::sqrt (genFFT[b * 2] * genFFT[b * 2] + genFFT[b * 2 + 1] * genFFT[b * 2 + 1]);

                float refLog = std::log (std::max (1e-7f, refMag));
                float genLog = std::log (std::max (1e-7f, genMag));

                // Mild perceptual weighting: sqrt(A-weight) with high floor
                // Avoids killing sub-bass (critical for kicks/bass) while still
                // boosting perceptually important mid-range (1-5kHz)
                float binFreq = (float) b * sampleRate / (float) fftSize;
                float aW = std::max (0.4f, std::sqrt (aWeightForFreq (binFreq))); // sqrt + floor=0.4

                float diff = refLog - genLog;
                scDiffSq += aW * diff * diff;
                scRefSq += aW * refLog * refLog;
                logMagSum += aW * std::abs (diff);
                ++totalBinCount;
            }
        }

        float sc = (scRefSq > 1e-10f) ? std::sqrt (scDiffSq / scRefSq) : std::sqrt (scDiffSq);
        float lm = (totalBinCount > 0) ? logMagSum / (float) totalBinCount : 0.0f;

        totalLoss += resWeights[r] * (sc + lm);
        totalWeight += resWeights[r];
    }

    return (totalWeight > 0.01f) ? totalLoss / totalWeight : totalLoss;
}

// Time-domain attack waveform loss: L1 shape + energy matching on first 10ms
// Critical for drums — the transient shape AND energy define the "hit" character
inline float computeAttackWaveformLoss (const std::vector<float>& refMono,
                                         const std::vector<float>& genMono,
                                         float sampleRate)
{
    int attackSamples = std::min ((int)(sampleRate * 0.010f), (int) refMono.size()); // 10ms
    if (attackSamples < 8) return 0.0f;

    // Compute RMS of attack regions (unnormalized energy)
    float refSq = 0.0f, genSq = 0.0f;
    float refPeak = 0.0f, genPeak = 0.0f;
    for (int i = 0; i < attackSamples; ++i)
    {
        float rv = std::abs (refMono[i]);
        float gv = (i < (int) genMono.size()) ? std::abs (genMono[i]) : 0.0f;
        refSq += rv * rv;
        genSq += gv * gv;
        refPeak = std::max (refPeak, rv);
        genPeak = std::max (genPeak, gv);
    }
    if (refPeak < 1e-8f) return 0.0f;

    float refRMS = std::sqrt (refSq / (float) attackSamples);
    float genRMS = std::sqrt (genSq / (float) attackSamples);

    // Shape loss: L1 on peak-normalized waveform
    float refScale = 1.0f / refPeak;
    float genScale = (genPeak > 1e-8f) ? 1.0f / genPeak : 1.0f;

    float l1Sum = 0.0f;
    for (int i = 0; i < attackSamples; ++i)
    {
        float rv = refMono[i] * refScale;
        float gv = (i < (int) genMono.size()) ? genMono[i] * genScale : 0.0f;
        l1Sum += std::abs (rv - gv);
    }
    float shapeLoss = l1Sum / (float) attackSamples;

    // Energy loss: relative RMS difference (captures transient energy, not just shape)
    float energyLoss = (refRMS > 1e-8f) ? std::abs (refRMS - genRMS) / refRMS : 0.0f;

    return 0.7f * shapeLoss + 0.3f * std::min (2.0f, energyLoss);
}

// Point-by-point RMS envelope distance (2ms frames) + derivative matching
// Measures actual amplitude values AND slopes for transient fidelity
inline float computeEnvelopeDistance (const std::vector<float>& refMono,
                                      const std::vector<float>& genMono,
                                      float sampleRate)
{
    int hopSamples = std::max (1, (int)(sampleRate * 0.002f)); // 2ms
    int refLen = (int) refMono.size();
    int numFrames = refLen / hopSamples;
    if (numFrames < 2) return 1.0f;

    std::vector<float> refEnv (numFrames), genEnv (numFrames);

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
        refEnv[f] = std::sqrt (refSq);
        genEnv[f] = std::sqrt (genSq);
    }

    // Value loss: L1 on RMS per frame
    float totalDiff = 0.0f, totalRef = 0.0f;
    for (int f = 0; f < numFrames; ++f)
    {
        totalDiff += std::abs (refEnv[f] - genEnv[f]);
        totalRef += refEnv[f];
    }
    float valueLoss = (totalRef > 1e-6f) ? totalDiff / totalRef : totalDiff;

    // Derivative loss: L1 on slope (rewards matching attack/decay shape)
    float derivDiff = 0.0f, derivRef = 0.0f;
    for (int f = 1; f < numFrames; ++f)
    {
        float dRef = refEnv[f] - refEnv[f - 1];
        float dGen = genEnv[f] - genEnv[f - 1];
        derivDiff += std::abs (dRef - dGen);
        derivRef += std::abs (dRef);
    }
    float derivLoss = (derivRef > 1e-6f) ? derivDiff / derivRef : derivDiff;

    return 0.65f * valueLoss + 0.35f * derivLoss;
}

// Composite spectral match loss v6
// 7-resolution STFT + mel + envelope distance + attack waveform + pitch + RMS
inline float computeSpectralMatchLoss (const std::vector<float>& refMono,
                                        const std::vector<float>& genMono,
                                        float sampleRate, float hnr)
{
    float stftLoss = computeLinearSTFTLoss (refMono, genMono, sampleRate);
    float melLoss = computeMelSpectrogramLoss (refMono, genMono, sampleRate);
    float envDist = computeEnvelopeDistance (refMono, genMono, sampleRate);
    float attackLoss = computeAttackWaveformLoss (refMono, genMono, sampleRate);
    float pitchLoss = computePitchContourLoss (refMono, genMono, sampleRate, hnr);
    float rmsLoss = computeRMSLoss (refMono, genMono);

    return 0.30f * stftLoss          // 7-res linear STFT with A-weighting — full spectral
         + 0.25f * melLoss           // mel spectrogram — perceptual timbre
         + 0.20f * envDist           // envelope distance + derivative matching
         + 0.10f * attackLoss        // transient shape + energy
         + 0.05f * pitchLoss         // pitch accuracy (also captured by STFT)
         + 0.10f * rmsLoss;          // loudness (also captured by envelope)
}

// ========== Distance computation ==========
// Weighted multi-region distance between descriptor sets.
// Includes spectrotemporal comparison for precise time-varying spectral matching.
// Includes microTransient comparison for fine transient detail matching.

// A-weighting approximation (perceptual loudness curve)
// Returns a multiplier ~0..1 for how perceptually important a frequency band is
inline float aWeightBand (int bandIdx, int numBands)
{
    // Approximate center frequencies for Mel-spaced bands
    float melLow = 2595.0f * std::log10 (1.0f + 20.0f / 700.0f);
    float melHigh = 2595.0f * std::log10 (1.0f + 18000.0f / 700.0f);
    float melStep = (melHigh - melLow) / (float) numBands;
    float melCenter = melLow + ((float) bandIdx + 0.5f) * melStep;
    float fc = 700.0f * (std::pow (10.0f, melCenter / 2595.0f) - 1.0f);

    // Simplified A-weighting: peak sensitivity around 2-5 kHz, roll off at extremes
    float f2 = fc * fc;
    float num = 12194.0f * 12194.0f * f2 * f2;
    float den = (f2 + 20.6f * 20.6f) * std::sqrt ((f2 + 107.7f * 107.7f) * (f2 + 737.9f * 737.9f)) * (f2 + 12194.0f * 12194.0f);
    float aW = (den > 0.0f) ? num / den : 0.0f;
    // Normalize so max ~= 1.0 (at ~2.5 kHz)
    return std::min (1.0f, aW * 1.25f);
}

// Adaptive distance weight multipliers (all default to 1.0)
struct DistanceWeights
{
    float envelope = 1.0f;
    float pitch = 1.0f;
    float spectral = 1.0f;
    float sub = 1.0f;
    float transient = 1.0f;
    float spectroTemporal = 1.0f;
};

inline float computeDistance (const MatchDescriptors& ref, const MatchDescriptors& gen,
                              const DistanceWeights& w = {})
{
    if (! ref.valid || ! gen.valid) return 999.0f;

    float dist = 0.0f;

    // Helper: normalized squared difference
    auto nsd = [](float a, float b, float scale) -> float
    {
        float d = (a - b) / std::max (scale, 0.001f);
        return d * d;
    };

    // Precompute perceptual band weights
    float bandWeight[NUM_SPECTRAL_BANDS];
    for (int i = 0; i < NUM_SPECTRAL_BANDS; ++i)
        bandWeight[i] = 0.3f + 0.7f * aWeightBand (i, NUM_SPECTRAL_BANDS); // blend: 30% flat + 70% A-weighted

    // === Loudness matching (weight: ~4) — guides optimizer toward correct level ===
    // Using both linear RMS and log-scale (dB) for better gradient at all levels
    dist += 2.5f * w.envelope * nsd (ref.rmsLoudness, gen.rmsLoudness, std::max (0.05f, ref.rmsLoudness * 0.3f));
    dist += 1.5f * w.envelope * nsd (ref.lufs, gen.lufs, 6.0f);  // 6 dB scale

    // === Duration penalty — prevents optimizer from making sound too long/short ===
    dist += 3.0f * w.envelope * nsd (ref.totalDuration, gen.totalDuration, std::max (0.05f, ref.totalDuration * 0.3f));

    // === Global temporal (weight: ~10) — adaptive: envelope ===
    dist += 2.0f * w.envelope * nsd (ref.attackTime, gen.attackTime, std::max (0.002f, ref.attackTime * 0.5f));
    dist += 2.0f * w.envelope * nsd (ref.decayTime, gen.decayTime, std::max (0.005f, ref.decayTime * 0.5f));
    dist += 1.0f * w.envelope * nsd (ref.decayTime40, gen.decayTime40, std::max (0.01f, ref.decayTime40 * 0.5f));
    dist += 1.5f * w.envelope * nsd (ref.transientStrength, gen.transientStrength, std::max (1.0f, ref.transientStrength * 0.3f));
    dist += 0.5f * w.envelope * nsd (ref.envelopeShape, gen.envelopeShape, 0.5f);

    // === Amplitude envelope shape (weight: 6) — adaptive: envelope ===
    {
        float envDist = 0.0f;
        for (int i = 0; i < MatchDescriptors::ENV_POINTS; ++i)
            envDist += (ref.ampEnvelope[i] - gen.ampEnvelope[i]) * (ref.ampEnvelope[i] - gen.ampEnvelope[i]);
        dist += 6.0f * w.envelope * envDist / (float) MatchDescriptors::ENV_POINTS;
    }

    // === Pitch (weight: ~14) — adaptive: pitch ===
    // Boosted — pitch contour is THE difference between a kick and a sub drop
    dist += 4.0f * w.pitch * nsd (ref.fundamentalFreq, gen.fundamentalFreq, std::max (10.0f, ref.fundamentalFreq));
    dist += 3.0f * w.pitch * nsd (ref.pitchDropSemitones, gen.pitchDropSemitones, std::max (6.0f, std::abs (ref.pitchDropSemitones) + 1.0f));
    dist += 3.0f * w.pitch * nsd (ref.pitchDropTime, gen.pitchDropTime, 0.01f);  // tighter scale: 10ms matters hugely
    dist += 2.0f * w.pitch * nsd (ref.pitchStart, gen.pitchStart, std::max (50.0f, ref.pitchStart));

    // === Pitch envelope shape (weight: 6) ===
    // Boosted from 3 — this captures the SPEED of the pitch drop, critical for kick vs sub drop
    {
        float penvDist = 0.0f;
        for (int i = 0; i < MatchDescriptors::PITCH_ENV_POINTS; ++i)
        {
            float scale = std::max (20.0f, ref.pitchEnvelope[i]);
            penvDist += nsd (ref.pitchEnvelope[i], gen.pitchEnvelope[i], scale);
        }
        dist += 6.0f * w.pitch * penvDist / (float) MatchDescriptors::PITCH_ENV_POINTS;
    }

    // === Global spectral (weight: ~12) — adaptive: spectral ===
    // Boosted from 5 — spectral character is the most perceptually salient
    dist += 3.0f * w.spectral * nsd (ref.spectralCentroid, gen.spectralCentroid, std::max (30.0f, ref.spectralCentroid * 0.3f));
    dist += 2.0f * w.spectral * nsd (ref.spectralRolloff, gen.spectralRolloff, std::max (200.0f, ref.spectralRolloff));
    dist += 2.5f * w.spectral * nsd (ref.brightness, gen.brightness, std::max (0.01f, ref.brightness + 0.01f));
    dist += 1.5f * w.spectral * nsd (ref.harmonicNoiseRatio, gen.harmonicNoiseRatio, 0.3f);
    dist += 1.5f * w.spectral * nsd (ref.spectralTilt, gen.spectralTilt, 5.0f);

    // === Energy band distribution (weight: ~14) — adaptive: sub ===
    // subEnergy and lowMidEnergy are THE most perceptually important for kicks
    dist += 4.0f * w.sub * nsd (ref.subEnergy, gen.subEnergy, 0.08f);
    dist += 4.0f * w.spectral * nsd (ref.lowMidEnergy, gen.lowMidEnergy, 0.07f);
    dist += 1.2f * w.spectral * nsd (ref.midEnergy, gen.midEnergy, 0.1f);
    dist += 1.0f * w.spectral * nsd (ref.highEnergy, gen.highEnergy, 0.05f);

    // === Transient region (weight: ~8) — adaptive: transient ===
    // Boosted — transient character is very perceptually distinctive
    dist += 2.0f * w.transient * nsd (ref.transientRegion.spectralCentroid, gen.transientRegion.spectralCentroid, std::max (500.0f, ref.transientRegion.spectralCentroid));
    dist += 1.5f * w.transient * nsd (ref.transientRegion.spectralFlatness, gen.transientRegion.spectralFlatness, 0.3f);
    dist += 1.0f * w.transient * nsd (ref.transientRegion.rmsEnergy, gen.transientRegion.rmsEnergy, std::max (0.05f, ref.transientRegion.rmsEnergy));
    dist += 1.0f * w.transient * nsd (ref.transientRegion.peakLevel, gen.transientRegion.peakLevel, std::max (0.1f, ref.transientRegion.peakLevel));
    {
        float bDist = 0.0f;
        for (int i = 0; i < NUM_SPECTRAL_BANDS; ++i)
        {
            float diff = ref.transientRegion.bandEnergy[i] - gen.transientRegion.bandEnergy[i];
            bDist += diff * diff * bandWeight[i];
        }
        dist += 2.5f * w.transient * bDist;
    }

    // === Body region (weight: ~7) ===
    // Boosted — body is the "meat" of the sound
    dist += 2.0f * w.spectral * nsd (ref.bodyRegion.spectralCentroid, gen.bodyRegion.spectralCentroid, std::max (100.0f, ref.bodyRegion.spectralCentroid));
    dist += 1.5f * w.spectral * nsd (ref.bodyRegion.rmsEnergy, gen.bodyRegion.rmsEnergy, std::max (0.05f, ref.bodyRegion.rmsEnergy));
    dist += 1.5f * w.spectral * nsd (ref.bodyRegion.spectralFlatness, gen.bodyRegion.spectralFlatness, 0.3f);
    {
        float bDist = 0.0f;
        for (int i = 0; i < NUM_SPECTRAL_BANDS; ++i)
        {
            float diff = ref.bodyRegion.bandEnergy[i] - gen.bodyRegion.bandEnergy[i];
            bDist += diff * diff * bandWeight[i];
        }
        dist += 2.5f * w.spectral * bDist;
    }

    // === High-resolution body bands (weight: 6) — finer spectral matching in 50-2000 Hz ===
    // Boosted — this is the most detailed timbral comparison
    {
        float hrDist = 0.0f;
        for (int i = 0; i < NUM_BODY_HIRES_BANDS; ++i)
        {
            float diff = ref.bodyHiResBands[i] - gen.bodyHiResBands[i];
            hrDist += diff * diff;
        }
        dist += 6.0f * w.spectral * hrDist / (float) NUM_BODY_HIRES_BANDS;
    }

    // === Tail region (weight: 3) ===
    dist += 0.8f * nsd (ref.tailRegion.spectralCentroid, gen.tailRegion.spectralCentroid, std::max (50.0f, ref.tailRegion.spectralCentroid));
    dist += 0.7f * nsd (ref.tailRegion.rmsEnergy, gen.tailRegion.rmsEnergy, std::max (0.02f, ref.tailRegion.rmsEnergy));
    {
        float bDist = 0.0f;
        for (int i = 0; i < NUM_SPECTRAL_BANDS; ++i)
        {
            float diff = ref.tailRegion.bandEnergy[i] - gen.tailRegion.bandEnergy[i];
            bDist += diff * diff * bandWeight[i];
        }
        dist += 1.0f * bDist;
    }

    // === Spectral flux between regions (weight: 2) ===
    dist += 1.0f * nsd (ref.bodyRegion.spectralFlux, gen.bodyRegion.spectralFlux, 0.2f);
    dist += 1.0f * nsd (ref.tailRegion.spectralFlux, gen.tailRegion.spectralFlux, 0.2f);

    // === Spectrotemporal matrix (weight: 6) — adaptive: spectroTemporal ===
    // Captures time-varying spectral evolution — perceptually weighted per band
    {
        float stDist = 0.0f;
        for (int f = 0; f < SPECTRO_FRAMES; ++f)
        {
            for (int b = 0; b < SPECTRO_BANDS; ++b)
            {
                int idx = f * SPECTRO_BANDS + b;
                float diff = ref.spectroTemporal[idx] - gen.spectroTemporal[idx];
                stDist += diff * diff * bandWeight[b];
            }
        }
        dist += 8.0f * w.spectroTemporal * stDist / (float) SPECTRO_SIZE;
    }

    // === Micro transient (weight: 5) ===
    // Boosted — fine transient detail distinguishes kick from sub drop
    {
        float mtDist = 0.0f;
        for (int i = 0; i < MatchDescriptors::MICRO_POINTS; ++i)
        {
            float diff = ref.microTransient[i] - gen.microTransient[i];
            mtDist += diff * diff;
        }
        dist += 5.0f * w.transient * mtDist / (float) MatchDescriptors::MICRO_POINTS;
    }

    // === Harmonic profile (weight: 3) — only when both have valid fundamental ===
    if (ref.fundamentalFreq > 20.0f && gen.fundamentalFreq > 20.0f)
    {
        float hpDist = 0.0f;
        for (int i = 0; i < MatchDescriptors::NUM_HARMONICS; ++i)
        {
            float diff = ref.harmonicProfile[i] - gen.harmonicProfile[i];
            hpDist += diff * diff;
        }
        dist += 3.0f * w.spectral * hpDist / (float) MatchDescriptors::NUM_HARMONICS;
    }

    // === Sub harmonic ratio (weight: 1.5) ===
    dist += 1.5f * w.sub * nsd (ref.subHarmonicRatio, gen.subHarmonicRatio, 0.3f);

    // === Noise spectral centroid (weight: 1.0, gated on noise content) ===
    if (ref.harmonicNoiseRatio < 0.8f)
        dist += 1.0f * w.spectral * nsd (ref.noiseSpectralCentroid, gen.noiseSpectralCentroid,
                                          std::max (500.0f, ref.noiseSpectralCentroid));

    // === Spectral crest (weight: 0.8) ===
    dist += 0.8f * w.spectral * nsd (ref.spectralCrest, gen.spectralCrest,
                                      std::max (2.0f, ref.spectralCrest));

    return dist;
}

// ========== Gap analysis for self-improving synth ==========
// Compares ref vs matched descriptors and identifies which extension
// modules would help close the gap.

struct GapAnalysis
{
    // v1 extensions
    bool needsFM            = false;  // Complex harmonic content mismatch
    bool needsResonance     = false;  // Tuned body resonance missing
    bool needsWobble        = false;  // Pitch instability detected
    bool needsTransientSnap = false;  // Transient shape mismatch
    bool needsComb          = false;  // Metallic / comb-like character
    bool needsMultibandSat  = false;  // Spectral tilt / band energy mismatch
    bool needsPhaseDistort  = false;  // Complex waveshape mismatch
    // v2 extensions
    bool needsAdditive      = false;  // Individual harmonic control needed
    bool needsMultiReson    = false;  // Multiple resonance peaks
    bool needsNoiseShape    = false;  // Noise character mismatch
    bool needsEQ            = false;  // Overall spectral shape correction
    bool needsEnvComplex    = false;  // Complex envelope (sustain/release)
    bool needsStereo        = false;  // Stereo content detected
    // v3 extensions
    bool needsUnison        = false;  // Wide body needs detuned unison
    bool needsFormant       = false;  // Mid-band formant shaping needed
    bool needsTransLayer    = false;  // Transient layering needed
    bool needsReverb        = false;  // Reverb tail detected
    // v4 extensions
    bool needsMixControl    = true;   // Always active — mix levels always optimizable
    bool needsFilterSweep   = false;  // Dynamic filter sweep detected
    bool needsSubPitch      = false;  // Independent sub frequency needed
    // formantQ is activated with needsFormant
    // v5 extensions
    bool needsResidual      = false;  // Reference noise residual needed
    bool needsSpectralMatch = false;  // Spectral envelope correction needed
    bool needsTransientCapture = false; // Reference transient sample needed
    // harmonics 5-8 activated with needsAdditive
    // subWavetable activated with wavetable available
    // v6 extensions
    bool needsPitchBounce   = false;  // Non-monotonic pitch envelope detected
    bool needsClickType     = false;  // Transient character mismatch (beyond noise)
    bool needsMasterSat     = false;  // Post-mix saturation needed
    bool needsSubPhase      = false;  // Sub phase coherence optimization
    bool needsCompressor    = false;  // Internal dynamics needed (sustain-like envelope)
    bool needsSubCrossover  = false;  // Sub/body frequency boundary optimization

    // OR-merge another GapAnalysis into this one
    void merge (const GapAnalysis& other)
    {
        needsFM            = needsFM            || other.needsFM;
        needsResonance     = needsResonance     || other.needsResonance;
        needsWobble        = needsWobble        || other.needsWobble;
        needsTransientSnap = needsTransientSnap || other.needsTransientSnap;
        needsComb          = needsComb          || other.needsComb;
        needsMultibandSat  = needsMultibandSat  || other.needsMultibandSat;
        needsPhaseDistort  = needsPhaseDistort  || other.needsPhaseDistort;
        needsAdditive      = needsAdditive      || other.needsAdditive;
        needsMultiReson    = needsMultiReson    || other.needsMultiReson;
        needsNoiseShape    = needsNoiseShape    || other.needsNoiseShape;
        needsEQ            = needsEQ            || other.needsEQ;
        needsEnvComplex    = needsEnvComplex    || other.needsEnvComplex;
        needsStereo        = needsStereo        || other.needsStereo;
        needsUnison        = needsUnison        || other.needsUnison;
        needsFormant       = needsFormant       || other.needsFormant;
        needsTransLayer    = needsTransLayer    || other.needsTransLayer;
        needsReverb        = needsReverb        || other.needsReverb;
        needsMixControl    = true; // always active
        needsFilterSweep   = needsFilterSweep   || other.needsFilterSweep;
        needsSubPitch      = needsSubPitch      || other.needsSubPitch;
        needsResidual      = needsResidual      || other.needsResidual;
        needsSpectralMatch = needsSpectralMatch || other.needsSpectralMatch;
        needsTransientCapture = needsTransientCapture || other.needsTransientCapture;
        needsPitchBounce   = needsPitchBounce   || other.needsPitchBounce;
        needsClickType     = needsClickType     || other.needsClickType;
        needsMasterSat     = needsMasterSat     || other.needsMasterSat;
        needsSubPhase      = needsSubPhase      || other.needsSubPhase;
        needsCompressor    = needsCompressor    || other.needsCompressor;
        needsSubCrossover  = needsSubCrossover  || other.needsSubCrossover;
    }

    int extensionCount() const
    {
        return (int) needsFM + (int) needsResonance + (int) needsWobble +
               (int) needsTransientSnap + (int) needsComb +
               (int) needsMultibandSat + (int) needsPhaseDistort +
               (int) needsAdditive + (int) needsMultiReson +
               (int) needsNoiseShape + (int) needsEQ +
               (int) needsEnvComplex + (int) needsStereo +
               (int) needsUnison + (int) needsFormant +
               (int) needsTransLayer + (int) needsReverb +
               (int) needsMixControl + (int) needsFilterSweep +
               (int) needsSubPitch +
               (int) needsResidual + (int) needsSpectralMatch +
               (int) needsTransientCapture +
               (int) needsPitchBounce + (int) needsClickType +
               (int) needsMasterSat + (int) needsSubPhase +
               (int) needsCompressor + (int) needsSubCrossover;
    }

    // Returns param indices that should be activated for optimization
    // Mapped to UniversalSynthParams 88-param layout:
    //   0-5: Global, 6-13: Pitch, 14-31: Tonal, 32-45: Noise,
    //   46-57: Modal/KS, 58-65: Transient, 66-73: Envelope,
    //   74-81: Filter, 82-87: Effects
    std::vector<int> getExtensionIndices() const
    {
        std::vector<int> indices;
        // FM synthesis (Layer A)
        if (needsFM)            { indices.push_back (16); indices.push_back (17); indices.push_back (18); } // fmDepth, fmRatio, fmDecay
        // Modal resonance (Layer C) — replaces old bodyResonance + comb
        if (needsResonance || needsComb) {
            indices.push_back (46); indices.push_back (47); // modalLevel, modalMode
            indices.push_back (48); indices.push_back (49); // numModes, modeDecay
            indices.push_back (50); indices.push_back (51); // modeSpread, modeRatioBase
            indices.push_back (52);                         // modeDamping
        }
        // Wobble
        if (needsWobble)        { indices.push_back (12); indices.push_back (13); } // pitchWobble, wobbleRate
        // Transient snap (Layer D)
        if (needsTransientSnap) { indices.push_back (58); indices.push_back (63); } // transientLevel, snapAmount
        // Saturation (replaces multibandSat)
        if (needsMultibandSat || needsMasterSat)  { indices.push_back (85); indices.push_back (86); } // satAmount, satType
        // Phase distortion (Layer A)
        if (needsPhaseDistort)  { indices.push_back (28); indices.push_back (29); } // phaseDistort, phaseDistDecay
        // Additive harmonics (Layer A)
        if (needsAdditive)      { indices.push_back (19); indices.push_back (20); indices.push_back (21);
                                  indices.push_back (22); indices.push_back (23); indices.push_back (24); } // additiveAmt, h2-h5, inharmonicity
        // Multi-resonance → more modal modes
        if (needsMultiReson)    { indices.push_back (46); indices.push_back (48); indices.push_back (50); indices.push_back (51); }
        // Noise shaping (Layer B)
        if (needsNoiseShape)    { indices.push_back (32); indices.push_back (33); indices.push_back (34);
                                  indices.push_back (35); indices.push_back (42); } // noiseLevel, color, filterFreq, Q, HP
        // Complex envelope
        if (needsEnvComplex)    { indices.push_back (70); indices.push_back (71);
                                  indices.push_back (72); indices.push_back (73); } // sustain, sustainTime, release, curve
        // Stereo
        if (needsStereo)        { indices.push_back (4); indices.push_back (43); } // stereoWidth, noiseStereo
        // Unison (Layer A)
        if (needsUnison)        { indices.push_back (25); indices.push_back (26); indices.push_back (27); } // unisonVoices, detune, drift
        // Formant (Filter Chain)
        if (needsFormant)       { indices.push_back (79); indices.push_back (80); indices.push_back (81); } // formantAmt, freq1, freq2
        // Transient layer (Layer D)
        if (needsTransLayer)    { indices.push_back (58); indices.push_back (59);
                                  indices.push_back (60); indices.push_back (61); } // transientLevel, clickType, clickFreq, clickDecay
        // Reverb (Effects)
        if (needsReverb)        { indices.push_back (82); indices.push_back (83); } // reverbAmt, reverbDecay
        // Layer gates (always active)
        if (needsMixControl)    { indices.push_back (14); indices.push_back (32);
                                  indices.push_back (46); indices.push_back (58); } // tonalLevel, noiseLevel, modalLevel, transientLevel
        // Filter sweep
        if (needsFilterSweep)   { indices.push_back (76); indices.push_back (77); indices.push_back (78); } // sweepAmt, start, end
        // Sub pitch
        if (needsSubPitch)      { indices.push_back (31); } // subDetune
        // Residual noise (Layer B)
        if (needsResidual)      { indices.push_back (40); indices.push_back (41); } // residualAmt, residualLevel
        // Transient capture (Layer D)
        if (needsTransientCapture) { indices.push_back (64); } // transientSampleAmt
        // Pitch bounce
        if (needsPitchBounce)   { indices.push_back (11); } // pitchBounce
        // Click type
        if (needsClickType)     { indices.push_back (59); } // clickType
        // Compressor
        if (needsCompressor)    { indices.push_back (87); } // compAmount
        // Karplus-Strong (when comb+resonance both needed)
        if (needsComb && needsResonance) {
            indices.push_back (53); indices.push_back (54); // ksFeedback, ksDamping
            indices.push_back (55); indices.push_back (56); // ksBrightness, ksPickPosition
            indices.push_back (57);                         // ksBodyResonance
        }
        // Noise bursts (for clap-like sounds)
        if (needsNoiseShape && needsTransientSnap) {
            indices.push_back (37); indices.push_back (38); // burstCount, burstSpacing
        }
        // Granular (for textures)
        if (needsNoiseShape && needsStereo) {
            indices.push_back (45); indices.push_back (44); // granularDensity, noiseEvolution
        }
        // Chorus
        if (needsStereo || needsUnison) { indices.push_back (84); } // chorusAmt
        // Remove duplicates
        std::sort (indices.begin(), indices.end());
        indices.erase (std::unique (indices.begin(), indices.end()), indices.end());
        return indices;
    }
};

inline GapAnalysis analyzeGaps (const MatchDescriptors& ref, const MatchDescriptors& matched)
{
    GapAnalysis g;
    if (! ref.valid || ! matched.valid) return g;

    // Helper: relative error
    auto relErr = [](float a, float b, float scale) -> float {
        return std::abs (a - b) / std::max (scale, 0.001f);
    };

    // --- FM: harmonic content mismatch ---
    float hnrGap = relErr (ref.harmonicNoiseRatio, matched.harmonicNoiseRatio, 0.2f);
    float bodyFlatGap = relErr (ref.bodyRegion.spectralFlatness, matched.bodyRegion.spectralFlatness, 0.15f);
    if (hnrGap > 0.5f || bodyFlatGap > 0.4f)
        g.needsFM = true;

    // --- Resonance: body spectral centroid/spread mismatch ---
    float bodyCentGap = relErr (ref.bodyRegion.spectralCentroid, matched.bodyRegion.spectralCentroid,
                                std::max (80.0f, ref.bodyRegion.spectralCentroid * 0.15f));
    float bodySpreadGap = relErr (ref.bodyRegion.spectralSpread, matched.bodyRegion.spectralSpread,
                                  std::max (50.0f, ref.bodyRegion.spectralSpread * 0.2f));
    if (bodyCentGap > 0.5f && bodySpreadGap > 0.3f)
        g.needsResonance = true;

    // --- Pitch wobble: pitch envelope shape mismatch beyond simple drop ---
    {
        float pitchEnvErr = 0.0f;
        for (int i = 0; i < MatchDescriptors::PITCH_ENV_POINTS; ++i)
        {
            float scale = std::max (15.0f, ref.pitchEnvelope[i] * 0.1f);
            pitchEnvErr += relErr (ref.pitchEnvelope[i], matched.pitchEnvelope[i], scale);
        }
        pitchEnvErr /= (float) MatchDescriptors::PITCH_ENV_POINTS;
        if (pitchEnvErr > 0.6f)
            g.needsWobble = true;
    }

    // --- Transient snap: transient peak/shape mismatch ---
    float transPeakGap = relErr (ref.transientRegion.peakLevel, matched.transientRegion.peakLevel,
                                 std::max (0.05f, ref.transientRegion.peakLevel * 0.1f));
    float transRMSGap = relErr (ref.transientRegion.rmsEnergy, matched.transientRegion.rmsEnergy,
                                std::max (0.03f, ref.transientRegion.rmsEnergy * 0.15f));
    if (transPeakGap > 0.4f || transRMSGap > 0.5f)
        g.needsTransientSnap = true;

    // --- Comb: high-frequency spectral pattern mismatch ---
    float highBandErr = 0.0f;
    for (int b = 5; b < NUM_SPECTRAL_BANDS; ++b)
    {
        highBandErr += std::abs (ref.transientRegion.bandEnergy[b] - matched.transientRegion.bandEnergy[b]);
        highBandErr += std::abs (ref.bodyRegion.bandEnergy[b] - matched.bodyRegion.bandEnergy[b]);
    }
    if (highBandErr > 0.15f)
        g.needsComb = true;

    // --- Multi-band saturation: spectral tilt or band distribution mismatch ---
    float tiltGap = relErr (ref.spectralTilt, matched.spectralTilt, 3.0f);
    float subGap = relErr (ref.subEnergy, matched.subEnergy, 0.08f);
    float highGap = relErr (ref.highEnergy, matched.highEnergy, 0.03f);
    if (tiltGap > 0.5f || (subGap > 0.4f && highGap > 0.3f))
        g.needsMultibandSat = true;

    // --- Phase distortion: body harmonic complexity beyond waveshaping ---
    float brightGap = relErr (ref.brightness, matched.brightness, std::max (0.01f, ref.brightness));
    if (bodyFlatGap > 0.3f && brightGap > 0.4f && ! g.needsFM)
        g.needsPhaseDistort = true;

    // ========== v2 gap analysis ==========

    // --- Additive: individual harmonic structure needed ---
    // If tonal (low flatness) with harmonic content mismatch and FM alone isn't enough
    if (ref.bodyRegion.spectralFlatness < 0.15f && bodyCentGap > 0.3f && g.needsFM)
        g.needsAdditive = true;
    // Also trigger if harmonic profile shows significant upper harmonics (h3+)
    if (ref.fundamentalFreq > 20.0f)
    {
        float upperHarm = ref.harmonicProfile[2] + ref.harmonicProfile[3]
                        + ref.harmonicProfile[4] + ref.harmonicProfile[5];
        if (upperHarm > 0.2f)
            g.needsAdditive = true;
    }

    // --- Multi-resonance: multiple spectral peaks ---
    // If single resonance activated but body spread still doesn't match
    if (g.needsResonance && bodySpreadGap > 0.5f)
        g.needsMultiReson = true;

    // --- Noise shaping: noise character mismatch ---
    float transFlatGap = relErr (ref.transientRegion.spectralFlatness, matched.transientRegion.spectralFlatness, 0.2f);
    float transZCRGap = relErr (ref.transientRegion.zeroCrossingRate, matched.transientRegion.zeroCrossingRate,
                                std::max (500.0f, ref.transientRegion.zeroCrossingRate * 0.15f));
    if (transFlatGap > 0.3f || transZCRGap > 0.4f)
        g.needsNoiseShape = true;
    // Also trigger from noise centroid when significant aperiodic content exists
    if (ref.noiseSpectralCentroid > 200.0f && ref.harmonicNoiseRatio < 0.6f)
        g.needsNoiseShape = true;

    // --- EQ: overall spectral shape correction (catch-all) ---
    {
        float overallSpecErr = 0.0f;
        overallSpecErr += relErr (ref.subEnergy, matched.subEnergy, 0.1f);
        overallSpecErr += relErr (ref.lowMidEnergy, matched.lowMidEnergy, 0.08f);
        overallSpecErr += relErr (ref.midEnergy, matched.midEnergy, 0.08f);
        overallSpecErr += relErr (ref.highEnergy, matched.highEnergy, 0.04f);
        overallSpecErr /= 4.0f;
        if (overallSpecErr > 0.4f)
            g.needsEQ = true;
    }

    // --- Envelope complexity: sustain/plateau detected ---
    {
        // Check if mid-section of envelope stays high (sustain-like behavior)
        float envMidLevel = 0.0f;
        for (int i = 4; i < 10; ++i)
            envMidLevel += ref.ampEnvelope[i];
        envMidLevel /= 6.0f;

        float envEndLevel = 0.0f;
        for (int i = 12; i < MatchDescriptors::ENV_POINTS; ++i)
            envEndLevel += ref.ampEnvelope[i];
        envEndLevel /= 4.0f;

        if (envMidLevel > 0.3f && envMidLevel > envEndLevel * 3.0f)
        {
            float envErr = 0.0f;
            for (int i = 4; i < MatchDescriptors::ENV_POINTS; ++i)
                envErr += std::abs (ref.ampEnvelope[i] - matched.ampEnvelope[i]);
            envErr /= (float)(MatchDescriptors::ENV_POINTS - 4);
            if (envErr > 0.15f)
                g.needsEnvComplex = true;
        }
    }

    // --- Stereo: L/R decorrelation detected ---
    if (ref.stereoCorrelation < 0.95f)
        g.needsStereo = true;

    // ========== v3 gap analysis ==========

    // --- Unison: wide body spectral spread suggests detuned unison needed ---
    // If ref body has much wider spectral spread than matched, but centroid is close
    if (ref.bodyRegion.spectralSpread > matched.bodyRegion.spectralSpread * 1.5f && bodyCentGap < 0.3f)
        g.needsUnison = true;

    // --- Formant: mid-band shaping needed ---
    // If mid bands (bands 2-4) have significant error and body flatness is in formant range
    {
        float midBandErr = 0.0f;
        for (int b = 2; b <= 4; ++b)
            midBandErr += std::abs (ref.bodyRegion.bandEnergy[b] - matched.bodyRegion.bandEnergy[b]);
        midBandErr /= 3.0f;

        if (midBandErr > 0.15f && ref.bodyRegion.spectralFlatness > 0.05f && ref.bodyRegion.spectralFlatness < 0.4f)
            g.needsFormant = true;
    }

    // --- Transient layer: transient snap needed AND noise character mismatch ---
    if (g.needsTransientSnap && transFlatGap > 0.2f)
        g.needsTransLayer = true;

    // --- Reverb: significant tail energy relative to body with mismatch ---
    {
        float refTailBodyRatio = (ref.bodyRegion.rmsEnergy > 0.001f)
            ? ref.tailRegion.rmsEnergy / ref.bodyRegion.rmsEnergy : 0.0f;
        float matchedTailBodyRatio = (matched.bodyRegion.rmsEnergy > 0.001f)
            ? matched.tailRegion.rmsEnergy / matched.bodyRegion.rmsEnergy : 0.0f;

        if (refTailBodyRatio > 0.15f)
        {
            float ratioRelErr = std::abs (refTailBodyRatio - matchedTailBodyRatio)
                              / std::max (refTailBodyRatio, 0.001f);
            if (ratioRelErr > 0.4f)
                g.needsReverb = true;
        }
    }

    // ========== v4 gap analysis ==========

    // --- Mix control: always active (handled by needsMixControl = true default) ---

    // --- Filter sweep: spectral evolution shows descending brightness over time ---
    {
        // Compare brightness in early vs late spectrotemporal frames
        float earlyBright = 0.0f, lateBright = 0.0f;
        for (int b = 4; b < SPECTRO_BANDS; ++b) // upper half of bands = brightness
        {
            earlyBright += ref.spectroTemporal[0 * SPECTRO_BANDS + b] + ref.spectroTemporal[1 * SPECTRO_BANDS + b];
            lateBright  += ref.spectroTemporal[6 * SPECTRO_BANDS + b] + ref.spectroTemporal[7 * SPECTRO_BANDS + b];
        }
        // If early frames are significantly brighter than late frames → filter sweep character
        if (earlyBright > lateBright * 2.0f && earlyBright > 0.05f)
            g.needsFilterSweep = true;

        // Also activate if spectrotemporal still has large error and brightness changes over time
        float stErr = 0.0f;
        for (int i = 0; i < SPECTRO_SIZE; ++i)
        {
            float diff = ref.spectroTemporal[i] - matched.spectroTemporal[i];
            stErr += diff * diff;
        }
        stErr /= (float) SPECTRO_SIZE;
        if (stErr > 0.03f && earlyBright > lateBright * 1.3f)
            g.needsFilterSweep = true;
    }

    // --- Sub pitch: independent sub frequency needed when sub is prominent
    //     but fundamental doesn't match well ---
    if (ref.subEnergy > 0.2f && subGap > 0.3f)
        g.needsSubPitch = true;

    // ========== v5 gap analysis ==========

    // --- Residual: noise character still doesn't match after noise shaping ---
    if (g.needsNoiseShape && transFlatGap > 0.4f)
        g.needsResidual = true;
    // Also activate for noisy references where synthetic noise can't match
    if (ref.transientRegion.spectralFlatness > 0.4f && transFlatGap > 0.25f)
        g.needsResidual = true;

    // --- Spectral match: overall band energy error still large ---
    {
        float overallBandErr = 0.0f;
        for (int b = 0; b < NUM_SPECTRAL_BANDS; ++b)
        {
            overallBandErr += std::abs (ref.bodyRegion.bandEnergy[b] - matched.bodyRegion.bandEnergy[b]);
            overallBandErr += std::abs (ref.tailRegion.bandEnergy[b] - matched.tailRegion.bandEnergy[b]);
        }
        overallBandErr /= (float)(NUM_SPECTRAL_BANDS * 2);
        if (overallBandErr > 0.06f)
            g.needsSpectralMatch = true;
    }

    // --- Transient capture: transient detail still unmatched ---
    {
        float mtErr = 0.0f;
        for (int i = 0; i < MatchDescriptors::MICRO_POINTS; ++i)
        {
            float diff = ref.microTransient[i] - matched.microTransient[i];
            mtErr += diff * diff;
        }
        mtErr /= (float) MatchDescriptors::MICRO_POINTS;
        if (mtErr > 0.05f && ref.transientStrength > 3.0f)
            g.needsTransientCapture = true;
    }

    // ========== v6 gap analysis ==========

    // --- Pitch bounce: non-monotonic pitch envelope (dip below fundamental) ---
    {
        // Check for non-monotonic behavior in pitch envelope
        bool hasReversal = false;
        for (int i = 2; i < MatchDescriptors::PITCH_ENV_POINTS - 1; ++i)
        {
            // Detect a dip: pitch goes below fundamental then comes back
            if (ref.pitchEnvelope[i] < ref.pitchEnvelope[i + 1] &&
                ref.pitchEnvelope[i] < ref.pitchEnvelope[i - 1])
                hasReversal = true;
        }
        float pitchEnvErr = 0.0f;
        for (int i = 0; i < MatchDescriptors::PITCH_ENV_POINTS; ++i)
        {
            float scale = std::max (15.0f, ref.pitchEnvelope[i] * 0.1f);
            pitchEnvErr += relErr (ref.pitchEnvelope[i], matched.pitchEnvelope[i], scale);
        }
        pitchEnvErr /= (float) MatchDescriptors::PITCH_ENV_POINTS;
        if (hasReversal || pitchEnvErr > 0.8f)
            g.needsPitchBounce = true;
    }

    // --- Click type: transient character mismatch even after noise + snap activated ---
    if (g.needsTransientSnap && transPeakGap > 0.3f)
        g.needsClickType = true;
    // Also if transient spectral centroid is very high (FM-like character)
    if (ref.transientRegion.spectralCentroid > 5000.0f &&
        relErr (ref.transientRegion.spectralCentroid, matched.transientRegion.spectralCentroid,
                ref.transientRegion.spectralCentroid * 0.15f) > 0.4f)
        g.needsClickType = true;

    // --- Master saturation: overall harmonic content/tilt mismatch post-processing ---
    if (tiltGap > 0.6f || (hnrGap > 0.4f && brightGap > 0.3f))
        g.needsMasterSat = true;
    // Also when high-res body bands show consistent error (processed character)
    {
        float hrErr = 0.0f;
        for (int b = 0; b < NUM_BODY_HIRES_BANDS; ++b)
            hrErr += std::abs (ref.bodyHiResBands[b] - matched.bodyHiResBands[b]);
        hrErr /= (float) NUM_BODY_HIRES_BANDS;
        if (hrErr > 0.08f)
            g.needsMasterSat = true;
    }

    // --- Sub phase: sub energy prominent but punch (transient peak) doesn't match ---
    if (ref.subEnergy > 0.15f && transPeakGap > 0.3f)
        g.needsSubPhase = true;

    // --- Compressor: envelope shape suggests dynamics processing ---
    {
        // Compressed sounds have: high mid-envelope relative to peak, less dynamic range
        float envMidLevel = 0.0f;
        for (int i = 3; i < 8; ++i)
            envMidLevel += ref.ampEnvelope[i];
        envMidLevel /= 5.0f;

        // High crest factor in ref but matched has different envelope shape
        float envErr = 0.0f;
        for (int i = 2; i < MatchDescriptors::ENV_POINTS; ++i)
            envErr += std::abs (ref.ampEnvelope[i] - matched.ampEnvelope[i]);
        envErr /= (float)(MatchDescriptors::ENV_POINTS - 2);

        // Compressed sounds: relatively flat sustain portion
        bool looksCompressed = (envMidLevel > 0.25f && ref.transientStrength < 6.0f);
        if (looksCompressed && envErr > 0.1f)
            g.needsCompressor = true;
        // Also activate if envelope error large and sustain/complex already couldn't fix
        if (g.needsEnvComplex && envErr > 0.15f)
            g.needsCompressor = true;
    }

    // --- Sub crossover: sub boundary mismatch ---
    // If sub energy and low-mid energy both have error, crossover point may be wrong
    if (subGap > 0.25f && relErr (ref.lowMidEnergy, matched.lowMidEnergy, 0.08f) > 0.3f)
        g.needsSubCrossover = true;

    return g;
}

} // namespace oneshotmatch
