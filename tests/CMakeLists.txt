# Tests configuration

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(BUILD_GMOCK OFF CACHE BOOL "Disable building GoogleMock" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "Do not install GoogleTest" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

find_package(Threads REQUIRED)

# Define test sources
set(TEST_SOURCES
    smoke_tmux_test.cpp
    exit_without_save_test.cpp
    save_and_exit_test.cpp
    scroll_vertical_test.cpp
    session_restore_test.cpp
    search_test.cpp
    filter_test.cpp
    multiple_files_test.cpp
    alternative_workspace_test.cpp
    area_selection_test.cpp
    rectangular_block_test.cpp
    help_file_test.cpp
    signal_handling_test.cpp
    segment_test.cpp
    segment_unit_test.cpp
    command_mode_test.cpp
    TmuxDriver.cpp
)

add_executable(v_edit_tests
    ${TEST_SOURCES}
)

target_include_directories(v_edit_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR})
target_compile_features(v_edit_tests PRIVATE cxx_std_17)
target_link_libraries(v_edit_tests PRIVATE GTest::gtest_main Threads::Threads v_edit)

find_package(Curses REQUIRED)
if(TARGET CURSES::CURSES)
    target_link_libraries(v_edit_tests PRIVATE CURSES::CURSES)
else()
    target_include_directories(v_edit_tests PRIVATE ${CURSES_INCLUDE_DIR})
    target_link_libraries(v_edit_tests PRIVATE ${CURSES_LIBRARIES})
endif()

add_dependencies(v_edit_tests v_edit ve)
target_compile_definitions(v_edit_tests PRIVATE V_EDIT_BIN_PATH="$<TARGET_FILE:ve>")

include(GoogleTest)
gtest_discover_tests(v_edit_tests EXTRA_ARGS --gtest_repeat=1 PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
