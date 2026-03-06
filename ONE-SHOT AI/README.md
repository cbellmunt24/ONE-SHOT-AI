# AI One-Shot Generator – Descripción del Plugin

Este proyecto consiste en el desarrollo de un **plugin híbrido de audio** que combina un **motor de síntesis procedural** con un **modelo de IA generativa de parámetros** para crear **one-shots únicos y coherentes musicalmente**, pensado para productores de música urbana, electrónica y estilos relacionados.

---

## Funcionalidad principal

- Generación de **one-shots para múltiples instrumentos**:
  - Drums: kicks, snares, hi-hats, 808s
  - Sintetizadores: leads, plucks, pads
  - Efectos y texturas sonoras

- **Selección de estilo y género (9 opciones fijas)**:
  1. Trap
  2. Hip-hop
  3. Techno
  4. House
  5. Ambient
  6. Reggaeton
  7. Afrobeat
  8. RnB
  9. EDM

- **Controles de carácter del sonido (4 sliders intuitivos, escala 0–1)**:
  1. Brillo: presencia de agudos / claridad
  2. Cuerpo: sensación de plenitud / grosor
  3. Textura: suave, áspero, granular, percusivo
  4. Movimiento: dinámica interna, vibrato o variaciones de pitch y modulación

- **Tipo de ataque (3 opciones fijas)**:
  1. Rápido
  2. Medio
  3. Lento

- **Impacto / presencia (1 slider, escala 0–1)**: controla la pegada y fuerza percibida del sonido.

- Cada sonido generado es **nuevo y único**, coherente con las opciones seleccionadas.

---

## Procedimiento del plugin

1. **Definición del espacio paramétrico**
   - Para cada instrumento se definen los parámetros acústicos relevantes (por ejemplo, para un kick: frecuencia base, pitch decay, amp decay, distorsión, click, noise, filtro).
   - Los rangos garantizan que los sonidos sean musicales y coherentes desde el inicio.

2. **Motor de síntesis procedural**
   - Recibe los parámetros y genera audio determinista.
   - Cada parámetro controla: forma de onda, envolventes, transient/noise, distorsión y filtros.
   - Genera sonidos coherentes y de alta calidad de manera rápida.

3. **IA generativa de parámetros**
   - La IA genera **vectores de parámetros** dentro del espacio definido.
   - Aprende qué combinaciones son coherentes según: estilo, carácter (brillo, cuerpo, textura, movimiento), tipo de ataque e impacto.
   - Cada clic del botón "Generate" produce un vector distinto y musicalmente válido.

4. **UX y generación interactiva**
   - Selección de instrumento, estilo/género, carácter, tipo de ataque e impacto.
   - **Botón "Generate"**: genera un nuevo one-shot cada vez según los campos seleccionados.
   - Opciones para mutar sonidos o bloquear parámetros y explorar variaciones controladas.

---

## Ventajas del enfoque

- Generación **en tiempo real** sin depender de librerías de samples.
- **Control total del productor** sobre sonido, estilo y carácter.
- Sistema **inteligible y escalable**, permitiendo añadir nuevos instrumentos, estilos o características en el futuro.
- Combina **eficiencia computacional** con resultados musicales de alta calidad.
