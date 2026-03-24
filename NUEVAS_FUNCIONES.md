# ONE-SHOT AI — Nuevas Funciones Propuestas

## Estado actual del plugin
- **Generator Mode**: ML/reglas → 10 synths × 9 géneros → one-shot
- **One-Shot Match Mode**: referencia → 106 params → reconstrucción por síntesis
- **Infraestructura**: 10 synth engines, 9 efectos DSP, descriptores perceptuales, optimizer DE+NM, WebUI con tabs

---

## 1. Interactive Evolution — Diseño sonoro genético

**Concepto**: Presentar N variaciones de un sonido. El usuario escucha y elige las que más le gustan. El sistema hace crossover genético + mutación y genera la siguiente generación. Se repite hasta converger en el sonido deseado.

**Cómo funciona**:
- Generación 0: 8 sonidos aleatorios (o perturbaciones de un seed)
- El usuario marca 2-3 favoritos
- Crossover: mezclar params de los padres seleccionados
- Mutación: perturbar ±5-15% algunos params aleatoriamente
- Generación N+1: 8 nuevos hijos + 2 élites (mejores de la generación anterior)
- Repetir hasta satisfacción

**Por qué es innovador**: El usuario no necesita saber nada de síntesis — solo escuchar y elegir. Fitness perceptual humana en vez de función de distancia numérica. Referencia directa a algoritmos genéticos aplicados al arte sonoro.

**Usa**: Cualquiera de los 10 synth engines o el synth universal del Match.

**UI**: Grid de 8 botones play + checkboxes de selección + botón "Evolve" + contador de generación.

---

## 2. Descriptor-Guided Generation — "Quiero un kick con..."

**Concepto**: El usuario define descriptores target (fundamental=55Hz, decay=300ms, brightness=0.3, sub_energy=0.6) y el sistema encuentra los params que los producen. Como el Match, pero sin audio de referencia — la referencia son números.

**Cómo funciona**:
- UI con sliders para ~8 descriptores clave: pitch, decay, brightness, sub energy, transient strength, spectral tilt, noise amount, stereo width
- El optimizer (DE+NM) busca params cuyo sonido generado tenga esos descriptores
- Reutiliza toda la infraestructura del Match (distancia perceptual, gap analysis, extensiones adaptativas)

**Por qué es innovador**: Diseño sonoro "científico" — control preciso sobre características perceptuales sin manipular parámetros de síntesis directamente. Puente entre percepción y síntesis.

**Usa**: OneShotMatchSynth (106 params) + DescriptorExtractor + optimizer.

**UI**: 8-10 sliders con labels descriptivos + botón "Generate" + visualización de descriptores conseguidos vs target.

---

## 3. Morph Mode — Interpolación entre sonidos

**Concepto**: Tomar dos sonidos (A y B) e interpolar entre ellos. No solo crossfade de audio, sino interpolación en el espacio de parámetros de síntesis.

**Cómo funciona**:
- Slot A y Slot B cargan un sonido cada uno (del Generator, del Match, o importado+matched)
- Slider 0.0–1.0 interpola linealmente los parámetros
- Modo avanzado: interpolación guiada por descriptores — en vez de interpolar params directamente (que puede sonar errático), el optimizer encuentra params intermedios cuyos descriptores estén entre los de A y B

**Variante — Morph Path**: Renderizar 8-16 puntos intermedios y exportar como pack de one-shots que evolucionan de A a B.

**Por qué es innovador**: Exploración del espacio paramétrico con sentido perceptual. Descubrir sonidos que están "entre" dos referencias.

**Usa**: MatchSynthParams (array de floats), opcionalmente el optimizer para guía perceptual.

**UI**: Dos slots (A/B) con botones load/generate + slider de morph + waveform preview + export path.

---

## 4. Live Tweak — Edición post-Match en tiempo real

**Concepto**: Después de que el Match encuentra los params óptimos, exponer los más importantes como sliders editables. El usuario ajusta y escucha el resultado regenerado al soltar el slider.

