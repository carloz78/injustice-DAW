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
