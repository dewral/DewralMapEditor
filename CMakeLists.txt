cmake_minimum_required(VERSION 3.24)

# Dependency setup is handled explicitly by CMakePresets.json. Prevent Qt
# Creator from injecting a second, unrelated vcpkg toolchain into local kits.
set(QT_CREATOR_SKIP_VCPKG_SETUP ON CACHE BOOL
    "Disable Qt Creator package-manager auto setup" FORCE)

project(DewralMapEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.7 COMPONENTS Core Gui Qml Quick ShaderTools Network Widgets Concurrent REQUIRED)
if(NOT TARGET Qt6::GuiPrivate)
    find_package(Qt6 REQUIRED COMPONENTS GuiPrivate)
endif()
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
    libs/otformats/compactvector.h
    libs/otformats/compactqstring.h
    libs/otformats/tilepositionindex.h
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
    editor/core/aimapassistant.cpp
    editor/core/aimapassistant.h
    editor/core/brushstore.cpp
    editor/core/brushstore.h
    editor/core/tilesetstore.cpp
    editor/core/tilesetstore.h
    editor/core/palettefilter.cpp
    editor/core/palettefilter.h
    editor/core/paletteimageprovider.cpp
    editor/core/paletteimageprovider.h
    editor/core/filetools.cpp
    editor/core/filetools.h
    editor/core/uitheme.cpp
    editor/core/uitheme.h
    editor/core/documentmanager.cpp
    editor/core/documentmanager.h
    editor/core/creaturestore.cpp
    editor/core/creaturestore.h
    editor/core/creaturefilter.cpp
    editor/core/creaturefilter.h
    editor/core/updateservice.cpp
    editor/core/updateservice.h
    editor/core/updateversion.cpp
    editor/core/updateversion.h
    editor/core/worktimerservice.cpp
    editor/core/worktimerservice.h
    editor/map/mapview.cpp
    editor/map/mapview_edit.cpp
    editor/map/mapview_brush_core.cpp
    editor/map/mapview_brush_ground.cpp
    editor/map/mapview_brush_types.cpp
    editor/map/mapview_item_properties.cpp
    editor/map/mapview_item_context.cpp
    editor/map/mapview_query.cpp
    editor/map/mapview_selection.cpp
    editor/map/mapview_chunks.cpp
    editor/map/mapview_atlas.cpp
    editor/map/mapview_render.cpp
    editor/map/mapview_input.cpp
    editor/map/mapview_path.cpp
    editor/map/mappathbuilder.cpp
    editor/map/mappathbuilder.h
    editor/map/mapview_terrain.cpp
    editor/map/mapview_clusters.cpp
    editor/map/mapgroundclustergenerator.cpp
    editor/map/mapgroundclustergenerator.h
    editor/map/mapterraingenerator.cpp
    editor/map/mapterraingenerator.h
    editor/map/mapterrainprofile.cpp
    editor/map/mapterrainprofile.h
    editor/map/mapview_dungeon.cpp
    editor/map/mapdungeongenerator.cpp
    editor/map/mapdungeongenerator.h
    editor/map/mapview.h
    editor/map/mapview_p.h
    editor/map/mapservices.cpp
    editor/map/mapservices.h
    editor/map/maptypes.h
    editor/map/mapchunkstore.h
    editor/map/mapatlasservice.h
    editor/map/mapeditcontroller.h
    editor/map/mapbrushcontroller.h
    editor/map/mapselectioncontroller.h
    editor/map/mapnavigationcontroller.h
    editor/map/mapitemcontroller.h
    editor/map/mapminimapservice.h
    editor/map/mapspawnindexservice.h
    editor/map/maprhibackend.cpp
    editor/map/maprhibackend.h
    editor/map/maprhiview.cpp
    editor/map/maprhiview.h
    editor/map/minimapview.cpp
    editor/map/minimapview.h
    editor/preview/ingamepreviewcontroller.cpp
    editor/preview/ingamepreviewcontroller.h
    editor/resources.qrc
)

