#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <thread>
#include <functional>
#include <map>
#include "OneShotMatchDescriptors.h"
#include "OneShotMatchSynth.h"
#include "OneShotMatchOptimizer.h"

// Top-level engine for the OneShotMatch system.
//
// Orchestrates: load → analyze → optimize → result.
// Runs optimization on a background thread.
// Persists match results to disk for learning and dataset building.
// Warm-starts from previous match results when available.

namespace oneshotmatch
{

enum class MatchState
{
    Idle,
    Analyzing,
    Matching,
    Done,
    Error
};

// Stored match result (persisted to JSON)
struct MatchRecord
{
    juce::String referenceFile;
    MatchSynthParams params;
    float distance;
    int iterations;
    bool converged;
    juce::String timestamp;
};

struct MatchHistoryEntry
{
    // Key descriptors for similarity search (normalized)
    float fundamentalFreq = 0.0f;
    float attackTime = 0.0f;
    float decayTime = 0.0f;
    float spectralCentroid = 0.0f;
    float brightness = 0.0f;
    float subEnergy = 0.0f;
    float transientStrength = 0.0f;

    MatchSynthParams params;
    float finalDistance = 999.0f;
    int bestOscType = 0;

    // Which extensions were activated
    bool usedFM = false, usedResonance = false, usedAdditive = false, usedFormant = false;
    bool usedFilterSweep = false, usedResidual = false, usedSpectralMatch = false;
    bool usedTransientCapture = false, usedComb = false, usedPhaseDistort = false;

    static MatchHistoryEntry fromDescriptors (const MatchDescriptors& d, const MatchSynthParams& p)
    {
        MatchHistoryEntry e;
        e.fundamentalFreq = d.fundamentalFreq;
        e.attackTime = d.attackTime;
        e.decayTime = d.decayTime;
        e.spectralCentroid = d.spectralCentroid;
        e.brightness = d.brightness;
        e.subEnergy = d.subEnergy;
        e.transientStrength = d.transientStrength;
        e.params = p;
        return e;
    }

    static MatchHistoryEntry fromResult (const MatchDescriptors& d, const MatchSynthParams& p,
                                          const OptimizationResult& r)
    {
        auto e = fromDescriptors (d, p);
        e.finalDistance = r.bestDistance;
        e.bestOscType = p.oscType;
        e.usedFM = r.gaps.needsFM;
        e.usedResonance = r.gaps.needsResonance;
        e.usedAdditive = r.gaps.needsAdditive;
        e.usedFormant = r.gaps.needsFormant;
        e.usedFilterSweep = r.gaps.needsFilterSweep;
        e.usedResidual = r.gaps.needsResidual;
        e.usedSpectralMatch = r.gaps.needsSpectralMatch;
        e.usedTransientCapture = r.gaps.needsTransientCapture;
        e.usedComb = r.gaps.needsComb;
        e.usedPhaseDistort = r.gaps.needsPhaseDistort;
        return e;
    }

    float distanceTo (const MatchHistoryEntry& other) const
    {
        auto nsd = [](float a, float b, float scale) -> float {
            float d = (a - b) / std::max (scale, 0.001f);
            return d * d;
        };
        return nsd (fundamentalFreq, other.fundamentalFreq, 50.0f)
             + nsd (attackTime, other.attackTime, 0.005f)
             + nsd (decayTime, other.decayTime, 0.1f)
             + nsd (spectralCentroid, other.spectralCentroid, 500.0f)
             + nsd (brightness, other.brightness, 0.1f)
             + nsd (subEnergy, other.subEnergy, 0.2f)
             + nsd (transientStrength, other.transientStrength, 3.0f);
    }
};

class OneShotMatchEngine
{
public:
    OneShotMatchEngine()
    {
        // Locate match data directory
        auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
        matchDataDir = appData.getChildFile ("ONE-SHOT AI").getChildFile ("OneShotMatch");
        matchDataDir.createDirectory();
    }

    ~OneShotMatchEngine()
    {
        cancelMatch();
    }

    // === Reference loading ===

    bool loadReference (const juce::File& file)
    {
        juce::AudioFormatManager formatManager;
        formatManager.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
        if (reader == nullptr)
        {
            lastError = "Could not read audio file: " + file.getFullPathName();
            state.store (MatchState::Error);
            return false;
        }

        referenceBuffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&referenceBuffer, 0, (int) reader->lengthInSamples, 0, true, true);
        referenceSampleRate = reader->sampleRate;
        referenceFile = file;

        // Reset previous results
        matchedBuffer.setSize (0, 0);
        hasPreviousBest = false;
        state.store (MatchState::Idle);
        return true;
    }

    bool loadReference (const juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        if (buffer.getNumSamples() < 64)
        {
            lastError = "Buffer too short";
            state.store (MatchState::Error);
            return false;
        }

        referenceBuffer = buffer;
        referenceSampleRate = sampleRate;
        referenceFile = juce::File();
        matchedBuffer.setSize (0, 0);
        hasPreviousBest = false;
        state.store (MatchState::Idle);
        return true;
    }

    // === Analysis ===

    bool analyzeReference()
    {
        if (referenceBuffer.getNumSamples() < 64)
        {
            lastError = "No reference loaded";
            state.store (MatchState::Error);
            return false;
        }

        state.store (MatchState::Analyzing);
        refDescriptors = extractor.extract (referenceBuffer, referenceSampleRate);

        if (! refDescriptors.valid)
        {
            lastError = "Descriptor extraction failed";
            state.store (MatchState::Error);
            return false;
        }

        // === Extract side-channel data from reference ===
        extractSideChannelData();

        // Check if we have a previous match for this file to use as seed
        loadPreviousBest();

        state.store (MatchState::Idle);
        return true;
    }

