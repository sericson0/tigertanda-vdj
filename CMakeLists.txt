cmake_minimum_required(VERSION 3.22)
project(TigerTanda VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ══════════════════════════════════════════════════════════════════════════════
#  TigerTanda VDJ Native Plugin (Windows DLL — no JUCE dependency)
# ══════════════════════════════════════════════════════════════════════════════
if(NOT WIN32)
    message(FATAL_ERROR "TigerTanda is a Windows-only VDJ plugin.")
endif()

add_library(TigerTanda MODULE
    Source/TigerTanda.cpp
    Source/TigerTandaHelpers.cpp
    Source/TigerTandaUI.cpp
    Source/TigerTandaMatching.cpp
    Source/TangoMatcher.cpp
    Source/TigerTanda.def
    Source/TigerTanda.rc
)

set_target_properties(TigerTanda PROPERTIES
    OUTPUT_NAME "TigerTanda"
    SUFFIX      ".dll"
)

target_compile_definitions(TigerTanda PRIVATE
    WIN32_LEAN_AND_MEAN
    _CRT_SECURE_NO_WARNINGS
    UNICODE
    _UNICODE
)

target_compile_features(TigerTanda PRIVATE cxx_std_17)

# Force-include shlobj.h before all sources so GDI+ COM types resolve
# under WIN32_LEAN_AND_MEAN
target_compile_options(TigerTanda PRIVATE /FIshlobj.h)

target_include_directories(TigerTanda PRIVATE
    "${CMAKE_SOURCE_DIR}/Source"
)

# Links: comctl32, gdi32, gdiplus, shell32, ole32, uxtheme provided via
# #pragma comment in TigerTandaHelpers.cpp; gdi32 added explicitly here.
target_link_libraries(TigerTanda PRIVATE
    gdi32
    comctl32
    gdiplus
    shell32
    ole32
    uxtheme
)

# Copy metadata.csv next to the VDJ plugin DLL
add_custom_command(TARGET TigerTanda POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/metadata.csv"
        "$<TARGET_FILE_DIR:TigerTanda>/metadata.csv"
    COMMENT "Copying metadata.csv to VDJ plugin output directory"
)

# Copy logo PNG next to the VDJ plugin DLL
add_custom_command(TARGET TigerTanda POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/TigerTandaLogoV2.png"
        "$<TARGET_FILE_DIR:TigerTanda>/TigerTandaLogoV2.png"
    COMMENT "Copying TigerTandaLogoV2.png to VDJ plugin output directory"
)
