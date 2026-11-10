# Omnichord Vol. 1

Archivo original aportado por el usuario: `omnichord_samples.zip` (7.895.300 bytes).
Se conserva sin modificar. Solo se decodifican los 15 WAV; se ignoran `.asd`,
`.DS_Store` y los recursos de `__MACOSX`.

| Instrumento | Nota raíz MIDI | Sustain |
|---|---:|---|
| Omni Axe C | 72 | Decaimiento natural |
| Omni Bass F | 41 | Decaimiento natural |
| Omni Church C | 60 | Loop con crossfade |
| Omni Piano C | 60 | Decaimiento natural |
| Omni Voice C | 72 | Decaimiento natural |

Los seis golpes son Kick, Snare 1, Snare 2, Hat, Cajon y Clave. Las cinco pistas
del secuenciador usan Kick, Snare 1, Hat, Cajon y Clave; se puede elegir Snare 2.

Los loops 4toFloor, Dusty hip hop, March y Tango duran 9,6 segundos a 44.100 Hz:
**100 BPM, 16 negras, cuatro compases de 4/4**. Su sincronización conserva la
afinación usando un reproductor de fragmentos con solapamiento y alineación.

CMake incrusta el ZIP en ambos formatos del instrumento. Las notas raíz y los
ajustes iniciales están en `Source/FactorySamples.h`.

## GraySlate's Omnichord Pack (0.7.4)

477 WAV originales aportados por el usuario: 36 acordes completos, 431 notas
individuales de arpa y 10 grabaciones de ritmos. Se conservan sin modificar.
374 son PCM de 16 bits estéreo y 103 IEEE float64 mono; todos a 96 kHz.
El lector convierte float64 al formato interno float32 al cargar, conservando
la frecuencia de muestreo. No se modifica ningún WAV del repositorio.

En Samples abre **GraySlate Omnichord** y elige un grupo de acordes:
- **Chord** selecciona el acorde grabado completo y su nota raíz, con loop.
  Al transponer mantiene la calidad del acorde grabado; para pasar de mayor a
  menor elige la grabación del grupo correspondiente.
- **Harp** selecciona un golpe individual, con raíz estimada a partir de su
  frecuencia dominante. Los botones de acordes generan los intervalos como
  en el sampler anterior. Puedes corregir la raíz y el nivel manualmente.
- **RHYTHM** reproduce grabaciones completas como one-shots dentro de Samples.
  No se añaden al motor de loops sincronizados: su BPM no está confirmado.

`grayslate-index.json` guarda nombres, raíces, niveles iniciales y SHA-256.
Los ajustes de nivel equilibran el RMS inicial y limitan el pico estimado.
`python3 scripts/index-grayslate.py` lo regenera (requiere numpy; solo desarrollo).
CMake usa el índice ya guardado y Python estándar para verificar todos los hashes
y generar `GraySlateSamples.zip`. Este ZIP y el índice se incluyen en Resources
**tanto del VST3 como de la app**, también al instalarlos con el PKG.

El catálogo es ligero; la selección decodifica un solo WAV fuera del callback
de audio y descarga la grabación anterior. Presets y proyectos usan la referencia
portable `factory`, manteniendo los índices originales 0–14. El selector se amplía
a 1024 posiciones; las automatizaciones normalizadas antiguas del selector pueden
necesitar ajuste. El ZIP original y su importación manual siguen funcionando.
