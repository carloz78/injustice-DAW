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
| - | Ajustes v0.4.1 | Hecho: pincel (Playlist y Piano Roll), cabezal que se arrastra sin trabarse, el Step Sequencer reproduce solo su patron y sus samples suenan completos, biblioteca de samples con importar/arrastrar/nueva carpeta, logo nuevo |
| - | Ajustes v0.4.2 | Hecho: escuchar samples en el Browser, varias carpetas con busqueda en todas, selector (rectangulo) y mover clips entre pistas, ventana de plugin que abre a la primera, Piano Roll siempre disponible |
| - | Ajustes v0.4.4 | Hecho: Un gesto en la Playlist = un solo Ctrl+Z; Ctrl+A selecciona todos los clips y Esc deselecciona; se quita el botón S del Master (no hacía nada); la previa del browser suena al 80 % y se detiene al dar Play |
| - | v0.5.0 | Hecho: Step Sequencer nuevo (hasta 8 patrones como clips de la Playlist, volumen y pan por fila, fuerza por paso). Pitch y Decay no estan: el sampler del motor no los tiene |
| - | v0.5.1 | Hecho: barra de estado (dispositivo, frecuencia, bufer, proyecto), posicion en compases o minutos (clic), menu de clip con renombrar y silenciar, textos mas cortos y claros |
| - | v0.6.0 | Hecho: exportar (WAV 16/24, FLAC, 44.1/48/96 kHz, normalizar, pistas por separado), copiar/cortar/pegar clips, color por clip, limitador y medidor de pico en el Master (InjLimiter.h), guardar con los samples, envios y retornos (A/B), armar pista y grabar (MIDI/audio, sin probar con hardware real), automatizacion de volumen por pista |
| - | v0.6.1 | Hecho: autoguardado y recuperacion, barra de estado con dispositivo de audio, bloque ASIO opcional (CMake + clon del SDK en CI, sin probar), recorte de dibujo para proyectos grandes, fundidos de entrada/salida en clips de audio, exportar clip MIDI desde la playlist, Archivo > Importar MIDI |
| - | v0.6.2 | Hecho: cuantizar notas en el piano roll (boton con iman / tecla Q), automatizacion de cualquier parametro de plugin por pista (menu de la pista > Parametro a automatizar; ademas de volumen y pan), tono (semitonos) y velocidad de clips de audio con Rubber Band (GPL-2+, se baja en CMake con FetchContent; -DINJ_RUBBERBAND=OFF lo desactiva). Pendiente de probar en Windows/MSVC |
| - | v0.6.3 | Hecho: panel Medidores (F11 o menu Ver): espectro del Master, volumen LUFS momentaneo/corto/integrado (ITU-R BS.1770, puerta -70/-10), pico, fase y vectorscopio, con toma de audio InjTap al final del Master; piano roll con escala resaltada y notas fantasma de otros clips; vista general del proyecto en la barra de desplazamiento horizontal; marcadores en la regla (clic derecho, Ctrl+M, Ctrl+flechas). No hecho: compas variable (la regla asume 4 tiempos) y carpetas de pistas |
| - | v0.6.4 | Hecho: iman al tamano original al editar el largo de un clip, zoom con la rueda sobre la regla, explorador con modo Categorias (kicks, snares, hats...) y fix del boton Muestras, metronomo con acento en el primer tiempo del compas. Tambien: colores de pista mas vivos, compas 1 pegado a la cabecera, el boton de automatizacion alterna y oculta las curvas. Pendiente: carpetas de pistas, compas variable. | - |
| - | v0.7.0 | Hecho: instrumentos incluidos (menu de la pista: Instrumento incluido) con 8 sonidos de sintetizador interno (leads, bajos, pad) y Kit INJUSTICE; paquete de 26 sonidos generados por codigo en INJUSTICE Pack dentro de Muestras. Los stems por pista ya existian (Exportar > Cada pista por separado). | - |
| - | v0.7.1 | Hecho: browser con zonas Instrumentos / Injustice Packs / Mis carpetas (plegables); paquete Injustice Core Pack de 152 sonidos creado en segundo plano; flechita de la pista pliega y despliega; tijeras con linea guia y destello; pincel que pinta el clip seleccionado o el ultimo usado; letra de interfaz unificada (14 px). | - |
| - | v0.7.2 | Hecho: barra de desplazamiento horizontal siempre visible y delgada; browser con pestanas Instrumentos / Packs / Mis samples; el boton de automatizacion muestra las curvas al activarlo y las oculta al desactivarlo; el pincel recuerda el ultimo clip tocado y lo pega al lado. | - |
| 5 | Step Sequencer (patrones) y Browser | Pendiente |

