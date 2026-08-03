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

# Shared between the app and the snapshot tool so the tool can never drift from what
# actually ships.
set(QC_APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/MainComponent.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FileAnalysisJob.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TargetLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/LoudnessGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/VerdictPanel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MediaFoundationAudioFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BatchAnalyser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/BatchTable.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GlassStyle.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GlassLookAndFeel.cpp
)

set(QC_APP_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    # JUCE's own MP3 decoder, so podcast deliverables open without a system codec.
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

juce_add_gui_app(QCApp
    PRODUCT_NAME "QC App"
    COMPANY_NAME "QC App"
    # Derived from the product name it would contain spaces, which is not a valid ID.
    BUNDLE_ID "com.qcapp.analyser"
    VERSION ${PROJECT_VERSION}
)

target_sources(QCApp PRIVATE ${QC_APP_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/Main.cpp)

if(WIN32)
    # AAC/M4A decoding via the codecs already present on the system.
    target_link_libraries(QCApp PRIVATE mfplat mfreadwrite mfuuid ole32)
endif()

target_compile_definitions(QCApp PRIVATE
    ${QC_APP_DEFINITIONS}
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

# ---------------------------------------------------------------------------
# qc_uishot - renders the interface to a PNG without opening a window.
#
# Off by default. It exists so the interface can be reviewed and compared across
# changes, which a screenshot of a running app cannot do repeatably.
# ---------------------------------------------------------------------------
option(QC_BUILD_UI_SNAPSHOT "Build the offscreen UI snapshot tool" OFF)

if(QC_BUILD_UI_SNAPSHOT)
    juce_add_console_app(qc_uishot PRODUCT_NAME "qc_uishot")

    target_sources(qc_uishot PRIVATE
        ${QC_APP_SOURCES}
        ${CMAKE_SOURCE_DIR}/Tools/UiSnapshot.cpp
    )

    target_compile_definitions(qc_uishot PRIVATE
        ${QC_APP_DEFINITIONS}
        JUCE_MODAL_LOOPS_PERMITTED=1
    )

    if(WIN32)
        target_link_libraries(qc_uishot PRIVATE mfplat mfreadwrite mfuuid ole32)
    endif()

    target_link_libraries(qc_uishot PRIVATE
        qc_engine
        qc_resources
        juce::juce_audio_formats
        juce::juce_audio_utils
        juce::juce_gui_extra
        PUBLIC
        juce::juce_recommended_config_flags
    )
endif()