# The sponsor logo is supplied separately. When present, embed it at the stable
# qrc URL used by AboutDialog.qml without making a missing optional image break
# local builds.
set(DME_SPONSOR_IMAGE "${CMAKE_SOURCE_DIR}/editor/ui/Midhem.png")
if(EXISTS "${DME_SPONSOR_IMAGE}")
    set_source_files_properties("${DME_SPONSOR_IMAGE}" PROPERTIES
        QT_RESOURCE_ALIAS "ui/Midhem.png")
    qt_add_resources(DewralMapEditor dme_sponsor_assets
        PREFIX "/"
        FILES "${DME_SPONSOR_IMAGE}")
endif()

if(WIN32)
    enable_language(RC)
    target_sources(DewralMapEditor PRIVATE editor/windows/DME.rc)
endif()

qt_add_qml_module(DewralMapEditor
    URI Tibia
    VERSION 1.0
    SOURCES
        editor/core/backend.h
        editor/core/aimapassistant.h
        editor/map/mapview.h
        editor/map/maprhiview.h
        editor/map/minimapview.h
        editor/preview/ingamepreviewcontroller.h
        editor/core/palettefilter.h
        editor/core/documentmanager.h
        editor/core/filetools.h
        editor/core/tilesetstore.h
        editor/core/brushstore.h
        editor/core/uitheme.h
        editor/core/creaturestore.h
        editor/core/creaturefilter.h
        editor/core/updateservice.h
        editor/core/worktimerservice.h
        libs/otformats/sprreader.h
        libs/otformats/datreader.h
        libs/otformats/otbreader.h
        libs/otformats/otbmreader.h
        libs/otformats/otfireader.h
        libs/otformats/itemsxmlreader.h
)

target_include_directories(DewralMapEditor PRIVATE editor/core editor/map editor/preview)

target_compile_definitions(DewralMapEditor PRIVATE
    DME_VERSION="${PROJECT_VERSION}")

target_link_libraries(DewralMapEditor
    PRIVATE
        OTFormats
        Qt6::Quick
        Qt6::GuiPrivate
        Qt6::Network
        Qt6::Concurrent
)

