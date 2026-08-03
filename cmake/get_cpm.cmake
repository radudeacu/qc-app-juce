# Downloads CPM (CMake Package Manager) on first configure and makes CPMAddPackage
# available. Pinned so that a build is reproducible from a clean clone.

set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
        ${CPM_DOWNLOAD_LOCATION}
        STATUS download_status
    )

    list(GET download_status 0 download_result)
    if(NOT download_result EQUAL 0)
        list(GET download_status 1 download_message)
        file(REMOVE ${CPM_DOWNLOAD_LOCATION})
        message(FATAL_ERROR "Could not download CPM.cmake: ${download_message}")
    endif()
endif()

include(${CPM_DOWNLOAD_LOCATION})
