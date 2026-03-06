#pragma once

#include <random>
#include <cmath>
#include <algorithm>
#include "../Params/SynthParams.h"
#include "GenreRules.h"

// Generador de parámetros basado en reglas musicales (Opción A).
// Toma un GenerationRequest del UX y produce un GenerationResult
// con parámetros coherentes, únicos y listos para el motor de síntesis.
// Diseñado para ser reemplazable por un modelo ML en el futuro.

class ParameterGenerator
{
public:
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

private:
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

        // Randomización controlada
        p.subFreq       = clamp (jitter (p.subFreq, 4.0f, rng), 30.0f, 70.0f);
        p.clickAmount   = clamp (jitter (p.clickAmount, 0.08f, rng), 0.0f, 1.0f);
        p.bodyDecay     = clamp (jitter (p.bodyDecay, 0.03f, rng), 0.05f, 0.6f);
        p.pitchDrop     = clamp (jitter (p.pitchDrop, 4.0f, rng), 8.0f, 72.0f);
        p.pitchDropTime = clamp (jitter (p.pitchDropTime, 0.005f, rng), 0.005f, 0.1f);
        p.driveAmount   = clamp (jitter (p.driveAmount, 0.05f, rng), 0.0f, 0.8f);
        p.subLevel      = clamp (jitter (p.subLevel, 0.05f, rng), 0.3f, 1.0f);
        p.tailLength    = clamp (jitter (p.tailLength, 0.05f, rng), 0.1f, 1.0f);

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

        p.bodyFreq       = clamp (jitter (p.bodyFreq, 15.0f, rng), 120.0f, 300.0f);
        p.bodyDecay      = clamp (jitter (p.bodyDecay, 0.02f, rng), 0.03f, 0.25f);
        p.noiseDecay     = clamp (jitter (p.noiseDecay, 0.02f, rng), 0.04f, 0.25f);
        p.noiseColor     = clamp (jitter (p.noiseColor, 0.08f, rng), 0.0f, 1.0f);
        p.snapAmount     = clamp (jitter (p.snapAmount, 0.08f, rng), 0.0f, 1.0f);
        p.ringFreq       = clamp (jitter (p.ringFreq, 30.0f, rng), 200.0f, 500.0f);
        p.ringAmount     = clamp (jitter (p.ringAmount, 0.06f, rng), 0.0f, 0.6f);
        p.wireAmount     = clamp (jitter (p.wireAmount, 0.06f, rng), 0.0f, 0.7f);
        p.noiseTightness = clamp (jitter (p.noiseTightness, 0.06f, rng), 0.2f, 0.85f);
        p.bodyMix        = clamp (jitter (p.bodyMix, 0.05f, rng), 0.10f, 0.55f);
        p.noiseLP        = clamp (jitter (p.noiseLP, 500.0f, rng), 3000.0f, 12000.0f);

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

        p.freqRange   = clamp (jitter (p.freqRange, 500.0f, rng), 3000.0f, 14000.0f);
        p.metallic    = clamp (jitter (p.metallic, 0.08f, rng), 0.0f, 1.0f);
        p.noiseColor  = clamp (jitter (p.noiseColor, 0.08f, rng), 0.0f, 1.0f);
        p.openDecay   = clamp (jitter (p.openDecay, 0.05f, rng), 0.05f, 0.8f);
        p.closedDecay = clamp (jitter (p.closedDecay, 0.005f, rng), 0.005f, 0.08f);
        p.openAmount  = clamp (jitter (p.openAmount, 0.08f, rng), 0.0f, 1.0f);
        p.ringAmount  = clamp (jitter (p.ringAmount, 0.06f, rng), 0.0f, 0.7f);
        p.highPassFreq = clamp (jitter (p.highPassFreq, 300.0f, rng), 1500.0f, 10000.0f);

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

