#pragma once

#include <JuceHeader.h>
#include <cmath>
#include <random>
#include <algorithm>
#include "UniversalSynthParams.h"
#include "ModalBank.h"
#include "../DSP/DSPConstants.h"
#include "../DSP/Oscillator.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Envelope.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/DelayLine.h"
#include "../DSP/Saturator.h"
#include "../DSP/DCBlocker.h"
#include "../SynthEngine/SynthUtils.h"

namespace universalsynth
{

// ==========================================================================
// UniversalSynth — Universal one-shot synthesizer
//
// Architecture: 4 parallel layers mixed into shared filter/effects chain
//   Layer A (Tonal):     oscillators + FM + additive + unison + sub
//   Layer B (Noise):     colored noise + bursts + granular + residual
//   Layer C (Modal/KS):  modal resonators + Karplus-Strong
//   Layer D (Transient): clicks + snaps + impulses + transient samples
//
// Signal flow:
//   [A] + [B] + [C] + [D] → Mixer → Filter → Formant → Stereo → FX → Dynamics
//
// Used by BOTH the generator (via GenreRules) and the match system (via optimizer).
// ==========================================================================

class UniversalSynth
{
public:
    // Set optional side-channel data (from reference audio in match mode)
    void setWavetable (const WavetableData* wt)             { wavetable = wt; }
    void setResidualNoise (const ResidualNoiseData* rn)     { residualNoise = rn; }
    void setTransientSample (const TransientSampleData* ts) { transientSample = ts; }
    void setHarmonicPhases (const HarmonicPhaseData* hp)    { harmonicPhases = hp; }
    void setSpectralEnvelope (const SpectralEnvelopeData* se) { spectralEnvelope = se; }

    // Main generate function — renders a complete one-shot from parameters
    juce::AudioBuffer<float> generate (const UniversalSynthParams& p, double sampleRate,
                                       unsigned int seed = 0)
    {
        const float sr = (float) sampleRate;

        // Deterministic RNG
        std::mt19937 rng (seed);
        std::uniform_real_distribution<float> randDist (-1.0f, 1.0f);

        // === Compute duration ===
        float totalDuration = p.duration;
        if (p.reverbAmt > 0.01f)
            totalDuration += p.reverbDecay * p.reverbAmt * 0.5f;
        totalDuration = std::min (totalDuration, 6.0f);
        int numSamples = std::max (64, (int) (totalDuration * sr));

        // === Create stereo buffer (reuse pre-allocated member to avoid heap churn) ===
        if (mBuffer.getNumChannels() < 2 || mBuffer.getNumSamples() < numSamples)
            mBuffer.setSize (2, std::max (numSamples, 44100)); // alloc once, reuse
        mBuffer.clear (0, numSamples);
        auto& buffer = mBuffer;

        // === Detect which layers are active (for CPU gating) ===
        const bool layerA = p.tonalLevel > 0.005f;
        const bool layerB = p.noiseLevel > 0.005f;
        const bool layerC = p.modalLevel > 0.005f;
        const bool layerD = p.transientLevel > 0.005f;
        const bool hasSub = layerA && p.subLevel > 0.005f;
        const bool hasUnison = layerA && p.unisonVoices > 0.01f;
        const bool hasFM = layerA && p.fmDepth > 0.005f;
        const bool hasAdditive = layerA && p.additiveAmt > 0.005f;
        const bool hasPD = layerA && p.phaseDistort > 0.005f;
        const bool hasBursts = layerB && p.burstCount > 1.5f;
        const bool hasGranular = layerB && p.granularDensity > 0.01f;
        const bool hasResidual = layerB && p.residualAmt > 0.005f && residualNoise && residualNoise->valid;
        const bool isKS = layerC && p.modalMode >= 0.5f;
        const bool isModal = layerC && p.modalMode < 0.5f;
        const bool hasSnap = layerD && p.snapAmount > 0.005f;
        const bool hasTransientSample = layerD && p.transientSampleAmt > 0.005f
                                        && transientSample && transientSample->valid;
        const bool hasFilterSweep = p.filterSweepAmt > 0.005f;
        const bool hasFormant = p.formantAmt > 0.005f;
        const bool hasReverb = p.reverbAmt > 0.005f;
        const bool hasChorus = p.chorusAmt > 0.005f;
        const bool hasSat = p.satAmount > 0.005f;
        const bool hasComp = p.compAmount > 0.005f;

        // === Initialize DSP ===
        dsputil::Oscillator bodyOsc;
        bodyOsc.reset();
        bodyOsc.setSeed (seed);

        dsputil::Oscillator subOsc;
        subOsc.reset();

        dsputil::Oscillator fmModOsc;
        fmModOsc.reset();

        // Unison oscillators (up to 8)
        static constexpr int MAX_UNISON = 8;
        dsputil::Oscillator unisonOscs[MAX_UNISON];
        float unisonPans[MAX_UNISON] = {};
        int numUnisonVoices = 1;

        if (hasUnison)
        {
            numUnisonVoices = 2 + (int) (p.unisonVoices * 6.0f); // 2-8
            numUnisonVoices = std::min (numUnisonVoices, MAX_UNISON);
            for (int v = 0; v < numUnisonVoices; ++v)
            {
                unisonOscs[v].reset ((float) v / (float) numUnisonVoices);
                unisonPans[v] = 0.5f + ((float) v / (float) (numUnisonVoices - 1) - 0.5f)
                                * p.stereoWidth;
            }
        }

        // Noise generator
        dsputil::NoiseGenerator noiseGen;
        noiseGen.setSeed (seed + 1);

        // Noise filter (bandpass)
        dsputil::SVFilter noiseBP;
        noiseBP.setParameters (p.noiseFilterFreq, p.noiseFilterQ,
                               FilterType::BandPass, sr);
        dsputil::OnePole noiseHPFilter;
        noiseHPFilter.setHighPass (p.noiseHP, sr);

        // Modal bank (uses pre-allocated member)
        if (layerC)
        {
            prepareDsp (sr);
            mModalBank.reset();

            if (isModal)
            {
                mModalBank.configure (p.basePitch, (int) p.numModes, p.modeSpread,
                                      p.modeRatioBase, p.modeDamping, p.modeDecay, sr);
            }
            else // KS
            {
                mModalBank.configureKS (p.basePitch, p.ksFeedback, p.ksDamping,
                                        p.ksBrightness, p.ksPickPosition,
                                        p.ksBodyResonance, sr);
            }
        }

        // Transient noise generator
        dsputil::NoiseGenerator clickNoise;
        clickNoise.setSeed (seed + 2);

        // Main filter
        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.filterCutoff, p.filterReso,
                                  FilterType::LowPass, sr);

