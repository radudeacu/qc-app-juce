add_executable(qc_engine_tests
    main.cpp
    EngineTests.cpp
    VerdictTests.cpp
)

target_link_libraries(qc_engine_tests PRIVATE qc_engine)
target_include_directories(qc_engine_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(qc_engine_tests PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(qc_engine_tests PRIVATE -Wall -Wextra)
endif()

add_test(NAME engine COMMAND qc_engine_tests)

# The file-reading path needs JUCE, so it is covered by a separate binary that only
# exists when the app is being built. Keeping it out of qc_engine_tests preserves the
# property that the measurement engine can be tested with no dependencies at all.
if(TARGET juce::juce_audio_formats)
    add_executable(qc_file_tests
        main.cpp
        FileTests.cpp
        ${CMAKE_SOURCE_DIR}/Source/App/FileAnalysisJob.cpp
        ${CMAKE_SOURCE_DIR}/Source/App/MediaFoundationAudioFormat.cpp
    )

    if(WIN32)
        target_sources(qc_file_tests PRIVATE AacTests.cpp)
        target_link_libraries(qc_file_tests PRIVATE mfplat mfreadwrite mfuuid ole32)
    endif()

    target_link_libraries(qc_file_tests
        PRIVATE
            qc_engine
            juce::juce_audio_formats
        PUBLIC
            juce::juce_recommended_config_flags
    )

    target_compile_definitions(qc_file_tests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_USE_MP3AUDIOFORMAT=1
        JUCE_STANDALONE_APPLICATION=1
    )

    target_include_directories(qc_file_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/Source
    )

    add_test(NAME files COMMAND qc_file_tests)
endif()
