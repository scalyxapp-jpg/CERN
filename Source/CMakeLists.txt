# --- CERN Source Files ---
set(CERN_COMMON_SOURCES
    PluginProcessor.cpp
    PluginEditor.cpp
    PluginEntry.cpp

    Parameters/ParameterLayout.cpp

    DSP/KickSynthesisEngine.cpp
    DSP/PatchGraph.cpp
    DSP/ParameterPatchGraph.cpp
    DSP/MixerStage.cpp
    DSP/LayeringStage.cpp

    Sequencer/StepSequencer.cpp

    Presets/PresetManager.cpp

    GUI/MainComponent.cpp
    GUI/SectionPanel.cpp
    GUI/ParamKnob.cpp
    GUI/CERNFader.cpp
    GUI/LogoComponent.cpp
    GUI/Theme/CERNLookAndFeel.cpp
    GUI/PowerSwitch.cpp
    GUI/WaveformDisplay.cpp
    GUI/SpectrumAnalyzer.cpp
    GUI/EnvelopeEditor.cpp
    GUI/HiggsField3D.cpp
    GUI/PatchView.cpp
    GUI/ParameterPatchView.cpp
    GUI/MixerView.cpp
)

# --- UI-Assets (Logo, Metall-/Kappen-Texturen) als BinaryData ---
juce_add_binary_data(CERNAssets
    SOURCES
        ../Assets/logo-emblem.png
        ../Assets/knob-cap.png
        ../Assets/metal-brushed.png
        ../Assets/fader-track.png
        ../Assets/grain.png
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
)

# --- Plugin Target (VST3) ---
juce_add_plugin(CERN
    PLUGIN_NAME "${PLUGIN_NAME}"
    COMPANY_NAME "${PLUGIN_MANUFACTURER}"
    BUNDLE_ID "${PLUGIN_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE "${PLUGIN_MANUFACTURER_CODE}"
    PLUGIN_CODE "${PLUGIN_PLUGIN_CODE}"
    PLUGIN_VERSION "${PLUGIN_VERSION}"
    DESCRIPTION "${PLUGIN_DESCRIPTION}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS VST3
)

target_sources(CERN PRIVATE
    ${CERN_COMMON_SOURCES}
)

target_include_directories(CERN PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Theme
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets
    ${CMAKE_CURRENT_SOURCE_DIR}/Sequencer
)

target_compile_definitions(CERN PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DONT_AUTO_INSTALL_OPENGL_PROCS=1
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
    CERN_VERSION="${PLUGIN_VERSION}"
)

target_compile_features(CERN PRIVATE cxx_std_20)

target_link_libraries(CERN PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    CERNAssets
)