        // Filter sweep
        dsputil::SVFilter sweepFilter;
        if (hasFilterSweep)
            sweepFilter.setParameters (p.filterSweepStart, p.filterReso,
                                       FilterType::LowPass, sr);

        // Formant filters (2 parallel bandpasses)
        dsputil::SVFilter formant1, formant2;
        if (hasFormant)
        {
            formant1.setParameters (p.formantFreq1, 0.7f, FilterType::BandPass, sr);
            formant2.setParameters (p.formantFreq2, 0.6f, FilterType::BandPass, sr);
        }

        // Saturator
        dsputil::Saturator saturator;

        // DC blocker
        dsputil::DCBlocker dcBlockerL, dcBlockerR;

        // Compressor state
        float compGain = 1.0f;
        float compEnvLevel = 0.0f;

        // Reverb state (uses pre-allocated member delay lines)
        float reverbFeedback = 0.0f;

        if (hasReverb)
        {
            prepareDsp (sr);
            reverbFeedback = std::min (0.95f, 0.3f + p.reverbDecay * 0.4f);
            for (int i = 0; i < NUM_REVERB_LINES; ++i)
            {
                mReverbLines[i].clear();
                mReverbDamp[i].setLowPass (2000.0f + (1.0f - p.reverbDecay * 0.5f) * 8000.0f, sr);
            }
        }

        // Chorus state (uses pre-allocated member delay line)
        float chorusPhase = 0.0f;
        if (hasChorus)
        {
            prepareDsp (sr);
            mChorusDelay.clear();
            mChorusDelay.clear();
        }

        // Granular state
        float grainTimer = 0.0f;
        float grainEnv = 0.0f;
        float grainPhase = 0.0f;

        // =====================================================================
        // MAIN RENDER LOOP
        // =====================================================================
        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;
            float tNorm = (float) i / (float) numSamples; // 0..1

            // === AMPLITUDE ENVELOPE ===
            float ampEnv = computeAmpEnvelope (t, p);

            // Early exit if envelope is dead
            if (ampEnv < 0.00001f && t > (p.ampAttack + p.ampPunchDecay + 0.01f))
            {
                // Check if reverb tail is still active
                if (! hasReverb || t > totalDuration * 0.95f)
                    break;
            }

            // === PITCH ENGINE ===
            float currentFreq = computePitchEnvelope (t, p, sr, rng);

            // === LAYER A: TONAL ===
            float tonalOut = 0.0f;
            float tonalOutR = 0.0f; // for stereo unison

