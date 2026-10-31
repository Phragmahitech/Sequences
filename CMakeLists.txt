cmake_minimum_required(VERSION 3.22)

# --- Réglages ----------------------------------------------------------------
set(SEQUENCES_PRODUCT_NAME "Sequences")          # nom du plugin dans le DAW, et nom du fichier .vst3
set(SEQUENCES_COMPANY_NAME "Phragma")            # nom affiché comme éditeur dans le DAW
set(SEQUENCES_BUNDLE_ID    "com.phragma.sequences")
# -----------------------------------------------------------------------------
# Ne changer ni PLUGIN_MANUFACTURER_CODE ni PLUGIN_CODE : ce sont eux (et pas le nom)
# qui permettent aux DAW de retrouver le plugin dans les projets.

# macOS : binaire universel (Apple Silicon + Intel), compatible macOS 10.13+
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Version minimale de macOS")
if(NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Architectures macOS")
endif()

# Windows : runtime C++ lié en statique -> aucun "Visual C++ Redistributable" à installer
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(Sequences VERSION 0.3.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(SEQUENCES_BUILD_TESTS "Compiler les tests" OFF)

# --- JUCE (téléchargé automatiquement) ---------------------------------------
# Pour utiliser une copie locale : -DFETCHCONTENT_SOURCE_DIR_JUCE=/chemin/vers/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.15
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# --- Plugin ------------------------------------------------------------------
juce_add_plugin(Sequences
    PRODUCT_NAME                "${SEQUENCES_PRODUCT_NAME}"
    COMPANY_NAME                "${SEQUENCES_COMPANY_NAME}"
    BUNDLE_ID                   "${SEQUENCES_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE    Phrg
    PLUGIN_CODE                 Sqnc
    FORMATS                     VST3
    VST3_CATEGORIES             Instrument Synth
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE)

target_sources(Sequences PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

set(SEQUENCES_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    SEQUENCES_VERSION="${PROJECT_VERSION}")

target_compile_definitions(Sequences PUBLIC ${SEQUENCES_DEFINITIONS})

target_link_libraries(Sequences
    PRIVATE
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# --- Tests (optionnels) ------------------------------------------------------
if(SEQUENCES_BUILD_TESTS)
    enable_testing()

    # Moteur seul, sans JUCE
    add_executable(SequencesEngineTests Tests/EngineTests.cpp)
    target_include_directories(SequencesEngineTests PRIVATE Source)
    add_test(NAME engine COMMAND SequencesEngineTests)

    # Plugin complet (paramètres, MIDI, état, presets, interface)
    juce_add_console_app(SequencesPluginTests PRODUCT_NAME "SequencesPluginTests")
    target_sources(SequencesPluginTests PRIVATE
        Tests/PluginTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(SequencesPluginTests PRIVATE ${SEQUENCES_DEFINITIONS} JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SequencesPluginTests PRIVATE
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    add_test(NAME plugin COMMAND SequencesPluginTests)

    # Rendu d'extraits audio et d'une capture de l'interface
    juce_add_console_app(SequencesRender PRODUCT_NAME "SequencesRender")
    target_sources(SequencesRender PRIVATE
        Tools/Render.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(SequencesRender PRIVATE ${SEQUENCES_DEFINITIONS} JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SequencesRender PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
