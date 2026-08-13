if(NOT DEFINED RELEASE_ROOT)
    message(FATAL_ERROR "CreateChecksums.cmake requires RELEASE_ROOT")
endif()

set(binary_archive "${RELEASE_ROOT}/DewralMapEditor-windows-x64.zip")
set(source_archive "${RELEASE_ROOT}/DewralMapEditor-source.zip")
if(NOT EXISTS "${binary_archive}" OR NOT EXISTS "${source_archive}")
    message(FATAL_ERROR "Both release archives must exist before checksums are generated")
endif()

file(SHA256 "${binary_archive}" binary_hash)
file(SHA256 "${source_archive}" source_hash)
file(WRITE "${RELEASE_ROOT}/SHA256SUMS.txt"
    "${binary_hash}  DewralMapEditor-windows-x64.zip\n"
    "${source_hash}  DewralMapEditor-source.zip\n")

message(STATUS "Release SHA-256 checksums updated")