            if (layerA)
            {
                // Phase distortion
                float pdAmount = 0.0f;
                if (hasPD)
                {
                    float pdEnv = std::exp (-t / std::max (p.phaseDistDecay, 0.001f));
                    pdAmount = p.phaseDistort * pdEnv;
                }

                // FM modulation
                float fmMod = 0.0f;
                if (hasFM)
                {
                    float fmEnv = std::exp (-t / std::max (p.fmDecay, 0.001f));
                    float fmFreq = currentFreq * p.fmRatio;
                    fmModOsc.setFrequency (fmFreq, sr);
                    fmMod = fmModOsc.next (OscillatorType::Sine) * p.fmDepth * fmEnv * currentFreq;
                }

                float oscFreq = currentFreq + fmMod;

                if (hasUnison)
                {
                    // Multi-voice unison
                    float detuneCents = p.unisonDetune;
                    float sumL = 0.0f, sumR = 0.0f;

                    for (int v = 0; v < numUnisonVoices; ++v)
                    {
                        // Spread detune symmetrically
                        float detuneOffset = ((float) v / (float) (numUnisonVoices - 1) - 0.5f) * 2.0f;
                        float drift = 0.0f;
                        if (p.unisonDrift > 0.01f)
                            drift = std::sin (t * (2.0f + (float) v * 0.7f)) * p.unisonDrift * 5.0f;

                        float voiceFreq = oscFreq * std::pow (2.0f, (detuneOffset * detuneCents + drift) / 1200.0f);
                        unisonOscs[v].setFrequency (voiceFreq, sr);

                        float sample = generateOscSample (unisonOscs[v], p, pdAmount);

                        float panL, panR;
                        synthutil::panMonoToStereo (sample, unisonPans[v], panL, panR);
                        sumL += panL;
                        sumR += panR;
                    }

                    float normFactor = 1.0f / std::sqrt ((float) numUnisonVoices);
                    tonalOut = sumL * normFactor;
                    tonalOutR = sumR * normFactor;
                }
                else
                {
                    // Single voice
                    bodyOsc.setFrequency (oscFreq, sr);
                    tonalOut = generateOscSample (bodyOsc, p, pdAmount);
                    tonalOutR = tonalOut;
                }

                // Additive harmonics
                if (hasAdditive)
                {
                    float additiveSum = 0.0f;
                    float harmonicLevels[4] = { p.harmonic2, p.harmonic3, p.harmonic4, p.harmonic5 };

                    for (int h = 0; h < 4; ++h)
                    {
                        if (harmonicLevels[h] < 0.005f) continue;

                        float hNum = (float) (h + 2);
                        float inharm = 1.0f + p.inharmonicity * hNum * hNum;
                        float hFreq = currentFreq * hNum * inharm;

                        if (hFreq > sr * 0.45f) continue;

                        // Use harmonic phases from reference if available
                        float phase = 0.0f;
                        if (harmonicPhases && harmonicPhases->valid && h + 1 < harmonicPhases->numHarmonics)
                            phase = harmonicPhases->phases[h + 1];

                        float hSample = std::sin (dsputil::TWO_PI * hFreq * t + phase);
                        additiveSum += hSample * harmonicLevels[h];
                    }

                    tonalOut += additiveSum * p.additiveAmt;
                    tonalOutR += additiveSum * p.additiveAmt;
                }

                // Body harmonics (waveshaping richness)
                if (p.bodyHarmonics > 0.01f)
                {
                    float drive = p.bodyHarmonics * 3.0f;
                    tonalOut = std::tanh (tonalOut * (1.0f + drive)) / std::tanh (1.0f + drive);
                    tonalOutR = std::tanh (tonalOutR * (1.0f + drive)) / std::tanh (1.0f + drive);
                }

                // Sub oscillator
                if (hasSub)
                {
                    float subFreq = currentFreq * std::pow (2.0f, p.subDetune / 12.0f);
                    subFreq = std::max (subFreq, 15.0f);
                    subOsc.setFrequency (subFreq, sr);
                    float subSample = subOsc.next (OscillatorType::Sine);

                    // Sub envelope: slightly longer than body
                    float subEnv = computeSubEnvelope (t, p);
                    subSample *= subEnv * p.subLevel;

                    tonalOut += subSample;
                    tonalOutR += subSample;
                }

                // Apply tonal level
                tonalOut *= p.tonalLevel;
                tonalOutR *= p.tonalLevel;
            }

            // === LAYER B: NOISE ===
            float noiseOutL = 0.0f;
            float noiseOutR = 0.0f;

