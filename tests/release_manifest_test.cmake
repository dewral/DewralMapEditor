if(NOT DEFINED SOURCE_ROOT OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "SOURCE_ROOT and TEST_ROOT are required")
endif()

file(MAKE_DIRECTORY "${TEST_ROOT}")
set(binary_archive "${TEST_ROOT}/DewralMapEditor-windows-x64.zip")
set(source_archive "${TEST_ROOT}/DewralMapEditor-source.zip")
file(WRITE "${source_archive}" "source package fixture")

# Rebuilding must replace both the version and the package identity, so local
# publications cannot accidentally reuse the previous release's metadata.
foreach(version IN ITEMS "1.2.3" "1.2.4")
    file(WRITE "${binary_archive}" "binary package fixture ${version}")
    execute_process(COMMAND "${CMAKE_COMMAND}"
        "-DRELEASE_ROOT=${TEST_ROOT}"
        "-DRELEASE_VERSION=${version}"
        "-DRELEASE_REPOSITORY=example/DME"
        -P "${SOURCE_ROOT}/cmake/CreateChecksums.cmake"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "Release metadata generation failed")
    endif()

    file(READ "${TEST_ROOT}/update-manifest.json" manifest)
    string(JSON schema GET "${manifest}" schemaVersion)
    string(JSON actual_version GET "${manifest}" version)
    string(JSON digest GET "${manifest}" sha256)
    string(JSON size GET "${manifest}" size)
    string(JSON download GET "${manifest}" downloadUrl)
    file(SHA256 "${binary_archive}" expected_digest)
    file(SIZE "${binary_archive}" expected_size)
    if(NOT schema EQUAL 1 OR NOT actual_version STREQUAL version
       OR NOT digest STREQUAL expected_digest OR NOT size EQUAL expected_size
       OR NOT download STREQUAL
           "https://github.com/example/DME/releases/download/1.0/DewralMapEditor-windows-x64.zip")
        message(FATAL_ERROR "The manifest does not describe the newly built package")
    endif()
    file(SHA256 "${source_archive}" source_digest)
    file(READ "${TEST_ROOT}/SHA256SUMS.txt" checksums)
    set(expected_checksums
        "${expected_digest}  DewralMapEditor-windows-x64.zip\n${source_digest}  DewralMapEditor-source.zip\n")
    if(NOT checksums STREQUAL expected_checksums)
        message(FATAL_ERROR "Release checksums disagree with the manifest or archives")
    endif()
endforeach()

execute_process(COMMAND "${CMAKE_COMMAND}"
    "-DRELEASE_ROOT=${TEST_ROOT}" "-DRELEASE_VERSION=invalid"
    -P "${SOURCE_ROOT}/cmake/CreateChecksums.cmake"
    RESULT_VARIABLE invalid_result OUTPUT_QUIET ERROR_QUIET)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "Invalid stable release versions must be rejected")
endif()
file(READ "${TEST_ROOT}/update-manifest.json" unchanged_manifest)
if(NOT unchanged_manifest STREQUAL manifest)
    message(FATAL_ERROR "Invalid metadata must not overwrite a valid manifest")
endif()
