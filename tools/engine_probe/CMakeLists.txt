juce_add_console_app(EngineProbe PRODUCT_NAME "EngineProbe")
target_sources(EngineProbe PRIVATE main.cpp)
target_compile_definitions(EngineProbe PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_XRANDR=0 JUCE_USE_XINERAMA=0 JUCE_USE_XCURSOR=0 JUCE_USE_XSHM=0 JUCE_USE_XRENDER=0
    JUCE_ALSA=0 JUCE_JACK=0)
target_link_libraries(EngineProbe PRIVATE
    tracktion::tracktion_engine
    juce::juce_recommended_config_flags)
