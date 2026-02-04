cmake_minimum_required(VERSION 3.20)
project(SevenZipView VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

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
    # 7-Zip SDK configuration
    _7ZIP_ST                    # Single-threaded decoding (simpler, sufficient for shell extension)
    Z7_EXTRACT_ONLY             # Only extraction, no compression needed
)

# 7-Zip SDK C library (static) - embedded in project
set(SEVENZIPSDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/7zip-sdk")

set(SEVENZIP_C_SOURCES
    ${SEVENZIPSDK_ROOT}/7zAlloc.c
    ${SEVENZIPSDK_ROOT}/7zArcIn.c
    ${SEVENZIPSDK_ROOT}/7zBuf.c
    ${SEVENZIPSDK_ROOT}/7zBuf2.c
    ${SEVENZIPSDK_ROOT}/7zCrc.c
    ${SEVENZIPSDK_ROOT}/7zCrcOpt.c
    ${SEVENZIPSDK_ROOT}/7zDec.c
    ${SEVENZIPSDK_ROOT}/7zFile.c
    ${SEVENZIPSDK_ROOT}/7zStream.c
    ${SEVENZIPSDK_ROOT}/Bcj2.c
    ${SEVENZIPSDK_ROOT}/Bra.c
    ${SEVENZIPSDK_ROOT}/Bra86.c
    ${SEVENZIPSDK_ROOT}/BraIA64.c
    ${SEVENZIPSDK_ROOT}/CpuArch.c
    ${SEVENZIPSDK_ROOT}/Delta.c
    ${SEVENZIPSDK_ROOT}/Lzma2Dec.c
    ${SEVENZIPSDK_ROOT}/LzmaDec.c
    ${SEVENZIPSDK_ROOT}/Ppmd7.c
    ${SEVENZIPSDK_ROOT}/Ppmd7Dec.c
    ${SEVENZIPSDK_ROOT}/Sha256.c
    ${SEVENZIPSDK_ROOT}/Sha256Opt.c
    ${SEVENZIPSDK_ROOT}/Aes.c
    ${SEVENZIPSDK_ROOT}/AesOpt.c
)

add_library(7zsdk STATIC ${SEVENZIP_C_SOURCES})
target_include_directories(7zsdk PUBLIC ${SEVENZIPSDK_ROOT})

# Collect source files
file(GLOB_RECURSE SOURCES
    "src/*.cpp"
    "src/*.c"
)

file(GLOB_RECURSE HEADERS
    "include/*.h"
    "include/*.hpp"
)

# Resources
set(RESOURCES
    resources/SevenZipView.rc
)

# Main DLL
add_library(SevenZipView SHARED
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
    SevenZipView.def
)

target_include_directories(SevenZipView PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SEVENZIPSDK_ROOT}
)

target_link_libraries(SevenZipView PRIVATE
    7zsdk
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

# DLL properties - output to x64 folder at solution level
set_target_properties(SevenZipView PROPERTIES
    OUTPUT_NAME "SevenZipView"
    SUFFIX ".dll"
    PREFIX ""
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../x64/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../x64/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/../x64/$<CONFIG>"
)

# Copy PDB to output directory in Debug/RelWithDebInfo
if(MSVC)
    add_custom_command(TARGET SevenZipView POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_PDB_FILE:SevenZipView>
            $<TARGET_FILE_DIR:SevenZipView>
        COMMENT "Copying PDB file..."
        VERBATIM
    )
endif()

# Install rules
install(TARGETS SevenZipView
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