            if (layerB)
            {
                float noiseSample = 0.0f;

                if (hasGranular)
                {
                    // Granular noise mode
                    float grainRate = p.granularDensity * 200.0f; // up to 200 grains/sec
                    grainTimer += grainRate / sr;

                    if (grainTimer >= 1.0f)
                    {
                        grainTimer -= 1.0f;
                        grainEnv = 1.0f;
                        grainPhase = randDist (rng) * 0.5f; // random start
                    }

                    if (grainEnv > 0.001f)
                    {
                        // Grain window (raised cosine)
                        float grainDur = p.noiseDecay * 0.1f;
                        grainEnv *= std::exp (-6.9f / std::max (grainDur * sr, 1.0f));

                        noiseSample = noiseGen.nextColored (p.noiseColor) * grainEnv;

                        // Optional pitched grain
                        if (p.tonalLevel > 0.01f)
                        {
                            grainPhase += currentFreq / sr;
                            if (grainPhase >= 1.0f) grainPhase -= 1.0f;
                            noiseSample += std::sin (dsputil::TWO_PI * grainPhase) * grainEnv * 0.5f;
                        }
                    }
                }
                else if (hasBursts)
                {
                    // Multi-burst mode (for claps)
                    int numBursts = (int) p.burstCount;
                    float spacing = p.burstSpacing;

                    for (int b = 0; b < numBursts; ++b)
                    {
                        float burstOnset = (float) b * spacing;
                        float timeSinceBurst = t - burstOnset;

                        if (timeSinceBurst >= 0.0f)
                        {
                            float burstEnv = std::exp (-timeSinceBurst / std::max (p.noiseDecay, 0.001f));

                            // Add attack ramp for first burst
                            if (b == 0 && p.noiseAttack > 0.0001f && timeSinceBurst < p.noiseAttack)
                                burstEnv *= timeSinceBurst / p.noiseAttack;

                            noiseSample += noiseGen.nextColored (p.noiseColor) * burstEnv;
                        }
                    }

                    noiseSample /= std::sqrt ((float) numBursts); // normalize
                }
                else
                {
                    // Continuous noise
                    noiseSample = noiseGen.nextColored (p.noiseColor);

                    // Noise envelope
                    float noiseEnv = 1.0f;
                    if (p.noiseAttack > 0.0001f && t < p.noiseAttack)
                        noiseEnv = t / p.noiseAttack;
                    noiseEnv *= std::exp (-t / std::max (p.noiseDecay, 0.001f));

                    noiseSample *= noiseEnv;
                }

                // Spectral evolution: drift the noise filter
                float evolvedFilterFreq = p.noiseFilterFreq;
                if (p.noiseEvolution > 0.01f)
                {
                    float drift = std::sin (t * 3.0f) * p.noiseEvolution * 0.3f;
                    evolvedFilterFreq *= (1.0f + drift);
                    evolvedFilterFreq = std::max (100.0f, std::min (evolvedFilterFreq, sr * 0.45f));
                    noiseBP.setParameters (evolvedFilterFreq, p.noiseFilterQ,
                                           FilterType::BandPass, sr);
                }

                // Apply bandpass and highpass
                noiseSample = noiseBP.process (noiseSample);
                noiseSample = noiseHPFilter.process (noiseSample);

                // Residual noise from reference
                if (hasResidual)
                {
                    int resIdx = (int) ((float) i * residualNoise->sampleRate / sr);
                    if (resIdx < (int) residualNoise->residual.size())
                    {
                        float resEnv = (resIdx < (int) residualNoise->envelope.size())
                            ? residualNoise->envelope[(size_t) resIdx] : 1.0f;
                        noiseSample += residualNoise->residual[(size_t) resIdx]
                                       * p.residualAmt * p.residualLevel * resEnv;
                    }
                }

                // Stereo decorrelation
                noiseOutL = noiseSample;
                noiseOutR = noiseSample;
                if (p.noiseStereo > 0.01f)
                {
                    float noise2 = noiseGen.nextColored (p.noiseColor);
                    noiseOutR = noiseSample * (1.0f - p.noiseStereo) + noise2 * p.noiseStereo;
                }

                noiseOutL *= p.noiseLevel;
                noiseOutR *= p.noiseLevel;
            }

            // === LAYER C: MODAL / KARPLUS-STRONG ===
            float modalOut = 0.0f;

