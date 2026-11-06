# Omnichord — BLINBLIN · versión 5

Prototipo de instrumento inspirado en el Omnichord para Ableton Live en macOS.
Incluye 48 acordes (mayores, menores, séptimas y séptimas mayores), una franja de rasgueo de
12 notas, órgano de acordes opcional y controles automatizables de volumen,
timbre, sustain y vibrato. Guarda sus parámetros con el proyecto del DAW.

El motor usa síntesis original: todavía no es una emulación calibrada de un
modelo concreto de Omnichord. No incluye muestras del instrumento ni ritmos.

La versión 5 incorpora una carcasa con volumen sobre un fondo gris opaco, registros
de órgano de iglesia y tonewheel, presets completos con un estado de inicio
seleccionable, predelay/EQ/primeras reflexiones en la reverb y sincronía musical
del delay. Conserva los parámetros anteriores y sus sesiones.

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
Estos paquetes se compilan con `CHORDHARP_GUI_EDITOR=OFF`, sin las herramientas
de edición de Foleys. Conservan el diseño de `Assets/magic.xml` y los temas.
La firma es ad hoc, sin notarización de Apple; la carga en Ableton y la fidelidad
respecto al instrumento real requieren una prueba manual en un Mac.

El artefacto incluye **Omnichord-0.5.1.pkg**. Ábrelo con el instalador de macOS:
instala el VST3 en `/Library/Audio/Plug-Ins/VST3/Omnichord.vst3` y la aplicación
en `/Applications/Omnichord.app`. El asistente solicita permisos de administrador.
Después vuelve a explorar los plugins en Ableton Live. La compilación automática
comprueba el contenido del PKG, sus firmas y la instalación y reinstalación reales
en un servidor de macOS. El VST3 y la app tienen firma ad hoc; el PKG no tiene
certificado Developer ID Installer ni notarización de Apple.

Para generar el PKG de una compilación local, firma primero ambos bundles:

```sh
codesign --force --deep --sign - \
  "build-release/Omnichord_artefacts/Release/VST3/Omnichord.vst3"
codesign --force --deep --sign - \
  "build-release/Omnichord_artefacts/Release/Standalone/Omnichord.app"
bash scripts/build-macos-pkg.sh build-release .
```

El empaquetado utiliza `pkgbuild`, `pkgutil` y Python 3.

## Tocar

- Selecciona un acorde y arrastra por la franja para rasguear hacia ambos lados.
- Activa ORGAN para escuchar el acorde sostenido; SILENCIO apaga el órgano
  y corta las colas.
- Las notas MIDI C0–B3 seleccionan acordes por posición. C4 en adelante toca
  las notas del acorde como la franja; desactiva FOLLOW CHORDS para tocar
  alturas cromáticas libres. La velocidad MIDI controla la intensidad del arpa.
  Note Off deja decaer el arpa y libera el órgano si SUSTAIN está apagado;
  el pedal MIDI de sustain no está implementado.
- El acorde seleccionado queda memorizado y puede automatizarse en Live.
- El rasgueo con ratón produce audio, pero no genera notas MIDI para grabar.

## Diseño BLINBLIN

Panel inspirado en la distribución de un Omnichord: carcasa crema redondeada,
acordes circulares ordenados por quintas a la izquierda, controles de sonido
y superficie de rasgueo vertical a la derecha. La rejilla de altavoz es decorativa.

El logo original de BLINBLIN, `Assets/BlinblinLogo.png`, viene incluido en el
VST3 y en la aplicación. Aparece automáticamente al abrir el instrumento,
conservando su transparencia y proporciones, sin necesitar un archivo externo.
El logo se muestra sin un recuadro de fondo. Se incluyen las variantes normal
y blanca, elegidas automáticamente por contraste con el color real del panel,
también al editar Personalizado. Pinky sustituye a Crema en el mismo índice,
conservando la selección de los proyectos existentes. La marca es fija: se ha eliminado el selector de imagen y se ignoran los logos
personalizados de sesiones antiguas. El editor visual permite colocar y escalar
el componente, conservando siempre el PNG original incluido.
El workflow incluye una captura real del editor en el artefacto descargable.

## Editar visualmente con Foleys GUI Magic

El diseño está en `Assets/magic.xml`. Las posiciones, tamaños, textos, colores y
enlaces a parámetros se editan desde la ventana **foleys GUI magic**. Cada botón
de acorde es un control independiente. La paleta incluye `OmniSwitch` (botones físicos), `OmniStrum` (rasgueo),
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

| Fila | Notas MIDI en Ableton | Números MIDI |
|---|---|---|
| Mayores | C0–B0 | 24–35 |
| Menores | C1–B1 | 36–47 |
| Séptima dominante | C2–B2 | 48–59 |
| Séptima mayor (Maj7) | C3–B3 | 60–71 |

Las doce teclas consecutivas de cada octava corresponden, de izquierda a derecha,
a **Db, Ab, Eb, Bb, F, C, G, D, A, E, B, F#**. Por ejemplo, C0 selecciona Db mayor,
C#0 selecciona Ab mayor y B3 selecciona F#Maj7. Estas cuatro filas seleccionan
el acorde; activa ORGAN para escuchar el acompañamiento sostenido.
Maj7 usa los intervalos 0, 4, 7, 11; la séptima dominante usa 0, 4, 7, 10.

