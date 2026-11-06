Omnichord 0.5.1 para macOS 11 o posterior, Apple Silicon e Intel.

Sustituye a la 0.5.0 retirada. Corrige una lectura fuera de limites en el punto de vuelta de los buffers fraccionarios y añade una prueba de regresion del redondeo.

- Fondo gris neutro totalmente opaco, común a standalone, VST3 y todos los temas. Carcasa redondeada con relieve, reborde, sombreado, textura sutil y tornillos.
- Once selecciones de órgano: los sonidos clásicos, cuatro registros de iglesia y tres tonewheel. Síntesis aditiva de registros y nueve tiradores, suboctava de 16 pies, foldback, percusión y rotary estéreo. Controles Rotary y Percusión. Las sesiones antiguas conservan su órgano clásico.
- Biblioteca de presets completos: guarda y recupera todos los parámetros, incluidos instrumentos, volúmenes, Tape, efectos, acordes, sustain, tema y sincronía. Archivos `.omnipreset`, presets de fábrica y elección del estado completo de inicio. El diseño del panel se conserva por separado. El standalone arranca con el inicio elegido; los proyectos del DAW conservan su propio estado.
- Reverb con predelay de 0–250 ms, EQ de graves/medios/agudos (±12 dB) y primeras reflexiones. El procesamiento actúa sobre la reverb, manteniendo la señal directa.
- Delay con Sync al BPM del DAW, divisiones de nota, tresillos (`t`) y puntillos (`dot`); alterna entre milisegundos y notas. En standalone usa el control BPM propio. Rango de tempo: 20–400 BPM.

El PKG instala el VST3 en `/Library/Audio/Plug-Ins/VST3/` y la app en `/Applications/`. Compilado sin las herramientas de edición de Foleys. Los bundles tienen firma local ad hoc; el PKG no tiene certificado Developer ID Installer ni notarización de Apple. La comparación con órganos reales y la prueba dentro de Ableton son manuales.
