cmake_minimum_required(VERSION 3.22)

project(QCApp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(QC_BUILD_TESTS "Build the measurement engine test suite" ON)

# Off until Source/App lands, so that a clean clone configures and tests without
# needing a JUCE download.
option(QC_BUILD_APP "Build the JUCE desktop application (downloads JUCE on first configure)" OFF)

# ---------------------------------------------------------------------------
# qc_engine - measurement and verdict logic.
#
# Deliberately free of any JUCE dependency so the whole of the analysis path can
# be built and tested without a window, an audio device, or a network fetch.
# ---------------------------------------------------------------------------
add_library(qc_engine STATIC
    Source/Engine/KWeighting.cpp
    Source/Engine/LoudnessMeter.cpp
    Source/Engine/TruePeakDetector.cpp
    Source/Engine/QualityChecks.cpp
    Source/Engine/AudioAnalyser.cpp
    Source/Verdict/VerdictEngine.cpp
)

target_include_directories(qc_engine PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)

if(MSVC)
    target_compile_options(qc_engine PRIVATE /W4 /permissive-)
else()
    target_compile_options(qc_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(QC_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(QC_BUILD_APP)
    add_subdirectory(Source/App)
endif()
