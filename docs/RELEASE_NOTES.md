Omnichord 0.7.2 para macOS 11 o posterior, Apple Silicon e Intel.

Novedades de 0.7.2:

- El LED de cada pestaña parpadea en amarillo mientras su canal está en solo. El aviso permanece visible al cambiar de página. FX también parpadea si Delay o Reverb están en solo.
- Apagar un canal elimina automáticamente su solo desde mixer, pestañas o automatización. CHORDS y FX limpian los solos de sus grupos. Al reactivarlos no vuelve el solo anterior.
- Presets y proyectos descartan solos guardados en canales apagados. Ctrl+clic en MSTR conserva la función de limpiar todos los solos.

Incluye las mejoras de 0.7.1:

- Ritmos sincronizados con Play/Stop, tempo y posición del DAW, incluidos rebobinado y loops. Con el canal Ritmos armado y DAW activado, la reproducción arranca al pulsar Play en el host. All Notes Off del DAW conserva el ritmo sincronizado; Stop lo detiene. También respeta el transporte cuando no hay posición PPQ. Standalone mantiene su reloj interno.
- Estado de transporte visible y botón ARM cuando se sigue al DAW. SILENCIO y All Sound Off siguen funcionando como parada completa.
- Niveles compensados: Organ +9 dB, FM +7.5 dB y ajustes individuales en los cinco samples de fábrica. Se mantienen los audios originales y su dinámica. El balance de los presets anteriores cambia con esta revisión; los valores de sus parámetros se conservan.
- Volúmenes, retornos y envíos en dB, con lectura permanente y editable debajo de los faders. 0 dB equivale a ganancia unitaria y -inf dB a silencio. Las escalas internas de los parámetros y el formato de presets se conservan.
- Pan dibujado desde el centro y Width con indicadores y puntos que se abren de forma simétrica.
- FM abre en modo sencillo con preset, voces, octava y seis potes esenciales. AVANZADO muestra los seis operadores, algoritmos y envolventes; cambiar de vista conserva el sonido editado.

Conserva mixer, capas simultáneas, CHORD/CHROMATIC, Tape en Master, efectos por envío, librería Omnichord Vol. 1 y presets completos de la 0.7.

El PKG instala VST3 en `/Library/Audio/Plug-Ins/VST3/` y standalone en `/Applications/`. Los bundles usan firma ad hoc; el instalador no está notarizado. La carga y la escucha dentro de Ableton requieren un Mac.
