Omnichord 0.7.0 para macOS 11 o posterior, Apple Silicon e Intel.

- Mixer como primera pestaña: Harp, Organ, FM, Samples, Ritmos, Delay, Rev y MSTR. Cada fuente tiene volumen, panorama y envíos post-fader independientes a Delay/Reverb. Retornos paralelos, con Width en Delay, Rev y Master. Tape insertado en Master.
- Capas simultáneas de órgano subtractivo, FM de seis operadores y samples. Diez registros de órgano, incluidos Principal, Pleno, Flauta y Tonewheel. El botón amarillo CHORDS gobierna las tres capas de acordes.
- Botones físicos de canal y LEDs pulsables en las pestañas. Ctrl+clic activa solo; Ctrl+clic en MSTR elimina los solos. Medidores por canal.
- Etiquetas negras CHORD/CHROMATIC. CHORD conserva el funcionamiento del Omnichord. CHROMATIC interpreta todo el MIDI como notas normales y polifónicas, con Note Off y pedal CC64 por canal. El teclado del Mac usa una distribución de piano y cambia de octava con Z/X. La botonera gráfica conserva sus acordes en ambos modos.
- Corregido el selector Voices vacío: los parámetros enteros anuncian sus siete opciones a Foleys. Detune actúa sobre 2–7 voces, con controles independientes para Harp, Organ y FM y ayuda en la interfaz.
- Presets completos y proyectos incluyen todas las capas, envíos, solos, Width y modos. Migración de presets/proyectos de 0.6 a la nueva mezcla. Los de 0.5 no se importan.

Se conserva la librería original Omnichord Vol. 1: cinco instrumentos, seis golpes y cuatro loops a 100 BPM. Los loops se adaptan al BPM del DAW o standalone conservando afinación. Los 23 presets completos, temas, logo por contraste y el icono de app están incluidos en VST3, standalone y PKG.

La FM usa las tablas de algoritmos de Google MSFA (Apache-2.0) con motor propio; no es una emulación exacta de DX7 ni admite SysEx.

El PKG instala VST3 en `/Library/Audio/Plug-Ins/VST3/` y standalone en `/Applications/`. Los bundles usan firma ad hoc; el instalador no está notarizado. La escucha y la prueba de carga/rendimiento dentro de Ableton requieren un Mac.
