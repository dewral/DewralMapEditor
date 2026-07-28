if(NOT DEFINED DEPLOYED_DIRECTORY
   OR NOT DEFINED RELEASE_ROOT
   OR NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "CreateReleasePackage.cmake requires all package paths")
endif()

set(package_name "DewralMapEditor-windows-x64")
set(package_directory "${RELEASE_ROOT}/${package_name}")
set(package_archive "${RELEASE_ROOT}/${package_name}.zip")

file(REMOVE_RECURSE "${package_directory}")
file(REMOVE "${package_archive}")
file(MAKE_DIRECTORY "${RELEASE_ROOT}")
file(COPY "${DEPLOYED_DIRECTORY}/" DESTINATION "${package_directory}")

foreach(release_document IN ITEMS LICENSE NOTICE README.md)
    set(release_document_path "${SOURCE_ROOT}/${release_document}")
    if(NOT EXISTS "${release_document_path}")
        message(FATAL_ERROR "Required release document is missing: ${release_document}")
    endif()
    file(COPY "${release_document_path}" DESTINATION "${package_directory}")
endforeach()

set(screenshot_directory "${SOURCE_ROOT}/docs/screenshots")
if(IS_DIRECTORY "${screenshot_directory}")
    file(MAKE_DIRECTORY "${package_directory}/docs")
    file(COPY "${screenshot_directory}" DESTINATION "${package_directory}/docs")
endif()

set(package_data_directory "${package_directory}/data")
if(IS_DIRECTORY "${package_data_directory}")
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

    file(GLOB_RECURSE client_binary_files
        LIST_DIRECTORIES FALSE
        "${package_data_directory}/*.dat"
        "${package_data_directory}/*.spr"
        "${package_data_directory}/*.otb"
        "${package_data_directory}/*.otfi"
    )
    if(client_binary_files)
        file(REMOVE ${client_binary_files})
    endif()
endif()

set(application_file "${package_directory}/DME.exe")
if(NOT EXISTS "${application_file}")
    message(FATAL_ERROR "The deployed DME.exe was not found")
endif()

if(DEFINED STRIP_EXECUTABLE
   AND NOT "${STRIP_EXECUTABLE}" STREQUAL ""
   AND EXISTS "${STRIP_EXECUTABLE}")
    execute_process(
        COMMAND "${STRIP_EXECUTABLE}" --strip-all "${application_file}"
        RESULT_VARIABLE strip_result
    )
    if(NOT strip_result EQUAL 0)
        message(FATAL_ERROR "Stripping DME.exe failed with exit code ${strip_result}")
    endif()
endif()

# qmltypes files are tooling metadata used by IDEs and linters, not by the
# runtime QML importer.
file(GLOB_RECURSE qml_tooling_metadata
    LIST_DIRECTORIES FALSE
    "${package_directory}/qml/*.qmltypes"
)
if(qml_tooling_metadata)
    file(REMOVE ${qml_tooling_metadata})
endif()

file(GLOB_RECURSE development_artifacts
    LIST_DIRECTORIES FALSE
    "${package_directory}/*.exp"
    "${package_directory}/*.ilk"
    "${package_directory}/*.lib"
    "${package_directory}/*.pdb"
)
if(development_artifacts)
    file(REMOVE ${development_artifacts})
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -E tar cf "${package_archive}"
        --format=zip
        -- "${package_name}"
    WORKING_DIRECTORY "${RELEASE_ROOT}"
    RESULT_VARIABLE archive_result
)
if(NOT archive_result EQUAL 0)
    message(FATAL_ERROR "Creating the release archive failed with exit code ${archive_result}")
endif()

file(SIZE "${application_file}" application_size)
file(SIZE "${package_archive}" archive_size)
message(STATUS "Production executable: ${application_size} bytes")
message(STATUS "Release archive: ${archive_size} bytes")
