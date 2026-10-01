if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED RELEASE_ROOT)
    message(FATAL_ERROR "CreateSourcePackage.cmake requires SOURCE_ROOT and RELEASE_ROOT")
endif()

set(package_name "DewralMapEditor-source")
set(package_directory "${RELEASE_ROOT}/${package_name}")
set(package_archive "${RELEASE_ROOT}/${package_name}.zip")

file(REMOVE_RECURSE "${package_directory}")
file(REMOVE "${package_archive}")
file(MAKE_DIRECTORY "${package_directory}")

set(source_files
    ".gitattributes"
    ".gitignore"
    "build-release.bat"
    "CMakeLists.txt"
    "CMakePresets.json"
    "LICENSE"
    "NOTICE"
    "README.md"
    "vcpkg.json"
)

set(source_directories
    "cmake"
    "data"
    "docs"
    "editor"
    "libs"
    "scripts"
    "tests"
    "triplets"
)

foreach(relative_path IN LISTS source_files)
    set(source_path "${SOURCE_ROOT}/${relative_path}")
    if(NOT EXISTS "${source_path}")
        message(FATAL_ERROR "Required source package file is missing: ${relative_path}")
    endif()
    file(COPY "${source_path}" DESTINATION "${package_directory}")
endforeach()

foreach(relative_path IN LISTS source_directories)
    set(source_path "${SOURCE_ROOT}/${relative_path}")
    if(NOT IS_DIRECTORY "${source_path}")
        message(FATAL_ERROR "Required source package directory is missing: ${relative_path}")
    endif()
    file(COPY "${source_path}" DESTINATION "${package_directory}")
endforeach()

# Only numeric built-in profiles belong to a public source release. Named
# profiles are local user configuration and may contain private data.
set(package_data_directory "${package_directory}/data")
file(GLOB packaged_data_entries
    LIST_DIRECTORIES TRUE
    "${package_data_directory}/*"
)
foreach(packaged_data_entry IN LISTS packaged_data_entries)
    if(IS_DIRECTORY "${packaged_data_entry}")
        get_filename_component(data_entry_name "${packaged_data_entry}" NAME)
        if(NOT data_entry_name MATCHES "^[0-9]+$")
            file(REMOVE_RECURSE "${packaged_data_entry}")
        endif()
    endif()
endforeach()

file(GLOB_RECURSE forbidden_source_files
    LIST_DIRECTORIES FALSE
    "${package_directory}/*.dat"
    "${package_directory}/*.spr"
    "${package_directory}/*.otb"
    "${package_directory}/*.otfi"
    "${package_directory}/*.exe"
    "${package_directory}/*.dll"
    "${package_directory}/*.pdb"
    "${package_directory}/*.pyc"
    "${package_directory}/*.user"
    "${package_directory}/*.autosave"
)
if(forbidden_source_files)
    file(REMOVE ${forbidden_source_files})
endif()

set(known_cache_directories
    "${package_directory}/scripts/__pycache__"
)
foreach(cache_directory IN LISTS known_cache_directories)
    if(IS_DIRECTORY "${cache_directory}")
        file(REMOVE_RECURSE "${cache_directory}")
    endif()
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${package_archive}"
        --format=zip
        -- "${package_name}"
    WORKING_DIRECTORY "${RELEASE_ROOT}"
    RESULT_VARIABLE archive_result
)
if(NOT archive_result EQUAL 0)
    message(FATAL_ERROR "Creating the source release archive failed with exit code ${archive_result}")
endif()

file(SIZE "${package_archive}" archive_size)
message(STATUS "Source release archive: ${archive_size} bytes")
