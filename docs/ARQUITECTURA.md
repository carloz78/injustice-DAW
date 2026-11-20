# INJUSTICE DAW - Arquitectura (borrador Etapa 0)

## Decisiones tomadas
| Tema | Decisión |
|---|---|
| Alcance v1 | DAW completo con el diseño de los 5 mockups. Sin instrumentos nativos ni librería de samples. |
| Instrumentos y efectos | Solo plugins VST3 (los de Injustice Audio y los de terceros). |
| Pista y mixer | Una pista = un canal del mixer. |
| Step Sequencer | Genera un clip de patrón que se coloca en la Playlist. |
| Mixer | Con medidores de nivel planos, sin brillos. |
| Licencia | Código abierto. Tracktion Engine (GPLv3) + JUCE (AGPLv3) => el conjunto se publica bajo AGPLv3. |

## Dependencias fijadas
- JUCE 8.0.6, commit 19edd538 (el que fija Tracktion v3.2.0).
- Tracktion Engine v3.2.0.
- C++20, CMake, Ninja/MSVC. CI en GitHub Actions (windows-latest).

## Capas
```
Interfaz (JUCE)  ->  Modelo del proyecto (Tracktion Edit, ValueTree)  ->  Grafo de audio  ->  Dispositivo (WASAPI/ASIO)
```
La interfaz solo lee y modifica el modelo (con deshacer). El hilo de audio nunca espera a la interfaz.

## Etapa 0: resultados
Ver tools/engine_probe: render offline de un clip MIDI y de un StepClip, verificados con datos sobre el WAV.
El sinte (FourOsc) y el sampler de Tracktion se usan solo como banco de pruebas, no como funciones del producto.

## Pendiente
- Prueba de carga de un VST3 real en Windows (no se puede en el sandbox Linux; lo prueba el CI/usuario).
- Escaneo de plugins en proceso aparte (para que un plugin que falla no cierre el DAW).
- Formato de proyecto versionado y autoguardado.

## Hallazgos de la Etapa 0 (verificados en Linux; ver docs/ETAPA0_RESULTADOS.txt)
| Hallazgo | Consecuencia para el diseño |
|---|---|
| Clip MIDI -> instrumento -> WAV: las notas caen en el instante exacto y hay silencio entre ellas. | El modelo de clips MIDI de Tracktion sirve para el Piano Roll. |
| StepClip + sampler: golpes en la cuadrícula de 16avos, sin golpes fuera de ella. | El Step Sequencer puede apoyarse en StepClip (clip de patrón en la Playlist). |
| La información de los archivos de audio se carga en segundo plano; sin esperarla el sampler sale mudo. | La exportación a WAV debe esperar a que todos los archivos estén cargados. |
| Con JUCE 8.0.6 + Tracktion v3.2.0 todo compila y funciona; Tracktion "master" exige JUCE 9 (en desarrollo). | Nos quedamos en v3.2.0 + JUCE 8.0.6 hasta que haya motivo para subir. |

## Etapa 1A (estado)
| Pieza | Estado |
|---|---|
| Ventana, barra superior, transporte (play/stop/loop), BPM (arrastrar o rueda), deshacer/rehacer | Hecho |
| Playlist: pistas fijas 150 px, regla de compases, cabezal, zoom (Ctrl+rueda), scroll | Hecho |
| Clips de audio: arrastrar archivo desde el explorador, mover, cortar (tijeras), borrar (Supr) | Hecho (sin probar con audio real en Windows) |
| Ajustes de audio (selector de dispositivo) | Hecho |
| Nuevo/Abrir/Guardar, exportar, ayuda, plugins | Hecho en la ronda 2 (ver abajo) |
| Grabar en vivo | Pendiente (el botón avisa) |
| Tonalidad "F#m" | Solo visual, sin función todavía |

## Etapa 1B, parte 1: Piano Roll (estado)
| Pieza | Estado |
|---|---|
| Piano Roll: teclado, cuadrícula, notas, carril de velocity, cuantización 1/4-1/32, zoom y scroll | Hecho |
| Lápiz (añadir y alargar), selector (mover/redimensionar), borrador, Supr, deshacer/rehacer | Hecho y probado con clics simulados (8 comprobaciones) |
| Playlist: lápiz crea un clip MIDI de 4 compases; doble clic en el clip abre el Piano Roll | Hecho |
| Sonido de las notas | Requiere un instrumento VST3 en la pista (siguiente paso); sin él el clip es mudo |

## Etapa 1C (estado)
| Pieza | Estado |
|---|---|
| Mixer flotante (arrastrable, maximizable): 10 canales = 10 pistas, Master, volumen/pan/solo/mute reales, medidores, botón +, panel de efectos | Hecho y probado con clics simulados |
| Panel de efectos del Mixer: lista de plugins del canal; "Agregar efecto" abre la lista de plugins | Hecho (ronda 2); falta verificar con VST3 reales en Windows |
| Step Sequencer: 4 filas (Kick, Clap, Hat, Open Hat), +, 8/12/16/24/32 pasos, power por fila, menú de sample, arrastrar sample a la fila | Hecho; el patrón vive como clip en la Playlist y se probó con render real |
| Browser: categorías, árbol real de carpetas de samples, vista previa de forma de onda, separador y ocultar | Hecho. Las categorías de presets y plugins aparecen atenuadas hasta su etapa |
| Arrastrar un sample del Browser a la Playlist o a una fila | Implementado con arrastre del sistema; probado solo por separado, falta verificar en Windows |
| Escuchar el sample en el Browser (pre-escucha) | Pendiente |