C4–B4 (MIDI 72–83) recorre las 12 notas de la franja del arpa en el acorde
seleccionado. Cada bloque de 12 teclas superior transpone esa franja una octava.
Con FOLLOW CHORDS desactivado, C4 en adelante toca la altura MIDI original.
El acorde queda memorizado al soltar la tecla; All Notes Off/All Sound Off corta
las voces y los efectos. Cambiar la posición gráfica de un botón no modifica
este mapa MIDI, fijado al orden del instrumento original.

En la pestaña **Teclado**, activa el control del Mac y da foco a la ventana:

| Fila | Db / Ab | Eb, Bb, F, C, G, D, A, E, B, F# |
|---|---|---|
| Mayores | Ctrl+Q / Ctrl+W | Q W E R T Y U I O P |
| Menores | Ctrl+A / Ctrl+S | A S D F G H J K L Ñ |
| Séptimas | Ctrl+Z / Ctrl+X | Z X C V B N M , . - |
| Maj7 | Ctrl+Mayús+A / Ctrl+Mayús+S | Mayús + A S D F G H J K L Ñ |

El arpa se toca con **1 2 3 4 5 6 7 8 9 0**, más las dos teclas a la derecha
del 0 para las cuerdas 11 y 12 (apóstrofo e ¡ en un teclado español).
En Mac se usan las posiciones físicas del teclado español QWERTY, también al
mantener Ctrl o Mayús para seleccionar Db/Ab o Maj7. El teclado numérico permite las primeras
diez cuerdas. Los acordes quedan memorizados al soltar las teclas.

El teclado del Mac se pausa al editar texto o valores y mientras está activo
el modo Edit de Foleys. En Ableton el host debe entregar el foco al plugin;
si usas este control, desactiva el teclado MIDI del ordenador de Live (M) para
que no se dupliquen los eventos. También puedes usar el teclado MIDI de Live
para enviar las notas de las cuatro filas. Standalone recibe las teclas directamente.

## Sonido y efectos

El registro predeterminado del órgano está una octava por encima de la versión
anterior. La pestaña Organo permite elegir Clasico, Reed, Suave o Brillante,
ajustar timbre, volumen y transposición de octava (-1, 0, +1). Apertura añade
un registro agudo al acorde y Coro mezcla voces ligeramente desafinadas en
estéreo. Coro a cero deja el órgano en mono.

Harp ofrece Clasico, Campana, Seno y Cuerdas, junto con ataque de 1 a 80 ms, sustain, timbre,
vibrato, volumen propio y el modo de notas del acorde/cromático. GENERAL controla
la salida de ambas fuentes después de los efectos.

Delay añade tiempo libre de 40 a 1200 ms o divisiones sincronizadas, feedback
hasta 85 % y mezcla. Reverb ofrece mezcla, tamaño, amortiguación, predelay,
EQ y primeras reflexiones. Ambos efectos son estéreo, están después de la
mezcla de órgano y arpa y comienzan con mezcla cero. Sus colas siguen sonando al
bajar el volumen de una fuente; SILENCIO limpia también esas colas.
Todos los ajustes se automatizan y se guardan con la sesión del DAW.

## Biblioteca de presets y estado de inicio

La barra superior permite elegir presets de fábrica y tu biblioteca, **Cargar**
un archivo y **Guardar** el estado completo en un `.omnipreset`. Se incluyen
TODOS los 44 parámetros: órgano, arpa, acordes, sustain, volúmenes, Tape, delay,
reverb, EQ, tempo, divisiones de nota, controles del teclado y selección de tema.
Los archivos se validan antes de cambiar el sonido. La posición y el diseño XML
de los controles se conservan independientemente del preset.

La biblioteca está en `~/Library/Application Support/BLINBLIN/Omnichord/Presets/`.
Puedes guardar o cargar archivos desde otras carpetas con el selector de archivos.
**Inicio → Usar el estado actual al iniciar** guarda una copia completa de todos
los valores actuales como inicio. Cambiar luego los potenciómetros no modifica
esa copia hasta volver a elegir la opción. **Volver al inicio de fábrica** elimina
solo la preferencia de inicio, conservando tus presets.

El standalone arranca con ese estado de inicio y conserva su configuración de
Audio/MIDI. Las nuevas instancias VST3 utilizan también ese inicio, pero al abrir
un proyecto de Ableton prevalece el estado que el proyecto guardó para cada
instancia. La biblioteca incluye Inicio, Iglesia Principal, Catedral ambiental,
Tonewheel Jazz/Gospel/Rock y Omnichord clásico.

Los botones **DELAY** y **REVERB**, junto a SILENCIO, activan cada efecto con una transición suave y conservan su mezcla y demás ajustes. Sus estados se guardan en presets y en el proyecto. Las sesiones anteriores mantienen los efectos activos para respetar su sonido.

## Órgano, reverb y sincronía

