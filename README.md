# ChordHarp

Prototipo de instrumento inspirado en el Omnichord para Ableton Live en macOS.
Incluye 36 acordes (mayores, menores y séptimas), una franja de rasgueo de
12 notas, órgano de acordes opcional y controles automatizables de volumen,
timbre, sustain y vibrato. Guarda sus parámetros con el proyecto del DAW.

El motor usa síntesis original: todavía no es una emulación calibrada de un
modelo concreto de Omnichord. No incluye muestras del instrumento ni ritmos.

## Compilar en macOS

Requisitos: Xcode y sus herramientas de línea de comandos, CMake 3.22 o posterior
y el código fuente de JUCE 8.0.6. Revisa la licencia de JUCE para tu distribución.

```sh
git clone --branch 8.0.6 --depth 1 https://github.com/juce-framework/JUCE.git ../JUCE
cmake -S . -B build -G Xcode -DJUCE_PATH="$(pwd)/../JUCE" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ChordHarp_VST3 ChordHarp_Standalone
```

El VST3 estará en `build/ChordHarp_artefacts/Release/VST3/ChordHarp.vst3`.
Cópialo a `~/Library/Audio/Plug-Ins/VST3/` (crea esa carpeta si hace falta).
En Live, activa las carpetas de sistema VST3 en los ajustes de Plug-ins y vuelve
a explorar. Arrastra ChordHarp a una pista MIDI.

La compilación, firma y carga en macOS/Ableton están pendientes de verificar.
Este repositorio contiene código fuente, no un plugin ya compilado.

## Compilación automática

El workflow `macOS VST3 build and tests` compila para Apple Silicon e Intel,
ejecuta las pruebas del motor y del procesador JUCE y comprueba las firmas
locales de los bundles. En una ejecución correcta, el apartado Artifacts de
GitHub Actions ofrece `ChordHarp-macOS-universal`, con el VST3 y la app standalone.
La firma es ad hoc, sin notarización de Apple; la carga en Ableton y la fidelidad
respecto al instrumento real requieren una prueba manual en un Mac.

## Tocar

- Selecciona un acorde y arrastra por la franja para rasguear hacia ambos lados.
- Activa ORGANO DE ACORDES para escuchar el acorde sostenido; SILENCIO apaga el órgano
  y corta las colas.
- Las notas MIDI disparan directamente las voces de arpa con velocidad y decay.
  Note Off no corta la cola; el pedal de sustain no está implementado.
- El acorde seleccionado queda memorizado y puede automatizarse en Live.
- El rasgueo con ratón produce audio, pero no genera notas MIDI para grabar.

## Diseño BLINBLIN

Panel inspirado en la distribución de un Omnichord: carcasa crema redondeada,
acordes circulares ordenados por quintas a la izquierda, cuatro potenciómetros
y superficie de rasgueo vertical a la derecha. La rejilla de altavoz es decorativa.

Pulsa CARGAR LOGO para elegir tu PNG o JPEG original (hasta 8 MB y 4096 x 4096).
Se respeta su transparencia y proporción, y sus datos se guardan dentro del estado
del plugin con el proyecto de Ableton. El texto BLINBLIN es provisional hasta
cargar el archivo: el logotipo del usuario no está incluido en el repositorio.
El workflow incluye una captura real del editor en el artefacto descargable.

## Verificar el motor sin JUCE

```sh
cmake -S . -B build-tests -DCHORDHARP_DSP_ONLY=ON
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

También puede probarse directamente con C++17:

```sh
mkdir -p work
c++ -std=c++17 -Wall -Wextra -Wpedantic -O2 -ISource tests/engine_tests.cpp -o work/engine_tests
./work/engine_tests
```

Las pruebas comprueban acordes, decaimiento, silencio y estabilidad de voces a
44.1, 48 y 96 kHz; no miden fidelidad respecto a hardware real.
