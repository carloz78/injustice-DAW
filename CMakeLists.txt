cmake_minimum_required(VERSION 3.22)
project(InjusticeDAW VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Dependencias fijadas (ver docs/ARQUITECTURA.md). Para compilar sin red,
# apunta a copias locales con -DINJ_JUCE_DIR=... -DINJ_TRACKTION_DIR=...
set(INJ_JUCE_REF      19edd538429c93d277bf95b55aaa7e3eb545f951 CACHE STRING "JUCE 8.0.6 (el que fija Tracktion v3.2.0)")
set(INJ_TRACKTION_REF v3.2.0 CACHE STRING "Tracktion Engine")

include(FetchContent)
if(INJ_JUCE_DIR)
    add_subdirectory(${INJ_JUCE_DIR} juce_build)
else()
    FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG ${INJ_JUCE_REF})
    FetchContent_MakeAvailable(juce)
endif()

if(INJ_TRACKTION_DIR)
    add_subdirectory(${INJ_TRACKTION_DIR}/modules tracktion_build)
else()
    # Solo los módulos (carpeta modules/); JUCE ya lo trajimos arriba, sin submódulos.
    FetchContent_Declare(tracktion
        GIT_REPOSITORY https://github.com/Tracktion/tracktion_engine.git
        GIT_TAG ${INJ_TRACKTION_REF}
        GIT_SUBMODULES ""
        SOURCE_SUBDIR modules)
    FetchContent_MakeAvailable(tracktion)
endif()

add_subdirectory(tools/engine_probe)
