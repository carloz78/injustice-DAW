juce_add_gui_app(InjusticeDAW PRODUCT_NAME "INJUSTICE DAW" COMPANY_NAME "Injustice Audio" VERSION 0.1.0 ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png" ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png")
target_sources(InjusticeDAW PRIVATE Main.cpp SysInfo.cpp)
target_compile_definitions(InjusticeDAW PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_APPLICATION_NAME_STRING="INJUSTICE DAW" JUCE_APPLICATION_VERSION_STRING="0.1.0")
target_link_libraries(InjusticeDAW PRIVATE tracktion::tracktion_engine juce::juce_recommended_config_flags)

# Estirar tiempo y cambiar tono con Rubber Band (GPL-2+, compatible con AGPL-3.0). Se compila junto al motor (un solo .cpp).
# Con -DINJ_RUBBERBAND_DIR=<carpeta llamada "rubberband"> se usa una copia local; con -DINJ_RUBBERBAND=OFF se compila sin estiramiento.
option(INJ_RUBBERBAND "Estirar tiempo y cambiar tono (Rubber Band)" ON)
if(INJ_RUBBERBAND)
    set(INJ_RUBBERBAND_DIR "" CACHE PATH "Copia local de Rubber Band (carpeta llamada rubberband)")
    if(NOT INJ_RUBBERBAND_DIR)
        include(FetchContent)
        FetchContent_Declare(rubberband GIT_REPOSITORY https://github.com/breakfastquay/rubberband.git GIT_TAG v4.0.0 GIT_SHALLOW TRUE
            SOURCE_DIR ${CMAKE_BINARY_DIR}/rbdeps/rubberband)
        FetchContent_MakeAvailable(rubberband)
        set(INJ_RUBBERBAND_PARENT ${CMAKE_BINARY_DIR}/rbdeps)
    else()
        get_filename_component(INJ_RUBBERBAND_PARENT "${INJ_RUBBERBAND_DIR}" DIRECTORY)
    endif()
    target_compile_definitions(InjusticeDAW PRIVATE TRACKTION_ENABLE_TIMESTRETCH_RUBBERBAND=1 TRACKTION_BUILD_RUBBERBAND=1)
    target_include_directories(InjusticeDAW PRIVATE "${INJ_RUBBERBAND_PARENT}")
endif()

# ASIO (baja latencia en Windows). Steinberg publico su SDK con licencia GPLv3, compatible con AGPL-3.0.
# Es opcional: si el SDK esta en third_party/asiosdk (o se indica con -DINJ_ASIO_SDK_DIR=...) se activa; si no, la app compila igual sin ASIO.
if(WIN32)
    set(INJ_ASIO_SDK_DIR "" CACHE PATH "Carpeta del SDK de ASIO (opcional)")
    if(NOT INJ_ASIO_SDK_DIR AND EXISTS "${CMAKE_SOURCE_DIR}/third_party/asiosdk/common/iasiodrv.h")
        set(INJ_ASIO_SDK_DIR "${CMAKE_SOURCE_DIR}/third_party/asiosdk")
    endif()
    if(INJ_ASIO_SDK_DIR AND EXISTS "${INJ_ASIO_SDK_DIR}/common/iasiodrv.h")
        message(STATUS "ASIO activado: ${INJ_ASIO_SDK_DIR}")
        target_compile_definitions(InjusticeDAW PRIVATE JUCE_ASIO=1)
        target_include_directories(InjusticeDAW PRIVATE "${INJ_ASIO_SDK_DIR}/common" "${INJ_ASIO_SDK_DIR}/host" "${INJ_ASIO_SDK_DIR}/host/pc")
    else()
        message(STATUS "ASIO no disponible (no se encontro el SDK): se compila solo con el audio normal de Windows")
    endif()
endif()
