# Flujo de desarrollo completo – AI One-Shot Generator

Este documento define todas las fases del desarrollo para asegurar generación de sonidos de alta calidad, modularidad y coherencia musical.

---

## Fase 1: Definición del espacio paramétrico

- Crear estructuras de parámetros completas para cada instrumento: kicks, snares, hi-hats, 808s, leads, plucks, pads, efectos/texturas.
- Incluir todos los parámetros de síntesis posibles: osciladores, formas de onda, envelopes (ADSR), pitch decay, amp decay, filtros (cutoff/resonancia), distorsión/saturación, LFOs, pan, volumen, click/noise, modulaciones, granular si aplica.
- Definir controles de UX: instrumento, estilo/género (Trap, Hip-hop, Techno, House, Ambient), carácter (Brillo, Cuerpo, Textura, Movimiento), tipo de ataque (Rápido/Medio/Lento), impacto (0–1).
- Representación en structs o JSON para Claude Code.

## Fase 2: Motor de síntesis procedural

- Genera sonido "seco" a partir de los parámetros de Fase 1.
- Permite testear y escuchar resultados de forma determinista antes de integrar IA o efectos.
- Modular: cada instrumento tiene su propio motor o clase, separado del resto.

## Fase 3: Integración de IA generativa de parámetros

- La IA genera únicamente vectores de parámetros dentro del espacio definido.
- Cada clic en "Generate" produce un one-shot musicalmente coherente según estilo y carácter.
- La IA puede generar valores opcionales de efectos para variar espacialidad automáticamente.
- **Implementación inicial**: sistema basado en reglas musicales (Opción A). Diseñado para ser reemplazable por un modelo ML en el futuro sin cambiar el motor de síntesis ni la UX (misma interfaz GenerationResult).

## Fase 4: Integración de efectos DSP

- Añadir efectos como módulos independientes:
  - Reverb (size, decay, wet)
  - Delay (time, feedback, wet)
  - Chorus / Phaser / Flanger (rate, depth, wet)
- Aplicar efectos sobre el sonido generado por el motor procedural.
- La IA puede decidir los valores de efectos para cada one-shot si se desea.

## Fase 5: UX / WebUI

- Crear interfaz para seleccionar instrumento, estilo/género, carácter, ataque e impacto.
- Botón "Generate" que llama a la IA y reproduce un sonido final con todos los parámetros y efectos aplicados.
- Opciones de mutar parámetros o bloquear algunos para explorar variaciones controladas.
- **Auto-lock de ataque en instrumentos percusivos**: al seleccionar Kick, Snare, HiHat, Clap o Perc, el selector de tipo de ataque se bloquea automáticamente a "Fast" (o valor coherente) para evitar contradicciones como "Kick con ataque lento". Solo se desbloquea para instrumentos melódicos/sostenidos (Bass808, Lead, Pluck, Pad, Texture).

## Fase 6: Iteración y refinamiento

- Ajustar rangos de parámetros, envelopes, filtros y efectos para lograr calidad profesional.
- Añadir nuevos instrumentos, estilos, modulaciones o efectos avanzados.
- Entrenamiento o ajuste fino de la IA según resultados deseados.

---

## Ventajas del flujo completo

- Generación en tiempo real de sonidos de alta calidad.
- Control total del productor sobre timbre, carácter y espacialidad.
- Modular y escalable: síntesis, IA y efectos separados.
- Evita generar waveform directamente desde IA, manteniendo control y coherencia musical.
