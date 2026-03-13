#pragma once

#include <random>
#include <cmath>
#include <algorithm>
#include <array>
#include "../Params/SynthParams.h"
#include "GenreRules.h"
#include "MutationAxes.h"

// ================================================================
// ONNX Runtime inference support (conditional compilation)
// Define USE_ONNX_INFERENCE=1 and link onnxruntime to enable.
// Without it, the plugin uses rule-based generation as fallback.
// ================================================================
#if USE_ONNX_INFERENCE
  #include <onnxruntime_cxx_api.h>
  #include <memory>
  #include <vector>
  #ifdef _WIN32
    #include <Windows.h>
  #endif
#endif

// Generador de parámetros basado en reglas musicales (Opción A) con
// soporte opcional para inferencia ONNX (Opción B).
// Toma un GenerationRequest del UX y produce un GenerationResult
// con parámetros coherentes, únicos y listos para el motor de síntesis.

class ParameterGenerator
{
public:

#if USE_ONNX_INFERENCE
    // ================================================================
    // ONNX MODEL LOADING
    // ================================================================

    // Call once at startup with the path to the Resources folder.
    // Returns true if models loaded successfully.
    bool loadONNXModels (const std::string& resourcesPath)
    {
        try
        {
            Ort::SessionOptions sessionOptions;
            sessionOptions.SetIntraOpNumThreads (1);
            sessionOptions.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);

            std::string paramModelPath = resourcesPath + "/oneshot_param_predictor.onnx";
            std::string qualityModelPath = resourcesPath + "/quality_scorer.onnx";

            // Wide string conversion for Windows (handles UTF-8 and special chars)
            #ifdef _WIN32
                auto toWide = [] (const std::string& s) -> std::wstring {
                    if (s.empty()) return {};
                    int needed = MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), nullptr, 0);
                    std::wstring ws (needed, 0);
                    MultiByteToWideChar (CP_UTF8, 0, s.c_str(), (int) s.size(), ws.data(), needed);
                    return ws;
                };
                paramSession = std::make_unique<Ort::Session> (env, toWide (paramModelPath).c_str(), sessionOptions);
                qualitySession = std::make_unique<Ort::Session> (env, toWide (qualityModelPath).c_str(), sessionOptions);
            #else
                paramSession = std::make_unique<Ort::Session> (env, paramModelPath.c_str(), sessionOptions);
                qualitySession = std::make_unique<Ort::Session> (env, qualityModelPath.c_str(), sessionOptions);
            #endif

            onnxAvailable = true;
            return true;
        }
        catch (const Ort::Exception& e)
        {
            onnxAvailable = false;
            onnxLoadError = e.what();
            return false;
        }
    }

    bool isONNXAvailable() const { return onnxAvailable; }
    const std::string& getONNXError() const { return onnxLoadError; }
#endif

    // ================================================================
    // MAIN GENERATE — dispatches to ONNX or rule-based
    // ================================================================
    GenerationResult generate (const GenerationRequest& req, unsigned int seed = 0)
    {
        if (seed == 0)
        {
            std::random_device rd;
            seed = rd();
        }

        std::mt19937 rng (seed);
        GenerationResult result;
        result.instrument = req.instrument;

#if USE_ONNX_INFERENCE
        if (onnxAvailable)
        {
            // Try ONNX inference, fall back to rules on failure
            try
            {
                result.params = generateFromONNX (req, rng);
                result.effects = generateEffects (req, rng);
                return result;
            }
            catch (...)
            {
                // Fall through to rule-based generation
            }
        }
#endif

        // Rule-based generation (original path)
        switch (req.instrument)
        {
            case InstrumentType::Kick:    result.params = generateKick (req, rng);    break;
            case InstrumentType::Snare:   result.params = generateSnare (req, rng);   break;
            case InstrumentType::HiHat:   result.params = generateHiHat (req, rng);   break;
            case InstrumentType::Clap:    result.params = generateClap (req, rng);     break;
            case InstrumentType::Perc:    result.params = generatePerc (req, rng);     break;
            case InstrumentType::Bass808: result.params = generateBass808 (req, rng);  break;
            case InstrumentType::Lead:    result.params = generateLead (req, rng);     break;
            case InstrumentType::Pluck:   result.params = generatePluck (req, rng);    break;
            case InstrumentType::Pad:     result.params = generatePad (req, rng);      break;
            case InstrumentType::Texture: result.params = generateTexture (req, rng);  break;
        }

        // Fase 4: generar parámetros de efectos
        result.effects = generateEffects (req, rng);

        return result;
    }

#if USE_ONNX_INFERENCE
    // ================================================================
    // ONNX PARAMETER PREDICTION
    // ================================================================
    // Input tensor: [instrument_onehot(10) + genre_onehot(9) + sliders(5)] = 24 floats
    // Sliders: [brillo, cuerpo, textura, movimiento, impacto]
    // Output: raw parameter vector from the model

    struct ONNXPrediction
    {
        std::vector<float> params;
        bool success = false;
    };

    ONNXPrediction predictFromONNX (int instrumentIdx, int genreIdx, float sliders[5])
    {
        ONNXPrediction pred;
        if (!onnxAvailable || !paramSession) return pred;

        try
        {
            // Build input tensor: 24 floats
            std::array<float, 24> inputData {};
            inputData.fill (0.0f);

            // Instrument one-hot (10 classes)
            if (instrumentIdx >= 0 && instrumentIdx < 10)
                inputData[instrumentIdx] = 1.0f;

            // Genre one-hot (9 classes)
            if (genreIdx >= 0 && genreIdx < 9)
                inputData[10 + genreIdx] = 1.0f;

            // Sliders (5 floats)
            for (int i = 0; i < 5; ++i)
                inputData[19 + i] = sliders[i];

            // Create input tensor
            std::array<int64_t, 2> inputShape = { 1, 24 };
            Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value inputTensor = Ort::Value::CreateTensor<float> (
                memInfo, inputData.data(), inputData.size(),
                inputShape.data(), inputShape.size());

            // Run inference (I/O names must match 06_export_onnx.py)
            const char* inputNames[] = { "controls" };
            const char* outputNames[] = { "params" };

            auto outputTensors = paramSession->Run (
                Ort::RunOptions { nullptr },
                inputNames, &inputTensor, 1,
                outputNames, 1);

            // Extract output
            float* outputData = outputTensors[0].GetTensorMutableData<float>();
            auto outputInfo = outputTensors[0].GetTensorTypeAndShapeInfo();
            size_t outputSize = outputInfo.GetElementCount();

            pred.params.assign (outputData, outputData + outputSize);
            pred.success = true;
        }
        catch (const Ort::Exception&)
        {
            pred.success = false;
        }

        return pred;
    }

    // ================================================================
    // ONNX QUALITY SCORING
    // ================================================================
    // Scores generated audio quality on [0, 1] using the quality_scorer model.
    // Input: 33 audio features extracted from the rendered buffer:
    //   20 scalar features (attack_time, decay_time, duration, peak, crest,
    //   transient_sharpness, spectral_centroid, spectral_rolloff, spectral_bandwidth,
    //   spectral_flatness, spectral_tilt, fundamental_freq, harmonic_ratio,
    //   pitch_drop_semitones, pitch_drop_time, brightness_index, warmth_index,
    //   noise_ratio, stereo_width, zero_crossing_rate)
    //   + 13 MFCCs = 33 total
    // Returns quality score [0, 1], or -1.0f on failure.

    static constexpr int kQualityScorerInputDim = 33;

    float scoreQuality (const float* audioFeatures, int numFeatures)
    {
        if (!onnxAvailable || !qualitySession) return -1.0f;
        if (audioFeatures == nullptr || numFeatures != kQualityScorerInputDim) return -1.0f;

        try
        {
            std::array<float, kQualityScorerInputDim> inputData {};
            for (int i = 0; i < kQualityScorerInputDim; ++i)
                inputData[i] = audioFeatures[i];

            std::array<int64_t, 2> inputShape = { 1, kQualityScorerInputDim };
            Ort::MemoryInfo memInfo = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
            Ort::Value inputTensor = Ort::Value::CreateTensor<float> (
                memInfo, inputData.data(), inputData.size(),
                inputShape.data(), inputShape.size());

            // I/O names must match 07_train_quality_scorer.py export
            const char* inputNames[] = { "features" };
            const char* outputNames[] = { "quality_score" };

            auto outputTensors = qualitySession->Run (
                Ort::RunOptions { nullptr },
                inputNames, &inputTensor, 1,
                outputNames, 1);

            float* outputData = outputTensors[0].GetTensorMutableData<float>();
            return outputData[0];
        }
        catch (const Ort::Exception&)
        {
            return -1.0f;
        }
    }
