cmake_minimum_required(VERSION 3.20)
project(nachtzug VERSION 0.4.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Qt 6.4 is the oldest tried (Debian 12, Ubuntu 24.04); an older one stops here with its version, not mid-build
find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(SDL2 REQUIRED)

# every source file in src/ is built, so adding a file needs no change here
file(GLOB NACHTZUG_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
add_executable(nachtzug ${NACHTZUG_SOURCES} assets/resources.qrc)
if(MSVC)
  target_compile_options(nachtzug PRIVATE /W3 /utf-8)
else()
  target_compile_options(nachtzug PRIVATE -Wall -Wextra)
endif()
# the release builds stop on any warning (NACHTZUG_WERROR=1 in .github/workflows/release.yml), so a new one is
# noticed; building it yourself with a newer compiler that warns about something new still works
if(DEFINED ENV{NACHTZUG_WERROR} AND NOT MSVC)
  target_compile_options(nachtzug PRIVATE -Werror)
endif()
target_link_libraries(nachtzug PRIVATE Qt6::Widgets SDL2::SDL2)
target_compile_definitions(nachtzug PRIVATE NACHTZUG_VERSION="${PROJECT_VERSION}")

# MIDI keyboards, knobs and clock (src/midiio.cpp), on each system's own MIDI service
if(APPLE)
  target_link_libraries(nachtzug PRIVATE "-framework CoreMIDI" "-framework CoreFoundation")
elseif(WIN32)
  target_link_libraries(nachtzug PRIVATE winmm)
else()
  find_package(ALSA REQUIRED)
  find_package(Threads REQUIRED)
  target_link_libraries(nachtzug PRIVATE ALSA::ALSA Threads::Threads)
endif()

# translations: i18n/nachtzug_de.ts is compiled into the program as :/i18n/nachtzug_de.qm
# (run "cmake --build build --target update_translations" after changing texts in the code)
# the sample library, made by tools/samplegen: built into the program as :/samples/<Category>/<Name>.wav
file(GLOB_RECURSE NACHTZUG_SAMPLES CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/assets/samples assets/samples/*.wav)
list(TRANSFORM NACHTZUG_SAMPLES PREPEND assets/samples/)
if(NOT NACHTZUG_SAMPLES)
  message(FATAL_ERROR "No samples in assets/samples/: this copy of the source is incomplete. The .wav files there "
    "are part of the source code, and Nachtzug can't be built without them.")
endif()
# an ordinary resource, not BIG_RESOURCES: that one has rcc look for a marker in a compiled object file, which isn't
# there when a packager builds with link-time optimisation (-flto, the default for openSUSE and Fedora packages), so
# the build stopped with "No data signature found"; at about 10 MB the samples don't need it
qt_add_resources(nachtzug "sample_library" PREFIX "/samples" BASE assets/samples FILES ${NACHTZUG_SAMPLES})

# the starter songs of the start window (src/starters.h), CC0: built in as :/starters/<id>.nachtzug
file(GLOB NACHTZUG_STARTERS CONFIGURE_DEPENDS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/assets/starters assets/starters/*.nachtzug)
list(TRANSFORM NACHTZUG_STARTERS PREPEND assets/starters/)
if(NACHTZUG_STARTERS)
  qt_add_resources(nachtzug "starter_songs" PREFIX "/starters" BASE assets/starters FILES ${NACHTZUG_STARTERS})
endif()

qt_add_translations(nachtzug TS_FILES i18n/nachtzug_de.ts RESOURCE_PREFIX /i18n LUPDATE_OPTIONS -no-obsolete -locations none)

if(APPLE)
  # a normal Mac program: Nachtzug.app with its icon, opening .nachtzug songs from the Finder
  # (tools/build-macos.sh builds it and packs Qt and SDL into it, for a .dmg that runs on other Macs)
  set_source_files_properties(assets/nachtzug.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  target_sources(nachtzug PRIVATE assets/nachtzug.icns)
  set_target_properties(nachtzug PROPERTIES
    MACOSX_BUNDLE ON
    OUTPUT_NAME Nachtzug
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/assets/Info.plist.in)
  install(TARGETS nachtzug BUNDLE DESTINATION .)
elseif(WIN32)
  # a window program (no console), Nachtzug.exe with its icon; tools/build-windows.sh packs it with Qt and SDL
  target_sources(nachtzug PRIVATE assets/windows/nachtzug.rc)
  set_target_properties(nachtzug PROPERTIES WIN32_EXECUTABLE ON OUTPUT_NAME Nachtzug)
  install(TARGETS nachtzug RUNTIME DESTINATION .)
else()
  install(TARGETS nachtzug RUNTIME DESTINATION bin)
  install(FILES assets/nachtzug.desktop DESTINATION share/applications)
  # the icon at every size the desktops ask for (tools/logo/make_icons.sh makes them), and as an SVG
  foreach(size 16 24 32 48 64 128 256)
    install(FILES assets/icons/nachtzug-${size}.png DESTINATION share/icons/hicolor/${size}x${size}/apps
            RENAME nachtzug.png)
  endforeach()
  install(FILES assets/logo.png DESTINATION share/icons/hicolor/512x512/apps RENAME nachtzug.png)
  install(FILES assets/nachtzug.svg DESTINATION share/icons/hicolor/scalable/apps)
  # what software centres show about Nachtzug, and the .nachtzug song file, so it opens with a double-click
  install(FILES assets/io.github.mjohanning99.nachtzug.metainfo.xml DESTINATION share/metainfo)
  install(FILES assets/nachtzug-mime.xml DESTINATION share/mime/packages RENAME nachtzug.xml)
  # the licences where Fedora, openSUSE and Arch look for them (LICENSING.md links to the others by their paths);
  # the list of the AppImage's libraries with the documentation
  install(FILES LICENSE LICENSING.md DESTINATION share/licenses/nachtzug)
  install(DIRECTORY LICENSES DESTINATION share/licenses/nachtzug)
  install(FILES docs/THIRD-PARTY.md DESTINATION share/doc/nachtzug)
endif()
