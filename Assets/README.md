# Logotipo original

`omnichord.png` es el icono de aplicación de 1024 × 1024 píxeles aportado por el
usuario. JUCE genera el `Icon.icns` de macOS y lo incorpora a los recursos del
standalone con su entrada `CFBundleIconFile`. El PKG conserva ese recurso al
instalar `/Applications/Omnichord.app`, para que Finder y el Dock usen el icono.

`Assets/BlinblinLogo.png` es el PNG original de BLINBLIN aportado por el usuario.
CMake lo incrusta en el VST3 y en la aplicación standalone al compilar.
La interfaz lo muestra automáticamente como marca fija, sin selector de imagen.
Lo dibuja, respetando proporción y transparencia,
sin depender de un archivo en el ordenador del músico.

Se conserva el archivo original de 2172 x 724 píxeles con canal alfa.

`magic.xml` contiene el diseño visual de Omnichord v2 para Foleys GUI Magic,
incluidas las pestañas Harp, Organo, Delay, Reverb y Teclado.
