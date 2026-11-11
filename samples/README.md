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

## GraySlate's Omnichord Pack (0.7.5)

477 WAV originales aportados por el usuario: 36 acordes completos, 431 grabaciones
de arpa y 10 ritmos. 374 son PCM16 estéreo y 103 IEEE float64 mono, todos a 96 kHz.
Los originales no se modifican; el banco instalado se empaqueta sin pérdida en ZIP.

- **Samples → Grayslate samples**: una sola opción. Los botones y octavas MIDI
  eligen automáticamente el acorde grabado correcto (12 mayores, 12 menores y
  12 séptimas dominantes). No se construye una tríada usando un acorde como nota.
- **Harp → Grayslate samples**: una sola opción independiente. Cada posición 0–11
  toca su WAV numerado dentro del acorde seleccionado, con pitch original.
  Las posiciones MIDI adicionales transponen la posición correspondiente por octavas.
- Falta `bb/bb11.wav`: esa posición se obtiene del vecino disponible, transpuesto.
- El pack no contiene Maj7: Samples combina el acorde mayor grabado con una nota
  de séptima mayor muestreada. Harp usa el arpa de séptima dominante y sube solo
  sus notas de séptima un semitono. No se presenta como una grabación Maj7 original.
- CHROMATIC y FOLLOW CHORDS desactivado usan la nota de arpa más cercana y ajustan
  su pitch. Cada canal conserva su volumen, panorama, filtro y envíos.

Los bancos necesarios se preparan al elegir el sonido, cargar el preset o preparar
el procesador. Toda la reproducción y los cambios de acorde posteriores usan
memoria, sin acceso a disco en el callback de audio. Las instancias comparten bancos
inmutables. Se conserva PCM16 en memoria y se evita duplicar canales mono; float64
se convierte al formato interno float32. La frecuencia de muestreo sigue siendo 96 kHz.

Se mantienen los 15 samples anteriores, sus índices y sus presets. Las selecciones
individuales GraySlate de la 0.7.4 se interpretan ahora como el banco de acordes.
Los diez WAV de ritmos nuevos se conservan en el archivo pero ya no aparecen como
sonidos individuales de Samples; los cuatro loops sincronizados anteriores siguen.

`grayslate-index.json` contiene metadatos y SHA-256. El script de desarrollo
`index-grayslate.py` requiere numpy; CMake solo necesita Python estándar para
verificar y empaquetar los originales. VST3, standalone y PKG llevan ZIP e índice
en Resources y usan la referencia portable `factory` en presets y proyectos.
