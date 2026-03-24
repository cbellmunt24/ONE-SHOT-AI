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

        // === Per-region spectral analysis (skip in fast mode — 3 FFTs) ===
        if (! fastMode)
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

        // === Spectrotemporal matrix (skip in fast mode — 8 FFTs) ===
        if (! fastMode)
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

    // ========== Pitch estimation (autocorrelation) ==========

    float estimateF0Autocorrelation (const std::vector<float>& mono, int start, int end, float sr)
    {
        int len = end - start;
        if (len < 32) return 0.0f;

        int minLag = (int)(sr / 500.0f);
        int maxLag = std::min (len / 2, (int)(sr / 20.0f));
        if (maxLag <= minLag) return 0.0f;

        float bestCorr = -1.0f;
        int bestLag = minLag;

        for (int lag = minLag; lag <= maxLag; ++lag)
        {
            float corr = 0.0f, norm1 = 0.0f, norm2 = 0.0f;
            int corrLen = len - lag;
            for (int i = 0; i < corrLen; ++i)
            {
                float a = mono[start + i];
                float b = mono[start + i + lag];
                corr += a * b;
                norm1 += a * a;
                norm2 += b * b;
            }

            float denom = std::sqrt (norm1 * norm2);
            if (denom > 0.0f)
            {
                float normalized = corr / denom;
                if (normalized > bestCorr)
                {
                    bestCorr = normalized;
                    bestLag = lag;
                }
            }
        }

        if (bestCorr < 0.3f) return 0.0f;

        // Parabolic interpolation for sub-sample accuracy
        if (bestLag > minLag && bestLag < maxLag)
        {
            auto corrAt = [&](int lag) -> float
            {
                float c = 0.0f, n1 = 0.0f, n2 = 0.0f;
                int cLen = len - lag;
                for (int i = 0; i < cLen; ++i)
                {
                    float a = mono[start + i];
                    float b = mono[start + i + lag];
                    c += a * b; n1 += a * a; n2 += b * b;
                }
                float d = std::sqrt (n1 * n2);
                return (d > 0.0f) ? c / d : 0.0f;
            };

            float y0 = corrAt (bestLag - 1);
            float y1 = bestCorr;
            float y2 = corrAt (bestLag + 1);
            float shift = 0.5f * (y0 - y2) / (y0 - 2.0f * y1 + y2 + 1e-10f);
            float refinedLag = (float) bestLag + std::max (-0.5f, std::min (0.5f, shift));
            return sr / refinedLag;
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
    std::vector<int> getExtensionIndices() const
    {
        std::vector<int> indices;
        // v1
        if (needsFM)            { indices.push_back (22); indices.push_back (23); indices.push_back (24); }
        if (needsResonance)     { indices.push_back (25); indices.push_back (26); }
        if (needsWobble)        { indices.push_back (27); indices.push_back (28); indices.push_back (29); }
        if (needsTransientSnap) { indices.push_back (30); indices.push_back (31); }
        if (needsComb)          { indices.push_back (32); indices.push_back (33); indices.push_back (34); }
        if (needsMultibandSat)  { indices.push_back (35); indices.push_back (36); }
        if (needsPhaseDistort)  { indices.push_back (37); indices.push_back (38); }
        // v2
        if (needsAdditive)      { for (int i = 39; i <= 44; ++i) indices.push_back (i); }
        if (needsMultiReson)    { for (int i = 45; i <= 48; ++i) indices.push_back (i); }
        if (needsNoiseShape)    { for (int i = 49; i <= 52; ++i) indices.push_back (i); }
        if (needsEQ)            { for (int i = 53; i <= 57; ++i) indices.push_back (i); }
        if (needsEnvComplex)    { for (int i = 58; i <= 61; ++i) indices.push_back (i); }
        if (needsStereo)        { for (int i = 62; i <= 63; ++i) indices.push_back (i); }
        // v3
        if (needsUnison)        { indices.push_back (64); indices.push_back (65); indices.push_back (66); }
        if (needsFormant)       { indices.push_back (67); indices.push_back (68); indices.push_back (69); }
        if (needsTransLayer)    { indices.push_back (70); indices.push_back (71); indices.push_back (72); }
        if (needsReverb)        { indices.push_back (73); indices.push_back (74); indices.push_back (75); }
        // v4
        if (needsMixControl)    { for (int i = 76; i <= 79; ++i) indices.push_back (i); }
        if (needsFilterSweep)   { for (int i = 80; i <= 83; ++i) indices.push_back (i); }
        if (needsSubPitch)      { indices.push_back (84); }
        if (needsFormant)       { indices.push_back (85); } // formantQ activated with formant
        // v5
        if (needsResidual)          { indices.push_back (86); indices.push_back (87); }
        if (needsAdditive)          { for (int i = 88; i <= 91; ++i) indices.push_back (i); } // h5-h8 with additive
        if (needsSpectralMatch)     { indices.push_back (92); }
        // subWavetable (93) activated when wavetable is available (handled by optimizer)
        if (needsTransientCapture)  { indices.push_back (94); }
        // v6
        if (needsPitchBounce)  { indices.push_back (95); indices.push_back (96); }
        if (needsClickType)    { indices.push_back (97); }
        if (needsMasterSat)    { indices.push_back (98); indices.push_back (99); }
        if (needsSubPhase)     { indices.push_back (100); }
        if (needsCompressor)   { for (int i = 101; i <= 104; ++i) indices.push_back (i); }
        if (needsSubCrossover) { indices.push_back (105); }
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
