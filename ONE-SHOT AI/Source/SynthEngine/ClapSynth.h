#pragma once

#include <JuceHeader.h>
#include "../Params/InstrumentParams.h"
#include "../DSP/NoiseGenerator.h"
#include "../DSP/Envelope.h"
#include "../DSP/SynthFilter.h"
#include "../DSP/Saturator.h"
#include "../DSP/DCBlocker.h"
#include "../DSP/Oscillator.h"
#include "SynthUtils.h"

// Clap profesional v3 — Splice-quality
//
// Modelo basado en claps reales (TR-808/909 + samples pro):
//   — Transiente inicial DURO ("crack") con white noise a través de BP ancho
//   — Layer flutter con gaps claros entre palmadas (no blurred)
//   — Body resonante post-flutter (1.5-3 kHz)
//   — Tail con decay controlable
//   — Saturación + compresión para pegada máxima
//   — White noise como base (NO pink — los claps necesitan ese "crack" agudo)

class ClapSynth
{
public:
    juce::AudioBuffer<float> generate (const ClapParams& p, double sampleRate, unsigned int seed)
    {
        const float sr = (float) sampleRate;

        int numLayers = std::max (2, std::min (8, (int) p.numLayers));

        // Spacing entre layers: controlado para que se oigan como palmadas separadas
        float effectiveSpacing = std::max (0.005f,
            std::min (p.layerSpacing, 0.020f / std::max (1.0f, (float) numLayers)));
        float totalBurstTime = (float)(numLayers - 1) * effectiveSpacing;

        float duration = totalBurstTime + p.noiseDecay * 2.0f + 0.15f;
        int numSamples = synthutil::durationInSamples (duration, sampleRate);

        juce::AudioBuffer<float> buffer (2, numSamples);
        buffer.clear();

        dsputil::NoiseGenerator noise;
        noise.setSeed (seed);

        dsputil::NoiseGenerator crackNoise;
        crackNoise.setSeed (seed + 4219);

        // === FILTROS ===

        // BP ancho para el CRACK — corazón del sonido del clap (1.2-5 kHz)
        dsputil::SVFilter crackBP;
        float crackFreq = 1800.0f + p.noiseColor * 2000.0f;
        crackBP.setParameters (crackFreq, 0.12f + p.noiseTightness * 0.20f,
                               FilterType::BandPass, sr);

        // BP para body — resonancia más estrecha, da "cuerpo" al clap
        dsputil::SVFilter bodyBP;
        float bodyFreq = 1200.0f + p.noiseColor * 1500.0f;
        bodyBP.setParameters (bodyFreq, 0.18f + p.thickness * 0.20f,
                              FilterType::BandPass, sr);

        // LP para controlar brillo máximo — elimina hiss residual
        dsputil::SVFilter noiseLP;
        float lpFreq = std::min (p.noiseLP, sr * 0.48f);
        noiseLP.setParameters (lpFreq, 0.05f, FilterType::LowPass, sr);

        // HP sutil para limpiar sub innecesario
        dsputil::SVFilter noiseHP;
        noiseHP.setParameters (250.0f, 0.05f, FilterType::HighPass, sr);

        // BP para snap agudo (presencia 3-5 kHz)
        dsputil::SVFilter snapBP;
        float snapFreq = 3000.0f + p.noiseColor * 2000.0f;
        snapBP.setParameters (snapFreq, 0.20f + p.transientSnap * 0.20f,
                              FilterType::BandPass, sr);

        dsputil::SVFilter mainFilter;
        mainFilter.setParameters (p.base.filterCutoff, p.base.filterResonance,
                                  p.base.filterType, sr);

        dsputil::Saturator saturator;
        dsputil::DCBlocker dcBlock;

        // === LAYER INFO ===
        // Cada palmada: tiempo de inicio, amplitud creciente, decay cortísimo
        struct LayerInfo { float startTime; float amplitude; float decayTime; };
        std::vector<LayerInfo> layers ((size_t) numLayers);
        for (int l = 0; l < numLayers; ++l)
        {
            layers[(size_t) l].startTime = (float) l * effectiveSpacing;
            // Amplitud creciente: última palmada es la más fuerte
            float progress = (float)(l + 1) / (float) numLayers;
            layers[(size_t) l].amplitude = 0.3f + 0.7f * progress;
            // Decay corto para cada palmada (1-3 ms) — clave para que se oigan separadas
            layers[(size_t) l].decayTime = 0.001f + p.thickness * 0.002f;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float t = (float) i / sr;

            // === INITIAL CRACK: transiente duro que da pegada ===
            float crackRaw = crackNoise.next();  // WHITE noise
            float crackShaped = crackBP.process (crackRaw);
            float crackEnv = std::exp (-t / (0.00015f + (1.0f - p.transientSnap) * 0.00025f));
            float crack = crackShaped * crackEnv * (0.5f + p.transientSnap * 0.5f);

            // === LAYER FLUTTER: palmadas separadas ===
            float burstSig = 0.0f;
            for (int l = 0; l < numLayers; ++l)
            {
                float dt = t - layers[(size_t) l].startTime;
                if (dt >= 0.0f)
                {
                    // Cada layer: ataque instantáneo + decay rápido
                    float layerEnv = std::exp (-dt / layers[(size_t) l].decayTime);
                    if (layerEnv > 0.001f)
                        burstSig += layerEnv * layers[(size_t) l].amplitude;
                }
            }

            // Noise para el burst: WHITE, no pink — esencial para el "crack"
            float burstNoise = noise.next();
            float burstShaped = bodyBP.process (burstNoise);
            float burst = burstShaped * std::min (burstSig, 1.5f);

            // === SNAP: presencia aguda (3-5 kHz) ===
            float snapRaw = noise.next();
            float snapShaped = snapBP.process (snapRaw);
            // Snap sigue la envolvente del burst pero con su propio decay corto
            float snapEnv = 0.0f;
            for (int l = 0; l < numLayers; ++l)
            {
                float dt = t - layers[(size_t) l].startTime;
                if (dt >= 0.0f)
                {
                    float sEnv = std::exp (-dt / 0.0008f);
                    if (sEnv > 0.001f)
                        snapEnv += sEnv * layers[(size_t) l].amplitude;
                }
            }
            float snap = snapShaped * std::min (snapEnv, 1.2f) * p.transientSnap * 0.5f;

            // === BODY + TAIL: decay después del flutter ===
            float bodyNoise = noise.nextColored (p.noiseColor * 0.6f);
            float bodyShaped = bodyBP.process (bodyNoise);
            // Body arranca al final del burst, decae con noiseDecay
            float bodyEnv = 0.0f;
            if (t > totalBurstTime * 0.5f)
            {
                float dt = t - totalBurstTime * 0.5f;
                float bodyAttack = std::min (dt / 0.002f, 1.0f);
                float bodyDecay = std::exp (-dt / std::max (0.005f, p.noiseDecay));
                bodyEnv = bodyAttack * bodyDecay;
            }
            float body = bodyShaped * bodyEnv * 0.4f;

            // === TAIL: cola de reverb integrada ===
            float tailEnv = 0.0f;
            if (t > totalBurstTime)
            {
                float dt = t - totalBurstTime;
                tailEnv = std::exp (-dt / std::max (0.01f, p.noiseDecay * 1.5f));
            }
            float tailNoise = noise.nextColored (std::max (0.0f, p.noiseColor - 0.2f));
            float tail = bodyBP.process (tailNoise) * tailEnv * p.reverbAmount * 0.35f;

            // === TONO (opcional): resonancia que da peso ===
            float toneContrib = 0.0f;
            if (p.toneAmount > 0.01f)
            {
                float toneEnv = std::exp (-t / std::max (0.003f, p.noiseDecay * 0.3f));
                // Sigue las layers para que el tono esté en los transientes
                float toneBurstEnv = 0.0f;
                for (int l = 0; l < numLayers; ++l)
                {
                    float dt = t - layers[(size_t) l].startTime;
                    if (dt >= 0.0f)
                    {
                        float tEnv = std::exp (-dt / 0.004f);
                        if (tEnv > 0.001f)
                            toneBurstEnv += tEnv * layers[(size_t) l].amplitude;
                    }
                }
                toneContrib = std::sin (dsputil::TWO_PI * p.toneFreq * t)
                            * p.toneAmount * std::min (toneBurstEnv, 1.0f) * toneEnv * 0.2f;
            }

            // === MIX FINAL ===
            float sample = crack + burst + snap + body + tail + toneContrib;

            // === LP + HP ===
            sample = noiseLP.process (sample);
            sample = noiseHP.process (sample);

            // === SATURACIÓN para pegada ===
            float satAmount = 0.15f + p.thickness * 0.25f;
            sample = saturator.process (sample, satAmount, dsputil::SaturationMode::Tape);

            // === DC BLOCK ===
            sample = dcBlock.process (sample);

            sample = mainFilter.process (sample);
            sample *= p.base.volume;

            float L, R;
            synthutil::panMonoToStereo (sample, p.base.pan, L, R);
            buffer.setSample (0, i, L);
            buffer.setSample (1, i, R);
        }

        synthutil::applyFadeOut (buffer, (int)(0.008f * sr));
        return buffer;
    }
};
