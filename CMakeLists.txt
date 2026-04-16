cmake_minimum_required(VERSION 3.22)
project(TigerTanda VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ══════════════════════════════════════════════════════════════════════════════
#  TigerTanda VDJ Native Plugin — cross-platform (Windows DLL / macOS bundle)
# ══════════════════════════════════════════════════════════════════════════════

# Shared sources (compiled on both platforms)
set(SHARED_SOURCES
    Source/TangoMatcher.cpp
    Source/TigerTanda.cpp
    Source/TigerTandaMatching.cpp
    Source/TigerTandaHelpers.cpp
)

if(WIN32)
    # ── Windows build ────────────────────────────────────────────────────────
    add_library(TigerTanda MODULE
        ${SHARED_SOURCES}
        Source/TigerTandaUI.cpp
        Source/CoverArt.cpp
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

    target_link_libraries(TigerTanda PRIVATE
        gdi32
        comctl32
        gdiplus
        shell32
        ole32
        uxtheme
    )

elseif(APPLE)
    # ── macOS build ──────────────────────────────────────────────────────────
    enable_language(OBJCXX)

    add_library(TigerTanda MODULE
        ${SHARED_SOURCES}
        Source/TigerTandaUI_Mac.mm
        Source/TigerTandaHelpers_Mac.mm
        Source/CoverArt_Mac.mm
    )

    set_target_properties(TigerTanda PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "bundle"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Source/Info.plist"
        OUTPUT_NAME "TigerTanda"
    )

    # Objective-C++ settings
    set_source_files_properties(
        Source/TigerTandaUI_Mac.mm
        Source/TigerTandaHelpers_Mac.mm
        Source/CoverArt_Mac.mm
        PROPERTIES COMPILE_FLAGS "-fobjc-arc"
    )

    target_compile_features(TigerTanda PRIVATE cxx_std_17)

    target_link_libraries(TigerTanda PRIVATE
        "-framework Cocoa"
        "-framework CoreGraphics"
        "-framework CoreText"
        "-framework CoreFoundation"
    )

    # Copy logo into bundle Resources
    set_source_files_properties("${CMAKE_SOURCE_DIR}/TigerTandaLogoV2.png" PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources)
    target_sources(TigerTanda PRIVATE "${CMAKE_SOURCE_DIR}/TigerTandaLogoV2.png")
else()
    message(FATAL_ERROR "TigerTanda requires Windows or macOS.")
endif()

# Common settings
target_include_directories(TigerTanda PRIVATE
    "${CMAKE_SOURCE_DIR}/Source"
)

# Copy metadata.csv next to the VDJ plugin output
if(APPLE)
    # On Mac, TARGET_FILE_DIR points inside the .bundle — put csv next to it instead
    add_custom_command(TARGET TigerTanda POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/metadata.csv"
            "${CMAKE_BINARY_DIR}/metadata.csv"
        COMMENT "Copying metadata.csv to build directory"
    )
else()
    add_custom_command(TARGET TigerTanda POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/metadata.csv"
            "$<TARGET_FILE_DIR:TigerTanda>/metadata.csv"
        COMMENT "Copying metadata.csv to VDJ plugin output directory"
    )
endif()
