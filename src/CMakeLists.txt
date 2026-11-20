juce_add_gui_app(InjusticeDAW PRODUCT_NAME "INJUSTICE DAW" COMPANY_NAME "Injustice Audio" VERSION 0.1.0 ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png" ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/../assets/icon.png")
target_sources(InjusticeDAW PRIVATE Main.cpp SysInfo.cpp)
target_compile_definitions(InjusticeDAW PRIVATE
    JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_APPLICATION_NAME_STRING="INJUSTICE DAW" JUCE_APPLICATION_VERSION_STRING="0.1.0")
target_link_libraries(InjusticeDAW PRIVATE tracktion::tracktion_engine juce::juce_recommended_config_flags)