            if (layerC)
            {
                if (isModal)
                {
                    // Excitation: impulse at start, then silence
                    float excitation = 0.0f;
                    if (i == 0)
                        excitation = 1.0f;
                    else if (i < (int) (sr * 0.002f)) // 2ms noise burst excitation
                        excitation = noiseGen.nextColored (0.7f) * (1.0f - (float) i / (sr * 0.002f));

                    // Feed noise from Layer B as continuous excitation (if both active)
                    if (layerB && p.noiseLevel > 0.01f)
                        excitation += (noiseOutL + noiseOutR) * 0.1f;

                    modalOut = mModalBank.processModal (excitation);
                }
                else // KS
                {
                    modalOut = mModalBank.processKS();
                }

                modalOut *= p.modalLevel;
            }

            // === LAYER D: TRANSIENT ===
            float transOut = 0.0f;

            if (layerD)
            {
                // Click envelope (very fast decay)
                float clickEnv = std::exp (-t / std::max (p.clickDecay, 0.00001f));

                if (clickEnv > 0.001f)
                {
                    float clickSample = 0.0f;

                    switch (p.clickType)
                    {
                        case 0: // Noise burst
                        {
                            float noise = clickNoise.nextColored (0.8f + p.clickWidth * 0.2f);
                            clickSample = noise;
                            break;
                        }
                        case 1: // Impulse
                        {
                            clickSample = (i == 0) ? 1.0f : 0.0f;
                            break;
                        }
                        case 2: // FM burst
                        {
                            float fmBurst = std::sin (dsputil::TWO_PI * p.clickFreq * t
                                + 4.0f * clickEnv * std::sin (dsputil::TWO_PI * p.clickFreq * 2.17f * t));
                            clickSample = fmBurst;
                            break;
                        }
                        case 3: // Chirp (pitch sweep)
                        {
                            float chirpFreq = p.clickFreq * (1.0f + clickEnv * 3.0f);
                            clickSample = std::sin (dsputil::TWO_PI * chirpFreq * t);
                            break;
                        }
                    }

                    transOut = clickSample * clickEnv * p.clickWidth;
                }

                // Snap: ultra-short impulse
                if (hasSnap)
                {
                    float snapEnv = (t < 0.0003f) ? 1.0f :
                                    std::exp (-(t - 0.0003f) / 0.0002f);
                    if (snapEnv > 0.001f)
                        transOut += clickNoise.next() * snapEnv * p.snapAmount;
                }

                // Top noise: broadband attack burst
                if (p.topNoise > 0.005f)
                {
                    float topEnv = std::exp (-t / 0.003f);
                    if (topEnv > 0.001f)
                        transOut += clickNoise.next() * topEnv * p.topNoise;
                }

                // Transient sample from reference
                if (hasTransientSample)
                {
                    int tsIdx = (int) ((float) i * transientSample->sampleRate / sr);
                    if (tsIdx < (int) transientSample->samples.size())
                        transOut += transientSample->samples[(size_t) tsIdx] * p.transientSampleAmt;
                }

                transOut *= p.transientLevel;
            }

            // === MIX ALL LAYERS ===
            float mixL = tonalOut + noiseOutL + modalOut + transOut;
            float mixR = (hasUnison ? tonalOutR : tonalOut) + noiseOutR + modalOut + transOut;

            // Apply master amplitude envelope
            mixL *= ampEnv;
            mixR *= ampEnv;

            // === FILTER CHAIN ===
            mixL = mainFilter.process (mixL);
            // Re-apply same filter state for R would be wrong — use mono filter for now
            // TODO: stereo filter if needed
            mixR = mixL + (mixR - mixL) * p.stereoWidth; // blend stereo difference

            // Filter sweep
            if (hasFilterSweep)
            {
                float sweepFreq = p.filterSweepStart + (p.filterSweepEnd - p.filterSweepStart) * tNorm;
                sweepFilter.setParameters (sweepFreq, p.filterReso + p.filterSweepAmt * 0.3f,
                                           FilterType::LowPass, sr);
                float swept = sweepFilter.process (mixL);
                mixL = mixL * (1.0f - p.filterSweepAmt) + swept * p.filterSweepAmt;
                mixR = mixR * (1.0f - p.filterSweepAmt) + swept * p.filterSweepAmt;
            }

            // Formant filters
            if (hasFormant)
            {
                float f1 = formant1.process (mixL);
                float f2 = formant2.process (mixL);
                float formantMix = (f1 + f2) * 0.7f;
                mixL = mixL * (1.0f - p.formantAmt) + formantMix * p.formantAmt;
                mixR = mixR * (1.0f - p.formantAmt) + formantMix * p.formantAmt;
            }

            // === EFFECTS ===