Pruebas automáticas internas: `INJ_SELFTEST=1 INJ_SAMPLE_FILE=<wav> INJ_DEMO_MIDI=1` (22 comprobaciones).

## Ronda 2 (mejoras pedidas por el usuario)
| Pieza | Estado |
|---|---|
| Paneles flotantes redimensionables desde bordes y esquinas (Mixer, Step Sequencer); el contenido se escala con proporción fija | Hecho (arrastre real de borde sin probar en Windows) |
| Mixer rediseñado: perillas de pan, tapas de fader metálicas, medidores, tornillos, escala de dB lineal, Master, panel de efectos; scroll cuando hay más de 10 pistas | Hecho |
| Proyecto: Nuevo / Abrir / Guardar / Guardar como (`.injdaw`), aviso de cambios sin guardar al salir, título con `*` | Hecho y probado (guardar -> reabrir conserva pistas y clips) |
| Exportar a WAV 24 bits con ventana de progreso | Hecho y probado (render offline) |
| Ayuda con atajos, avisos (toasts), tooltips en todos los botones | Hecho |
| Atajos: Espacio, Inicio, Ctrl+N/O/S/Mayús+S/E/Z/Y/D, F1, F5-F10 | Hecho |
| Metrónomo (icono de altavoz); botón Grabar avisa que llega después; herramienta Recortar | Hecho |
| Playlist: menú de pista (renombrar, mute, solo, color, agregar, eliminar), botones M/S en cabecera, "Agregar pista", recortar clip por bordes, Ctrl+D, clic derecho, barras de desplazamiento | Hecho y probado con clics simulados |
| Lista de plugins VST3: escaneo en un proceso aparte por plugin (si uno se cuelga, la app no), lista persistente con buscador, carpetas extra, agregar como instrumento o efecto, ventana del editor del plugin | Hecho; el escáner se probó solo con un archivo inexistente; **falta verificar con VST3 reales en Windows** |
| Proyecto nuevo o abierto con 500 pistas (los guardados se completan a 500) (límite del motor subido de 400 a 1000 con EngineBehaviour; creación acotada para no colgarse) | Hecho y probado (arranque ~2 s en Linux) |
| Etiquetas del Step Sequencer leídas del sampler (sobreviven a guardar/abrir) | Hecho |

Pruebas automáticas internas (`INJ_SELFTEST=1 INJ_SAMPLE_FILE=<wav> INJ_DEMO_MIDI=1`): 42 comprobaciones.

## Ronda 3 - rediseño pastel (diseños nuevos del usuario)
Decisiones acordadas: todo el DAW en pastel rosa/morado; barra de título nativa de Windows con la barra propia debajo; Mixer de 10 canales con "+"; sin panel de EQ en la v1; S/M fuera de la Playlist (doble clic = silenciar, clic derecho = solo, F2/menú = renombrar); Piano Roll con importar/exportar MIDI y humanizar; Browser con categorías propias (no las de Ableton); nombres reales de plugins (no los de FL); Step Sequencer con varios patrones.

| Paso | Contenido | Estado |
|---|---|---|
| 1 | Paleta pastel (Theme.h, muestreada de los diseños), barra superior única (logo, menús File/Edit/View/Options/Tools/Help, transporte, BPM, 4/4, posición, metrónomo, CPU/RAM reales), botones con hover/pulsado/activo, Grabar rojo al armarlo, aspecto oscuro de menús y diálogos, rendimiento (redibujo solo ante cambios) | Hecho (v0.3.0) |
| 2 | Playlist nueva (cabeceras sin S/M, pistas más altas, doble clic = silenciar) | Hecho (v0.3.1): sin S/M, doble clic = silenciar, Alt+clic = solo, clips en color pastel sólido, estilo Ableton |
| 3 | Mixer nuevo (10 canales + "+") | Hecho (v0.3.2): canales sin color, Master morado, 10 ranuras de efectos, LED, circulo de armado y menu del canal. Ademas: iman con cuadricula adaptable al zoom |
| 4 | Piano Roll nuevo | Hecho (v0.4.0): barra propia con herramientas, cuantizacion, importar/exportar MIDI, humanizar y cortar notas |
| 5 | Step Sequencer (patrones) y Browser | Pendiente |

Rendimiento: antes cada vista redibujaba todo a 30 Hz. Ahora la Playlist compara una firma de su contenido y, si solo se mueve el cabezal, redibuja únicamente esa franja; el Piano Roll y el Mixer redibujan solo si cambia la posición o los medidores. En reposo el proceso usa ~2 % de un núcleo (Linux, sin audio).
Grabar: el botón se arma (rojo) pero la grabación en vivo aún no está implementada.