        p.numLayers      = clamp (jitter (p.numLayers, 0.5f, rng), 2.0f, 8.0f);
        p.layerSpacing   = clamp (jitter (p.layerSpacing, 0.003f, rng), 0.004f, 0.025f);
        p.noiseDecay     = clamp (jitter (p.noiseDecay, 0.02f, rng), 0.03f, 0.20f);
        p.noiseColor     = clamp (jitter (p.noiseColor, 0.08f, rng), 0.0f, 1.0f);
        p.toneAmount     = clamp (jitter (p.toneAmount, 0.05f, rng), 0.0f, 0.5f);
        p.toneFreq       = clamp (jitter (p.toneFreq, 100.0f, rng), 600.0f, 1500.0f);
        p.reverbAmount   = clamp (jitter (p.reverbAmount, 0.06f, rng), 0.0f, 0.6f);
        p.thickness      = clamp (jitter (p.thickness, 0.08f, rng), 0.1f, 1.0f);
        p.noiseTightness = clamp (jitter (p.noiseTightness, 0.06f, rng), 0.2f, 0.85f);
        p.noiseLP        = clamp (jitter (p.noiseLP, 500.0f, rng), 3000.0f, 10000.0f);
        p.transientSnap  = clamp (jitter (p.transientSnap, 0.06f, rng), 0.2f, 1.0f);

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

        p.freq          = clamp (jitter (p.freq, 40.0f, rng), 150.0f, 800.0f);
        p.toneDecay     = clamp (jitter (p.toneDecay, 0.015f, rng), 0.02f, 0.25f);
        p.metallic      = clamp (jitter (p.metallic, 0.08f, rng), 0.0f, 1.0f);
        p.woodiness     = clamp (jitter (p.woodiness, 0.08f, rng), 0.0f, 1.0f);
        p.membrane      = clamp (jitter (p.membrane, 0.08f, rng), 0.0f, 1.0f);
        p.clickAmount   = clamp (jitter (p.clickAmount, 0.08f, rng), 0.0f, 1.0f);
        p.pitchDrop     = clamp (jitter (p.pitchDrop, 3.0f, rng), 0.0f, 36.0f);
        p.harmonicRatio = clamp (jitter (p.harmonicRatio, 0.15f, rng), 0.5f, 3.0f);

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

        p.subFreq       = clamp (jitter (p.subFreq, 3.0f, rng), 28.0f, 65.0f);
        p.sustainLevel  = clamp (jitter (p.sustainLevel, 0.06f, rng), 0.15f, 0.90f);
        p.glideTime     = clamp (jitter (p.glideTime, 0.01f, rng), 0.0f, 0.15f);
        p.glideAmount   = clamp (jitter (p.glideAmount, 1.0f, rng), 0.0f, 10.0f);
        p.saturation    = clamp (jitter (p.saturation, 0.06f, rng), 0.0f, 0.8f);
        p.subOctave     = clamp (jitter (p.subOctave, 0.04f, rng), 0.0f, 0.4f);
        p.harmonics     = clamp (jitter (p.harmonics, 0.05f, rng), 0.0f, 0.6f);
        p.tailLength    = clamp (jitter (p.tailLength, 0.10f, rng), 0.2f, 1.8f);
        p.reeseMix      = clamp (jitter (p.reeseMix, 0.06f, rng), 0.0f, 1.0f);
        p.reeseDetune   = clamp (jitter (p.reeseDetune, 0.05f, rng), 0.0f, 0.8f);
        p.punchiness    = clamp (jitter (p.punchiness, 0.06f, rng), 0.0f, 1.0f);
        p.filterEnvAmt  = clamp (jitter (p.filterEnvAmt, 0.05f, rng), 0.0f, 1.0f);

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

