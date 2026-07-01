cmake_minimum_required(VERSION 3.20)
project(SQLLocalDBView VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # Runtime estatico
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

    add_compile_options(
        /W4
        /WX-
        /MP
        /Zi
        /Gy
        /GS
        /sdl
        /permissive-
        /utf-8
    )

    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/GL>)

    add_link_options($<$<CONFIG:Release>:/LTCG>)
    add_link_options(/DEBUG)
endif()

add_compile_definitions(
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

# Coletar arquivos fonte (ODBC/LocalDB - sem SQLite)
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

set(RESOURCES
    resources/SQLLocalDBView.rc
)

# DLL principal
add_library(SQLLocalDBView SHARED
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    SQLLocalDBView.def
)

target_include_directories(SQLLocalDBView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(SQLLocalDBView PRIVATE
    odbc32
    shell32
    ole32
    oleaut32
    uuid
    shlwapi
    gdi32
    user32
    advapi32
    comctl32
    propsys
    uxtheme
    dwmapi
)

set_target_properties(SQLLocalDBView PROPERTIES
    OUTPUT_NAME "SQLLocalDBView"
    PREFIX ""
    SUFFIX ".dll"
)

install(TARGETS SQLLocalDBView
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(FILES
    README.md
    DESTINATION .
)
