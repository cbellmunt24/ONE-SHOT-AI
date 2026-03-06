# Especificaciones de Output de Código para AI One-Shot Generator

Este documento define cómo debe generar Claude Code el código del plugin, para asegurar consistencia, modularidad y que todo sea testeable fase por fase.

---

## 1. Separación de módulos

- El proyecto debe estar claramente dividido en módulos/clases separados:
  1. **Motor de síntesis procedural**
  2. **IA generativa de parámetros**
  3. **WebUI / UX**

- Cada módulo debe poder probarse de forma independiente antes de integrarlo con los demás.
- No generar código que mezcle responsabilidades de estos módulos.

## 2. Generación de audio

- La IA **no genera waveform directamente**, solo vectores de parámetros.
- El motor procedural es el único responsable de convertir parámetros en audio.
- Esto asegura control, estabilidad y coherencia musical.

## 3. Fase de pruebas

- Cada función o módulo generado debe ser **probado y funcional** antes de pasar a la siguiente fase.
- Evitar generar código "completo" que no pueda ejecutarse de forma independiente.

## 4. Compatibilidad con preset base

- Mantener compatibilidad con el preset base de JUCE (`WebUI Plugin`)
- Preparar la estructura para que la **UI web se pueda integrar más adelante** sin reescribir código.

## 5. Lenguaje y estilo de código

- Lenguaje: **C++ estándar para JUCE**, usando clases y structs según convenga.
- Nombres de variables y funciones **claros y musicales**, por ejemplo: freqBase, pitchDecay, brillo, cuerpo, textura, movimiento, ataque, impacto.
- Comentarios **mínimos y claros**: explicar la función y su relación con la música, sin largas justificaciones.
- Mantener consistencia en formato y nomenclatura entre todos los módulos.
