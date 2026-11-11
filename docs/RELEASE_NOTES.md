Omnichord 0.7.5 para macOS, Apple Silicon e Intel.

- Corrige GraySlate: una sola opción «Grayslate samples» en Samples reproduce el acorde grabado correspondiente a cada botón/MIDI, sin construir tríadas a partir de un acorde.
- La misma opción en Harp reproduce las 12 grabaciones de arpa del acorde seleccionado a su pitch original. Fuera de esas posiciones se transpone el sample correspondiente.
- Bancos precargados y compartidos entre instancias: los cambios de acorde no leen archivos. PCM16 compacto, canales mono compartidos y soporte IEEE float64, conservando los 96 kHz.
- Excepciones del material disponible: Bb no tiene la última nota de arpa, que se obtiene transponiendo una vecina. Maj7 combina el mayor grabado con una séptima mayor; su arpa adapta las séptimas dominantes.
- Conserva los sonidos originales, los efectos, el contraste mejorado de Delay y los presets completos. Selecciones individuales GraySlate de la 0.7.4 pasan al banco automático.

Los diez ritmos GraySlate quedan archivados, sin mostrarse como instrumentos separados. Windows sigue pendiente para la futura 0.8.