    // === Matching ===

    void startMatch()
    {
        if (! refDescriptors.valid) return;

        cancelMatch();

        shouldCancel.store (false);
        state.store (MatchState::Matching);
        currentIteration.store (0);
        currentDistance.store (999.0f);

        matchThread = std::thread ([this]()
        {
            OneShotMatchOptimizer optimizer;
            optimizer.setMaxIterations (maxIterations);
            optimizer.setTargetDistance (targetDistance);
            optimizer.setPopulationSize (populationSize);
            optimizer.setWavetable (&refWavetable);
            optimizer.setResidualNoise (&refResidual);
            optimizer.setTransientSample (&refTransient);
            optimizer.setSpectralEnvelope (&refSpectralEnvelope);
            optimizer.setHarmonicPhases (&refHarmonicPhases);

            // Build learned profile from match history (adaptive weights + bounds + extensions)
            LearnedProfile learned = buildLearnedProfile (refDescriptors);
            if (learned.valid)
                optimizer.setLearnedProfile (&learned);

            auto progressCallback = [this] (int iter, float dist, int total) -> bool
            {
                currentIteration.store (iter);
                currentDistance.store (dist);
                return ! shouldCancel.load();
            };

            // Global learning: find K-nearest previous matches as seeds
            auto globalSeeds = findNearestSeeds (refDescriptors, 3);

            // Warm-start: pass previous best or best global seed
            const MatchSynthParams* seed = nullptr;
            MatchSynthParams bestGlobalSeed;
            if (hasPreviousBest)
            {
                seed = &previousBestParams;
            }
            else if (! globalSeeds.empty())
            {
                bestGlobalSeed = globalSeeds[0];
                seed = &bestGlobalSeed;
            }

            auto result = optimizer.optimize (refDescriptors, referenceSampleRate, progressCallback, seed);

            if (shouldCancel.load())
            {
                state.store (MatchState::Idle);
                return;
            }

            bestResult = result;
            bestParams = result.bestParams;

            // Generate final matched one-shot (with side-channel data)
            OneShotMatchSynth synth;
            synth.setWavetable (&refWavetable);
            synth.setResidualNoise (&refResidual);
            synth.setTransientSample (&refTransient);
            synth.setSpectralEnvelope (&refSpectralEnvelope);
            synth.setHarmonicPhases (&refHarmonicPhases);
            matchedBuffer = synth.generate (bestParams, referenceSampleRate);

            // Extract descriptors for comparison display
            matchedDescriptors = extractor.extract (matchedBuffer, referenceSampleRate);

            // Persist match result
            saveMatchResult();

            state.store (MatchState::Done);
        });
    }

    void cancelMatch()
    {
        shouldCancel.store (true);
        if (matchThread.joinable())
            matchThread.join();
    }

    // === Export ===

    bool exportAudio (const juce::File& file) const
    {
        if (matchedBuffer.getNumSamples() == 0) return false;

        file.deleteFile();
        auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream());
        if (stream == nullptr) return false;

