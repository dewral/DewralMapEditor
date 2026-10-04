if(NOT DEFINED RELEASE_ROOT)
    message(FATAL_ERROR "CreateChecksums.cmake requires RELEASE_ROOT")
endif()

if(NOT DEFINED RELEASE_VERSION
   OR NOT RELEASE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "CreateChecksums.cmake requires a stable RELEASE_VERSION")
endif()
if(NOT DEFINED RELEASE_REPOSITORY)
    set(RELEASE_REPOSITORY "dewral/DewralMapEditor")
endif()
if(NOT RELEASE_REPOSITORY MATCHES "^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")
    message(FATAL_ERROR "RELEASE_REPOSITORY must be a GitHub owner/repository")
endif()

set(binary_archive "${RELEASE_ROOT}/DewralMapEditor-windows-x64.zip")
set(source_archive "${RELEASE_ROOT}/DewralMapEditor-source.zip")
if(NOT EXISTS "${binary_archive}" OR NOT EXISTS "${source_archive}")
    message(FATAL_ERROR "Both release archives must exist before checksums are generated")
endif()

file(SHA256 "${binary_archive}" binary_hash)
file(SHA256 "${source_archive}" source_hash)
file(SIZE "${binary_archive}" binary_size)
string(TIMESTAMP published_at "%Y-%m-%dT%H:%M:%SZ" UTC)
set(release_url "https://github.com/${RELEASE_REPOSITORY}/releases")
string(JSON manifest SET "{}" schemaVersion 1)
string(JSON manifest SET "${manifest}" version "\"${RELEASE_VERSION}\"")
string(JSON manifest SET "${manifest}" publishedAt "\"${published_at}\"")
string(JSON manifest SET "${manifest}" downloadUrl
    "\"${release_url}/download/1.0/DewralMapEditor-windows-x64.zip\"")
string(JSON manifest SET "${manifest}" releasePageUrl "\"${release_url}/tag/1.0\"")
string(JSON manifest SET "${manifest}" sha256 "\"${binary_hash}\"")
string(JSON manifest SET "${manifest}" size "${binary_size}")
string(JSON manifest SET "${manifest}" notes
    "\"Dewral Map Editor ${RELEASE_VERSION} stable release.\"")
file(WRITE "${RELEASE_ROOT}/update-manifest.json" "${manifest}\n")
file(WRITE "${RELEASE_ROOT}/SHA256SUMS.txt"
    "${binary_hash}  DewralMapEditor-windows-x64.zip\n"
    "${source_hash}  DewralMapEditor-source.zip\n")

message(STATUS "Release ${RELEASE_VERSION} manifest and SHA-256 checksums updated")
