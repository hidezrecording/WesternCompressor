# OSC-72 — JUCE plugin shell (VST3 / AU / Standalone)
#
# Working title "OSC-72": a one-knob stereo parallel FET limiter,
# a reconstruction of a documented 1971-72 Record Plant session trick
# (see ../README.md and ../dsp/Osc72Dsp.h). The entire sound lives in
# ../dsp/Osc72Dsp.{h,cpp} (framework-free). This directory is only the
# plugin wrapper: one parameter, DAW I/O, one-knob GUI.
#
# Build (macOS, Xcode):  cmake -B build -G Xcode ; cmake --build build --config Release
# Build (Linux):         cmake -B build && cmake --build build -j
#
# NOTE: JUCE 8 is AGPLv3-or-commercial. DSP core has no JUCE dependency.

cmake_minimum_required(VERSION 3.22)
project(OSC72 VERSION 1.1.7)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE 8.0.6: use $JUCE_DIR if set (CI), else the vendored copy.
if (DEFINED ENV{JUCE_DIR} AND NOT "$ENV{JUCE_DIR}" STREQUAL "")
    set(JUCE_DIR "$ENV{JUCE_DIR}")
else ()
    set(JUCE_DIR "$ENV{HOME}/workspace/vendor/juce-8.0.6")
endif ()
if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE not found at ${JUCE_DIR}")
endif ()
add_subdirectory("${JUCE_DIR}" juce EXCLUDE_FROM_ALL)

