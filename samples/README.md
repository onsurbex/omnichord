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
