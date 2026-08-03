include(${CMAKE_SOURCE_DIR}/cmake/get_cpm.cmake)

# Pinned deliberately. JUCE 9 is available but nothing here has been verified against
# its API changes; bump this line and run the app once before trusting it.
CPMAddPackage(
    NAME JUCE
    GITHUB_REPOSITORY juce-framework/JUCE
    GIT_TAG 8.0.9
)

# targets.json ships inside the binary as the factory default. On first run the app
# writes a copy to the user's application data directory, and prefers that copy
# thereafter, so spec drift is fixed by editing a file rather than by rebuilding.
juce_add_binary_data(qc_resources SOURCES ${CMAKE_SOURCE_DIR}/Resources/targets.json)

juce_add_gui_app(QCApp
    PRODUCT_NAME "QC App"
    COMPANY_NAME "QC App"
    # Derived from the product name it would contain spaces, which is not a valid ID.
    BUNDLE_ID "com.qcapp.analyser"
    VERSION ${PROJECT_VERSION}
)

target_sources(QCApp PRIVATE
    Main.cpp
    MainComponent.cpp
    FileAnalysisJob.cpp
    TargetLibrary.cpp
    LoudnessGraph.cpp
    VerdictPanel.cpp
)

target_compile_definitions(QCApp PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    # JUCE's own MP3 decoder, so podcast deliverables open without a system codec.
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:QCApp,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:QCApp,JUCE_VERSION>"
)

# Note on licensing: JUCE_DISPLAY_SPLASH_SCREEN is left at its default. Turning it off
# is a licence decision, not a build decision - make it deliberately.

target_link_libraries(QCApp PRIVATE
    qc_engine
    qc_resources
    juce::juce_audio_formats
    juce::juce_audio_utils
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)