        juce::WavAudioFormat wav;
        auto* rawStream = stream.get();
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (rawStream, referenceSampleRate,
                                 (unsigned int) matchedBuffer.getNumChannels(), 24, {}, 0));
        if (writer == nullptr) return false;

        stream.release();
        writer->writeFromAudioSampleBuffer (matchedBuffer, 0, matchedBuffer.getNumSamples());
        return true;
    }

    juce::String exportPresetJSON() const
    {
        auto& p = bestParams;
        juce::String json = "{\n";

        float arr[MatchSynthParams::NUM_PARAMS];
        p.toArray (arr);

        for (int i = 0; i < MatchSynthParams::NUM_PARAMS; ++i)
        {
            json += "  \"" + juce::String (MatchSynthParams::getParamName (i)) + "\": ";

            if (i == 0) // oscType is int
                json += juce::String ((int) arr[i]);
            else
                json += juce::String (arr[i], 6);

            if (i < MatchSynthParams::NUM_PARAMS - 1) json += ",";
            json += "\n";
        }

        json += "}";
        return json;
    }

    // === Getters ===

    MatchState getState() const          { return state.load(); }
    int  getIteration() const            { return currentIteration.load(); }
    float getDistance() const             { return currentDistance.load(); }
    int  getMaxIterations() const        { return maxIterations; }
    const juce::String& getError() const { return lastError; }
    bool hasReference() const            { return referenceBuffer.getNumSamples() > 0; }
    bool hasResult() const               { return matchedBuffer.getNumSamples() > 0; }

    const MatchDescriptors& getRefDescriptors() const     { return refDescriptors; }
    const MatchDescriptors& getMatchedDescriptors() const { return matchedDescriptors; }
    const MatchSynthParams& getBestParams() const         { return bestParams; }
    const OptimizationResult& getOptResult() const        { return bestResult; }

    const juce::AudioBuffer<float>& getReferenceBuffer() const { return referenceBuffer; }
    const juce::AudioBuffer<float>& getMatchedBuffer() const   { return matchedBuffer; }
    double getSampleRate() const                               { return referenceSampleRate; }
    const juce::File& getReferenceFile() const                 { return referenceFile; }
    const juce::File& getMatchDataDir() const                  { return matchDataDir; }

    // === Global learning: K-Nearest-Neighbors seed lookup ===

    std::vector<MatchSynthParams> findNearestSeeds (const MatchDescriptors& refDesc, int k = 3)
    {
        loadMatchHistory();

        std::vector<MatchSynthParams> seeds;
        if (matchHistory.empty()) return seeds;

        MatchHistoryEntry query;
        query.fundamentalFreq = refDesc.fundamentalFreq;
        query.attackTime = refDesc.attackTime;
        query.decayTime = refDesc.decayTime;
        query.spectralCentroid = refDesc.spectralCentroid;
        query.brightness = refDesc.brightness;
        query.subEnergy = refDesc.subEnergy;
        query.transientStrength = refDesc.transientStrength;

        // Compute distances to all history entries
        std::vector<std::pair<float, int>> distances;
        for (int i = 0; i < (int) matchHistory.size(); ++i)
            distances.push_back ({query.distanceTo (matchHistory[i]), i});

        std::sort (distances.begin(), distances.end());

        int count = std::min (k, (int) distances.size());
        for (int i = 0; i < count; ++i)
            seeds.push_back (matchHistory[distances[i].second].params);

        return seeds;
    }

    // === Settings ===
    void setMaxIterations (int n) { maxIterations = n; }
    void setTargetDistance (float d) { targetDistance = d; }
    void setPopulationSize (int n) { populationSize = n; }

    // === JSON serialization for descriptors ===

    juce::String descriptorsToJSON (const MatchDescriptors& d) const
    {
        juce::String j = "{";
        j += "\"valid\":" + juce::String (d.valid ? "true" : "false") + ",";

        // Global temporal
        j += "\"totalDuration\":" + juce::String (d.totalDuration, 4) + ",";
        j += "\"attackTime\":" + juce::String (d.attackTime, 5) + ",";
        j += "\"decayTime\":" + juce::String (d.decayTime, 4) + ",";
        j += "\"decayTime40\":" + juce::String (d.decayTime40, 4) + ",";
        j += "\"transientStrength\":" + juce::String (d.transientStrength, 2) + ",";
        j += "\"envelopeShape\":" + juce::String (d.envelopeShape, 3) + ",";

        // Pitch
        j += "\"fundamentalFreq\":" + juce::String (d.fundamentalFreq, 2) + ",";
        j += "\"pitchStart\":" + juce::String (d.pitchStart, 2) + ",";
        j += "\"pitchEnd\":" + juce::String (d.pitchEnd, 2) + ",";
        j += "\"pitchDropSemitones\":" + juce::String (d.pitchDropSemitones, 2) + ",";
        j += "\"pitchDropTime\":" + juce::String (d.pitchDropTime, 4) + ",";

        // Spectral
        j += "\"spectralCentroid\":" + juce::String (d.spectralCentroid, 2) + ",";
        j += "\"spectralRolloff\":" + juce::String (d.spectralRolloff, 2) + ",";
        j += "\"brightness\":" + juce::String (d.brightness, 4) + ",";
        j += "\"harmonicNoiseRatio\":" + juce::String (d.harmonicNoiseRatio, 3) + ",";
        j += "\"spectralTilt\":" + juce::String (d.spectralTilt, 2) + ",";

        // Energy bands
        j += "\"subEnergy\":" + juce::String (d.subEnergy, 4) + ",";
        j += "\"lowMidEnergy\":" + juce::String (d.lowMidEnergy, 4) + ",";
        j += "\"midEnergy\":" + juce::String (d.midEnergy, 4) + ",";
        j += "\"highEnergy\":" + juce::String (d.highEnergy, 4) + ",";

        // Perceptual
        j += "\"rmsLoudness\":" + juce::String (d.rmsLoudness, 4) + ",";
        j += "\"lufs\":" + juce::String (d.lufs, 2) + ",";

        // Regions (summary)
        j += "\"transientCentroid\":" + juce::String (d.transientRegion.spectralCentroid, 1) + ",";
        j += "\"transientFlatness\":" + juce::String (d.transientRegion.spectralFlatness, 3) + ",";
        j += "\"transientPeak\":" + juce::String (d.transientRegion.peakLevel, 3) + ",";
        j += "\"bodyCentroid\":" + juce::String (d.bodyRegion.spectralCentroid, 1) + ",";
        j += "\"bodyFlatness\":" + juce::String (d.bodyRegion.spectralFlatness, 3) + ",";
        j += "\"tailCentroid\":" + juce::String (d.tailRegion.spectralCentroid, 1) + ",";
        j += "\"tailRMS\":" + juce::String (d.tailRegion.rmsEnergy, 4);

        j += "}";
        return j;
    }

    juce::String paramsToJSON (const MatchSynthParams& p) const
    {
        float arr[MatchSynthParams::NUM_PARAMS];
        p.toArray (arr);

        juce::String j = "{";
        for (int i = 0; i < MatchSynthParams::NUM_PARAMS; ++i)
        {
            if (i > 0) j += ",";
            j += "\"" + juce::String (MatchSynthParams::getParamName (i)) + "\":";
            if (i == 0) j += juce::String ((int) arr[i]);
            else j += juce::String (arr[i], 6);
        }
        j += "}";
        return j;
    }

    juce::String sensitivityToJSON () const
    {
        juce::String j = "{";
        for (int i = 0; i < MatchSynthParams::NUM_PARAMS; ++i)
        {
            if (i > 0) j += ",";
            j += "\"" + juce::String (MatchSynthParams::getParamName (i)) + "\":" +
                 juce::String (bestResult.sensitivity[i], 3);
        }
        j += "}";
        return j;
    }

    juce::String gapAnalysisToJSON () const
    {
        auto& g = bestResult.gaps;
        juce::String j = "{";
        j += "\"extensionsActivated\":" + juce::String (bestResult.extensionsActivated) + ",";
        j += "\"phase1Iterations\":" + juce::String (bestResult.phase1Iterations) + ",";
        j += "\"phase1Distance\":" + juce::String (bestResult.phase1Distance, 4) + ",";
        // v1 extensions
        j += "\"needsFM\":" + juce::String (g.needsFM ? "true" : "false") + ",";
        j += "\"needsResonance\":" + juce::String (g.needsResonance ? "true" : "false") + ",";
        j += "\"needsWobble\":" + juce::String (g.needsWobble ? "true" : "false") + ",";
        j += "\"needsTransientSnap\":" + juce::String (g.needsTransientSnap ? "true" : "false") + ",";
        j += "\"needsComb\":" + juce::String (g.needsComb ? "true" : "false") + ",";
        j += "\"needsMultibandSat\":" + juce::String (g.needsMultibandSat ? "true" : "false") + ",";
        j += "\"needsPhaseDistort\":" + juce::String (g.needsPhaseDistort ? "true" : "false") + ",";
        // v2 extensions
        j += "\"needsAdditive\":" + juce::String (g.needsAdditive ? "true" : "false") + ",";
        j += "\"needsMultiReson\":" + juce::String (g.needsMultiReson ? "true" : "false") + ",";
        j += "\"needsNoiseShape\":" + juce::String (g.needsNoiseShape ? "true" : "false") + ",";
        j += "\"needsEQ\":" + juce::String (g.needsEQ ? "true" : "false") + ",";
        j += "\"needsEnvComplex\":" + juce::String (g.needsEnvComplex ? "true" : "false") + ",";
        j += "\"needsStereo\":" + juce::String (g.needsStereo ? "true" : "false") + ",";
        // v3 extensions
        j += "\"needsUnison\":" + juce::String (g.needsUnison ? "true" : "false") + ",";
        j += "\"needsFormant\":" + juce::String (g.needsFormant ? "true" : "false") + ",";
        j += "\"needsTransLayer\":" + juce::String (g.needsTransLayer ? "true" : "false") + ",";
        j += "\"needsReverb\":" + juce::String (g.needsReverb ? "true" : "false") + ",";
        j += "\"needsFilterSweep\":" + juce::String (g.needsFilterSweep ? "true" : "false") + ",";
        j += "\"needsResidual\":" + juce::String (g.needsResidual ? "true" : "false") + ",";
        j += "\"needsSpectralMatch\":" + juce::String (g.needsSpectralMatch ? "true" : "false") + ",";
        j += "\"needsTransientCapture\":" + juce::String (g.needsTransientCapture ? "true" : "false") + ",";
        j += "\"wavetableValid\":" + juce::String (refWavetable.valid ? "true" : "false");
        j += "}";
        return j;
    }

