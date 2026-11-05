# Omnichord — BLINBLIN · versión 2

Prototipo de instrumento inspirado en el Omnichord para Ableton Live en macOS.
Incluye 36 acordes (mayores, menores y séptimas), una franja de rasgueo de
12 notas, órgano de acordes opcional y controles automatizables de volumen,
timbre, sustain y vibrato. Guarda sus parámetros con el proyecto del DAW.

El motor usa síntesis original: todavía no es una emulación calibrada de un
modelo concreto de Omnichord. No incluye muestras del instrumento ni ritmos.

La versión 2 incorpora selección de acordes por MIDI y teclado del Mac, timbres
de órgano y arpa, volúmenes independientes y delay y reverb estéreo.

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
cmake --build build --config Release --target Omnichord_VST3 Omnichord_Standalone
```

El VST3 estará en `build/Omnichord_artefacts/Release/VST3/Omnichord.vst3`.
Cópialo a `~/Library/Audio/Plug-Ins/VST3/` (crea esa carpeta si hace falta).
En Live, activa las carpetas de sistema VST3 en los ajustes de Plug-ins y vuelve
a explorar. Arrastra Omnichord a una pista MIDI.

Los binarios de prueba se descargan desde GitHub Actions; no se almacenan
en el repositorio. La prueba de carga dentro de Ableton requiere un Mac.

## Compilación automática

El workflow `macOS VST3 build and tests` compila para Apple Silicon e Intel,
ejecuta las pruebas del motor y del procesador JUCE y comprueba las firmas
locales de los bundles. En una ejecución correcta, el apartado Artifacts de
GitHub Actions ofrece `Omnichord-macOS-universal`, con el VST3 y la app standalone.
La firma es ad hoc, sin notarización de Apple; la carga en Ableton y la fidelidad
respecto al instrumento real requieren una prueba manual en un Mac.

## Tocar

- Selecciona un acorde y arrastra por la franja para rasguear hacia ambos lados.
- Activa ORGANO DE ACORDES para escuchar el acorde sostenido; SILENCIO apaga el órgano
  y corta las colas.
- Las notas MIDI C1–B3 seleccionan acordes por posición. C4 en adelante toca
  las notas del acorde como la franja; desactiva NOTAS DEL ACORDE para tocar
  alturas cromáticas libres. La velocidad MIDI controla la intensidad del arpa.
  Note Off deja decaer las voces; el pedal de sustain no está implementado.
- El acorde seleccionado queda memorizado y puede automatizarse en Live.
- El rasgueo con ratón produce audio, pero no genera notas MIDI para grabar.

## Diseño BLINBLIN

Panel inspirado en la distribución de un Omnichord: carcasa crema redondeada,
acordes circulares ordenados por quintas a la izquierda, cuatro potenciómetros
y superficie de rasgueo vertical a la derecha. La rejilla de altavoz es decorativa.

El logo original de BLINBLIN, `Assets/BlinblinLogo.png`, viene incluido en el
VST3 y en la aplicación. Aparece automáticamente al abrir el instrumento,
conservando su transparencia y proporciones, sin necesitar un archivo externo.
La marca es fija: se ha eliminado el selector de imagen y se ignoran los logos
personalizados de sesiones antiguas. El editor visual permite colocar y escalar
el componente, conservando siempre el PNG original incluido.
El workflow incluye una captura real del editor en el artefacto descargable.

## Editar visualmente con Foleys GUI Magic

El diseño está en `Assets/magic.xml`. Las posiciones, tamaños, textos, colores y
enlaces a parámetros se editan desde la ventana **foleys GUI magic**. Cada botón
de acorde es un control independiente. La paleta incluye `OmniStrum` (rasgueo),
`BlinblinLogo`, `ChordDisplay` y `Speaker`.

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
cmake --build build-macos --parallel 3 --target Omnichord_Standalone Omnichord_VST3
open build-macos/Omnichord_artefacts/Release/Standalone/Omnichord.app
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

## Teclados y orden del Omnichord

Se usa el orden físico **Db, Ab, Eb, Bb, F, C, G, D, A, E, B, F#**.
Las notas se asignan por posición, no por el nombre de la tecla pulsada.
Se emplea la numeración de Ableton Live: C3 = MIDI 60.

| Posición | Tecla en la primera fila | Acorde mayor |
|---|---|---|
| 1 | C1 (36) | Db |
| 2 | C#1 (37) | Ab |
| 3 | D1 (38) | Eb |
| 4 | Eb1 (39) | Bb |
| 5 | E1 (40) | F |
| 6 | F1 (41) | C |
| 7 | F#1 (42) | G |
| 8 | G1 (43) | D |
| 9 | Ab1 (44) | A |
| 10 | A1 (45) | E |
| 11 | Bb1 (46) | B |
| 12 | B1 (47) | F# |

C2–B2 (MIDI 48–59) repite las posiciones con acordes menores.
C3–B3 (MIDI 60–71) las repite con acordes de séptima dominante.
Estas tres filas seleccionan el acorde y no disparan el arpa.
Activa ORGANO DE ACORDES para oír el acompañamiento sostenido.

C4–B4 (MIDI 72–83) recorre las 12 notas de la franja del arpa en el acorde
seleccionado. Cada bloque de 12 teclas superior transpone esa franja una octava.
Con NOTAS DEL ACORDE desactivado, C4 en adelante toca la altura MIDI original.
El acorde queda memorizado al soltar la tecla; All Notes Off/All Sound Off corta
las voces y los efectos. Cambiar la posición gráfica de un botón no modifica
este mapa MIDI, fijado al orden del instrumento original.

En la pestaña **Teclado**, activa el control del Mac y da foco a la ventana:

- **Z / X / C**: elige mayor / menor / séptima para la posición seleccionada.
- **1 2 3 4 5 6 7 8 9 0 Q P**: elige las 12 posiciones, de Db a F#.
- **A W S E D F T G Y H U J**: toca las 12 notas del arpa.

El teclado del Mac se pausa al editar texto o valores y mientras está activo
el modo Edit de Foleys. En Ableton el host debe entregar el foco al plugin;
si usas este control, desactiva el teclado MIDI del ordenador de Live (M) para
que no se dupliquen los eventos. También puedes usar el teclado MIDI de Live
para enviar las notas de las tres filas. Standalone recibe las teclas directamente.

## Sonido y efectos

El registro predeterminado del órgano está una octava por encima de la versión
anterior. La pestaña Organo permite elegir Clasico, Reed, Suave o Brillante,
ajustar timbre, volumen y transposición de octava (-1, 0, +1).

Harp ofrece Clasico, Campana, Seno y Cuerdas, junto con ataque de 1 a 80 ms, sustain, timbre,
vibrato, volumen propio y el modo de notas del acorde/cromático. GENERAL controla
la salida de ambas fuentes después de los efectos.

Delay añade tiempo de 40 a 1200 ms, feedback hasta 85 % y mezcla. Reverb ofrece
mezcla, tamaño y amortiguación. Ambos efectos son estéreo, están después de la
mezcla de órgano y arpa y comienzan con mezcla cero. Sus colas siguen sonando al
bajar el volumen de una fuente; SILENCIO limpia también esas colas.
Todos los ajustes se automatizan y se guardan con la sesión del DAW.

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
