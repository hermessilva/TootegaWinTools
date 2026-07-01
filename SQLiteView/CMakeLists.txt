cmake_minimum_required(VERSION 3.20)
project(SQLiteView VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Configurações de compilação
if(MSVC)
    # Usar runtime estático
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    
    # Flags de compilação
    add_compile_options(
        /W4             # Warning level 4
        /WX-            # Warnings não são erros
        /MP             # Multi-processor compilation
        /Zi             # Debug info
        /Gy             # Function-level linking
        /GS             # Buffer security check
        /sdl            # Security Development Lifecycle checks
        /permissive-    # Strict conformance
        /utf-8          # UTF-8 source and execution
    )
    
    # Flags de release
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/GL>)  # Whole program optimization
    
    # Linker flags
    add_link_options($<$<CONFIG:Release>:/LTCG>)   # Link-time code generation
    add_link_options(/DEBUG)                        # Always generate PDB
endif()

# Definições
add_compile_definitions(
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    SQLITE_THREADSAFE=1
    SQLITE_ENABLE_COLUMN_METADATA
    SQLITE_ENABLE_FTS5
    SQLITE_OMIT_LOAD_EXTENSION
)

# SQLite como biblioteca estática
add_library(sqlite3 STATIC
    sqlite/sqlite3.c
    sqlite/sqlite3.h
)
target_include_directories(sqlite3 PUBLIC sqlite)

# Coletar arquivos fonte
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

# Recursos
set(RESOURCES
    resources/SQLiteView.rc
)

# DLL principal
add_library(SQLiteView SHARED
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    SQLiteView.def
)

target_include_directories(SQLiteView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite
)

target_link_libraries(SQLiteView PRIVATE
    sqlite3
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

# Propriedades da DLL
set_target_properties(SQLiteView PROPERTIES
    OUTPUT_NAME "SQLiteView"
    PREFIX ""
    SUFFIX ".dll"
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
)

# Instalação
install(TARGETS SQLiteView
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)

install(FILES
    Install.ps1
    Uninstall.ps1
    README.md
    DESTINATION .
)
