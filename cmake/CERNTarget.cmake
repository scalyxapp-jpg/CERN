# Compiler-Flags der Projekt-Targets (CERN, CERNStandalone, CERNTests).
#
# Ziel: Warnungen und Sanitizer ausschliesslich fuer den eigenen Code aktivieren,
# niemals fuer JUCE. Wichtig: JUCE-Module werden als Interface-Sources im
# konsumierenden Target kompiliert, deshalb wuerden target_compile_options() auch
# die JUCE-Modul-.cpp-Dateien treffen (hunderte Fremd-Warnungen). Die Flags werden
# daher pro Quelldatei des Projekt-Targets gesetzt (SOURCE COMPILE_OPTIONS).

include_guard(GLOBAL)

# Setzt Warnungen (und optional Sanitizer) fuer ein Projekt-Target.
function(cern_apply_build_flags target)
    set(_flags "")

    if(MSVC)
        list(APPEND _flags /W4)
        if(CERN_WERROR)
            list(APPEND _flags /WX)
        endif()
        set(_dedupe_key "/W4")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        # -Wshadow und -Wconversion zeigen bestehende Auffaelligkeiten im
        # Projektcode (Konstruktorparameter wie "sampleRate", implizite
        # Konvertierungen). Sie bleiben bewusst aktiv und sind keine Fehler:
        # CERN_WERROR ist standardmaessig AUS.
        list(APPEND _flags
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wconversion
        )
        if(CERN_WERROR)
            list(APPEND _flags -Werror)
        endif()
        if(CERN_SANITIZE)
            list(APPEND _flags
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
            target_link_options(${target} PRIVATE
                -fsanitize=address,undefined
            )
        endif()
        set(_dedupe_key "-Wall")
    else()
        return()
    endif()

    # JUCE-Header als SYSTEM-Includes behandeln, damit unsere Warnflags nicht auf
    # Fremdcode anschlagen (z. B. -Wconversion in juce_UMPUtils.h). Die
    # Include-Verzeichnisse stammen aus den juce::*-Modulen.
    get_target_property(_links ${target} LINK_LIBRARIES)
    foreach(_lib IN LISTS _links)
        if(_lib MATCHES "^juce::" AND TARGET ${_lib})
            get_target_property(_incs ${_lib} INTERFACE_INCLUDE_DIRECTORIES)
            if(_incs)
                target_include_directories(${target} SYSTEM PRIVATE ${_incs})
            endif()
        endif()
    endforeach()

    # Nur die eigenen Quellen des Targets; get_target_property(SOURCES) liefert
    # keine JUCE-Interface-Sources.
    get_target_property(_sources ${target} SOURCES)
    if(NOT _sources)
        return()
    endif()

    foreach(_src IN LISTS _sources)
        get_source_file_property(_existing "${_src}" COMPILE_OPTIONS)
        if(NOT _existing)
            set(_existing "")
        endif()
        list(FIND _existing "${_dedupe_key}" _already)
        if(NOT _already EQUAL -1)
            continue()
        endif()
        set_source_files_properties("${_src}" PROPERTIES
            COMPILE_OPTIONS "${_existing};${_flags}")
    endforeach()
endfunction()