            // Chorus
            if (hasChorus)
            {
                chorusPhase += 1.5f / sr; // 1.5 Hz LFO
                if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;

                float modDelay = 8.0f + 3.5f * std::sin (dsputil::TWO_PI * chorusPhase); // ms
                float delaySamples = modDelay * sr / 1000.0f;

                mChorusDelay.write (mixL);
                float chorusSample = mChorusDelay.readHermite (delaySamples);

                mixL = mixL * (1.0f - p.chorusAmt * 0.5f) + chorusSample * p.chorusAmt * 0.5f;
                mixR = mixR * (1.0f - p.chorusAmt * 0.5f) + chorusSample * p.chorusAmt * 0.5f;
            }

            // Reverb (simple FDN)
            if (hasReverb)
            {
                float reverbIn = (mixL + mixR) * 0.5f * p.reverbAmt * 0.3f;
                float reverbOut = 0.0f;

                for (int r = 0; r < NUM_REVERB_LINES; ++r)
                {
                    float delayed = mReverbLines[r].read (REVERB_DELAYS_MEMBER[r]);
                    float damped = mReverbDamp[r].process (delayed);
                    mReverbLines[r].write (reverbIn + damped * reverbFeedback);
                    reverbOut += delayed;
                }
                reverbOut *= 0.25f;

                mixL += reverbOut * p.reverbAmt;
                mixR += reverbOut * p.reverbAmt;
            }

            // === DYNAMICS ===

            // Saturation
            if (hasSat)
            {
                auto satMode = dsputil::SaturationMode::SoftClip;
                if (p.satType == 1) satMode = dsputil::SaturationMode::Tape;
                else if (p.satType == 2) satMode = dsputil::SaturationMode::Tube;

                mixL = saturator.process (mixL, p.satAmount, satMode);
                mixR = saturator.process (mixR, p.satAmount, satMode);
            }

            // Compressor (simple feed-forward)
            if (hasComp)
            {
                float absLevel = std::max (std::abs (mixL), std::abs (mixR));
                float threshold = 1.0f - p.compAmount * 0.7f;
                float ratio = 2.0f + p.compAmount * 8.0f;

                if (absLevel > threshold)
                {
                    float overDb = 20.0f * std::log10 (absLevel / threshold);
                    float targetGainDb = overDb * (1.0f - 1.0f / ratio);
                    float targetGain = std::pow (10.0f, -targetGainDb / 20.0f);

                    // Smooth gain changes
                    float attackCoeff = std::exp (-1.0f / (0.001f * sr));
                    float releaseCoeff = std::exp (-1.0f / (0.05f * sr));
                    float coeff = (targetGain < compGain) ? attackCoeff : releaseCoeff;
                    compGain = targetGain + coeff * (compGain - targetGain);
                }
                else
                {
                    float releaseCoeff = std::exp (-1.0f / (0.05f * sr));
                    compGain = 1.0f + releaseCoeff * (compGain - 1.0f);
                }

                mixL *= compGain;
                mixR *= compGain;
            }

            // DC blocking
            mixL = dcBlockerL.process (mixL);
            mixR = dcBlockerR.process (mixR);

            // Master gain
            mixL *= p.masterGain;
            mixR *= p.masterGain;

            // Stereo width (mid-side)
            if (p.stereoWidth > 0.01f && !hasUnison)
            {
                float mid = (mixL + mixR) * 0.5f;
                float side = (mixL - mixR) * 0.5f;
                side *= (1.0f + p.stereoWidth);
                mixL = mid + side;
                mixR = mid - side;
            }

            // Pan
            if (std::abs (p.pan - 0.5f) > 0.01f)
            {
                float panL = std::cos (p.pan * dsputil::PI * 0.5f);
                float panR = std::sin (p.pan * dsputil::PI * 0.5f);
                float mono = (mixL + mixR) * 0.5f;
                mixL = mono * panL;
                mixR = mono * panR;
            }

            // Hard clip safety
            mixL = std::max (-1.5f, std::min (mixL, 1.5f));
            mixR = std::max (-1.5f, std::min (mixR, 1.5f));

            buffer.setSample (0, i, mixL);
            buffer.setSample (1, i, mixR);
        }

        // === POST-PROCESSING ===
        int fadeSamples = std::max (32, (int) (0.005f * sr));
        synthutil::applyFadeOut (buffer, fadeSamples);

        // NaN/Inf safety
        sanitizeBuffer (buffer);

        // Return a correctly-sized copy (mBuffer may be larger than needed)
        juce::AudioBuffer<float> result (2, numSamples);
        for (int ch = 0; ch < 2; ++ch)
            result.copyFrom (ch, 0, buffer, ch, 0, numSamples);
        return result;
    }

