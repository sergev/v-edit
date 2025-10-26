cmake_minimum_required(VERSION 3.15)

project(
    v_edit
    VERSION 0.1.0
    DESCRIPTION "Minimal ncurses-based text editor"
    LANGUAGES CXX
)

# Use extraction-time timestamps for URL downloads when supported (CMP0135)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CE_SOURCES
    core.cpp
    display.cpp
    file.cpp
    input.cpp
    edit.cpp
    clipboard.cpp
    session.cpp
    filter.cpp
    files.cpp
    signal.cpp
    segment.cpp
    workspace.cpp
    macro.cpp
)

set(CE_MAIN
    main.cpp
)

# Create static library for v-edit without main
add_library(v_edit STATIC ${CE_SOURCES})
target_include_directories(v_edit PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    target_compile_options(v_edit PRIVATE /W4 /permissive-)
else()
    target_compile_options(v_edit PRIVATE -Wall -Werror -Wshadow)
endif()

find_package(Curses REQUIRED)

if(TARGET CURSES::CURSES)
    target_link_libraries(v_edit PRIVATE CURSES::CURSES)
else()
    target_include_directories(v_edit PRIVATE ${CURSES_INCLUDE_DIR})
    target_link_libraries(v_edit PRIVATE ${CURSES_LIBRARIES})
endif()

# Create executable
add_executable(ve ${CE_MAIN})
target_link_libraries(ve PRIVATE v_edit)

if(MSVC)
    target_compile_options(ve PRIVATE /W4 /permissive-)
else()
    target_compile_options(ve PRIVATE -Wall -Werror -Wshadow)
endif()

install(TARGETS ve RUNTIME DESTINATION bin ARCHIVE DESTINATION lib)

# --- Tests ---
add_subdirectory(tests)
