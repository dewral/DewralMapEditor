cmake_minimum_required(VERSION 3.24)

# Dependency setup is handled explicitly by CMakePresets.json. Prevent Qt
# Creator from injecting a second, unrelated vcpkg toolchain into local kits.
set(QT_CREATOR_SKIP_VCPKG_SETUP ON CACHE BOOL
    "Disable Qt Creator package-manager auto setup" FORCE)

project(DewralMapEditor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Core Gui Qml Quick OpenGL REQUIRED)
qt_policy(SET QTP0001 NEW)

add_library(OTFormats STATIC
    libs/otformats/sprreader.cpp
    libs/otformats/sprreader.h
    libs/otformats/datreader.cpp
    libs/otformats/datreader.h
    libs/otformats/otbreader.cpp
    libs/otformats/otbreader.h
    libs/otformats/otbmreader.cpp
    libs/otformats/otbmreader.h
    libs/otformats/nodefilereader.cpp
    libs/otformats/nodefilereader.h
    libs/otformats/binaryreader.cpp
    libs/otformats/binaryreader.h
    libs/otformats/canonicalflags.h
    libs/otformats/itemtype.h
    libs/otformats/otfireader.cpp
    libs/otformats/otfireader.h
    libs/otformats/itemsxmlreader.cpp
    libs/otformats/itemsxmlreader.h
    libs/otformats/dmedatadir.h
)

target_compile_definitions(OTFormats PUBLIC
    $<$<CONFIG:Debug>:DME_DATA_DIR="${CMAKE_SOURCE_DIR}/data">)

target_link_libraries(OTFormats
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
)

target_include_directories(OTFormats PUBLIC libs/otformats)

add_executable(DewralMapEditor
    editor/main.cpp
    editor/core/backend.cpp
    editor/core/backend.h
    editor/core/brushstore.cpp
    editor/core/brushstore.h
    editor/core/tilesetstore.cpp
    editor/core/tilesetstore.h
    editor/core/palettefilter.cpp
    editor/core/palettefilter.h
    editor/core/filetools.cpp
    editor/core/filetools.h
    editor/core/uitheme.cpp
    editor/core/uitheme.h
    editor/core/documentmanager.cpp
    editor/core/documentmanager.h
    editor/core/creaturestore.cpp
    editor/core/creaturestore.h
    editor/map/mapview.cpp
    editor/map/mapview_edit.cpp
    editor/map/mapview_chunks.cpp
    editor/map/mapview_atlas.cpp
    editor/map/mapview_gl.cpp
    editor/map/mapview_input.cpp
    editor/map/mapview.h
    editor/map/mapview_p.h
    editor/map/mapservices.cpp
    editor/map/mapservices.h
    editor/map/mapglview.cpp
    editor/map/mapglview.h
    editor/map/minimapview.cpp
    editor/map/minimapview.h
    editor/resources.qrc
)

if(WIN32)
    enable_language(RC)
    target_sources(DewralMapEditor PRIVATE editor/windows/DME.rc)
endif()

qt_add_qml_module(DewralMapEditor
    URI Tibia
    VERSION 1.0
    SOURCES
        editor/core/backend.h
        editor/map/mapview.h
        editor/map/mapglview.h
        editor/map/minimapview.h
        editor/core/palettefilter.h
        editor/core/documentmanager.h
        editor/core/filetools.h
        editor/core/tilesetstore.h
        editor/core/brushstore.h
        editor/core/uitheme.h
        editor/core/creaturestore.h
        libs/otformats/sprreader.h
        libs/otformats/datreader.h
        libs/otformats/otbreader.h
        libs/otformats/otbmreader.h
        libs/otformats/otfireader.h
        libs/otformats/itemsxmlreader.h
)

target_include_directories(DewralMapEditor PRIVATE editor/core editor/map)

target_link_libraries(DewralMapEditor
    PRIVATE
        OTFormats
        Qt6::Quick
        Qt6::OpenGL
)

set(DME_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/dist" CACHE PATH
    "Directory used for the built application and deployed Qt runtime")

set_target_properties(DewralMapEditor PROPERTIES
    OUTPUT_NAME "DME"
    WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${DME_RUNTIME_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DME_RUNTIME_OUTPUT_DIRECTORY}"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DME_RUNTIME_OUTPUT_DIRECTORY}")

add_custom_command(TARGET DewralMapEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:DewralMapEditor>/data"
)

if(WIN32)
    get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${_qt_bin_dir}")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET DewralMapEditor POST_BUILD
            COMMAND "${CMAKE_COMMAND}"
                    "-DWINDEPLOYQT_EXECUTABLE=${WINDEPLOYQT_EXE}"
                    "-DQML_SOURCE_DIRECTORY=${CMAKE_SOURCE_DIR}/editor/qml"
                    "-DAPPLICATION_FILE=$<TARGET_FILE:DewralMapEditor>"
                    "-DAPPLICATION_DIRECTORY=$<TARGET_FILE_DIR:DewralMapEditor>"
                    -P "${CMAKE_SOURCE_DIR}/cmake/DeployQtRuntime.cmake"
            COMMENT "Deploying the minimal Qt runtime next to DME.exe"
            VERBATIM)
    else()
        message(WARNING "windeployqt was not found - the release executable may not start "
                        "outside Qt Creator (QML modules are missing). Run windeployqt manually.")
    endif()
endif()

if(WIN32)
    add_custom_target(package_release
        DEPENDS DewralMapEditor
        COMMAND "${CMAKE_COMMAND}"
                "-DDEPLOYED_DIRECTORY=$<TARGET_FILE_DIR:DewralMapEditor>"
                "-DRELEASE_ROOT=${CMAKE_SOURCE_DIR}/release"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DSTRIP_EXECUTABLE=${CMAKE_STRIP}"
                -P "${CMAKE_SOURCE_DIR}/cmake/CreateReleasePackage.cmake"
        COMMENT "Creating the production Windows release package"
        VERBATIM)

    add_custom_target(package_source
        COMMAND "${CMAKE_COMMAND}"
                "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}"
                "-DRELEASE_ROOT=${CMAKE_SOURCE_DIR}/release"
                -P "${CMAKE_SOURCE_DIR}/cmake/CreateSourcePackage.cmake"
        COMMENT "Creating the source release package"
        VERBATIM)

    add_custom_target(package_all
        DEPENDS package_release package_source)
endif()
