Omnichord 0.6.0 — rama de desarrollo para macOS 11 o posterior, Apple Silicon e Intel.

- Motor FM original de seis operadores con 32 algoritmos DX y bancos independientes de órgano y arpa. Ratio, fine, nivel, ADSR, velocidad y feedback editables; ocho patches iniciales. Se mantienen Clásico, Reed y Suave.
- Unison de 1–7 voces con detune, panorama y drift independientes. Filtro LP/BP/HP por fuente. Sustituyen los controles Coro y Apertura. La polifonía del arpa se adapta al número de copias (32 notas con 1–3 voces, 16 con 7).
- Sampler de acordes con importación ZIP, nota raíz, loop con crossfade y release. Modo de nota aislada para construir acordes o de acorde grabado para transponerlo conservando sus intervalos.
- Ritmos de cinco pistas con seis patrones, edición de 16 pasos, swing, volumen y envío a efectos. Sincronía de tempo y transporte/posición del DAW; reloj y BPM propios en standalone.
- Presets completos y proyectos guardan todos los parámetros FM, pasos, asignaciones y la referencia a la librería de samples. La importación conserva una copia local del ZIP para recuperar los recursos.
- Conserva los temas, el logo automático por contraste, la carcasa opaca con relieve, los botones físicos, Tape, delay/reverb configurables y el icono de la app instalada.

Pendiente de completar: el archivo `samples/omnichord_samples.zip` subido a main solo contiene 2 bytes, sin audio. El sampler y los ritmos requieren importar una librería válida. Cuando el ZIP real esté disponible, CMake lo incluirá en ambos bundles y el instalador. No hay samples del Omnichord incluidos todavía.

La síntesis utiliza las tablas de algoritmos de Google MSFA (Apache-2.0) con un motor propio; no es una emulación exacta de DX7 ni admite SysEx. Cambia el formato de presets y el estado de síntesis; los de 0.5 no se importan. La prueba de escucha y carga en Ableton requiere macOS.

El PKG instala VST3 en `/Library/Audio/Plug-Ins/VST3/` y standalone en `/Applications/`. Los bundles usan firma ad hoc; el instalador no está notarizado. Esta versión permanece en una PR de desarrollo hasta integrar y comprobar los audios reales.
