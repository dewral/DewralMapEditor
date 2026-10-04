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
if(SYSTEM_RUNTIME_LIBRARIES)
    file(COPY ${SYSTEM_RUNTIME_LIBRARIES} DESTINATION "${package_directory}")
endif()

# The deployment directory is also used for local development and can contain
# old diagnostic executables. Only the application and its updater belong in
# the portable release.
file(GLOB packaged_executables
    LIST_DIRECTORIES FALSE
    "${package_directory}/*.exe"
)
foreach(packaged_executable IN LISTS packaged_executables)
    get_filename_component(executable_name "${packaged_executable}" NAME)
    if(NOT executable_name STREQUAL "DME.exe"
       AND NOT executable_name STREQUAL "DMEUpdater.exe")
        file(REMOVE "${packaged_executable}")
    endif()
endforeach()

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

# Qt builds from vcpkg can link additional shared libraries that windeployqt
# does not deploy. Resolve the actual imports of the application and plugins.
if(RUNTIME_DEPENDENCY_COMMAND)
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_PLATFORM "windows+pe")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_TOOL "dumpbin")
    set(CMAKE_GET_RUNTIME_DEPENDENCIES_COMMAND "${RUNTIME_DEPENDENCY_COMMAND}")
    get_filename_component(qt_runtime_directory "${QT_CORE_FILE}" DIRECTORY)
    file(GLOB_RECURSE runtime_libraries "${package_directory}/*.dll")
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES "${application_file}" "${package_directory}/DMEUpdater.exe"
        LIBRARIES ${runtime_libraries}
        DIRECTORIES "${package_directory}" "${qt_runtime_directory}"
        PRE_EXCLUDE_REGEXES "^(api-ms-|ext-ms-)"
        POST_EXCLUDE_REGEXES ".*[/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*"
        RESOLVED_DEPENDENCIES_VAR resolved_libraries
        UNRESOLVED_DEPENDENCIES_VAR unresolved_libraries)
    if(unresolved_libraries)
        message(FATAL_ERROR "Unresolved release dependencies: ${unresolved_libraries}")
    endif()
    foreach(runtime_library IN LISTS resolved_libraries)
        get_filename_component(runtime_name "${runtime_library}" NAME)
        if(NOT EXISTS "${package_directory}/${runtime_name}")
            file(COPY "${runtime_library}" DESTINATION "${package_directory}")
        endif()
    endforeach()
    # Preserve the dependency notices supplied by a vcpkg Qt installation.
    file(GLOB runtime_notices "${qt_runtime_directory}/../share/*/copyright")
    foreach(runtime_notice IN LISTS runtime_notices)
        get_filename_component(notice_directory "${runtime_notice}" DIRECTORY)
        get_filename_component(dependency_name "${notice_directory}" NAME)
        file(COPY "${runtime_notice}"
            DESTINATION "${package_directory}/licenses/${dependency_name}")
    endforeach()
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