#endif

private:

#if USE_ONNX_INFERENCE
    Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "OneShotAI" };
    std::unique_ptr<Ort::Session> paramSession;
    std::unique_ptr<Ort::Session> qualitySession;
    bool onnxAvailable = false;
    std::string onnxLoadError;

    // Convert enum to index for one-hot encoding
    static int instrumentToIndex (InstrumentType t)
    {
        switch (t)
        {
            case InstrumentType::Kick:    return 0;
            case InstrumentType::Snare:   return 1;
            case InstrumentType::HiHat:   return 2;
            case InstrumentType::Clap:    return 3;
            case InstrumentType::Perc:    return 4;
            case InstrumentType::Bass808: return 5;
            case InstrumentType::Lead:    return 6;
            case InstrumentType::Pluck:   return 7;
            case InstrumentType::Pad:     return 8;
            case InstrumentType::Texture: return 9;
        }
        return 0;
    }

    static int genreToIndex (GenreStyle g)
    {
        switch (g)
        {
            case GenreStyle::Trap:      return 0;
            case GenreStyle::HipHop:    return 1;
            case GenreStyle::Techno:    return 2;
            case GenreStyle::House:     return 3;
            case GenreStyle::Reggaeton: return 4;
            case GenreStyle::Afrobeat:  return 5;
            case GenreStyle::RnB:       return 6;
            case GenreStyle::EDM:       return 7;
            case GenreStyle::Ambient:   return 8;
        }
        return 0;
    }

    // Helper: denormalize a 0..1 value to [lo, hi]
    static float denorm (float normalized, float lo, float hi)
    {
        return lo + std::max (0.0f, std::min (1.0f, normalized)) * (hi - lo);
    }

    // Generate params from ONNX model output.
    // The model outputs 35 normalized floats (0..1 via Sigmoid).
    // Each instrument uses the first N slots matching its PARAM_BOUNDS order.
    // Jitter is applied after denormalization to add per-render variation.
    InstrumentParamsVariant generateFromONNX (const GenerationRequest& req, std::mt19937& rng)
    {
        int instIdx = instrumentToIndex (req.instrument);
        int genreIdx = genreToIndex (req.genre);
        float sliders[5] = {
            req.character.brillo,
            req.character.cuerpo,
            req.character.textura,
            req.character.movimiento,
            req.impacto
        };

        auto pred = predictFromONNX (instIdx, genreIdx, sliders);
        if (!pred.success || pred.params.size() < 5)
            throw std::runtime_error ("ONNX prediction failed");

        const auto& v = pred.params; // shorthand

        switch (req.instrument)
        {
            case InstrumentType::Kick:
            {
                KickParams p = genrerules::kickBase (req.genre);
                p.base.attack = attackTimeFor (req.attack, req.instrument);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                p.subFreq       = clamp (jitter (denorm (v[0], 30.0f, 70.0f), 8.0f, rng), 30.0f, 70.0f);
                p.clickAmount   = clamp (jitter (denorm (v[1], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.bodyDecay     = clamp (jitter (denorm (v[2], 0.05f, 0.5f), 0.08f, rng), 0.05f, 0.6f);
                p.pitchDrop     = clamp (jitter (denorm (v[3], 10.0f, 60.0f), 10.0f, rng), 8.0f, 72.0f);
                p.pitchDropTime = clamp (jitter (denorm (v[4], 0.01f, 0.1f), 0.015f, rng), 0.005f, 0.1f);
                p.driveAmount   = clamp (jitter (denorm (v[5], 0.0f, 0.5f), 0.12f, rng), 0.0f, 0.8f);
                p.subLevel      = clamp (jitter (denorm (v[6], 0.3f, 1.0f), 0.12f, rng), 0.3f, 1.0f);
                p.tailLength    = clamp (jitter (denorm (v[7], 0.05f, 0.6f), 0.12f, rng), 0.1f, 1.0f);
                return p;
            }
            case InstrumentType::Snare:
            {
                SnareParams p = genrerules::snareBase (req.genre);
                p.base.attack = attackTimeFor (req.attack, req.instrument);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                p.bodyFreq      = clamp (jitter (denorm (v[0], 120.0f, 280.0f), 30.0f, rng), 120.0f, 300.0f);
                p.bodyMix       = clamp (jitter (denorm (v[1], 0.2f, 0.9f), 0.12f, rng), 0.10f, 0.55f);
                p.bodyDecay     = clamp (jitter (denorm (v[2], 0.03f, 0.2f), 0.04f, rng), 0.03f, 0.25f);
                p.noiseDecay    = clamp (jitter (denorm (v[3], 0.05f, 0.25f), 0.04f, rng), 0.04f, 0.25f);
                p.snapAmount    = clamp (jitter (denorm (v[4], 0.0f, 0.8f), 0.15f, rng), 0.0f, 1.0f);
                p.noiseColor    = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.wireAmount    = clamp (jitter (denorm (v[6], 0.0f, 1.0f), 0.12f, rng), 0.0f, 1.0f);
                return p;
            }
            case InstrumentType::HiHat:
            {
                HiHatParams p = genrerules::hiHatBase (req.genre);
                p.base.attack = attackTimeFor (req.attack, req.instrument);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                p.closedDecay   = clamp (jitter (denorm (v[0], 0.01f, 0.3f), 0.015f, rng), 0.005f, 0.08f);
                p.openDecay     = clamp (jitter (denorm (v[0], 0.05f, 0.5f), 0.10f, rng), 0.05f, 0.8f);
                p.highPassFreq  = clamp (jitter (denorm (v[1], 2000.0f, 12000.0f), 800.0f, rng), 1500.0f, 10000.0f);
                p.freqRange     = clamp (jitter (denorm (v[2], 200.0f, 800.0f), 1200.0f, rng), 3000.0f, 14000.0f);
                p.metallic      = clamp (jitter (denorm (v[3], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.noiseColor    = clamp (jitter (denorm (v[4], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.ringAmount    = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.12f, rng), 0.0f, 0.7f);
                return p;
            }
            case InstrumentType::Clap:
            {
                ClapParams p = genrerules::clapBase (req.genre);
                p.base.attack = attackTimeFor (req.attack, req.instrument);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                p.noiseDecay    = clamp (jitter (denorm (v[0], 0.05f, 0.4f), 0.05f, rng), 0.03f, 0.20f);
                p.layerSpacing  = clamp (jitter (denorm (v[1], 0.005f, 0.03f), 0.006f, rng), 0.004f, 0.025f);
                p.numLayers     = clamp (jitter (denorm (v[2], 2.0f, 8.0f), 1.2f, rng), 2.0f, 8.0f);
                p.noiseColor    = clamp (jitter (denorm (v[3], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.thickness     = clamp (jitter (denorm (v[4], 0.0f, 1.0f), 0.15f, rng), 0.1f, 1.0f);
                p.transientSnap = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.12f, rng), 0.2f, 1.0f);
                return p;
            }
            case InstrumentType::Perc:
            {
                PercParams p = genrerules::percBase (req.genre);
                p.base.attack = attackTimeFor (req.attack, req.instrument);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                p.freq          = clamp (jitter (denorm (v[0], 100.0f, 2000.0f), 100.0f, rng), 150.0f, 2200.0f);
                p.toneDecay     = clamp (jitter (denorm (v[1], 0.02f, 0.3f), 0.04f, rng), 0.02f, 0.25f);
                p.clickAmount   = clamp (jitter (denorm (v[2], 0.0f, 0.8f), 0.15f, rng), 0.0f, 1.0f);
                p.pitchDrop     = clamp (jitter (denorm (v[3], 0.0f, 24.0f), 6.0f, rng), 0.0f, 36.0f);
                p.woodiness     = clamp (jitter (denorm (v[4], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.metallic      = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                return p;
            }
            case InstrumentType::Bass808:
            {
                Bass808Params p = genrerules::bass808Base (req.genre);
                p.base.attack = attackTimeFor (req.attack, req.instrument);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                p.subFreq       = clamp (jitter (denorm (v[0], 25.0f, 65.0f), 6.0f, rng), 28.0f, 65.0f);
                p.sustainLevel  = clamp (jitter (denorm (v[1], 0.1f, 1.5f), 0.15f, rng), 0.15f, 0.90f);
                p.saturation    = clamp (jitter (denorm (v[2], 0.0f, 0.8f), 0.12f, rng), 0.0f, 0.8f);
                p.glideAmount   = clamp (jitter (denorm (v[3], 0.0f, 12.0f), 2.0f, rng), 0.0f, 10.0f);
                p.reeseMix      = clamp (jitter (denorm (v[4], 0.0f, 1.0f), 0.12f, rng), 0.0f, 1.0f);
                p.filterEnvAmt  = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.10f, rng), 0.0f, 1.0f);
                p.punchiness    = clamp (jitter (denorm (v[6], 0.0f, 1.0f), 0.12f, rng), 0.0f, 1.0f);
                return p;
            }
            case InstrumentType::Lead:
            {
                LeadParams p = genrerules::leadBase (req.genre);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                float freq      = jitter (denorm (v[0], 200.0f, 1000.0f), 60.0f, rng);
                p.base.pitchBase = 69.0f + 12.0f * std::log2 (std::max (freq, 20.0f) / 440.0f);
                p.base.attack   = denorm (v[3], 0.001f, 0.1f);
                p.base.decay    = denorm (v[4], 0.1f, 1.0f);
                p.oscDetune     = clamp (jitter (denorm (v[1], 0.0f, 0.05f), 0.06f, rng), 0.0f, 0.5f);
                p.pulseWidth    = clamp (jitter (denorm (v[2], 0.1f, 0.9f), 0.12f, rng), 0.1f, 0.9f);
                p.brightness    = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.12f, rng), 0.1f, 1.0f);
                p.vibratoRate   = clamp (jitter (denorm (v[6], 0.5f, 8.0f), 1.0f, rng), 2.0f, 10.0f);
                return p;
            }
            case InstrumentType::Pluck:
            {
                PluckParams p = genrerules::pluckBase (req.genre);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                float freq      = jitter (denorm (v[0], 150.0f, 800.0f), 50.0f, rng);
                p.base.pitchBase = 69.0f + 12.0f * std::log2 (std::max (freq, 20.0f) / 440.0f);
                p.decayTime     = clamp (jitter (denorm (v[1], 0.05f, 0.5f), 0.12f, rng), 0.1f, 1.5f);
                p.brightness    = clamp (jitter (denorm (v[2], 0.1f, 0.9f), 0.12f, rng), 0.1f, 1.0f);
                p.damping       = clamp (jitter (denorm (v[3], 0.0f, 1.0f), 0.12f, rng), 0.1f, 0.9f);
                p.bodyResonance = clamp (jitter (denorm (v[4], 0.0f, 1.0f), 0.12f, rng), 0.0f, 0.7f);
                p.fmAmount      = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                return p;
            }
            case InstrumentType::Pad:
            {
                PadParams p = genrerules::padBase (req.genre);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                float freq      = jitter (denorm (v[0], 80.0f, 500.0f), 40.0f, rng);
                p.base.pitchBase = 69.0f + 12.0f * std::log2 (std::max (freq, 20.0f) / 440.0f);
                p.base.attack   = denorm (v[1], 0.05f, 1.0f);
                p.base.release  = denorm (v[2], 0.1f, 2.0f);
                p.detuneSpread  = clamp (jitter (denorm (v[3], 0.0f, 0.03f), 0.06f, rng), 0.02f, 0.5f);
                p.warmth        = clamp (jitter (denorm (v[4], 0.0f, 1.0f), 0.15f, rng), 0.1f, 0.9f);
                p.chorusAmount  = clamp (jitter (denorm (v[5], 0.0f, 1.0f), 0.12f, rng), 0.0f, 0.6f);
                p.filterSweep   = clamp (jitter (denorm (v[6], 0.0f, 1.0f), 0.10f, rng), 0.0f, 0.5f);
                p.evolutionRate = clamp (jitter (denorm (v[7], 0.0f, 1.0f), 0.08f, rng), 0.01f, 0.4f);
                return p;
            }
            case InstrumentType::Texture:
            {
                TextureParams p = genrerules::textureBase (req.genre);
                applyCharacterToBase (p.base, req.character, req.impacto, req.energy);
                p.density       = clamp (jitter (denorm (v[0], 0.1f, 0.9f), 0.15f, rng), 0.1f, 1.0f);
                p.base.filterCutoff = clamp (jitter (denorm (v[1], 1000.0f, 18000.0f), 2000.0f, rng), 500.0f, 20000.0f);
                p.movement      = clamp (jitter (denorm (v[2], 0.0f, 0.8f), 0.15f, rng), 0.0f, 1.0f);
                p.grainSize     = clamp (jitter (denorm (v[3], 0.005f, 0.1f), 0.02f, rng), 0.01f, 0.15f);
                p.noiseColor    = clamp (jitter (denorm (v[4], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.spectralTilt  = clamp (jitter (denorm (v[5], -1.0f, 1.0f), 0.15f, rng), -0.8f, 0.8f);
                p.pitchedness   = clamp (jitter (denorm (v[6], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                p.pitch         = clamp (jitter (denorm (v[7], 0.0f, 1.0f), 0.15f, rng), 0.0f, 1.0f);
                return p;
            }
        }

        // Fallback (should not reach here)
        return generateKick (req, rng);
    }
#endif

    // ================================================================
    // HELPERS
    // ================================================================

    static float jitter (float value, float amount, std::mt19937& rng)
    {
        std::uniform_real_distribution<float> dist (-1.0f, 1.0f);
        return value + dist (rng) * amount;
    }

    static float clamp (float v, float lo, float hi)
    {
        return std::max (lo, std::min (v, hi));
    }

    // Determina si el instrumento es percusivo (ataque forzado a Fast)
    static bool isPercussive (InstrumentType type)
    {
        return type == InstrumentType::Kick
            || type == InstrumentType::Snare
            || type == InstrumentType::HiHat
            || type == InstrumentType::Clap
            || type == InstrumentType::Perc;
    }

    // Retorna el valor de attack en segundos según AttackType
    static float attackTimeFor (AttackType type, InstrumentType instrument)
    {
        // Percusivos: siempre rápido
        if (isPercussive (instrument))
            return 0.001f;

        switch (type)
        {
            case AttackType::Fast:   return 0.005f;
            case AttackType::Medium: return 0.04f;
            case AttackType::Slow:   return 0.20f;
        }
        return 0.01f;
    }

    // Multiplicador de energía
    static float energyMult (EnergyLevel energy)
    {
        switch (energy)
        {
            case EnergyLevel::Low:    return 0.75f;
            case EnergyLevel::Medium: return 1.0f;
            case EnergyLevel::High:   return 1.25f;
        }
        return 1.0f;
    }

    // Aplica controles de carácter al BaseSoundParams
    static void applyCharacterToBase (BaseSoundParams& base,
                                      const CharacterControls& ch,
                                      float impacto,
                                      EnergyLevel energy)
    {
        float eMult = energyMult (energy);

        // --- Brillo ---
        float brilloFactor = 0.4f + ch.brillo * 1.2f; // 0.4x a 1.6x del cutoff base
        base.filterCutoff *= brilloFactor;
        base.filterCutoff = clamp (base.filterCutoff, 200.0f, 20000.0f);

        // --- Cuerpo ---
        base.filterResonance += ch.cuerpo * 0.15f;
        base.filterResonance = clamp (base.filterResonance, 0.0f, 0.85f);

        // --- Textura ---
        base.saturationAmount += ch.textura * 0.20f;
        base.noiseAmount += ch.textura * 0.15f;
        base.saturationAmount = clamp (base.saturationAmount, 0.0f, 1.0f);
        base.noiseAmount = clamp (base.noiseAmount, 0.0f, 1.0f);

        // --- Movimiento ---
        base.lfoRate  += ch.movimiento * 4.0f;
        base.lfoDepth += ch.movimiento * 0.3f;
        base.lfoDest = (ch.movimiento > 0.3f) ? LfoDestination::FilterCutoff : LfoDestination::None;

        // --- Impacto ---
        base.volume *= (0.75f + impacto * 0.25f);
        base.volume = clamp (base.volume, 0.0f, 1.0f);

        // --- Energía ---
        base.volume *= (0.85f + (eMult - 0.75f) * 0.3f);
        base.volume = clamp (base.volume, 0.0f, 1.0f);
        base.saturationAmount += (eMult - 1.0f) * 0.10f;
        base.saturationAmount = clamp (base.saturationAmount, 0.0f, 1.0f);
    }

    // ================================================================
    // KICK
    // ================================================================
    KickParams generateKick (const GenerationRequest& req, std::mt19937& rng)
    {
        KickParams p = genrerules::kickBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: más click, más presencia de ataque
        p.clickAmount += req.character.brillo * 0.20f;

        // Cuerpo: más body decay, más sub
        p.bodyDecay += req.character.cuerpo * 0.10f;
        p.subLevel  += req.character.cuerpo * 0.10f;
        p.pitchDrop -= req.character.cuerpo * 8.0f; // menos pitch drop = más cuerpo

        // Textura: más drive
        p.driveAmount += req.character.textura * 0.15f;

        // Impacto: más click, más drive, tail más corto (más pegada)
        p.clickAmount += req.impacto * 0.25f;
        p.driveAmount += req.impacto * 0.15f;
        p.pitchDropTime *= (1.0f - req.impacto * 0.3f); // más rápido = más punch

        // Energía
        float eMult = energyMult (req.energy);
        p.driveAmount *= eMult;

        // Randomización controlada (~20-25% del rango de cada param)
        p.subFreq       = clamp (jitter (p.subFreq, 8.0f, rng), 30.0f, 70.0f);
        p.clickAmount   = clamp (jitter (p.clickAmount, 0.20f, rng), 0.0f, 1.0f);
        p.bodyDecay     = clamp (jitter (p.bodyDecay, 0.10f, rng), 0.05f, 0.6f);
        p.pitchDrop     = clamp (jitter (p.pitchDrop, 12.0f, rng), 8.0f, 72.0f);
        p.pitchDropTime = clamp (jitter (p.pitchDropTime, 0.015f, rng), 0.005f, 0.1f);
        p.driveAmount   = clamp (jitter (p.driveAmount, 0.15f, rng), 0.0f, 0.8f);
        p.subLevel      = clamp (jitter (p.subLevel, 0.12f, rng), 0.3f, 1.0f);
        p.tailLength    = clamp (jitter (p.tailLength, 0.15f, rng), 0.1f, 1.0f);

        return p;
    }

    // ================================================================
    // SNARE
    // ================================================================
    SnareParams generateSnare (const GenerationRequest& req, std::mt19937& rng)
    {
        SnareParams p = genrerules::snareBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: más snap, ruido más brillante, LP más alto
        p.snapAmount += req.character.brillo * 0.20f;
        p.noiseColor += req.character.brillo * 0.20f;
        p.noiseLP    += req.character.brillo * 2000.0f;

        // Cuerpo: más body decay y freq más baja, más bodyMix
        p.bodyDecay += req.character.cuerpo * 0.06f;
        p.bodyFreq  -= req.character.cuerpo * 30.0f;
        p.bodyMix   += req.character.cuerpo * 0.10f;

        // Textura: más wire, más noiseTightness (menos tight = más textura)
        p.wireAmount     += req.character.textura * 0.15f;
        p.noiseTightness -= req.character.textura * 0.10f;

        // Impacto: más snap, más tightness
        p.snapAmount     += req.impacto * 0.25f;
        p.noiseTightness += req.impacto * 0.10f;

        // Energía
        float eMult = energyMult (req.energy);
        p.noiseDecay *= eMult;

        p.bodyFreq       = clamp (jitter (p.bodyFreq, 35.0f, rng), 120.0f, 300.0f);
        p.bodyDecay      = clamp (jitter (p.bodyDecay, 0.05f, rng), 0.03f, 0.25f);
        p.noiseDecay     = clamp (jitter (p.noiseDecay, 0.05f, rng), 0.04f, 0.25f);
        p.noiseColor     = clamp (jitter (p.noiseColor, 0.20f, rng), 0.0f, 1.0f);
        p.snapAmount     = clamp (jitter (p.snapAmount, 0.20f, rng), 0.0f, 1.0f);
        p.ringFreq       = clamp (jitter (p.ringFreq, 60.0f, rng), 200.0f, 500.0f);
        p.ringAmount     = clamp (jitter (p.ringAmount, 0.15f, rng), 0.0f, 0.6f);
        p.wireAmount     = clamp (jitter (p.wireAmount, 0.15f, rng), 0.0f, 0.7f);
        p.noiseTightness = clamp (jitter (p.noiseTightness, 0.15f, rng), 0.2f, 0.85f);
        p.bodyMix        = clamp (jitter (p.bodyMix, 0.10f, rng), 0.10f, 0.55f);
        p.noiseLP        = clamp (jitter (p.noiseLP, 1500.0f, rng), 3000.0f, 12000.0f);

        return p;
    }

    // ================================================================
    // HI-HAT
    // ================================================================
    HiHatParams generateHiHat (const GenerationRequest& req, std::mt19937& rng)
    {
        HiHatParams p = genrerules::hiHatBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: frecuencia más alta, más metálico
        p.freqRange += req.character.brillo * 3000.0f;
        p.metallic  += req.character.brillo * 0.15f;
        p.noiseColor += req.character.brillo * 0.15f;

        // Cuerpo: decay más largo, freq más baja
        p.closedDecay += req.character.cuerpo * 0.015f;
        p.openDecay   += req.character.cuerpo * 0.15f;
        p.freqRange   -= req.character.cuerpo * 1500.0f;

        // Textura: más ring
        p.ringAmount += req.character.textura * 0.20f;

        // Impacto: high pass más alto, más definición
        p.highPassFreq += req.impacto * 2000.0f;

        p.freqRange   = clamp (jitter (p.freqRange, 1500.0f, rng), 3000.0f, 14000.0f);
        p.metallic    = clamp (jitter (p.metallic, 0.20f, rng), 0.0f, 1.0f);
        p.noiseColor  = clamp (jitter (p.noiseColor, 0.20f, rng), 0.0f, 1.0f);
        p.openDecay   = clamp (jitter (p.openDecay, 0.12f, rng), 0.05f, 0.8f);
        p.closedDecay = clamp (jitter (p.closedDecay, 0.012f, rng), 0.005f, 0.08f);
        p.openAmount  = clamp (jitter (p.openAmount, 0.20f, rng), 0.0f, 1.0f);
        p.ringAmount  = clamp (jitter (p.ringAmount, 0.15f, rng), 0.0f, 0.7f);
        p.highPassFreq = clamp (jitter (p.highPassFreq, 800.0f, rng), 1500.0f, 10000.0f);

        return p;
    }

    // ================================================================
    // CLAP
    // ================================================================
    ClapParams generateClap (const GenerationRequest& req, std::mt19937& rng)
    {
        ClapParams p = genrerules::clapBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: noise más brillante, más tono, LP más alto
        p.noiseColor += req.character.brillo * 0.20f;
        p.toneAmount += req.character.brillo * 0.10f;
        p.noiseLP    += req.character.brillo * 1500.0f;

        // Cuerpo: más grosor, decay más largo
        p.thickness  += req.character.cuerpo * 0.25f;
        p.noiseDecay += req.character.cuerpo * 0.03f;

        // Textura: más capas, menos tightness (más spread)
        p.numLayers      += req.character.textura * 2.0f;
        p.reverbAmount   += req.character.textura * 0.10f;
        p.noiseTightness -= req.character.textura * 0.10f;

        // Impacto: capas más juntas, más snap, más tight
        p.layerSpacing   *= (1.0f - req.impacto * 0.4f);
        p.thickness      += req.impacto * 0.15f;
        p.transientSnap  += req.impacto * 0.15f;
        p.noiseTightness += req.impacto * 0.10f;

        p.numLayers      = clamp (jitter (p.numLayers, 1.5f, rng), 2.0f, 8.0f);
        p.layerSpacing   = clamp (jitter (p.layerSpacing, 0.006f, rng), 0.004f, 0.025f);
        p.noiseDecay     = clamp (jitter (p.noiseDecay, 0.05f, rng), 0.03f, 0.20f);
        p.noiseColor     = clamp (jitter (p.noiseColor, 0.20f, rng), 0.0f, 1.0f);
        p.toneAmount     = clamp (jitter (p.toneAmount, 0.12f, rng), 0.0f, 0.5f);
        p.toneFreq       = clamp (jitter (p.toneFreq, 200.0f, rng), 600.0f, 1500.0f);
        p.reverbAmount   = clamp (jitter (p.reverbAmount, 0.15f, rng), 0.0f, 0.6f);
        p.thickness      = clamp (jitter (p.thickness, 0.20f, rng), 0.1f, 1.0f);
        p.noiseTightness = clamp (jitter (p.noiseTightness, 0.15f, rng), 0.2f, 0.85f);
        p.noiseLP        = clamp (jitter (p.noiseLP, 1500.0f, rng), 3000.0f, 10000.0f);
        p.transientSnap  = clamp (jitter (p.transientSnap, 0.15f, rng), 0.2f, 1.0f);

        return p;
    }

    // ================================================================
    // PERC
    // ================================================================
    PercParams generatePerc (const GenerationRequest& req, std::mt19937& rng)
    {
        PercParams p = genrerules::percBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: freq más alta, más click
        p.freq        += req.character.brillo * 200.0f;
        p.clickAmount += req.character.brillo * 0.15f;

        // Cuerpo: más membrana, decay más largo
        p.membrane  += req.character.cuerpo * 0.25f;
        p.toneDecay += req.character.cuerpo * 0.04f;

        // Textura: más metallic, más inarmónico
        p.metallic     += req.character.textura * 0.30f;
        p.harmonicRatio += req.character.textura * 0.5f;

        // Impacto: más click, más pitch drop
        p.clickAmount += req.impacto * 0.20f;
        p.pitchDrop   += req.impacto * 6.0f;

        p.freq          = clamp (jitter (p.freq, 120.0f, rng), 150.0f, 2200.0f);
        p.toneDecay     = clamp (jitter (p.toneDecay, 0.04f, rng), 0.02f, 0.25f);
        p.metallic      = clamp (jitter (p.metallic, 0.20f, rng), 0.0f, 1.0f);
        p.woodiness     = clamp (jitter (p.woodiness, 0.20f, rng), 0.0f, 1.0f);
        p.membrane      = clamp (jitter (p.membrane, 0.20f, rng), 0.0f, 1.0f);
        p.clickAmount   = clamp (jitter (p.clickAmount, 0.20f, rng), 0.0f, 1.0f);
        p.pitchDrop     = clamp (jitter (p.pitchDrop, 8.0f, rng), 0.0f, 36.0f);
        p.harmonicRatio = clamp (jitter (p.harmonicRatio, 0.35f, rng), 0.5f, 3.0f);

        return p;
    }

    // ================================================================
    // BASS 808
    // ================================================================
    Bass808Params generateBass808 (const GenerationRequest& req, std::mt19937& rng)
    {
        Bass808Params p = genrerules::bass808Base (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: más armónicos, más saturación
        p.harmonics  += req.character.brillo * 0.15f;
        p.saturation += req.character.brillo * 0.12f;

        // Cuerpo: más sustain, más sub
        p.sustainLevel += req.character.cuerpo * 0.12f;
        p.subOctave    += req.character.cuerpo * 0.08f;
        p.tailLength   += req.character.cuerpo * 0.20f;

        // Textura: más saturación, más reese
        p.saturation += req.character.textura * 0.15f;
        p.reeseMix   += req.character.textura * 0.15f;

        // Movimiento: más glide, más reese detune
        p.glideTime    += req.character.movimiento * 0.06f;
        p.glideAmount  += req.character.movimiento * 3.0f;
        p.reeseDetune  += req.character.movimiento * 0.10f;

        // Impacto: más punchiness, más filter envelope
        p.punchiness    += req.impacto * 0.20f;
        p.filterEnvAmt  += req.impacto * 0.15f;

        // Energía
        float eMult = energyMult (req.energy);
        p.saturation *= eMult;
        p.harmonics  *= eMult;
        p.punchiness *= eMult;

        p.subFreq       = clamp (jitter (p.subFreq, 7.0f, rng), 28.0f, 65.0f);
        p.sustainLevel  = clamp (jitter (p.sustainLevel, 0.15f, rng), 0.15f, 0.90f);
        p.glideTime     = clamp (jitter (p.glideTime, 0.03f, rng), 0.0f, 0.15f);
        p.glideAmount   = clamp (jitter (p.glideAmount, 2.5f, rng), 0.0f, 10.0f);
        p.saturation    = clamp (jitter (p.saturation, 0.15f, rng), 0.0f, 0.8f);
        p.subOctave     = clamp (jitter (p.subOctave, 0.10f, rng), 0.0f, 0.4f);
        p.harmonics     = clamp (jitter (p.harmonics, 0.12f, rng), 0.0f, 0.6f);
        p.tailLength    = clamp (jitter (p.tailLength, 0.25f, rng), 0.2f, 1.8f);
        p.reeseMix      = clamp (jitter (p.reeseMix, 0.15f, rng), 0.0f, 1.0f);
        p.reeseDetune   = clamp (jitter (p.reeseDetune, 0.12f, rng), 0.0f, 0.8f);
        p.punchiness    = clamp (jitter (p.punchiness, 0.15f, rng), 0.0f, 1.0f);
        p.filterEnvAmt  = clamp (jitter (p.filterEnvAmt, 0.12f, rng), 0.0f, 1.0f);

        return p;
    }

    // ================================================================
    // LEAD
    // ================================================================
    LeadParams generateLead (const GenerationRequest& req, std::mt19937& rng)
    {
        LeadParams p = genrerules::leadBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: más brightness, más saw, más filter envelope
        p.brightness   += req.character.brillo * 0.25f;
        p.waveformMix  -= req.character.brillo * 0.20f; // más saw
        p.filterEnvAmt += req.character.brillo * 0.15f;

        // Cuerpo: más unison, más detune, más sub
        p.unisonVoices += req.character.cuerpo * 2.0f;
        p.oscDetune    += req.character.cuerpo * 0.08f;
        p.subOscLevel  += req.character.cuerpo * 0.12f;

        // Textura: más pulse width, más saturación
        p.pulseWidth += req.character.textura * 0.15f;

        // Movimiento: más vibrato, más filter envelope
        p.vibratoDepth += req.character.movimiento * 0.15f;
        p.portamento   += req.character.movimiento * 0.03f;
        p.filterEnvAmt += req.character.movimiento * 0.10f;

        // Impacto: más brightness, más filter envelope
        p.brightness   += req.impacto * 0.15f;
        p.filterEnvAmt += req.impacto * 0.10f;

        // Energía
        float eMult = energyMult (req.energy);
        p.unisonVoices *= eMult;
        p.filterEnvAmt *= eMult;

        p.oscDetune    = clamp (jitter (p.oscDetune, 0.08f, rng), 0.0f, 0.5f);
        p.pulseWidth   = clamp (jitter (p.pulseWidth, 0.15f, rng), 0.1f, 0.9f);
        p.vibratoRate  = clamp (jitter (p.vibratoRate, 1.5f, rng), 2.0f, 10.0f);
        p.vibratoDepth = clamp (jitter (p.vibratoDepth, 0.08f, rng), 0.0f, 0.5f);
        p.brightness   = clamp (jitter (p.brightness, 0.15f, rng), 0.1f, 1.0f);
        p.waveformMix  = clamp (jitter (p.waveformMix, 0.20f, rng), 0.0f, 1.0f);
        p.unisonVoices = clamp (jitter (p.unisonVoices, 1.0f, rng), 1.0f, 8.0f);
        p.portamento   = clamp (jitter (p.portamento, 0.03f, rng), 0.0f, 0.15f);
        p.filterEnvAmt = clamp (jitter (p.filterEnvAmt, 0.15f, rng), 0.0f, 0.8f);
        p.subOscLevel  = clamp (jitter (p.subOscLevel, 0.10f, rng), 0.0f, 0.5f);

        return p;
    }

    // ================================================================
    // PLUCK
    // ================================================================
    PluckParams generatePluck (const GenerationRequest& req, std::mt19937& rng)
    {
        PluckParams p = genrerules::pluckBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: más brightness, menos damping
        p.brightness += req.character.brillo * 0.25f;
        p.damping    -= req.character.brillo * 0.15f;

        // Cuerpo: más resonancia, más decay
        p.bodyResonance += req.character.cuerpo * 0.20f;
        p.decayTime     += req.character.cuerpo * 0.15f;

        // Textura: más armónicos
        p.harmonics += req.character.textura * 0.20f;

        // Movimiento: más stereo width
        p.stereoWidth += req.character.movimiento * 0.20f;

        // Impacto: más string tension, pick más arriba
        p.stringTension += req.impacto * 0.15f;
        p.pickPosition  += req.impacto * 0.10f;

        p.brightness    = clamp (jitter (p.brightness, 0.15f, rng), 0.1f, 1.0f);
        p.bodyResonance = clamp (jitter (p.bodyResonance, 0.15f, rng), 0.0f, 0.7f);
        p.decayTime     = clamp (jitter (p.decayTime, 0.15f, rng), 0.1f, 1.5f);
        p.damping       = clamp (jitter (p.damping, 0.15f, rng), 0.1f, 0.9f);
        p.pickPosition  = clamp (jitter (p.pickPosition, 0.15f, rng), 0.1f, 0.9f);
        p.stringTension = clamp (jitter (p.stringTension, 0.15f, rng), 0.2f, 0.9f);
        p.harmonics     = clamp (jitter (p.harmonics, 0.12f, rng), 0.0f, 0.7f);
        p.stereoWidth   = clamp (jitter (p.stereoWidth, 0.15f, rng), 0.0f, 0.8f);

        return p;
    }

    // ================================================================
    // PAD
    // ================================================================
    PadParams generatePad (const GenerationRequest& req, std::mt19937& rng)
    {
        PadParams p = genrerules::padBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: menos warmth, más filter cutoff (ya modulado en base)
        p.warmth -= req.character.brillo * 0.25f;

        // Cuerpo: más unison, más detune, más sub
        p.unisonVoices += req.character.cuerpo * 3.0f;
        p.detuneSpread += req.character.cuerpo * 0.10f;
        p.chorusAmount += req.character.cuerpo * 0.10f;
        p.subLevel     += req.character.cuerpo * 0.15f;

        // Textura: más chorus, más drift, más air
        p.chorusAmount += req.character.textura * 0.15f;
        p.driftRate    += req.character.textura * 0.10f;
        p.airAmount    += req.character.textura * 0.10f;

        // Movimiento: más evolution, más filter sweep
        p.evolutionRate += req.character.movimiento * 0.12f;
        p.filterSweep   += req.character.movimiento * 0.15f;

        // Impacto: más presencia, menos air (más definido)
        p.detuneSpread += req.impacto * 0.05f;
        p.airAmount    -= req.impacto * 0.05f;

        // Energía
        float eMult = energyMult (req.energy);
        p.unisonVoices *= eMult;
        p.stereoWidth  *= (0.8f + eMult * 0.2f);
        p.subLevel     *= eMult;

        p.unisonVoices = clamp (jitter (p.unisonVoices, 1.5f, rng), 2.0f, 16.0f);
        p.detuneSpread = clamp (jitter (p.detuneSpread, 0.08f, rng), 0.02f, 0.5f);
        p.driftRate    = clamp (jitter (p.driftRate, 0.12f, rng), 0.05f, 0.6f);
        p.warmth       = clamp (jitter (p.warmth, 0.18f, rng), 0.1f, 0.9f);
        p.evolutionRate = clamp (jitter (p.evolutionRate, 0.08f, rng), 0.01f, 0.4f);
        p.filterSweep  = clamp (jitter (p.filterSweep, 0.10f, rng), 0.0f, 0.5f);
        p.stereoWidth  = clamp (jitter (p.stereoWidth, 0.15f, rng), 0.2f, 1.0f);
        p.chorusAmount = clamp (jitter (p.chorusAmount, 0.15f, rng), 0.0f, 0.6f);
        p.airAmount    = clamp (jitter (p.airAmount, 0.10f, rng), 0.0f, 0.5f);
        p.subLevel     = clamp (jitter (p.subLevel, 0.12f, rng), 0.0f, 0.7f);

        return p;
    }

    // ================================================================
    // TEXTURE
    // ================================================================
    TextureParams generateTexture (const GenerationRequest& req, std::mt19937& rng)
    {
        TextureParams p = genrerules::textureBase (req.genre);
        p.base.attack = attackTimeFor (req.attack, req.instrument);

        applyCharacterToBase (p.base, req.character, req.impacto, req.energy);

        // Brillo: tilt positivo, noise más brillante
        p.spectralTilt += req.character.brillo * 0.30f;
        p.noiseColor   += req.character.brillo * 0.20f;

        // Cuerpo: granos más grandes, más densos
        p.grainSize += req.character.cuerpo * 0.04f;
        p.density   += req.character.cuerpo * 0.15f;

        // Textura: más scatter, más complejidad
        p.scatter += req.character.textura * 0.20f;

        // Movimiento: más movimiento y evolution
        p.movement      += req.character.movimiento * 0.30f;
        p.evolutionRate += req.character.movimiento * 0.12f;

        // Impacto: más densidad
        p.density += req.impacto * 0.15f;

        // Energía
        float eMult = energyMult (req.energy);
        p.density *= eMult;
        p.stereoWidth *= (0.8f + eMult * 0.2f);

        p.density       = clamp (jitter (p.density, 0.20f, rng), 0.1f, 1.0f);
        p.grainSize     = clamp (jitter (p.grainSize, 0.025f, rng), 0.01f, 0.15f);
        p.scatter       = clamp (jitter (p.scatter, 0.15f, rng), 0.0f, 0.7f);
        p.spectralTilt  = clamp (jitter (p.spectralTilt, 0.20f, rng), -0.8f, 0.8f);
        p.movement      = clamp (jitter (p.movement, 0.20f, rng), 0.0f, 1.0f);
        p.noiseColor    = clamp (jitter (p.noiseColor, 0.20f, rng), 0.0f, 1.0f);
        p.stereoWidth   = clamp (jitter (p.stereoWidth, 0.15f, rng), 0.2f, 1.0f);
        p.evolutionRate = clamp (jitter (p.evolutionRate, 0.10f, rng), 0.02f, 0.5f);

        return p;
    }

    // ================================================================
    // EFFECTS (Fase 4)
    // ================================================================
    EffectChainParams generateEffects (const GenerationRequest& req, std::mt19937& rng)
    {
        // Obtener perfil base de efectos según género + instrumento
        EffectChainParams fx = genrerules::effectsBase (req.genre, req.instrument);

        // === Modulación por controles de carácter ===

        // Brillo: más EQ high, más tono en distorsión
        if (fx.eq.enabled)
        {
            fx.eq.highGain += req.character.brillo * 1.5f;
            fx.eq.highGain = clamp (fx.eq.highGain, -12.0f, 12.0f);
        }
        if (fx.distortion.enabled)
        {
            fx.distortion.tone += req.character.brillo * 0.15f;
            fx.distortion.tone = clamp (fx.distortion.tone, 0.0f, 1.0f);
        }

        // Cuerpo: más EQ low, más compresión
        if (fx.eq.enabled)
        {
            fx.eq.lowGain += req.character.cuerpo * 1.5f;
            fx.eq.lowGain = clamp (fx.eq.lowGain, -12.0f, 12.0f);
        }
        if (fx.compressor.enabled)
        {
            fx.compressor.ratio += req.character.cuerpo * 1.0f;
            fx.compressor.ratio = clamp (fx.compressor.ratio, 1.0f, 20.0f);
        }

        // Textura: más distorsión, más bitcrusher
        if (fx.distortion.enabled)
        {
            fx.distortion.drive += req.character.textura * 0.08f;
            fx.distortion.drive = clamp (fx.distortion.drive, 0.0f, 1.0f);
        }
        if (fx.bitcrusher.enabled)
        {
            fx.bitcrusher.bitDepth -= req.character.textura * 2.0f;
            fx.bitcrusher.bitDepth = clamp (fx.bitcrusher.bitDepth, 4.0f, 16.0f);
        }

        // Movimiento: más chorus, más phaser, más delay
        if (fx.chorus.enabled)
        {
            fx.chorus.depth += req.character.movimiento * 0.20f;
            fx.chorus.depth = clamp (fx.chorus.depth, 0.0f, 1.0f);
        }
        if (fx.phaser.enabled)
        {
            fx.phaser.depth += req.character.movimiento * 0.20f;
            fx.phaser.depth = clamp (fx.phaser.depth, 0.0f, 1.0f);
        }
        if (fx.delay.enabled)
        {
            fx.delay.feedback += req.character.movimiento * 0.10f;
            fx.delay.feedback = clamp (fx.delay.feedback, 0.0f, 0.85f);
        }

        // Impacto: más compresión, menos reverb
        if (fx.compressor.enabled)
        {
            fx.compressor.threshold -= req.impacto * 2.0f;
            fx.compressor.threshold = clamp (fx.compressor.threshold, -30.0f, 0.0f);
        }
        if (fx.reverb.enabled)
        {
            fx.reverb.mix *= (1.0f - req.impacto * 0.3f);
            fx.reverb.mix = clamp (fx.reverb.mix, 0.0f, 1.0f);
        }

        // Energía: más distorsión, más compresión agresiva
        float eMult = energyMult (req.energy);
        if (fx.distortion.enabled)
        {
            fx.distortion.drive *= eMult;
            fx.distortion.drive = clamp (fx.distortion.drive, 0.0f, 1.0f);
        }
        if (fx.compressor.enabled)
        {
            fx.compressor.ratio *= eMult;
            fx.compressor.ratio = clamp (fx.compressor.ratio, 1.0f, 20.0f);
        }

        // === Randomización sutil de efectos habilitados ===
        if (fx.reverb.enabled)
        {
            fx.reverb.size    = clamp (jitter (fx.reverb.size, 0.08f, rng), 0.1f, 1.0f);
            fx.reverb.decay   = clamp (jitter (fx.reverb.decay, 0.08f, rng), 0.1f, 1.0f);
            fx.reverb.mix     = clamp (jitter (fx.reverb.mix, 0.04f, rng), 0.0f, 0.6f);
        }
        if (fx.delay.enabled)
        {
            fx.delay.time     = clamp (jitter (fx.delay.time, 0.03f, rng), 0.05f, 0.8f);
            fx.delay.feedback = clamp (jitter (fx.delay.feedback, 0.05f, rng), 0.0f, 0.85f);
            fx.delay.mix      = clamp (jitter (fx.delay.mix, 0.04f, rng), 0.0f, 0.5f);
        }
        if (fx.chorus.enabled)
        {
            fx.chorus.rate  = clamp (jitter (fx.chorus.rate, 0.3f, rng), 0.2f, 5.0f);
            fx.chorus.depth = clamp (jitter (fx.chorus.depth, 0.08f, rng), 0.1f, 1.0f);
            fx.chorus.mix   = clamp (jitter (fx.chorus.mix, 0.05f, rng), 0.0f, 0.6f);
        }
        if (fx.phaser.enabled)
        {
            fx.phaser.rate     = clamp (jitter (fx.phaser.rate, 0.2f, rng), 0.1f, 3.0f);
            fx.phaser.depth    = clamp (jitter (fx.phaser.depth, 0.08f, rng), 0.1f, 1.0f);
            fx.phaser.feedback = clamp (jitter (fx.phaser.feedback, 0.06f, rng), 0.0f, 0.85f);
            fx.phaser.mix      = clamp (jitter (fx.phaser.mix, 0.05f, rng), 0.0f, 0.6f);
        }
        if (fx.distortion.enabled)
        {
            fx.distortion.drive = clamp (jitter (fx.distortion.drive, 0.05f, rng), 0.0f, 1.0f);
            fx.distortion.tone  = clamp (jitter (fx.distortion.tone, 0.06f, rng), 0.0f, 1.0f);
            fx.distortion.mix   = clamp (jitter (fx.distortion.mix, 0.05f, rng), 0.0f, 1.0f);
        }
        if (fx.compressor.enabled)
        {
            fx.compressor.threshold = clamp (jitter (fx.compressor.threshold, 2.0f, rng), -30.0f, 0.0f);
            fx.compressor.ratio     = clamp (jitter (fx.compressor.ratio, 0.5f, rng), 1.0f, 20.0f);
            fx.compressor.makeupGain = clamp (jitter (fx.compressor.makeupGain, 1.0f, rng), 0.0f, 12.0f);
        }
        if (fx.eq.enabled)
        {
            fx.eq.lowGain  = clamp (jitter (fx.eq.lowGain, 0.5f, rng), -12.0f, 12.0f);
            fx.eq.midGain  = clamp (jitter (fx.eq.midGain, 0.5f, rng), -12.0f, 12.0f);
            fx.eq.highGain = clamp (jitter (fx.eq.highGain, 0.5f, rng), -12.0f, 12.0f);
        }

        return fx;
    }
};
