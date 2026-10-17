cmake_minimum_required(VERSION 3.24)
project(TileMapGenAssetTools LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 6.7 COMPONENTS Core Gui Qml Sql REQUIRED)
get_filename_component(DME_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
file(GLOB OT_SOURCES "${DME_ROOT}/libs/otformats/*.cpp")
add_library(OTFormats STATIC ${OT_SOURCES})
target_include_directories(OTFormats PUBLIC "${DME_ROOT}/libs/otformats")
target_link_libraries(OTFormats PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Sql)
add_executable(tilemapgen_asset_tool "${DME_ROOT}/tests/tilemapgen_asset_tool.cpp"
    "${DME_ROOT}/editor/core/brushstore.cpp" "${DME_ROOT}/editor/core/creaturestore.cpp"
    "${DME_ROOT}/editor/core/tilemapgenassets.cpp"
    "${DME_ROOT}/editor/core/tilemapgenmaterials.cpp" "${DME_ROOT}/editor/map/mapterrainprofile.cpp")
target_include_directories(tilemapgen_asset_tool PRIVATE "${DME_ROOT}/editor/core" "${DME_ROOT}/editor/map")
target_link_libraries(tilemapgen_asset_tool PRIVATE OTFormats)
add_executable(ground_border_priority_test "${DME_ROOT}/tests/ground_border_priority_test.cpp" "${DME_ROOT}/editor/core/brushstore.cpp")
target_include_directories(ground_border_priority_test PRIVATE "${DME_ROOT}/editor/core")
target_link_libraries(ground_border_priority_test PRIVATE OTFormats)