private:
    // Reference
    juce::AudioBuffer<float> referenceBuffer;
    double referenceSampleRate = 44100.0;
    juce::File referenceFile;
    MatchDescriptors refDescriptors;

    // Side-channel data (extracted from reference)
    WavetableData refWavetable;
    ResidualNoiseData refResidual;
    TransientSampleData refTransient;
    SpectralEnvelopeData refSpectralEnvelope;
    HarmonicPhaseData refHarmonicPhases;

    // Result
    juce::AudioBuffer<float> matchedBuffer;
    MatchDescriptors matchedDescriptors;
    MatchSynthParams bestParams;
    OptimizationResult bestResult;

    // Warm-start from previous matches
    MatchSynthParams previousBestParams;
    bool hasPreviousBest = false;

    // State
    std::atomic<MatchState> state { MatchState::Idle };
    std::atomic<int>   currentIteration { 0 };
    std::atomic<float> currentDistance { 999.0f };
    std::atomic<bool>  shouldCancel { false };
    std::thread matchThread;
    juce::String lastError;

    // Settings
    int   maxIterations  = 250;
    float targetDistance  = 0.4f;
    int   populationSize = 60;

    // Persistence
    juce::File matchDataDir;

    // Global learning history (KNN)
    std::vector<MatchHistoryEntry> matchHistory;
    bool historyLoaded = false;

    DescriptorExtractor extractor;

    // === Side-channel data extraction from reference ===

    void extractSideChannelData()
    {
        const int numSamples = referenceBuffer.getNumSamples();
        const float sr = (float) referenceSampleRate;

        // Mix to mono
        std::vector<float> mono (numSamples, 0.0f);
        for (int ch = 0; ch < referenceBuffer.getNumChannels(); ++ch)
        {
            const float* data = referenceBuffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                mono[i] += data[i];
        }
        float chScale = 1.0f / (float) referenceBuffer.getNumChannels();
        for (auto& s : mono) s *= chScale;

        // Find peak
        int peakIdx = 0;
        float peak = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            float v = std::abs (mono[i]);
            if (v > peak) { peak = v; peakIdx = i; }
        }

        float f0 = refDescriptors.fundamentalFreq;

        // --- Wavetable extraction (32 frames, multi-cycle, full duration) ---
        refWavetable = WavetableData();
        if (f0 > 20.0f)
        {
            float cycleSamples = sr / f0;
            // Determine cycles per frame: 2-4 depending on frequency (more cycles for high pitches)
            int cyclesPerFrame = (f0 > 200.0f) ? 4 : (f0 > 80.0f) ? 3 : 2;
            float captureLen = cycleSamples * (float) cyclesPerFrame;

            // Cover from peak to end of audible content (not just body)
            int wtStart = peakIdx;
            float thresh = peak * 0.005f; // -46 dB threshold
            int wtEnd = numSamples;
            for (int i = numSamples - 1; i > peakIdx; --i)
            {
                if (std::abs (mono[i]) > thresh) { wtEnd = std::min (numSamples, i + (int)(0.01f * sr)); break; }
            }

            int availableLen = wtEnd - wtStart;
            if (availableLen > (int)(captureLen * 2.0f))
            {
                int nFrames = std::min ((int) WavetableData::MAX_FRAMES,
                                        std::max (1, (int)(availableLen / captureLen) - 1));
                // Distribute frames with higher density at the start (where more happens)
                refWavetable.cyclesPerFrame = cyclesPerFrame;
                refWavetable.totalDuration = (float) availableLen / sr;

                for (int f = 0; f < nFrames; ++f)
                {
                    // Non-linear distribution: more frames at start, fewer at tail
                    float t = (float) f / (float)(nFrames - 1 + 1e-6f);
                    float tCurved = t * t; // quadratic: denser at beginning
                    int frameStart = wtStart + (int)(tCurved * (float)(availableLen - (int) captureLen));
                    int frameEnd = std::min (numSamples, frameStart + (int) captureLen);
                    int frameLen = frameEnd - frameStart;
                    if (frameLen < 8) break;

                    // Resample multi-cycle capture to FRAME_SIZE
                    for (int s = 0; s < WavetableData::FRAME_SIZE; ++s)
                    {
                        float srcPos = (float) s / (float) WavetableData::FRAME_SIZE * (float) frameLen;
                        int i0 = frameStart + std::min ((int) srcPos, frameLen - 1);
                        int i1 = std::min (i0 + 1, numSamples - 1);
                        float frac = srcPos - std::floor (srcPos);
                        refWavetable.frames[f][s] = mono[i0] * (1.0f - frac) + mono[i1] * frac;
                    }

                    // Normalize frame
                    float maxV = 0.0f;
                    for (int s = 0; s < WavetableData::FRAME_SIZE; ++s)
                        maxV = std::max (maxV, std::abs (refWavetable.frames[f][s]));
                    if (maxV > 0.001f)
                        for (int s = 0; s < WavetableData::FRAME_SIZE; ++s)
                            refWavetable.frames[f][s] /= maxV;

                    refWavetable.numFrames = f + 1;
                }
                refWavetable.valid = (refWavetable.numFrames > 0);
            }
        }

        // --- Transient sample extraction (first 5ms after peak) ---
        refTransient = TransientSampleData();
        {
            int transSamples = std::min ((int)(0.005f * sr), numSamples - peakIdx);
            if (transSamples > 8)
            {
                refTransient.samples.resize (transSamples);
                for (int i = 0; i < transSamples; ++i)
                    refTransient.samples[i] = mono[peakIdx + i];
                refTransient.sampleRate = sr;
                refTransient.valid = true;
            }
        }

        // --- Residual noise extraction (spectral subtraction + own envelope) ---
        refResidual = ResidualNoiseData();
        if (f0 > 20.0f)
        {
            const int fftOrder = 10;
            const int fftSize = 1 << fftOrder; // 1024
            const int hopSize = fftSize / 4;
            int analysisLen = std::min (numSamples, peakIdx + (int)(0.5f * sr));

            refResidual.residual.resize (analysisLen, 0.0f);
            refResidual.sampleRate = sr;
            std::vector<float> window (fftSize);
            for (int i = 0; i < fftSize; ++i)
                window[i] = 0.5f * (1.0f - std::cos (dsputil::TWO_PI * (float) i / (float) fftSize));

            std::vector<float> overlapAdd (analysisLen, 0.0f);
            std::vector<float> windowSum (analysisLen, 0.0f);
            juce::dsp::FFT fft (fftOrder);

            for (int pos = 0; pos + fftSize <= analysisLen; pos += hopSize)
            {
                std::vector<float> frame (fftSize * 2, 0.0f);
                for (int i = 0; i < fftSize; ++i)
                    frame[i] = mono[pos + i] * window[i];

                fft.performRealOnlyForwardTransform (frame.data());

                // Zero out harmonic bins
                float binW = sr / (float) fftSize;
                for (int h = 1; h <= 16; ++h)
                {
                    float hFreq = f0 * (float) h;
                    if (hFreq > sr * 0.45f) break;
                    int hBin = (int)(hFreq / binW);
                    for (int b = std::max (0, hBin - 3); b <= std::min (fftSize / 2 - 1, hBin + 3); ++b)
                    {
                        frame[b * 2] = 0.0f;
                        frame[b * 2 + 1] = 0.0f;
                    }
                }

                fft.performRealOnlyInverseTransform (frame.data());

                for (int i = 0; i < fftSize; ++i)
                {
                    overlapAdd[pos + i] += frame[i] / (float) fftSize * window[i];
                    windowSum[pos + i] += window[i] * window[i];
                }
            }

            for (int i = 0; i < analysisLen; ++i)
                refResidual.residual[i] = (windowSum[i] > 0.01f) ? overlapAdd[i] / windowSum[i] : 0.0f;

            // Extract residual's own amplitude envelope (RMS in 2ms windows)
            refResidual.envelope.resize (analysisLen, 0.0f);
            int envWindow = std::max (1, (int)(0.002f * sr));
            for (int i = 0; i < analysisLen; ++i)
            {
                float sumSq = 0.0f;
                int count = 0;
                for (int j = std::max (0, i - envWindow / 2); j < std::min (analysisLen, i + envWindow / 2); ++j)
                {
                    sumSq += refResidual.residual[j] * refResidual.residual[j];
                    ++count;
                }
                refResidual.envelope[i] = (count > 0) ? std::sqrt (sumSq / (float) count) : 0.0f;
            }
            // Normalize envelope to peak = 1
            float maxEnv = 0.0f;
            for (auto e : refResidual.envelope) maxEnv = std::max (maxEnv, e);
            if (maxEnv > 0.001f)
                for (auto& e : refResidual.envelope) e /= maxEnv;

            refResidual.valid = true;
        }

        // --- Harmonic phase extraction from reference body ---
        refHarmonicPhases = HarmonicPhaseData();
        if (f0 > 20.0f)
        {
            const int fftOrder = 11;
            const int fftSize = 1 << fftOrder; // 2048
            int bodyStart = peakIdx + (int)(0.010f * sr); // 10ms after peak (stable pitch region)
            if (bodyStart + fftSize < numSamples)
            {
                std::vector<float> fftData (fftSize * 2, 0.0f);
                for (int i = 0; i < fftSize; ++i)
                {
                    float w = 0.5f * (1.0f - std::cos (dsputil::TWO_PI * (float) i / (float) fftSize));
                    fftData[i] = mono[bodyStart + i] * w;
                }

                juce::dsp::FFT fft (fftOrder);
                fft.performRealOnlyForwardTransform (fftData.data());

                float binW = sr / (float) fftSize;
                int nHarm = std::min ((int) HarmonicPhaseData::MAX_HARMONICS, (int)(sr * 0.4f / f0));

                for (int h = 0; h < nHarm; ++h)
                {
                    float hFreq = f0 * (float)(h + 1);
                    int hBin = (int)(hFreq / binW + 0.5f);
                    if (hBin < 1 || hBin >= fftSize / 2) continue;

                    // Find strongest bin near harmonic
                    float bestMag = 0.0f;
                    int bestBin = hBin;
                    for (int b = std::max (1, hBin - 2); b <= std::min (fftSize / 2 - 1, hBin + 2); ++b)
                    {
                        float mag = fftData[b * 2] * fftData[b * 2] + fftData[b * 2 + 1] * fftData[b * 2 + 1];
                        if (mag > bestMag) { bestMag = mag; bestBin = b; }
                    }

                    refHarmonicPhases.phases[h] = std::atan2 (fftData[bestBin * 2 + 1], fftData[bestBin * 2]);
                }
                refHarmonicPhases.numHarmonics = nHarm;
                refHarmonicPhases.valid = true;
            }
        }

        // --- Spectral envelope extraction (body region band energies) ---
        refSpectralEnvelope = SpectralEnvelopeData();
        {
            for (int b = 0; b < SpectralEnvelopeData::NUM_BANDS; ++b)
                refSpectralEnvelope.targetBandEnergy[b] = refDescriptors.bodyRegion.bandEnergy[b];
            refSpectralEnvelope.valid = true;
        }
    }

    // === Build learned profile from K-NN match history ===

    LearnedProfile buildLearnedProfile (const MatchDescriptors& refDesc)
    {
        loadMatchHistory();
        LearnedProfile lp;
        if (matchHistory.size() < 3) return lp; // need at least 3 matches to learn from

        // Find K nearest matches
        MatchHistoryEntry query;
        query.fundamentalFreq = refDesc.fundamentalFreq;
        query.attackTime = refDesc.attackTime;
        query.decayTime = refDesc.decayTime;
        query.spectralCentroid = refDesc.spectralCentroid;
        query.brightness = refDesc.brightness;
        query.subEnergy = refDesc.subEnergy;
        query.transientStrength = refDesc.transientStrength;

        std::vector<std::pair<float, int>> distances;
        for (int i = 0; i < (int) matchHistory.size(); ++i)
            distances.push_back ({query.distanceTo (matchHistory[i]), i});
        std::sort (distances.begin(), distances.end());

        int K = std::min (5, (int) distances.size());
        // Only use matches that are reasonably similar (distance < 5.0)
        while (K > 0 && distances[K - 1].first > 5.0f) --K;
        if (K < 2) return lp;

        // === 1. Adaptive distance weights ===
        // If similar matches consistently had high final distance, boost weights for problem areas
        float avgDist = 0.0f;
        for (int i = 0; i < K; ++i)
            avgDist += matchHistory[distances[i].second].finalDistance;
        avgDist /= (float) K;

        // Analyze which descriptors had biggest residual error in similar matches
        // (we approximate this from the descriptor differences)
        float subErrSum = 0.0f, transErrSum = 0.0f, specErrSum = 0.0f;
        for (int i = 0; i < K; ++i)
        {
            auto& h = matchHistory[distances[i].second];
            subErrSum += std::abs (h.subEnergy - refDesc.subEnergy);
            transErrSum += std::abs (h.transientStrength - refDesc.transientStrength);
            specErrSum += std::abs (h.spectralCentroid - refDesc.spectralCentroid) / std::max (100.0f, refDesc.spectralCentroid);
        }
        // Boost weights for areas where similar sounds had issues
        if (avgDist > 1.0f)
        {
            float total = subErrSum + transErrSum + specErrSum + 0.001f;
            lp.subWeight = 1.0f + (subErrSum / total) * 0.5f;
            lp.transientWeight = 1.0f + (transErrSum / total) * 0.5f;
            lp.spectralWeight = 1.0f + (specErrSum / total) * 0.5f;
        }

        // === 2. Learned param bounds ===
        const int NP = MatchSynthParams::NUM_PARAMS;
        float paramMins[NP], paramMaxs[NP];
        MatchSynthParams::getBounds (paramMins, paramMaxs);
        lp.learnedMins.resize (NP);
        lp.learnedMaxs.resize (NP);
        // Initialize learned bounds to extreme values
        for (int j = 0; j < NP; ++j)
        {
            lp.learnedMins[j] = paramMaxs[j];
            lp.learnedMaxs[j] = paramMins[j];
        }
        // Expand bounds to cover all K nearest matches
        for (int i = 0; i < K; ++i)
        {
            float arr[NP];
            matchHistory[distances[i].second].params.toArray (arr);
            for (int j = 0; j < NP; ++j)
            {
                lp.learnedMins[j] = std::min (lp.learnedMins[j], arr[j]);
                lp.learnedMaxs[j] = std::max (lp.learnedMaxs[j], arr[j]);
            }
        }
        lp.boundsValid = true;

        // === 3. Pre-activate extensions ===
        // Count which extensions were activated in similar matches
        int fmCount = 0, resCount = 0, addCount = 0, formCount = 0;
        int sweepCount = 0, residCount = 0, specMatchCount = 0, transCapCount = 0;
        int combCount = 0, pdCount = 0;
        std::map<int, int> oscVotes;

        for (int i = 0; i < K; ++i)
        {
            auto& h = matchHistory[distances[i].second];
            if (h.usedFM) ++fmCount;
            if (h.usedResonance) ++resCount;
            if (h.usedAdditive) ++addCount;
            if (h.usedFormant) ++formCount;
            if (h.usedFilterSweep) ++sweepCount;
            if (h.usedResidual) ++residCount;
            if (h.usedSpectralMatch) ++specMatchCount;
            if (h.usedTransientCapture) ++transCapCount;
            if (h.usedComb) ++combCount;
            if (h.usedPhaseDistort) ++pdCount;
            oscVotes[h.bestOscType]++;
        }

        int threshold = K / 2; // >50% of similar matches used it → pre-activate
        auto addExt = [&](bool cond, const std::vector<int>& indices) {
            if (cond) for (int idx : indices) lp.preActivateExtensions.push_back (idx);
        };
        addExt (fmCount > threshold,       {22, 23, 24});
        addExt (resCount > threshold,      {25, 26});
        addExt (addCount > threshold,      {39, 40, 41, 42, 43, 44, 88, 89, 90, 91});
        addExt (formCount > threshold,     {67, 68, 69, 85});
        addExt (sweepCount > threshold,    {80, 81, 82, 83});
        addExt (residCount > threshold,    {86, 87});
        addExt (specMatchCount > threshold, {92});
        addExt (transCapCount > threshold, {94});
        addExt (combCount > threshold,     {32, 33, 34});
        addExt (pdCount > threshold,       {37, 38});

        // Find most popular oscType
        int bestOscVotes = 0;
        for (auto& [osc, votes] : oscVotes)
        {
            if (votes > bestOscVotes) { bestOscVotes = votes; lp.preferredOscType = osc; }
        }

        lp.valid = true;
        return lp;
    }

    // === Global learning: load history from all previous match results ===

    void loadMatchHistory()
    {
        if (historyLoaded) return;
        historyLoaded = true;
        matchHistory.clear();

        if (! matchDataDir.isDirectory()) return;

        auto files = matchDataDir.findChildFiles (juce::File::findFiles, false, "*.json");

        for (auto& file : files)
        {
            auto content = file.loadFileAsString();
            if (! content.contains ("\"basePitch\"") && ! content.contains ("\"params\"")) continue;

            // Parse params
            float arr[MatchSynthParams::NUM_PARAMS];
            MatchSynthParams defaults;
            defaults.toArray (arr);

            for (int i = 0; i < MatchSynthParams::NUM_PARAMS; ++i)
            {
                auto paramName = juce::String (MatchSynthParams::getParamName (i));
                auto searchStr = "\"" + paramName + "\":";
                int pos = content.indexOf (searchStr);
                if (pos >= 0)
                {
                    int valStart = pos + searchStr.length();
                    int valEnd = content.indexOfAnyOf (",}", valStart);
                    if (valEnd > valStart)
                        arr[i] = content.substring (valStart, valEnd).trim().getFloatValue();
                }
            }

            MatchSynthParams loadedParams;
            loadedParams.fromArray (arr);

            // Parse key descriptors from refDescriptors section
            MatchHistoryEntry entry;
            entry.params = loadedParams;

            auto parseField = [&](const juce::String& fieldName) -> float {
                auto search = "\"" + fieldName + "\":";
                // Search in refDescriptors section
                int refStart = content.indexOf ("\"refDescriptors\"");
                if (refStart < 0) refStart = 0;
                int pos2 = content.indexOf (refStart, search);
                if (pos2 >= 0)
                {
                    int vs = pos2 + search.length();
                    int ve = content.indexOfAnyOf (",}", vs);
                    if (ve > vs) return content.substring (vs, ve).trim().getFloatValue();
                }
                return 0.0f;
            };

            entry.fundamentalFreq = parseField ("fundamentalFreq");
            entry.attackTime = parseField ("attackTime");
            entry.decayTime = parseField ("decayTime");
            entry.spectralCentroid = parseField ("spectralCentroid");
            entry.brightness = parseField ("brightness");
            entry.subEnergy = parseField ("subEnergy");
            entry.transientStrength = parseField ("transientStrength");

            // Parse learning-relevant fields
            auto parseTopField = [&](const juce::String& fieldName) -> float {
                auto search = "\"" + fieldName + "\":";
                int pos2 = content.indexOf (search);
                if (pos2 >= 0)
                {
                    int vs = pos2 + search.length();
                    int ve = content.indexOfAnyOf (",}\n", vs);
                    if (ve > vs) return content.substring (vs, ve).trim().getFloatValue();
                }
                return 0.0f;
            };
            auto parseBool = [&](const juce::String& fieldName) -> bool {
                return content.contains ("\"" + fieldName + "\":true");
            };

            entry.finalDistance = parseTopField ("distance");
            entry.bestOscType = loadedParams.oscType;
            // Parse gap analysis flags
            entry.usedFM = parseBool ("needsFM");
            entry.usedResonance = parseBool ("needsResonance");
            entry.usedAdditive = parseBool ("needsAdditive");
            entry.usedFormant = parseBool ("needsFormant");
            entry.usedFilterSweep = parseBool ("needsFilterSweep");
            entry.usedResidual = parseBool ("needsResidual");
            entry.usedSpectralMatch = parseBool ("needsSpectralMatch");
            entry.usedTransientCapture = parseBool ("needsTransientCapture");
            entry.usedComb = parseBool ("needsComb");
            entry.usedPhaseDistort = parseBool ("needsPhaseDistort");

            matchHistory.push_back (entry);
        }

        DBG ("OneShotMatch: Loaded " + juce::String ((int) matchHistory.size()) + " history entries for global learning");
    }

    // === Match data persistence ===

    void saveMatchResult()
    {
        if (! matchDataDir.isDirectory()) matchDataDir.createDirectory();

        auto now = juce::Time::getCurrentTime();
        auto timestamp = now.formatted ("%Y%m%d_%H%M%S");

        // Determine filename
        juce::String baseName = referenceFile.getFileNameWithoutExtension();
        if (baseName.isEmpty()) baseName = "OneShotMatch";
        auto fileName = baseName + "_" + timestamp + ".json";

        auto file = matchDataDir.getChildFile (fileName);

        // Build complete match record JSON
        juce::String json = "{\n";
        json += "  \"timestamp\": \"" + now.toISO8601 (true) + "\",\n";
        json += "  \"referenceFile\": \"" + referenceFile.getFullPathName().replace ("\\", "\\\\") + "\",\n";
        json += "  \"distance\": " + juce::String (bestResult.bestDistance, 6) + ",\n";
        json += "  \"iterations\": " + juce::String (bestResult.iterations) + ",\n";
        json += "  \"converged\": " + juce::String (bestResult.converged ? "true" : "false") + ",\n";
        json += "  \"sampleRate\": " + juce::String (referenceSampleRate) + ",\n";
        json += "  \"params\": " + paramsToJSON (bestParams) + ",\n";
        json += "  \"refDescriptors\": " + descriptorsToJSON (refDescriptors) + ",\n";
        json += "  \"matchedDescriptors\": " + descriptorsToJSON (matchedDescriptors) + ",\n";
        json += "  \"sensitivity\": " + sensitivityToJSON() + ",\n";
        json += "  \"gapAnalysis\": " + gapAnalysisToJSON() + "\n";
        json += "}\n";

        file.replaceWithText (json);
        DBG ("OneShotMatch: Saved match result to " + file.getFullPathName());

        historyLoaded = false; // force reload on next match
    }

    void loadPreviousBest()
    {
        // Look for existing match results for this reference file
        if (! referenceFile.existsAsFile()) return;
        if (! matchDataDir.isDirectory()) return;

        auto baseName = referenceFile.getFileNameWithoutExtension();
        auto children = matchDataDir.findChildFiles (juce::File::findFiles, false, baseName + "_*.json");

        if (children.isEmpty()) return;

        // Sort by name (timestamp) — newest last
        children.sort();
        auto newest = children.getLast();

        auto content = newest.loadFileAsString();
        if (content.isEmpty()) return;

        // Parse params from JSON using simple string search
        float arr[MatchSynthParams::NUM_PARAMS];
        bool foundAny = false;

        for (int i = 0; i < MatchSynthParams::NUM_PARAMS; ++i)
        {
            juce::String paramName = MatchSynthParams::getParamName (i);
            juce::String searchKey = "\"" + paramName + "\":";

            int pos = content.indexOf (searchKey);
            if (pos < 0)
            {
                // Param not found — use default
                arr[i] = 0.0f;
                continue;
            }

            // Skip past the key
            int valueStart = pos + searchKey.length();

            // Skip whitespace
            while (valueStart < content.length() &&
                   (content[valueStart] == ' ' || content[valueStart] == '\t'))
                ++valueStart;

            // Extract the numeric value — find end (comma, }, whitespace, newline)
            int valueEnd = valueStart;
            while (valueEnd < content.length())
            {
                auto c = content[valueEnd];
                if (c == ',' || c == '}' || c == '\n' || c == '\r')
                    break;
                ++valueEnd;
            }

            juce::String valueStr = content.substring (valueStart, valueEnd).trim();
            arr[i] = valueStr.getFloatValue();
            foundAny = true;
        }

        if (foundAny)
        {
            previousBestParams.fromArray (arr);
            hasPreviousBest = true;
            DBG ("OneShotMatch: Loaded previous best from " + newest.getFileName());
        }
    }
};

} // namespace oneshotmatch