include(CTest)
if(BUILD_TESTING)
    add_executable(map_rhi_backend_test EXCLUDE_FROM_ALL
        tests/map_rhi_backend_test.cpp editor/map/maprhibackend.cpp)
    target_include_directories(map_rhi_backend_test PRIVATE editor/map)
    target_link_libraries(map_rhi_backend_test PRIVATE Qt6::GuiPrivate Qt6::Quick)
    qt_add_shaders(map_rhi_backend_test test_map_shaders
        PREFIX "/shaders" BASE "editor/map/shaders"
        GLSL "300es,330" HLSL 50 MSL 12
        FILES editor/map/shaders/map.vert editor/map/shaders/map.frag)
    add_test(NAME map_rhi_opengl COMMAND map_rhi_backend_test opengl)
    if(WIN32)
        add_test(NAME map_rhi_d3d11 COMMAND map_rhi_backend_test d3d11)
        set_tests_properties(map_rhi_opengl map_rhi_d3d11 PROPERTIES
            ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>"
            TIMEOUT 60)
    endif()
    add_executable(brushstore_advanced_test EXCLUDE_FROM_ALL
        tests/brushstore_advanced_test.cpp editor/core/brushstore.cpp)
    target_include_directories(brushstore_advanced_test PRIVATE editor/core libs/otformats)
    target_link_libraries(brushstore_advanced_test PRIVATE Qt6::Core)
    add_test(NAME brushstore_advanced COMMAND brushstore_advanced_test)
    find_program(DME_QML_TEST_RUNNER qmltestrunner HINTS "${Qt6_DIR}/../../../bin")
    if(DME_QML_TEST_RUNNER)
        add_test(NAME advanced_brush_qml COMMAND "${DME_QML_TEST_RUNNER}"
            -platform offscreen -input "${CMAKE_SOURCE_DIR}/tests/qml"
            -import "${CMAKE_SOURCE_DIR}/tests/qml/imports")
        set_tests_properties(advanced_brush_qml PROPERTIES
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            ENVIRONMENT "QT_QUICK_CONTROLS_STYLE=Basic"
            TIMEOUT 30)
    endif()
    add_executable(brushstore_border_variants_test EXCLUDE_FROM_ALL
        tests/brushstore_border_variants_test.cpp
        editor/core/brushstore.cpp)
    target_include_directories(brushstore_border_variants_test PRIVATE
        editor/core libs/otformats)
    target_link_libraries(brushstore_border_variants_test PRIVATE Qt6::Core)
    add_test(NAME brushstore_border_variants COMMAND brushstore_border_variants_test)

    add_executable(otbm_compact_storage_test EXCLUDE_FROM_ALL
        tests/otbm_compact_storage_test.cpp)
    target_include_directories(otbm_compact_storage_test PRIVATE libs/otformats)
    target_link_libraries(otbm_compact_storage_test PRIVATE OTFormats Qt6::Core)
    add_test(NAME otbm_compact_storage COMMAND otbm_compact_storage_test)
    add_executable(otbm_editing_storage_test EXCLUDE_FROM_ALL
        tests/otbm_editing_storage_test.cpp)
    target_include_directories(otbm_editing_storage_test PRIVATE libs/otformats)
    target_link_libraries(otbm_editing_storage_test PRIVATE OTFormats Qt6::Core)
    add_test(NAME otbm_editing_storage COMMAND otbm_editing_storage_test)
    add_executable(tile_position_index_test EXCLUDE_FROM_ALL
        tests/tile_position_index_test.cpp)
    target_include_directories(tile_position_index_test PRIVATE libs/otformats)
    add_test(NAME tile_position_index COMMAND tile_position_index_test)
    add_executable(map_selection_rotation_test EXCLUDE_FROM_ALL tests/map_selection_rotation_test.cpp)
    target_include_directories(map_selection_rotation_test PRIVATE editor/map)
    target_link_libraries(map_selection_rotation_test PRIVATE Qt6::Core)
    add_test(NAME map_selection_rotation COMMAND map_selection_rotation_test)

    add_executable(map_path_builder_test EXCLUDE_FROM_ALL
        tests/map_path_builder_test.cpp
        editor/map/mappathbuilder.cpp)
    target_include_directories(map_path_builder_test PRIVATE editor/map)
    target_link_libraries(map_path_builder_test PRIVATE Qt6::Core)
    add_test(NAME map_path_builder COMMAND map_path_builder_test)
    add_executable(map_ground_cluster_generator_test EXCLUDE_FROM_ALL
        tests/map_ground_cluster_generator_test.cpp
        editor/map/mapgroundclustergenerator.cpp)
    target_include_directories(map_ground_cluster_generator_test PRIVATE editor/map)
    target_link_libraries(map_ground_cluster_generator_test PRIVATE Qt6::Core)
    add_test(NAME map_ground_cluster_generator COMMAND map_ground_cluster_generator_test)
    add_executable(map_terrain_generator_test EXCLUDE_FROM_ALL
        tests/map_terrain_generator_test.cpp
        editor/map/mapterraingenerator.cpp)
    target_include_directories(map_terrain_generator_test PRIVATE editor/map)
    target_link_libraries(map_terrain_generator_test PRIVATE Qt6::Core)
    add_test(NAME map_terrain_generator COMMAND map_terrain_generator_test)
    add_executable(map_terrain_profile_test EXCLUDE_FROM_ALL
        tests/map_terrain_profile_test.cpp
        editor/map/mapterrainprofile.cpp)
    target_include_directories(map_terrain_profile_test PRIVATE
        editor/map libs/otformats)
    target_link_libraries(map_terrain_profile_test PRIVATE OTFormats Qt6::Core)
    add_test(NAME map_terrain_profile COMMAND map_terrain_profile_test)
    add_executable(map_dungeon_generator_test EXCLUDE_FROM_ALL
        tests/map_dungeon_generator_test.cpp
        editor/map/mapdungeongenerator.cpp)
    target_include_directories(map_dungeon_generator_test PRIVATE editor/map)
    target_link_libraries(map_dungeon_generator_test PRIVATE Qt6::Core)
    add_test(NAME map_dungeon_generator COMMAND map_dungeon_generator_test)
    add_executable(otbm_background_load_test EXCLUDE_FROM_ALL
        tests/otbm_background_load_test.cpp)
    target_include_directories(otbm_background_load_test PRIVATE libs/otformats)
    target_link_libraries(otbm_background_load_test PRIVATE OTFormats Qt6::Core)
    add_test(NAME otbm_background_load COMMAND otbm_background_load_test)
    add_executable(atlas_background_build_test EXCLUDE_FROM_ALL
        tests/atlas_background_build_test.cpp
        editor/map/mapservices.cpp
        editor/core/creaturestore.cpp)
    target_include_directories(atlas_background_build_test PRIVATE
        libs/otformats editor/map editor/core)
    target_link_libraries(atlas_background_build_test PRIVATE
        OTFormats Qt6::Core Qt6::Gui)
    add_test(NAME atlas_background_build COMMAND atlas_background_build_test)
    add_executable(document_recovery_test EXCLUDE_FROM_ALL
        tests/document_recovery_test.cpp
        editor/core/documentmanager.cpp)
    target_include_directories(document_recovery_test PRIVATE
        libs/otformats editor/core)
    target_link_libraries(document_recovery_test PRIVATE OTFormats Qt6::Core)
    add_test(NAME document_recovery COMMAND document_recovery_test)
    add_executable(creature_store_test EXCLUDE_FROM_ALL
        tests/creature_store_test.cpp
        editor/core/creaturestore.cpp)
    target_include_directories(creature_store_test PRIVATE
        libs/otformats editor/core)
    target_link_libraries(creature_store_test PRIVATE Qt6::Core)
    add_test(NAME creature_store COMMAND creature_store_test)
    add_executable(update_version_test EXCLUDE_FROM_ALL
        tests/update_version_test.cpp
        editor/core/updateversion.cpp)
    target_include_directories(update_version_test PRIVATE editor/core)
    target_link_libraries(update_version_test PRIVATE Qt6::Core)
    add_test(NAME update_version COMMAND update_version_test)
    if(WIN32)
        set_tests_properties(otbm_compact_storage otbm_editing_storage
                             otbm_background_load atlas_background_build map_path_builder
                             map_terrain_generator map_terrain_profile map_dungeon_generator
                             map_ground_cluster_generator map_selection_rotation
                             document_recovery creature_store update_version
                             brushstore_advanced brushstore_border_variants PROPERTIES
            ENVIRONMENT_MODIFICATION
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:Qt6::Core>")
    endif()
endif()

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
    add_executable(DMEUpdater WIN32
        editor/updater/main.cpp
        editor/windows/DME.rc)
    target_compile_features(DMEUpdater PRIVATE cxx_std_17)
    target_link_libraries(DMEUpdater PRIVATE Qt6::Widgets)
    set_target_properties(DMEUpdater PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${DME_RUNTIME_OUTPUT_DIRECTORY}"
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${DME_RUNTIME_OUTPUT_DIRECTORY}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DME_RUNTIME_OUTPUT_DIRECTORY}")
    add_custom_command(TARGET DMEUpdater POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_FILE:Qt6::Core>"
                "$<TARGET_FILE:Qt6::Gui>"
                "$<TARGET_FILE:Qt6::Widgets>"
                "$<TARGET_FILE_DIR:DMEUpdater>"
        VERBATIM)
    if(TARGET Qt6::QWindowsIntegrationPlugin)
        add_custom_command(TARGET DMEUpdater POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "$<TARGET_FILE_DIR:DMEUpdater>/platforms"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "$<TARGET_FILE:Qt6::QWindowsIntegrationPlugin>"
                    "$<TARGET_FILE_DIR:DMEUpdater>/platforms/qwindows.dll"
            VERBATIM)
    endif()

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
        DEPENDS DewralMapEditor DMEUpdater
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

    add_custom_target(package_checksums
        DEPENDS package_release package_source
        COMMAND "${CMAKE_COMMAND}"
                "-DRELEASE_ROOT=${CMAKE_SOURCE_DIR}/release"
                -P "${CMAKE_SOURCE_DIR}/cmake/CreateChecksums.cmake"
        COMMENT "Generating release checksums"
        VERBATIM)

    add_custom_target(package_all DEPENDS package_checksums)
endif()

qt_add_shaders(DewralMapEditor map_shaders
    PREFIX "/shaders"
    BASE "editor/map/shaders"
    GLSL "300es,330"
    HLSL 50
    MSL 12
    FILES editor/map/shaders/map.vert editor/map/shaders/map.frag)
