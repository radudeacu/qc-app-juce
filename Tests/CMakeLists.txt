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