**Cómo funciona**:
- El Match produce 106 params → el sistema identifica los 10-15 más sensibles (ya existe sensitivity analysis)
- Esos params aparecen como sliders con el valor encontrado
- Al mover un slider, se regenera el sonido con el nuevo valor
- Opción "lock": fijar ciertos params mientras se ajustan otros

**Por qué es innovador**: Convierte el plugin de "generador estático" a "sintetizador interactivo guiado por IA". La IA encuentra el punto de partida, el humano refina. Lo mejor de ambos mundos.

**Usa**: OneShotMatchSynth.generate() + sensitivity array del optimizer.

**UI**: Panel de sliders dinámicos (solo los sensibles) + botones play/export + toggle lock por param.

---

## 5. Layering Engine — Compositor de capas

**Concepto**: Combinar múltiples one-shots con control preciso de timing, nivel, pitch offset y efectos por capa. Automatizar lo que los productores hacen manualmente al layerear kicks.

**Cómo funciona**:
- Hasta 4 capas independientes
- Cada capa: selector de instrumento/género + offset temporal (0-10ms) + level (dB) + pitch offset (semitones) + pan
- Genera cada capa por separado, las mezcla con los offsets
- Opción de aplicar effect chain global al resultado
- Opción de aplicar el Match al resultado layereado (match → analizar el layer como referencia → encontrar params de synth único que lo reproduzcan)

**Ejemplo**:
```
Capa 1: Bass808/Trap    t=0ms    0dB    +0st   (sub)
Capa 2: Kick/Techno     t=0.5ms  -3dB   +0st   (body)
Capa 3: Perc/EDM        t=0ms    -6dB   +12st  (click)
Capa 4: HiHat/Trap      t=1ms    -12dB  +0st   (air)
```

**Por qué es innovador**: Sound design por capas es estándar en producción, pero hacerlo dentro de un generador procedural con control paramétrico es único.

**Usa**: Los 10 SynthEngines + EffectChain + mixer.

**UI**: 4 filas de controles (instrumento, género, offset, level, pitch, pan) + master output + export.

---

## 6. Sound DNA — Análisis y visualización

**Concepto**: Analizar cualquier sample y mostrar su "ADN" como visualización interactiva. Comparar dos sonidos lado a lado.

**Cómo funciona**:
- Cargar un WAV → extraer todos los descriptores (ya existe DescriptorExtractor)
- Mostrar como radar chart con 8-12 ejes: pitch, decay, brightness, sub energy, transient strength, spectral tilt, HNR, stereo width, envelope shape, noise amount
- Superponer dos sonidos para ver diferencias
- Mostrar la matriz spectrotemporal como heatmap (8×8)
- Exportar como JSON para uso externo

**Por qué es innovador**: Herramienta analítica para entender qué hace que un sonido suene como suena. Muy útil para educación en sonología.

**Usa**: DescriptorExtractor (ya implementado), solo necesita UI.

**UI**: Tab "Analyze" con: drop zone para WAV + radar chart SVG + heatmap spectrotemporal + tabla de valores + botón comparar.

---

## 7. Round-Robin / Humanizer — Pack de variaciones

**Concepto**: Dado un sonido (generado o matched), producir N variaciones sutiles para round-robins o velocity layers.

**Cómo funciona**:
- Tomar los params del sonido base
- Generar 4-8 variaciones perturbando params un ±2-8%:
  - Pitch: ±1-3 cents
  - Decay: ±5%
  - Click amount: ±10%
  - Saturation: ±5%
  - Noise amount: ±15%
  - Filter cutoff: ±3%
- Opción "velocity layers": perturbar más agresivamente el click/transient y el level
- Exportar todo como pack de WAVs numerados

**Por qué es innovador**: Automatiza la creación de variaciones naturales. Resuelve el "machine gun effect" en producción.

**Usa**: Cualquier synth engine o el MatchSynth + perturbación gaussiana de params.

**UI**: Selector de sonido base + slider "variación" (sutil→agresiva) + número de variaciones (4/8/16) + botón "Generate Pack" + export.

---

## 8. Style Transfer — Envelope de A + Timbre de B