El selector de órgano ofrece once registros: los cuatro clásicos, Principal,
Pleno, Catedral, Flauta litúrgica y tres tonewheel. Los registros de iglesia
combinan armónicos y registros de tubos; los tonewheel usan los nueve tiradores
habituales, suboctava de 16 pies, foldback en agudos y percusión. **Rotary** añade
modulación estéreo en los tonewheel; **Percusión** regula su ataque. Son modelos
sintetizados originales. Los proyectos anteriores siguen en el modo clásico para
mantener su timbre.

La reverb añade **Predelay** de 0–250 ms, **Early Reflections** y EQ de tres bandas
(250 Hz, 1 kHz y 5 kHz; ±12 dB). La EQ y las primeras reflexiones afectan al
componente reverberado; MEZCLA controla su nivel frente a la señal directa.

En Delay, **SYNC** sustituye el tiempo en ms por divisiones de nota. `t` indica
tresillo y `dot` puntillo; por ejemplo 1/8t es un tresillo de corcheas y 1/8dot una
corchea con puntillo. Se usa el BPM que entregue el DAW; sin host, el control
**BPM standalone** define el tempo. Se admiten 20–400 BPM y retardos de hasta
12 segundos. En Sync ambos canales respetan la misma duración musical.

## Fondo y carcasa

El editor rellena toda su superficie con gris neutro opaco, también en las
esquinas del instrumento, para verse igual dentro de un host o en standalone.
La carcasa conserva su contorno redondeado y añade profundidad, rebordes,
sombras y textura. El fondo exterior no cambia con el tema.

## Tape y sustain de los acordes

La pestaña **Tape** añade una simulación propia de cinta después del delay y la
reverb y antes del volumen GENERAL. **TAPE** activa el efecto. **Drive** añade
saturación suave, con compensación de ganancia y filtrado; **Noise** controla el
hiss estéreo (cero elimina el ruido), **Wow** la variación lenta de velocidad y
**Flutter** la rápida. Los controles van de 0 a 100 %, se suavizan al moverlos y
se guardan y automatizan con el proyecto. El efecto usa un retardo modulado
centrado en 4 ms para la variación de afinación. Es una implementación original.
TAPE comienza apagado y deja la señal intacta cuando está desactivado. SILENCIO
y los mensajes MIDI de pánico borran sus colas y cortan el ruido hasta volver a tocar.

**ORGAN** activa el órgano y **FOLLOW CHORDS** hace que el arpa siga las notas
del acorde. Ambos son botones amarillos. **SUSTAIN**, en el color del tema,
mantiene el acorde al soltar las teclas; empieza activado para conservar el
comportamiento de las sesiones anteriores. Si lo apagas, el órgano suena mientras
mantienes una tecla MIDI, una tecla de acorde del Mac o un botón de acorde y se
libera suavemente al soltar la última pulsación. Si mantienes varias, tiene
prioridad la más reciente; al soltarla vuelve a la anterior. Los canales MIDI,
el ratón y el teclado del Mac se siguen por separado. El arpa mantiene su
propio decaimiento, independiente de este botón.

## Temas de color

En la parte superior del panel, abre el desplegable con los cuadrados de color.
Cada opción incluye cuatro muestras: **Personalizado, Pinky, Medianoche, Bosque,
Coral y Lavanda**. Cambiar de tema recolorea el panel, los controles y el arpa;
conserva las posiciones de tu XML y la imagen original del logo. La elección se
guarda con el proyecto de Ableton y con el estado de la aplicación standalone.

**Personalizado** recupera los colores definidos en tu XML. Para que los temas
puedan cambiarse, el diseño usa referencias como `$color-ffeee6cf` y una paleta
en `Styles/Style/Palettes/default`. Selecciona Personalizado para editar esas referencias y la paleta
con Foleys; sus colores se conservan al pasar a otro tema y volver. Un color literal añadido manualmente queda fijo hasta volver a cargar
el diseño como diseño de fábrica recompilando. El selector es el componente
`ThemeSelector`; los botones de acorde son `OmniChord` y los interruptores
`OmniToggle`, que mantienen un único enlace con su parámetro.

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

Las pruebas comprueban las cuatro familias de acordes, el mapa MIDI y del Mac,
el decaimiento, el estéreo, el silencio y la estabilidad de voces a
44.1, 48 y 96 kHz; no miden fidelidad respecto a hardware real.

Con JUCE también puedes ejecutar las comprobaciones de presets, interfaz,
tempo del host y efectos:

```sh
cmake -S . -B build -DJUCE_PATH="$HOME/Developer/JUCE" -DCHORDHARP_PLUGIN_TESTS=ON -DCHORDHARP_GUI_EDITOR=OFF
cmake --build build --config Release --target engine_tests processor_tests ambience_tests
ctest --test-dir build -C Release --output-on-failure
```

`ambience_tests` usa AddressSanitizer y UndefinedBehaviorSanitizer con Clang/GCC
para verificar las lecturas de reverb, delay y Tape. Incluye el caso de redondeo
en el extremo del búfer que se corrige en la 0.5.1.