private:
    // Side-channel data pointers (not owned)
    const WavetableData* wavetable = nullptr;
    const ResidualNoiseData* residualNoise = nullptr;
    const TransientSampleData* transientSample = nullptr;
    const HarmonicPhaseData* harmonicPhases = nullptr;
    const SpectralEnvelopeData* spectralEnvelope = nullptr;

    // Pre-allocated buffer to avoid heap allocation per generate() call
    juce::AudioBuffer<float> mBuffer;

    // Pre-allocated DSP objects to avoid heap allocation in generate()
    static constexpr int NUM_REVERB_LINES = 4;
    static constexpr int REVERB_DELAYS_MEMBER[NUM_REVERB_LINES] = { 1557, 1617, 1491, 1422 };
    dsputil::DelayLine mReverbLines[NUM_REVERB_LINES];
    dsputil::OnePole mReverbDamp[NUM_REVERB_LINES];
    dsputil::DelayLine mChorusDelay;
    ModalBank mModalBank;
    bool mDspPrepared = false;

    void prepareDsp (float sr)
    {
        if (mDspPrepared) return;
        for (int i = 0; i < NUM_REVERB_LINES; ++i)
            mReverbLines[i].setSize (REVERB_DELAYS_MEMBER[i] + 4);
        mChorusDelay.setSize ((int) (sr * 0.05f));
        mModalBank.prepare (sr);
        mDspPrepared = true;
    }

    // Generate one oscillator sample with waveform selection + phase distortion
    float generateOscSample (dsputil::Oscillator& osc, const UniversalSynthParams& p,
                             float pdAmount)
    {
        // Wavetable mode
        if (p.oscType == 12 && wavetable && wavetable->valid && wavetable->numFrames > 0)
        {
            float phase = osc.getPhase();
            int frameIdx = 0; // Could morph across frames based on time
            int sampleIdx = (int) (phase * (float) WavetableData::FRAME_SIZE)
                            % WavetableData::FRAME_SIZE;
            float sample = wavetable->frames[(size_t) frameIdx][(size_t) sampleIdx];
            osc.advancePhase();
            return sample;
        }

        // Noise oscillator
        if (p.oscType == 13)
        {
            osc.advancePhase();
            return osc.next (OscillatorType::Noise);
        }

        // Standard waveforms with optional phase distortion
        float phase = osc.getPhase();

        if (pdAmount > 0.001f)
        {
            // Phase distortion: non-linear phase mapping
            phase = phase + pdAmount * std::sin (dsputil::TWO_PI * phase) * 0.3f;
            phase = phase - std::floor (phase); // wrap
        }

        float sample = 0.0f;

        switch (p.oscType)
        {
            case 0:  sample = std::sin (dsputil::TWO_PI * phase); break; // Sine
            case 1:  sample = osc.next (OscillatorType::Triangle); return sample; // Triangle (AA)
            case 2:  sample = osc.next (OscillatorType::Saw); return sample; // Saw (AA)
            case 3:  sample = osc.next (OscillatorType::Square); return sample; // Square (AA)
            case 4:  sample = osc.nextPWM (0.25f); osc.advancePhase(); return sample; // Pulse 25%
            case 5:  sample = osc.nextPWM (0.125f); osc.advancePhase(); return sample; // Pulse 12.5%
            case 6:  // Half-rectified sine
                sample = std::sin (dsputil::TWO_PI * phase);
                sample = (sample > 0.0f) ? sample : 0.0f;
                break;
            case 7:  // Absolute sine
                sample = std::abs (std::sin (dsputil::TWO_PI * phase)) * 2.0f - 1.0f;
                break;
            case 8:  // Parabolic
                sample = 1.0f - 4.0f * (phase - 0.5f) * (phase - 0.5f);
                break;
            case 9:  // Staircase (4 levels)
                sample = std::floor (phase * 4.0f) / 2.0f - 0.75f;
                break;
            case 10: // Double sine
                sample = std::sin (dsputil::TWO_PI * phase) + std::sin (dsputil::TWO_PI * phase * 2.0f) * 0.5f;
                sample *= 0.667f;
                break;
            case 11: // Clipped sine
            {
                float s = std::sin (dsputil::TWO_PI * phase);
                sample = std::max (-0.6f, std::min (s, 0.6f)) / 0.6f;
                break;
            }
            default: sample = std::sin (dsputil::TWO_PI * phase); break;
        }

        osc.advancePhase();
        return sample;
    }

    // Compute multi-stage amplitude envelope
    static float computeAmpEnvelope (float t, const UniversalSynthParams& p)
    {
        float env = 0.0f;

        // Attack phase
        if (t < p.ampAttack)
        {
            env = t / std::max (p.ampAttack, 0.00001f);
            // Curved attack
            if (p.envCurve > 1.01f)
                env = std::pow (env, 1.0f / p.envCurve);
            return env;
        }

        float tAfterAttack = t - p.ampAttack;

        // Punch decay phase
        float punchEnv = std::exp (-tAfterAttack / std::max (p.ampPunchDecay, 0.001f));
        // Body decay phase (slower)
        float bodyEnv = std::exp (-tAfterAttack / std::max (p.ampBodyDecay, 0.001f));

        // Mix punch and body
        env = punchEnv * p.ampPunchLevel + bodyEnv * (1.0f - p.ampPunchLevel * 0.5f);

        // Sustain plateau
        if (p.envSustainLevel > 0.005f && p.envSustainTime > 0.005f)
        {
            float sustainStart = p.ampPunchDecay * 2.0f;
            if (tAfterAttack > sustainStart && tAfterAttack < sustainStart + p.envSustainTime)
            {
                env = std::max (env, p.envSustainLevel);
            }
            else if (tAfterAttack >= sustainStart + p.envSustainTime)
            {
                // Release from sustain
                float tRelease = tAfterAttack - sustainStart - p.envSustainTime;
                float releaseEnv = p.envSustainLevel * std::exp (-tRelease / std::max (p.envRelease, 0.001f));
                env = std::max (env, releaseEnv);
            }
        }

        // Envelope curve shaping
        if (p.envCurve > 1.01f || p.envCurve < 0.99f)
            env = std::pow (std::max (env, 0.0f), p.envCurve);

        return std::max (0.0f, std::min (env, 1.0f));
    }

    // Compute pitch envelope with hold, bounce, and wobble
    static float computePitchEnvelope (float t, const UniversalSynthParams& p,
                                       float sr, std::mt19937& /*rng*/)
    {
        float freq = p.basePitch;

        if (p.pitchEnvDepth < 0.1f && p.pitchWobble < 0.005f)
            return freq;

        // Pitch envelope: multi-stage decay
        if (p.pitchEnvDepth > 0.1f)
        {
            float pitchT = t;

            // Hold: plateau before pitch drops
            if (p.pitchHoldTime > 0.0001f)
            {
                if (t < p.pitchHoldTime)
                    pitchT = 0.0f;
                else
                    pitchT = t - p.pitchHoldTime;
            }

            // Two-stage exponential decay
            float fastEnv = std::exp (-pitchT / std::max (p.pitchEnvFast, 0.0001f));
            float slowEnv = std::exp (-pitchT / std::max (p.pitchEnvSlow, 0.001f));
            float pitchEnv = fastEnv * p.pitchEnvBalance + slowEnv * (1.0f - p.pitchEnvBalance);

            // Bounce: damped oscillation
            if (p.pitchBounce > 0.01f)
            {
                float bounceFreq = 1.0f / std::max (p.pitchEnvSlow, 0.01f) * 0.5f;
                float bounceOsc = std::cos (dsputil::TWO_PI * bounceFreq * pitchT);
                float bounceDamp = std::exp (-pitchT / std::max (p.pitchEnvSlow * 2.0f, 0.01f));
                pitchEnv += bounceOsc * bounceDamp * p.pitchBounce * 0.3f;
                pitchEnv = std::max (0.0f, pitchEnv);
            }

            float pitchSemitones = p.pitchEnvDepth * pitchEnv;
            freq = p.basePitch * std::pow (2.0f, pitchSemitones / 12.0f);
        }

        // Wobble: periodic LFO on pitch
        if (p.pitchWobble > 0.005f)
        {
            float wobble = std::sin (dsputil::TWO_PI * p.wobbleRate * t) * p.pitchWobble;
            freq *= std::pow (2.0f, wobble / 12.0f);
        }

        return std::max (15.0f, freq);
    }

    // Sub oscillator envelope (longer than body)
    static float computeSubEnvelope (float t, const UniversalSynthParams& p)
    {
        float attackTime = p.ampAttack * 1.5f;
        if (t < attackTime)
            return t / std::max (attackTime, 0.00001f);

        float tAfterAttack = t - attackTime;
        float subDecay = p.ampBodyDecay * 1.5f;
        return std::exp (-tAfterAttack / std::max (subDecay, 0.01f));
    }

    // NaN/Inf safety
    static void sanitizeBuffer (juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                if (std::isnan (data[i]) || std::isinf (data[i]))
                    data[i] = 0.0f;
                data[i] = std::max (-1.5f, std::min (data[i], 1.5f));
            }
        }
    }
};

} // namespace universalsynth