        p.oscDetune    = clamp (jitter (p.oscDetune, 0.03f, rng), 0.0f, 0.5f);
        p.pulseWidth   = clamp (jitter (p.pulseWidth, 0.06f, rng), 0.1f, 0.9f);
        p.vibratoRate  = clamp (jitter (p.vibratoRate, 0.5f, rng), 2.0f, 10.0f);
        p.vibratoDepth = clamp (jitter (p.vibratoDepth, 0.03f, rng), 0.0f, 0.5f);
        p.brightness   = clamp (jitter (p.brightness, 0.06f, rng), 0.1f, 1.0f);
        p.waveformMix  = clamp (jitter (p.waveformMix, 0.08f, rng), 0.0f, 1.0f);
        p.unisonVoices = clamp (jitter (p.unisonVoices, 0.3f, rng), 1.0f, 8.0f);
        p.portamento   = clamp (jitter (p.portamento, 0.01f, rng), 0.0f, 0.15f);
        p.filterEnvAmt = clamp (jitter (p.filterEnvAmt, 0.06f, rng), 0.0f, 0.8f);
        p.subOscLevel  = clamp (jitter (p.subOscLevel, 0.04f, rng), 0.0f, 0.5f);

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

        p.brightness    = clamp (jitter (p.brightness, 0.06f, rng), 0.1f, 1.0f);
        p.bodyResonance = clamp (jitter (p.bodyResonance, 0.06f, rng), 0.0f, 0.7f);
        p.decayTime     = clamp (jitter (p.decayTime, 0.06f, rng), 0.1f, 1.5f);
        p.damping       = clamp (jitter (p.damping, 0.06f, rng), 0.1f, 0.9f);
        p.pickPosition  = clamp (jitter (p.pickPosition, 0.06f, rng), 0.1f, 0.9f);
        p.stringTension = clamp (jitter (p.stringTension, 0.06f, rng), 0.2f, 0.9f);
        p.harmonics     = clamp (jitter (p.harmonics, 0.05f, rng), 0.0f, 0.7f);
        p.stereoWidth   = clamp (jitter (p.stereoWidth, 0.06f, rng), 0.0f, 0.8f);

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

        p.unisonVoices = clamp (jitter (p.unisonVoices, 0.5f, rng), 2.0f, 16.0f);
        p.detuneSpread = clamp (jitter (p.detuneSpread, 0.03f, rng), 0.02f, 0.5f);
        p.driftRate    = clamp (jitter (p.driftRate, 0.05f, rng), 0.05f, 0.6f);
        p.warmth       = clamp (jitter (p.warmth, 0.08f, rng), 0.1f, 0.9f);
        p.evolutionRate = clamp (jitter (p.evolutionRate, 0.03f, rng), 0.01f, 0.4f);
        p.filterSweep  = clamp (jitter (p.filterSweep, 0.04f, rng), 0.0f, 0.5f);
        p.stereoWidth  = clamp (jitter (p.stereoWidth, 0.06f, rng), 0.2f, 1.0f);
        p.chorusAmount = clamp (jitter (p.chorusAmount, 0.06f, rng), 0.0f, 0.6f);
        p.airAmount    = clamp (jitter (p.airAmount, 0.04f, rng), 0.0f, 0.5f);
        p.subLevel     = clamp (jitter (p.subLevel, 0.05f, rng), 0.0f, 0.7f);

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

        p.density       = clamp (jitter (p.density, 0.08f, rng), 0.1f, 1.0f);
        p.grainSize     = clamp (jitter (p.grainSize, 0.01f, rng), 0.01f, 0.15f);
        p.scatter       = clamp (jitter (p.scatter, 0.06f, rng), 0.0f, 0.7f);
        p.spectralTilt  = clamp (jitter (p.spectralTilt, 0.08f, rng), -0.8f, 0.8f);
        p.movement      = clamp (jitter (p.movement, 0.08f, rng), 0.0f, 1.0f);
        p.noiseColor    = clamp (jitter (p.noiseColor, 0.08f, rng), 0.0f, 1.0f);
        p.stereoWidth   = clamp (jitter (p.stereoWidth, 0.06f, rng), 0.2f, 1.0f);
        p.evolutionRate = clamp (jitter (p.evolutionRate, 0.04f, rng), 0.02f, 0.5f);

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
