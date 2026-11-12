cmake_minimum_required(VERSION 3.22)
project(CERN VERSION 1.0.0 LANGUAGES C CXX)

# JUCE Komponenten konfigurieren – plattformabhängig
set(JUCE_BUILD_GUI_EXTRA OFF CACHE STRING "" FORCE)
set(JUCE_USE_CURL OFF CACHE STRING "" FORCE)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # GTK/X11/ALSA nur auf Linux erzwingen (für nativen GUI/Audio-Backend)
    set(JUCE_USE_GTK ON CACHE STRING "" FORCE)
    set(JUCE_USE_X11 ON CACHE STRING "" FORCE)
    set(JUCE_USE_ALSA ON CACHE STRING "" FORCE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # macOS nutzt CoreAudio/CoreMIDI, kein X11/GTK
    set(JUCE_USE_GTK OFF CACHE STRING "" FORCE)
    set(JUCE_USE_X11 OFF CACHE STRING "" FORCE)
    set(JUCE_USE_ALSA OFF CACHE STRING "" FORCE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Windows nutzt WASAPI, kein X11/GTK
    set(JUCE_USE_GTK OFF CACHE STRING "" FORCE)
    set(JUCE_USE_X11 OFF CACHE STRING "" FORCE)
    set(JUCE_USE_ALSA OFF CACHE STRING "" FORCE)
else()
    # Unbekannte Plattform: konservativ ohne X11/GTK/ALSA
    set(JUCE_USE_GTK OFF CACHE STRING "" FORCE)
    set(JUCE_USE_X11 OFF CACHE STRING "" FORCE)
    set(JUCE_USE_ALSA OFF CACHE STRING "" FORCE)
endif()

# WebKit/CURL auf allen Plattformen standardmäßig deaktivieren (siehe README)
add_compile_definitions(
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)

# --- Policies ---
cmake_policy(SET CMP0077 NEW)  # option() honors normal variables
cmake_policy(SET CMP0091 NEW)  # target_sources() converts relative paths

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Tooling ---
# compile_commands.json im Build-Verzeichnis fuer clangd/clang-tidy & Co.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Build-Flags fuer Projekt-Targets (Warnungen/Sanitizer) ---
# Optionen muessen im Top-Level stehen, damit sie im Cache landen.
option(CERN_SANITIZE "Enable ASan/UBSan (Debug)" OFF)
option(CERN_WERROR "Warnungen der Projekt-Targets als Fehler behandeln" OFF)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CERNTarget.cmake)

# --- JUCE Setup ---
option(JUCE_FETCH_FROM_GIT "Fetch JUCE from GitHub" ON)
option(JUCE_BRANCH "JUCE branch/tag" "develop")

if(JUCE_FETCH_FROM_GIT)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${JUCE_BRANCH}
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)
else()
    if(NOT DEFINED ENV{JUCE_DIR})
        message(FATAL_ERROR "JUCE_DIR not set and JUCE_FETCH_FROM_GIT=OFF")
    endif()
    set(JUCE_PATH $ENV{JUCE_DIR})
    add_subdirectory(${JUCE_PATH})
endif()

# --- Plugin Configuration ---
set(PLUGIN_NAME "C.E.R.N")
set(PLUGIN_MANUFACTURER "ScalyxSoft")
set(PLUGIN_BUNDLE_ID "com.scalyxsoft.cern")
set(PLUGIN_MANUFACTURER_CODE "ScKx")
# VST3/Plugin-Identitaet: 4 Zeichen, unabhaengig vom Produktnamen. Der Code
# "Crn1" ist die Plugin-ID im Host (bestehende Host-Projekte wuerden das Plugin
# bei einer Aenderung nicht mehr finden). Deshalb nur bewusst aendern.
set(PLUGIN_PLUGIN_CODE "Crn1")
set(PLUGIN_VERSION "1.0.0")
set(PLUGIN_DESCRIPTION "Physical Modeling Techno Kick Drum VST3")
set(PLUGIN_URL "https://scalyxsoft.com")
set(PLUGIN_EMAIL "support@scalyxsoft.com")

# VST3 only
set(JUCE_PLUGIN_FORMATS "VST3")

# --- Platform Specifics ---
if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    add_compile_options(/MP /permissive- /Zc:__cplusplus /Zc:inline /Zc:throwingNew)
    add_link_options(/LARGEADDRESSAWARE)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0")
endif()

# --- Testing ---
# Muss vor add_subdirectory(Source) aufgerufen werden, damit Source/CMakeLists.txt
# add_test() fuer CERNTests registrieren darf.
enable_testing()

# --- Include Project Sources ---
add_subdirectory(Source)

add_subdirectory(Tests)

include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME "${PLUGIN_NAME}")
set(CPACK_PACKAGE_VERSION "${PLUGIN_VERSION}")
set(CPACK_PACKAGE_VENDOR "${PLUGIN_MANUFACTURER}")
set(CPACK_GENERATOR "TGZ;ZIP")
include(CPack)
