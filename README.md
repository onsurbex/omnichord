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
Foleys GUI Magic está fijado como submódulo en el repositorio. Antes de configurar:

```sh
git submodule update --init --recursive
```

```sh
git clone --branch 8.0.6 --depth 1 https://github.com/juce-framework/JUCE.git ../JUCE
cmake -S . -B build -G Xcode -DJUCE_PATH="$(pwd)/../JUCE" -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
cmake --build build --config Release --target ChordHarp_VST3 ChordHarp_Standalone
```

El VST3 estará en `build/ChordHarp_artefacts/Release/VST3/ChordHarp.vst3`.
Cópialo a `~/Library/Audio/Plug-Ins/VST3/` (crea esa carpeta si hace falta).
En Live, activa las carpetas de sistema VST3 en los ajustes de Plug-ins y vuelve
a explorar. Arrastra ChordHarp a una pista MIDI.

Los binarios de prueba se descargan desde GitHub Actions; no se almacenan
en el repositorio. La prueba de carga dentro de Ableton requiere un Mac.

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

El logo original de BLINBLIN, `Assets/BlinblinLogo.png`, viene incluido en el
VST3 y en la aplicación. Aparece automáticamente al abrir el instrumento,
conservando su transparencia y proporciones, sin necesitar un archivo externo.
CAMBIAR LOGO permite elegir otro PNG o JPEG (hasta 8 MB y 4096 x 4096).
Ese cambio se guarda dentro del estado del plugin con el proyecto de Ableton.
El workflow incluye una captura real del editor en el artefacto descargable.

## Editar visualmente con Foleys GUI Magic

El diseño está en `Assets/magic.xml`. Las posiciones, tamaños, textos, colores y
enlaces a parámetros se editan desde la ventana **foleys GUI magic**. Cada botón
de acorde es un control independiente. La paleta incluye `OmniStrum` (rasgueo),
`BlinblinLogo`, `LogoPicker`, `ChordDisplay` y `Speaker`.

Para compilar una versión con el editor visual desde la carpeta `omnichord`,
con JUCE en la carpeta hermana `../JUCE`:

```sh
git submodule update --init --recursive
cmake -S . -B build-macos -G "Unix Makefiles" \
  -DJUCE_PATH="$(pwd)/../JUCE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$(xcrun --find clang)" \
  -DCMAKE_CXX_COMPILER="$(xcrun --find clang++)" \
  -DCMAKE_OSX_SYSROOT="$(xcrun --sdk macosx --show-sdk-path)" \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCHORDHARP_GUI_EDITOR=ON
cmake --build build-macos --parallel 3 --target ChordHarp_Standalone ChordHarp_VST3
open build-macos/ChordHarp_artefacts/Release/Standalone/ChordHarp.app
```

1. En la ventana de Foleys, activa **Edit** y selecciona un elemento en el panel
   o en el árbol. Arrastra los controles y sus bordes para moverlos y cambiar
   su tamaño. Las propiedades aparecen en el inspector.
2. Desactiva **Edit** para tocar los acordes, probar el rasgueo y mover los
   potenciómetros. Los controles conservan sus enlaces a los parámetros del DAW.
3. Usa **File → Save XML** y guarda en `Assets/magic.xml` del repositorio.
   **Load XML** permite cargar otro diseño. El editor también genera copias
   temporales `.sav*.xml`; guarda explícitamente el XML definitivo.
4. Cierra la aplicación y recompila para incluir ese XML como diseño de fábrica
   del VST3 y la aplicación. Las nuevas instancias usarán el diseño compilado.

El diseño editado también se guarda dentro del estado del plugin en el proyecto
de Ableton. Las sesiones existentes conservan su diseño guardado; guardar un
proyecto del DAW no modifica el XML del repositorio.

El editor visual viene activado por defecto. Para distribuir el instrumento con
el panel de ejecución, configura con `-DCHORDHARP_GUI_EDITOR=OFF` y recompila.
La carpeta `modules/foleys_gui_magic` contiene una dependencia: los archivos del
instrumento a editar son `Assets/magic.xml` y `Source/`, fuera del submódulo.

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