juce_add_plugin(OSC72
    COMPANY_NAME "Sabatino Audio"
    PLUGIN_NAME "Western Compressor"
    DESCRIPTION "One-knob stereo parallel FET limiter: a crushed limiter path blended under the dry signal, with a magic-eye gain-reduction display."
    PLUGIN_MANUFACTURER_CODE SabA
    PLUGIN_CODE OS72
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Western Compressor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Embedded assets: the faceplate is still drawn entirely in code,
# but v11 embeds ONE typeface so the panel lettering renders
# identically on every machine. v11/v12 used Oswald SemiBold;
# v13 replaced it with Limelight; v14 with Titan One; v15 with
# Michroma; v16 with Saira Stencil One; v18 replaced that
# with SPECIAL ELITE (Brian J. Bonislawsky DBA Astigmatic,
# Apache License 2.0, licence in Assets/) — Saira Stencil was
# "better but still a little too nice, especially the B", and
# Nathan pushed the lettering rougher and less refined:
# genuinely eroded, ink-starved utilitarian glyphs rather
# than a clean-cut stencil (see docs/build-report-v18.md).
# v19 replaces Special Elite with RUBIK DIRT (The Rubik
# Filtered Project Authors, SIL OFL 1.1, licence in
# Assets/): the erosion was "in the right direction" but
# the typewriter face itself wasn't loved — Rubik Dirt
# carries the same worn, eaten-away effect in a heavy
# stamped slab that reads industrial, not typed (see
# docs/build-report-v19.md).
# v20 replaces Rubik Dirt with SAIRA STENCIL ONE (The
# Saira Stencil Project Authors, SIL OFL 1.1, licence in
# Assets/) again — the face Nathan picked in v16 — this
# time carried with a LIGHT procedural worn treatment in
# the editor's text rendering: Rubik Dirt's baked-in
# erosion read "a little too blurry", and the brief is the
# same worn industrial style but SHARPER, THINNER-lined
# and brighter (see docs/build-report-v20.md).
# v21 replaces Saira Stencil with SMOKUM (Brian J.
# Bonislawsky DBA Astigmatic (AOETI), Apache License 2.0,
# licence in Assets/): a vintage WESTERN poster face in
# the "WESTERN GRANDE" specimen Nathan supplied — tall
# bracketed slab lettering, sturdy and plain at heart —
# after his v20 complaints that the lettering had gotten
# too big, too spaced out and overlapping (screws and
# labels included). So v21 also shrinks every panel string
# by about a third and tightens the tracking to near-
# natural, and the rendering treatment becomes a VERY
# SUBTLE spray-paint pass (faint overspray halo + speckle
# at the glyph edges) in place of the worn pinholes. The
# Oswald, Limelight, Titan One, Michroma, Special Elite,
# Rubik Dirt and Saira Stencil One files remain in
# Assets/ but are no longer embedded or referenced.
# v23 replaces Smokum with SANCREEK (The Sancreek Project
# Authors, SIL OFL 1.1, licence in Assets/): Nathan's v22
# verdict was that Smokum's lettering is "impossible to
# read" — its hairline strokes anti-alias into grey mush
# at panel sizes however clean the rendering is. Sancreek
# is the chunkier Western slab from the v21 candidate
# sheet, with roughly double Smokum's stroke weight;
# v23 also sets the lettering ~30% bigger and stretches
# it 1.5x (see docs/build-report-v23.md). The Smokum file
# remains in Assets/ but is no longer embedded.
# v24 replaces Sancreek with SMOKUM EMBOLDENED — a
# modified derivative of Smokum (Brian J. Bonislawsky DBA
# Astigmatic (AOETI), Apache License 2.0, licence in
# Assets/): Nathan rejected Sancreek, and from a specimen
# sheet of four bold faces he picked Smokum with its
# outlines expanded by a FreeType stroker at 3.5% of em,
# baked into a derived font file (see
# Assets/NOTE-SmokumEmboldened.txt). The Sancreek and
# plain Smokum files remain in Assets/ but are no longer
# embedded.
# v25 adds SMOKUM EMBOLDENED THIN — the same bake at a
# reduced stroker radius (48 units, 2.34% of em; see
# Assets/NOTE-SmokumEmboldenedThin.txt): Nathan asked for
# the panel lettering a little bigger and a little thinner.
# The v24 bake stays embedded alongside it — the flange
# numerals keep the v24 weight.
# v26 replaces both with SMOKUM EMBOLDENED XTHIN — the
# same bake at a further reduced stroker radius (29
# units, 1.42% of em; see
# Assets/NOTE-SmokumEmboldenedXThin.txt): Nathan found
# the v25 line still a little too thick ("all fonts…
# they're all a little bit too thick now") and cleared a
# thinner bake than the cautious band ("it can still
# thin out a little bit and be okay"). The XThin bake
# serves panel text AND the flange numerals. The v24
# and v25 bake files remain in Assets/ but are no longer
# embedded.
# (The v4 script-wordmark typeface was removed in v5; this is a
# different, single-purpose embed: panel text.)
juce_add_binary_data(Osc72Assets
    HEADER_NAME BinaryData.h
    NAMESPACE BinaryData
    SOURCES
        Assets/SmokumEmboldenedXThin-Regular.ttf
)

target_sources(OSC72 PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp
    ../dsp/Osc72Dsp.cpp
)

target_include_directories(OSC72 PRIVATE
    ../dsp
)

target_link_libraries(OSC72 PRIVATE
    Osc72Assets
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_dsp
)

target_compile_definitions(OSC72 PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

# ─────────────────────────────────────────────────────────────────────
# Headless GUI snapshot renderer (dev verification only, not shipped).
# Produces build/OscSnapshot; run under xvfb:
#   ./build/OscSnapshot ../docs/gui-v1.png
# ─────────────────────────────────────────────────────────────────────
add_executable(OscSnapshot
    ../tests/snapshot.cpp
    PluginProcessor.cpp
    PluginEditor.cpp
    ../dsp/Osc72Dsp.cpp
)
target_include_directories(OscSnapshot PRIVATE ../dsp ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(OscSnapshot PRIVATE
    Osc72Assets
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
)
target_compile_definitions(OscSnapshot PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STANDALONE_APPLICATION=1
)

# ─────────────────────────────────────────────────────────────────────
# Processor-level tests (dev verification only, not shipped).
# Drives Osc72Processor::processBlock (the full APVTS -> processor
# -> Chain path) — added in the v3 diagnosis round, when the DSP
# suite alone could not see whether the shipped wrapper delivers
# the MOD warble. Produces build/OscProcessorTest; run headless:
#   ./build/OscProcessorTest
# ─────────────────────────────────────────────────────────────────────
add_executable(OscProcessorTest
    ../tests/test_processor.cpp
    PluginProcessor.cpp
    PluginEditor.cpp
    ../dsp/Osc72Dsp.cpp
)
target_include_directories(OscProcessorTest PRIVATE ../dsp ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(OscProcessorTest PRIVATE
    Osc72Assets
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
)
target_compile_definitions(OscProcessorTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_STANDALONE_APPLICATION=1
)
