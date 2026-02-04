cmake_minimum_required(VERSION 3.20)
project(SQLiteView VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 17)

# Build configuration
if(MSVC)
    # Static runtime linkage
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Compiler flags
    add_compile_options(
        /W4             # Warning level 4
        /WX-            # Warnings are not errors
        /MP             # Multi-processor compilation
        /Zi             # Debug information
        /Gy             # Function-level linking
        /GS             # Buffer security check
        /sdl            # Security Development Lifecycle checks
        /permissive-    # Strict conformance
        /utf-8          # UTF-8 source and execution
    )
    
    # Release optimizations
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/GL>)  # Whole program optimization
    
    # Linker flags
    add_link_options($<$<CONFIG:Release>:/LTCG>)   # Link-time code generation
    add_link_options(/DEBUG)                        # Always generate PDB
endif()

# Preprocessor definitions
add_compile_definitions(
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    # SQLite configuration
    SQLITE_THREADSAFE=1         # Serialized threading mode
    SQLITE_ENABLE_FTS5          # Full-text search
    SQLITE_ENABLE_JSON1         # JSON extension
    SQLITEVIEW_EXPORTS          # DLL exports
)

# SQLite3 Amalgamation (static) - embedded in project
set(SQLITE3_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/sqlite3")

set(SQLITE3_SOURCES
    ${SQLITE3_ROOT}/sqlite3.c
)

add_library(sqlite3 STATIC ${SQLITE3_SOURCES})
target_include_directories(sqlite3 PUBLIC ${SQLITE3_ROOT})

# Suppress warnings for sqlite3.c (third-party code)
if(MSVC)
    set_source_files_properties(${SQLITE3_SOURCES} PROPERTIES COMPILE_FLAGS "/w")
endif()

# Collect source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

# Resources
set(RESOURCES
    resources/SQLiteView.rc
)

# Main DLL
add_library(SQLiteView SHARED
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    SQLiteView.def
)

target_include_directories(SQLiteView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3
    ${CMAKE_CURRENT_SOURCE_DIR}/../../TootegaWinLib/Include
)

target_link_libraries(SQLiteView PRIVATE
    sqlite3
    shlwapi.lib
    ole32.lib
    oleaut32.lib
    uuid.lib
    shell32.lib
    propsys.lib
    gdi32.lib
    user32.lib
    advapi32.lib
    comctl32.lib
    uxtheme.lib
    dwmapi.lib
    comdlg32.lib
)

# TootegaWinLib
target_link_libraries(SQLiteView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../x64/$<CONFIG>/TootegaWinLib.lib
)

# Output directory
set_target_properties(SQLiteView PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../x64/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../x64/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../x64/$<CONFIG>"
    PDB_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../x64/$<CONFIG>"
)

# Post-build: Register DLL for debugging (optional)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET SQLiteView POST_BUILD
        COMMAND regsvr32 /s "$<TARGET_FILE:SQLiteView>"
        COMMENT "Registering SQLiteView.dll for debugging..."
        VERBATIM
    )
endif()