**Concepto**: Tomar la envolvente temporal/pitch de un sonido y el carácter espectral/tímbrico de otro. Crear un híbrido.

**Cómo funciona**:
- Sonido A (source): extraer ampEnvelope, pitchEnvelope, attackTime, decayTime
- Sonido B (style): extraer spectralCentroid, brightness, subEnergy, HNR, spectralTilt, bandEnergy
- Construir descriptores target: temporales de A + espectrales de B
- Usar el optimizer para encontrar params que matcheen esos descriptores híbridos

**Ejemplo**: Envelope de un kick acústico + timbre de un 808 sintético = kick híbrido con el punch natural pero el sub del 808.

**Por qué es innovador**: Concepto de neural style transfer aplicado a síntesis de audio, pero sin redes neuronales — usando optimización paramétrica directa.

**Usa**: DescriptorExtractor + optimizer con descriptores target híbridos.

**UI**: Dos slots (Source/Style) + selector de qué extraer de cada uno + generate + preview.

---

## 9. Sound Space Explorer — Mapa 2D interactivo

**Concepto**: Visualización 2D del espacio de sonidos posibles. El usuario navega haciendo click en el mapa y escucha el sonido de esa posición.

**Cómo funciona**:
- Pre-generar una grid de ~100 sonidos variando 2 ejes principales (ej: pitch vs brightness, o sub vs transient)
- Extraer descriptores de cada uno
- Proyectar a 2D (PCA o simplemente los 2 ejes elegidos)
- Mostrar como mapa con puntos coloreados por algún descriptor (ej: color=genre)
- Click en cualquier punto → interpolar params → generar y reproducir
- Drag para explorar en tiempo real

**Por qué es innovador**: Exploración intuitiva de espacios sonoros complejos. Referencia a trabajos de sonología computacional (timbre spaces, Wessel 1979).

**Usa**: SynthEngine o MatchSynth + DescriptorExtractor + interpolación.

**UI**: Canvas 2D con ejes seleccionables + puntos clickeables + previsualización de descriptores al hover + reproducción al click.

---

## 10. Batch Match — Cola de procesamiento

**Concepto**: Cargar múltiples samples de referencia y matchearlos todos en secuencia. Útil para reconstruir un kit completo.

**Cómo funciona**:
- Drop zone para múltiples WAVs
- Cola de procesamiento: cada uno pasa por el pipeline completo del Match
- Resultados exportados como pack con nombres originales + "_matched"
- Estadísticas: score medio, mejor/peor match, extensiones más usadas
- Warm-start desde matches anteriores de la cola (cada match mejora el siguiente)

**Por qué es innovador**: Productividad — reconstruir un kit entero de 20+ samples automáticamente. El warm-start entre matches hace que cada uno sea mejor que el anterior.

**Usa**: OneShotMatchEngine (ya tiene warm-start y K-NN learning).

**UI**: Lista/cola de archivos + progress bars individuales + scores + export all.

---

## Resumen — Esfuerzo vs Impacto

| # | Función | Esfuerzo | Impacto creativo | Innovación académica |
|---|---------|----------|------------------|---------------------|
| 1 | Interactive Evolution | Medio | Muy alto | Muy alta |
| 2 | Descriptor-Guided Gen | Bajo | Alto | Alta |
| 3 | Morph Mode | Medio | Alto | Alta |
| 4 | Live Tweak | Bajo | Alto | Media |
| 5 | Layering Engine | Alto | Alto | Media |
| 6 | Sound DNA | Bajo | Medio | Alta |
| 7 | Round-Robin Pack | Bajo | Medio | Baja |
| 8 | Style Transfer | Medio | Muy alto | Muy alta |
| 9 | Sound Space Explorer | Alto | Alto | Muy alta |
| 10 | Batch Match | Bajo | Medio | Baja |

### Top 3 recomendados para proyecto de sonología:
1. **Interactive Evolution** — algoritmos genéticos + percepción humana
2. **Style Transfer** — concepto academicamente muy potente
3. **Descriptor-Guided Generation** — puente percepción↔síntesis
