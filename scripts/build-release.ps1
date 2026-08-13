[CmdletBinding()]
param(
    [ValidateSet("stable", "development")]
    [string] $BuildChannel = "stable"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$repositoryRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..")
)
$vcpkgRoot = Join-Path $repositoryRoot ".tools\vcpkg"
$vcpkgExecutable = Join-Path $vcpkgRoot "vcpkg.exe"
$vcpkgTag = "2026.03.18"
$vcpkgBaseline = "c3867e714dd3a51c272826eea77267876517ed99"
$releaseArchive = Join-Path $repositoryRoot "release\DewralMapEditor-windows-x64.zip"
$sourceArchive = Join-Path $repositoryRoot "release\DewralMapEditor-source.zip"

function Require-Command {
    param(
        [Parameter(Mandatory)]
        [string] $Name,

        [Parameter(Mandatory)]
        [string] $InstallHint
    )

    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name was not found. $InstallHint"
    }
}

Write-Host "Dewral Map Editor - Windows release build" -ForegroundColor Cyan
Write-Host "Repository: $repositoryRoot"

Require-Command "git.exe" "Install Git for Windows and reopen this terminal."
Require-Command "cmake.exe" "Install CMake 3.24 or newer and reopen this terminal."

$cmakeVersionText = (& cmake.exe --version | Select-Object -First 1)
if ($cmakeVersionText -notmatch "cmake version (\d+)\.(\d+)") {
    throw "The installed CMake version could not be detected."
}
if ([int]$Matches[1] -lt 3 -or
    ([int]$Matches[1] -eq 3 -and [int]$Matches[2] -lt 24)) {
    throw "CMake 3.24 or newer is required. Found: $cmakeVersionText"
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw "Visual Studio 2022 or Build Tools 2022 was not found. Install the Desktop development with C++ workload."
}

$visualStudioPath = (& $vswhere -latest -products * `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath)
if (-not $visualStudioPath) {
    throw "The Visual Studio C++ toolchain was not found. Install the Desktop development with C++ workload."
}
Write-Host "MSVC: $visualStudioPath"

if ((Test-Path -LiteralPath $vcpkgRoot) -and
    -not (Test-Path -LiteralPath (Join-Path $vcpkgRoot ".git"))) {
    throw "$vcpkgRoot exists but is not a valid vcpkg checkout. Remove that directory and run the build again."
}

if (-not (Test-Path -LiteralPath (Join-Path $vcpkgRoot ".git"))) {
    New-Item -ItemType Directory -Force -Path (Split-Path $vcpkgRoot) | Out-Null
    Write-Host "Downloading vcpkg $vcpkgTag..."
    & git.exe clone --branch $vcpkgTag --depth 1 `
        https://github.com/microsoft/vcpkg.git $vcpkgRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Cloning vcpkg failed with exit code $LASTEXITCODE."
    }
}

$currentVcpkgCommit = (& git.exe -C $vcpkgRoot rev-parse HEAD)
if ($LASTEXITCODE -ne 0) {
    throw "Reading the vcpkg revision failed with exit code $LASTEXITCODE."
}
$currentVcpkgCommit = $currentVcpkgCommit.Trim()
$vcpkgChanged = $false
if ($currentVcpkgCommit -ne $vcpkgBaseline) {
    Write-Host "Switching vcpkg to the pinned $vcpkgTag baseline..."
    & git.exe -C $vcpkgRoot fetch origin tag $vcpkgTag --depth 1 --force
    if ($LASTEXITCODE -ne 0) {
        throw "Updating the vcpkg checkout failed with exit code $LASTEXITCODE."
    }
    & git.exe -C $vcpkgRoot checkout --detach $vcpkgBaseline
    if ($LASTEXITCODE -ne 0) {
        throw "Checking out the pinned vcpkg baseline failed with exit code $LASTEXITCODE."
    }
    $vcpkgChanged = $true
}

if ($vcpkgChanged -or -not (Test-Path -LiteralPath $vcpkgExecutable)) {
    Write-Host "Bootstrapping vcpkg..."
    & (Join-Path $vcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
    if ($LASTEXITCODE -ne 0) {
        throw "Bootstrapping vcpkg failed with exit code $LASTEXITCODE."
    }
}

$binaryCache = Join-Path $repositoryRoot ".cache\vcpkg"
New-Item -ItemType Directory -Force -Path $binaryCache | Out-Null

$env:VCPKG_ROOT = $vcpkgRoot
$env:VCPKG_DISABLE_METRICS = "1"
if (-not $env:VCPKG_BINARY_SOURCES) {
    $env:VCPKG_BINARY_SOURCES = "clear;files,$binaryCache,readwrite"
}

Push-Location $repositoryRoot
try {
    Write-Host "Configuring the project and installing dependencies..."
    & cmake.exe --preset windows-vcpkg "-DDME_BUILD_CHANNEL=$BuildChannel"
    if ($LASTEXITCODE -ne 0) {
        $manifestLog = Join-Path $repositoryRoot `
            "build\vcpkg-windows\vcpkg-manifest-install.log"
        if (Test-Path -LiteralPath $manifestLog) {
            Write-Host ""
            Write-Host "Last vcpkg messages:" -ForegroundColor Yellow
            Get-Content -LiteralPath $manifestLog -Tail 50 |
                ForEach-Object { Write-Host $_ }
        }
        throw "CMake configuration failed with exit code $LASTEXITCODE."
    }

    Write-Host "Building and packaging the Release configuration..."
    & cmake.exe --build --preset windows-release
    if ($LASTEXITCODE -ne 0) {
        throw "The release build failed with exit code $LASTEXITCODE."
    }
}
finally {
    Pop-Location
}

if (-not (Test-Path -LiteralPath $releaseArchive)) {
    throw "The build completed, but the release archive was not created."
}
if (-not (Test-Path -LiteralPath $sourceArchive)) {
    throw "The build completed, but the source archive was not created."
}

$archiveSize = [math]::Round((Get-Item -LiteralPath $releaseArchive).Length / 1MB, 2)
$sourceArchiveSize = [math]::Round((Get-Item -LiteralPath $sourceArchive).Length / 1MB, 2)
Write-Host ""
Write-Host "Release packages created successfully." -ForegroundColor Green
Write-Host "Ready-to-run: $releaseArchive ($archiveSize MB)"
Write-Host "Source code:  $sourceArchive ($sourceArchiveSize MB)"
