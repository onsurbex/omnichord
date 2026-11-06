Nueva versión de Omnichord para macOS 11 o posterior, Apple Silicon e Intel.

- Pestaña **Tape** con Drive, Noise, Wow y Flutter. Simulación propia de cinta con saturación suave, hiss estéreo y variación de velocidad. TAPE permite activarla; comienza apagada para conservar el sonido de los proyectos anteriores.
- **ORGAN** y **FOLLOW CHORDS** pasan a ser botones físicos amarillos.
- Nuevo botón **SUSTAIN**, con el color del tema. Encendido mantiene el acorde. Apagado responde a la pulsación y suelta el órgano con una envolvente de release al recibir Note Off, al soltar el teclado del Mac o el botón de acorde.
- Prioridad de última pulsación: al soltar la más reciente, vuelve al acorde que siga pulsado. El arpa conserva su decaimiento natural.
- Se mantienen los 48 acordes, el mapa MIDI C0–C3, el arpa desde C4, los atajos del Mac, los seis temas y el logo original de BLINBLIN.

El instalador coloca el VST3 en `/Library/Audio/Plug-Ins/VST3/` y la app en `/Applications/`. Después vuelve a explorar los plugins en Ableton Live. Compilado sin las herramientas de edición de Foleys.

El VST3 y la app llevan firma local ad hoc. El PKG no tiene certificado Developer ID Installer ni notarización de Apple. Las pruebas automáticas verifican el sonido, los controles, la instalación y la reinstalación en macOS; la fidelidad respecto a una cinta real y la carga dentro de Ableton se comprueban manualmente.