Rendimiento: antes cada vista redibujaba todo a 30 Hz. Ahora la Playlist compara una firma de su contenido y, si solo se mueve el cabezal, redibuja únicamente esa franja; el Piano Roll y el Mixer redibujan solo si cambia la posición o los medidores. En reposo el proceso usa ~2 % de un núcleo (Linux, sin audio).
Grabar: arma una pista con el círculo del Mixer y pulsa Grabar; entra MIDI de teclados conectados (si la pista tiene instrumento) o audio. Ctrl+T activa el teclado de la computadora como piano (Z-M / Q-U, Re Pág y Av Pág cambian la octava) y también se graba.
| v0.7.3 | PackDSP.h (síntesis 88.2 kHz, polyBLEP, biquads, decimación FIR) + SamplePack.h v3: 506 sonidos 909/808/gabber/hardcore; 22 sintes y 4 kits |
| v0.7.4 | Samples v4 (656 sonidos; hats 909 con ruido+metal y niveles por familia), Piano Roll estilo FL (lápiz por defecto, clic derecho borra, rectángulo de selección, grupo mover/copiar/pegar, Ctrl+A/C/V/D, flechas), Audition.h (suena al poner notas, elegir instrumento y activar pasos), Cursors.h (lápiz/pincel/tijeras/borrador), tijeras con línea azul animada, Step Sequencer abre al instante (sampler perezoso) |
| v0.7.5 | Revisión: paquete de sonidos se crea oculto y se mueve al terminar (el navegador no ve carpetas a medias), sin DC ni chasquidos al inicio, Sirena más baja; Step Sequencer con 4 sonidos 909 por defecto; clip del piano roll crece al mover/estirar notas; teclas negras visibles; avisos al crear sonidos |
| v0.8.0 | Teclado de la computadora como piano (Ctrl+T, Z-M / Q-U, octava con Re/Av Pág), toca en la pista seleccionada y se graba a un clip MIDI al detener; corregido el texto de Grabar |
| v0.8.1 | Compás variable: clic en "4/4" de la barra superior (2/4, 3/4, 5/4, 6/8, 7/8, 9/8, 12/8…); rejilla, números de compás, piano roll y metrónomo lo siguen |
| v0.8.2 | Samples v5 (714: +58 bucles de hats, bajos, percusión y redobles de snare/clap/kick); buscador del Browser rediseñado (píldora con lupa, botón borrar, resultados en dos tonos); lista de plugins con sección INJUSTICE AUDIO al principio y resaltada |
| v0.8.3 | Samples v6: 1017 sonidos, todas las carpetas con 100 o más (Kicks 150, Snares 102, Claps 102, Hats 110, Percusiones 136, Bajos 105, Leads 104, FX 102, Loops 106) |
| v0.9.0 | Plantillas de género (8 estilos, 7 pistas y 28 compases cada una) en Archivo; Generador de ideas (Herramientas, Ctrl+G); grupos de pistas; teclas sin conflicto; corrección: pistas con sintetizador interno graban MIDI; samples v7 (ruido distinto en cada clap/hat/snare, hats/claps/kicks más variados); pruebas de rendimiento (100 pistas, 60 mil notas) |
| v0.9.1 | Clic derecho sobre un clip de la Playlist lo borra al instante (Mayús + clic derecho abre el menú del clip) |
| v0.9.2 | Samples v8: kicks, snares, claps y hats 909/808 ajustados a medidas de grabaciones reales de TR-909 y TR-808 (duración, tono y brillo), solo como referencia de medición (no se incluyen sus audios) |
| v0.9.3 | 84 bombos nuevos de 8 familias (Rotterdam, Frenchcore, Old School, Industrial, Screech, Hard Techno, Rumble, Saturado) con saturación distinta, kicks gabber/hardcore reducidos a los más distintos, hard techno (hats, claps, percusión, bajos rumble, 6 bucles a 150 BPM); pack v9 |
| v0.9.4 | Pista marcada resaltada (cabecera + carril), rueda sobre la barra inferior = zoom, Piano Roll: zoom vertical (Alt+rueda / rueda en teclas), zoom en regla, Ctrl+/-, L oculta velocity, menu Herramientas de notas (acordes, arpegio, strum, legato, staccato, unir, cortar, voltear, octava, velocidad); M silencia clips; bucles de bateria rehechos (6 patrones, golpes variados, rellenos, bombeo); pack v10 |
| v0.9.5 | Seleccion en rojo (rectangulo y clips/notas elegidos), linea de tijeras con resorte (Playlist y Piano Roll, con estela), zoom suave, 60 Hz; Piano Roll: tonos aleatorios, ajustar a escala, invertir acorde, estirar/encoger; Playlist: repetir x2/x4/x8, dividir en 2/4/8; Step: desplazar, aleatorizar, invertir, repetir mitad, humanizar |
| v0.9.6 | Animacion al borrar clips (sombra roja que se encoge y se desvanece), seleccion con relleno rojo en clips y notas |
| v0.9.7 | Sidechain real por pista (menu de la pista > Sidechain: elige la pista fuente, fuerza y recuperacion) con compresor interno de Tracktion y deteccion por la fuente; las 8 plantillas lo aplican al bajo, acordes y pad desde el bombo |
| v0.9.8 | Medidores del mixer con marca de pico retenido; inercia al desplazar la Playlist; resalte suave en las barras superiores; detector de tempo y tono (menu del navegador y del clip, ajustar al tempo del proyecto); acceso directo a los efectos INJUSTICE AUDIO en el menu de cada pista; Modo ensayo (F12): rejilla de clips para lanzar en vivo con cuantizacion |
| v0.9.9 | Animaciones estilo FL Studio: paneles que aparecen creciendo y se cierran con una sombra que se desvanece; los pasos del Step Sequencer destellan al sonar y al pintarlos; brillo suave en faders y perillas del mixer al pasar o arrastrar; la caja de BPM pulsa con cada tiempo; cabezal de la Playlist interpolado entre avisos del motor (sin saltos) |