# Include JuceUsings.h in every translation unit for brevity
set_source_files_properties(${CERN_COMMON_SOURCES} PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/JuceUsings.h")

# --- Standalone Target ---
# ASIO-Unterstützung (Windows). JUCE bringt die Steinberg-ASIO-Header bereits mit;
# ein externer SDK ist nicht nötig. Bei Aktivierung gelten die ASIO-Lizenzbedingungen
# von Steinberg. Aktivierung z. B. mit: cmake -B build -DCERN_WITH_ASIO=ON
option (CERN_WITH_ASIO "ASIO-Unterstuetzung aktivieren (Windows; JUCE-gebundene Steinberg-Header)" OFF)

# JACK-Unterstützung (Linux). JUCE lädt JACK zur Laufzeit weak-linked; es wird
# keine JACK-dev-Library benötigt. Aktivierung: cmake -B build -DCERN_WITH_JACK=ON
option (CERN_WITH_JACK "JACK-Unterstuetzung aktivieren (Linux; weak-linked)" OFF)

juce_add_gui_app(CERNStandalone
    PRODUCT_NAME "${PLUGIN_NAME} Standalone"
    COMPANY_NAME "${PLUGIN_MANUFACTURER}"
    BUNDLE_ID "${PLUGIN_BUNDLE_ID}.standalone"
)

set(CERN_STANDALONE_SOURCES ${CERN_COMMON_SOURCES})
list(REMOVE_ITEM CERN_STANDALONE_SOURCES PluginEntry.cpp)

target_sources(CERNStandalone PRIVATE
    Standalone/StandaloneApp.cpp
    ${CERN_STANDALONE_SOURCES}
)

juce_generate_juce_header(CERNStandalone)

target_include_directories(CERNStandalone PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Theme
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets
    ${CMAKE_CURRENT_SOURCE_DIR}/Sequencer
    ${CMAKE_CURRENT_SOURCE_DIR}/Standalone
)

target_link_libraries(CERNStandalone PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_opengl
    CERNAssets
)

target_compile_definitions(CERNStandalone PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DONT_AUTO_INSTALL_OPENGL_PROCS=1
    CERN_VERSION="${PLUGIN_VERSION}"
)

set_source_files_properties(${CERN_STANDALONE_SOURCES}
    Standalone/StandaloneApp.cpp PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/JuceUsings.h")

# ASIO/JACK nur für den Standalone-Build aktivieren (Plug-ins brauchen kein Geräte-API)
if (CERN_WITH_ASIO)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        target_compile_definitions (CERNStandalone PRIVATE JUCE_ASIO=1)
        message(STATUS "CERN: ASIO-Unterstützung für Standalone aktiviert (Windows)")
    else()
        message(WARNING "CERN_WITH_ASIO ist nur unter Windows relevant und wird auf dieser Plattform ignoriert")
    endif()
endif()

if (CERN_WITH_JACK)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_definitions (CERNStandalone PRIVATE JUCE_JACK=1)
        message(STATUS "CERN: JACK-Unterstützung für Standalone aktiviert (Linux)")
    else()
        message(STATUS "CERN_WITH_JACK ist auf dieser Plattform nicht aktiv (nur Linux)")
    endif()
endif()

# macOS: Standalone als App-Bundle korrekt kennzeichnen (JUCE setzt i.d.R. MACOSX_BUNDLE,
# hier explizit für Klarheit bei Packaging/Install)
if (APPLE)
    set_target_properties(CERNStandalone PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "${PLUGIN_BUNDLE_ID}.standalone"
        MACOSX_BUNDLE_BUNDLE_NAME "${PLUGIN_NAME} Standalone"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PLUGIN_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PLUGIN_VERSION}"
    )
endif()

target_compile_features(CERNStandalone PRIVATE cxx_std_20)

# --- Headless-GUI-Vorschau (visuelle Pruefung ohne X11/Audio) ---
juce_add_console_app(CERNPreview PRODUCT_NAME "CERN Preview")

set(CERN_PREVIEW_SOURCES ${CERN_COMMON_SOURCES})
list(REMOVE_ITEM CERN_PREVIEW_SOURCES PluginEntry.cpp)

target_sources(CERNPreview PRIVATE
    Preview/PreviewMain.cpp
    ${CERN_PREVIEW_SOURCES}
)

juce_generate_juce_header(CERNPreview)

target_include_directories(CERNPreview PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Theme
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets
    ${CMAKE_CURRENT_SOURCE_DIR}/Sequencer
)

target_link_libraries(CERNPreview PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_opengl
    CERNAssets
)

target_compile_definitions(CERNPreview PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DONT_AUTO_INSTALL_OPENGL_PROCS=1
    CERN_VERSION="${PLUGIN_VERSION}"
)

set_source_files_properties(${CERN_PREVIEW_SOURCES} Preview/PreviewMain.cpp PROPERTIES
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_SOURCE_DIR}/JuceUsings.h")

target_compile_features(CERNPreview PRIVATE cxx_std_20)
cern_apply_build_flags(CERNPreview)

# --- Tests Target ---
add_executable(CERNTests
    ../Tests/test_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters/ParameterLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/KickSynthesisEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/PatchGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/ParameterPatchGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/MixerStage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/LayeringStage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Sequencer/StepSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/HiggsField3D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Theme/CERNLookAndFeel.cpp
)

target_include_directories(CERNTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Theme
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets
    ${CMAKE_CURRENT_SOURCE_DIR}/Sequencer
)

target_link_libraries(CERNTests PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_opengl
    CERNAssets
)

target_compile_definitions(CERNTests PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DONT_AUTO_INSTALL_OPENGL_PROCS=1
    CERN_VERSION="${PLUGIN_VERSION}"
)

target_compile_features(CERNTests PRIVATE cxx_std_20)

# --- Lab Target: Offline-Render + Kennzahlen (A/B hoerbar) ---
add_executable(CERNLab
    Lab/LabRenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters/ParameterLayout.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/KickSynthesisEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/PatchGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/ParameterPatchGraph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/MixerStage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP/LayeringStage.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Sequencer/StepSequencer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/HiggsField3D.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Theme/CERNLookAndFeel.cpp
)

target_include_directories(CERNLab PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/DSP
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI
    ${CMAKE_CURRENT_SOURCE_DIR}/GUI/Theme
    ${CMAKE_CURRENT_SOURCE_DIR}/Parameters
    ${CMAKE_CURRENT_SOURCE_DIR}/Presets
    ${CMAKE_CURRENT_SOURCE_DIR}/Sequencer
)

target_link_libraries(CERNLab PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_dsp
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_opengl
    CERNAssets
)

target_compile_definitions(CERNLab PRIVATE
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    JUCE_DONT_AUTO_INSTALL_OPENGL_PROCS=1
    CERN_VERSION="${PLUGIN_VERSION}"
)

target_compile_features(CERNLab PRIVATE cxx_std_20)

# --- Build-Flags der Projekt-Targets (Warnungen, optional Sanitizer) ---
# Nur eigene Targets, nie JUCE. Definition in cmake/CERNTarget.cmake.
cern_apply_build_flags(CERN)
cern_apply_build_flags(CERNStandalone)
cern_apply_build_flags(CERNTests)

# --- CTest-Registrierung ---
add_test(NAME CERNTests COMMAND CERNTests)
set_tests_properties(CERNTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    LABELS "unit"
    TIMEOUT 60
)

# --- Installation (VST3 / Standalone) ---
# Standard-Zielpfade je Plattform. Kann via CMAKE_INSTALL_PREFIX oder
# komponentenbasiertem Install angepasst werden.
include(GNUInstallDirs)

if(WIN32)
    set(CERN_VST3_INSTALL_DIR "$ENV{COMMONPROGRAMFILES}/VST3" CACHE PATH "Installationspfad fuer VST3 (Windows)")
    set(CERN_STANDALONE_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}" CACHE PATH "Installationspfad fuer Standalone (Windows)")
elseif(APPLE)
    set(CERN_VST3_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3" CACHE PATH "Installationspfad fuer VST3 (macOS)")
    set(CERN_STANDALONE_INSTALL_DIR "/Applications" CACHE PATH "Installationspfad fuer Standalone (macOS)")
else() # Linux / Unix
    set(CERN_VST3_INSTALL_DIR "$ENV{HOME}/.vst3" CACHE PATH "Installationspfad fuer VST3 (Linux)")
    set(CERN_STANDALONE_INSTALL_DIR "${CMAKE_INSTALL_BINDIR}" CACHE PATH "Installationspfad fuer Standalone (Linux)")
endif()

# VST3-Bundle/Ordner installieren (JUCE-Artefaktpfad)
install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CERN_artefacts/$<CONFIG>/VST3/${PLUGIN_NAME}.vst3"
        DESTINATION "${CERN_VST3_INSTALL_DIR}"
        COMPONENT VST3
        OPTIONAL)

# Standalone installieren
if(WIN32)
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/CERNStandalone_artefacts/$<CONFIG>/Standalone/${PLUGIN_NAME} Standalone.exe"
            DESTINATION "${CERN_STANDALONE_INSTALL_DIR}"
            COMPONENT Standalone
            OPTIONAL)
elseif(APPLE)
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/CERNStandalone_artefacts/$<CONFIG>/Standalone/${PLUGIN_NAME} Standalone.app"
            DESTINATION "${CERN_STANDALONE_INSTALL_DIR}"
            COMPONENT Standalone
            OPTIONAL)
else()
    install(PROGRAMS "${CMAKE_CURRENT_BINARY_DIR}/CERNStandalone_artefacts/$<CONFIG>/Standalone/${PLUGIN_NAME} Standalone"
            DESTINATION "${CERN_STANDALONE_INSTALL_DIR}"
            COMPONENT Standalone
            OPTIONAL)
endif()
