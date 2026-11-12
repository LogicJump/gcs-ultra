[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $QtRoot,

    [string] $WorkRoot,
    [string] $SourceSeed,
    [string] $MsvcEnvScript,
    [string] $MsvcEnvArguments,
    [string] $VulkanHeaderRoot,

    [ValidateRange(1, 256)]
    [int] $Jobs = [Environment]::ProcessorCount,

    [switch] $ValidateOnly
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$expectedQtVersion = "6.12.0"
$expectedSourceRevision = "a50a2ff960ca58d4b9f87b7e6a7ddae02e620426"
$sourceUrl = "https://github.com/qt/qtquick3d.git"
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$patchPath = Join-Path $PSScriptRoot "patches\qtquick3d-6.12-cleanup-queue.patch"
$reproducerSource = Join-Path $PSScriptRoot "reproducer"

function Resolve-AbsolutePath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $PathValue,

        [switch] $MustExist
    )

    $expandedPath = [Environment]::ExpandEnvironmentVariables($PathValue)
    if (-not [IO.Path]::IsPathRooted($expandedPath)) {
        $expandedPath = Join-Path (Get-Location) $expandedPath
    }

    $absolutePath = [IO.Path]::GetFullPath($expandedPath)
    if ($MustExist -and -not (Test-Path -LiteralPath $absolutePath)) {
        throw "Path does not exist: $absolutePath"
    }

    return $absolutePath
}

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Command,

        [Parameter()]
        [string[]] $Arguments = @()
    )

    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command $($Arguments -join ' ')"
    }
}

function Invoke-ExternalCapture {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Command,

        [Parameter()]
        [string[]] $Arguments = @()
    )

    $output = @(& $Command @Arguments)
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Command $($Arguments -join ' ')"
    }

    return $output
}

function Get-SourcePatchState {
    param(
        [Parameter(Mandatory = $true)]
        [string] $SourceDirectory,

        [Parameter(Mandatory = $true)]
        [string] $PatchPath
    )

    $stagedFiles = @(Invoke-ExternalCapture -Command "git.exe" -Arguments @(
        "-C", $SourceDirectory, "diff", "--cached", "--name-only"
    ))
    if ($stagedFiles.Count -ne 0) {
        throw "The isolated source contains staged changes. Use a fresh -WorkRoot; no source files were modified."
    }

    $untrackedFiles = @(Invoke-ExternalCapture -Command "git.exe" -Arguments @(
        "-C", $SourceDirectory, "ls-files", "--others", "--exclude-standard"
    ))
    if ($untrackedFiles.Count -ne 0) {
        throw "The isolated source contains untracked files. Use a fresh -WorkRoot; no source files were modified."
    }

    $sourceDiff = @(Invoke-ExternalCapture -Command "git.exe" -Arguments @(
        "-C", $SourceDirectory, "diff", "--binary", "--no-color", "--no-ext-diff", "--unified=1"
    ))
    if ($sourceDiff.Count -eq 0) {
        return "Clean"
    }

    $expectedPatch = [IO.File]::ReadAllText($PatchPath).Replace("`r`n", "`n").TrimEnd("`n")
    $actualPatch = ($sourceDiff -join "`n").Replace("`r`n", "`n").TrimEnd("`n")
    if ($actualPatch -cne $expectedPatch) {
        throw "The isolated source differs from the promoted cleanup patch. Use a fresh -WorkRoot; no source files were modified."
    }

    return "Patched"
}

function Import-MsvcEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ScriptPath,

        [string] $Arguments
    )

    $resolvedScript = Resolve-AbsolutePath -PathValue $ScriptPath -MustExist
    if ([string]::IsNullOrWhiteSpace($Arguments) -and
        [IO.Path]::GetFileName($resolvedScript) -ieq "VsDevCmd.bat") {
        $Arguments = "-arch=x64 -host_arch=x64"
    }

    $commandLine = "call `"$resolvedScript`" $Arguments >nul && set"
    $environmentLines = & $env:ComSpec /d /c $commandLine
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to initialize the MSVC environment with $resolvedScript"
    }

    foreach ($line in $environmentLines) {
        if ($line -match "^([^=]+)=(.*)$") {
            [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
        }
    }
}

function Get-ShortPath {
    param(
        [Parameter(Mandatory = $true)]
        [string] $PathValue
    )

    if ($PathValue -notmatch "\s") {
        return $PathValue
    }

    $commandLine = "for %I in (`"$PathValue`") do @echo %~sI"
    $shortPath = (& $env:ComSpec /d /c $commandLine).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($shortPath) -or $shortPath -match "\s") {
        throw "Unable to obtain a space-free 8.3 path. Supply a space-free -WorkRoot. Path: $PathValue"
    }

    return $shortPath
}

function Assert-FilesUnlocked {
    param(
        [Parameter(Mandatory = $true)]
        [string[]] $Paths
    )

    foreach ($path in $Paths) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
            continue
        }

        try {
            $stream = [IO.File]::Open($path, [IO.FileMode]::Open, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
            $stream.Close()
        } catch {
            throw "An existing isolated runtime file is in use. Close processes using it or choose a different -WorkRoot: $path"
        }
    }
}

if ([string]::IsNullOrWhiteSpace($WorkRoot)) {
    $WorkRoot = Join-Path $repositoryRoot ".cache\qtquick3d-cleanup"
}

$qtRootResolved = Resolve-AbsolutePath -PathValue $QtRoot -MustExist
$workRootResolved = Resolve-AbsolutePath -PathValue $WorkRoot
if ($workRootResolved -eq $repositoryRoot) {
    throw "WorkRoot must be a dedicated directory, not the repository root."
}

if (-not [string]::IsNullOrWhiteSpace($MsvcEnvScript)) {
    Import-MsvcEnvironment -ScriptPath $MsvcEnvScript -Arguments $MsvcEnvArguments
}

foreach ($tool in @("cmake.exe", "git.exe", "ninja.exe", "cl.exe")) {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "Required tool is not available in PATH: $tool"
    }
}

$qmakePath = Join-Path $qtRootResolved "bin\qmake.exe"
$configureModule = Join-Path $qtRootResolved "bin\qt-configure-module.bat"
foreach ($requiredPath in @($qmakePath, $configureModule, $patchPath, $reproducerSource)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required path does not exist: $requiredPath"
    }
}

$qtVersion = (& $qmakePath -query QT_VERSION).Trim()
if ($LASTEXITCODE -ne 0 -or $qtVersion -ne $expectedQtVersion) {
    throw "This helper requires Qt $expectedQtVersion, but $qtRootResolved reports '$qtVersion'."
}

if ([string]::IsNullOrWhiteSpace($VulkanHeaderRoot) -and -not [string]::IsNullOrWhiteSpace($env:VULKAN_SDK)) {
    $VulkanHeaderRoot = Join-Path $env:VULKAN_SDK "Include"
}
if ([string]::IsNullOrWhiteSpace($VulkanHeaderRoot)) {
    throw "Specify -VulkanHeaderRoot or set VULKAN_SDK. Qt Quick3D requires vulkan/vulkan.h for this kit."
}

$vulkanHeaderRootResolved = Resolve-AbsolutePath -PathValue $VulkanHeaderRoot -MustExist
$vulkanHeader = Join-Path $vulkanHeaderRootResolved "vulkan\vulkan.h"
if (-not (Test-Path -LiteralPath $vulkanHeader -PathType Leaf)) {
    throw "VulkanHeaderRoot must contain vulkan\vulkan.h: $vulkanHeaderRootResolved"
}

$sourceSeedResolved = $null
if (-not [string]::IsNullOrWhiteSpace($SourceSeed)) {
    $sourceSeedResolved = Resolve-AbsolutePath -PathValue $SourceSeed -MustExist
    if (-not (Test-Path -LiteralPath (Join-Path $sourceSeedResolved ".git"))) {
        throw "SourceSeed must be a qtquick3d Git checkout: $sourceSeedResolved"
    }
}

$sourceDirectory = Join-Path $workRootResolved "source"
$buildDirectory = Join-Path $workRootResolved "build"
$installDirectory = Join-Path $workRootResolved "install"
$reproducerBuildDirectory = Join-Path $workRootResolved "reproducer-build"
$runtimeFiles = @(
    "bin\Qt6Quick3DUtilsd.dll",
    "bin\Qt6Quick3DRuntimeRenderd.dll",
    "bin\Qt6Quick3Dd.dll",
    "qml\QtQuick3D\qquick3dplugind.dll",
    "qml\QtQuick3D\qmldir"
)
$runtimePaths = $runtimeFiles | ForEach-Object { Join-Path $installDirectory $_ }

Assert-FilesUnlocked -Paths $runtimePaths

Write-Host "Qt root:          $qtRootResolved"
Write-Host "Work root:        $workRootResolved"
Write-Host "Vulkan headers:   $vulkanHeaderRootResolved"
Write-Host "Parallel jobs:    $Jobs"
if ($sourceSeedResolved) {
    Write-Host "Source seed:      $sourceSeedResolved"
} else {
    Write-Host "Source fetch:     $sourceUrl (tag v$expectedQtVersion)"
}

if ($ValidateOnly) {
    Write-Host "Validation succeeded. No source, build, or install files were changed."
    return
}

foreach ($directory in @($workRootResolved, $buildDirectory, $installDirectory)) {
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
}

if (-not (Test-Path -LiteralPath $sourceDirectory)) {
    if ($sourceSeedResolved) {
        Invoke-External -Command "git.exe" -Arguments @("clone", "--no-hardlinks", $sourceSeedResolved, $sourceDirectory)
        Invoke-External -Command "git.exe" -Arguments @("-C", $sourceDirectory, "checkout", "--detach", $expectedSourceRevision)
    } else {
        Invoke-External -Command "git.exe" -Arguments @(
            "clone", "--depth", "1", "--branch", "v$expectedQtVersion", $sourceUrl, $sourceDirectory
        )
    }
}

if (-not (Test-Path -LiteralPath (Join-Path $sourceDirectory ".git"))) {
    throw "Existing source directory is not a Git checkout: $sourceDirectory"
}

$actualRevisionOutput = @(Invoke-ExternalCapture -Command "git.exe" -Arguments @(
    "-C", $sourceDirectory, "rev-parse", "HEAD"
))
$actualRevision = ($actualRevisionOutput -join "`n").Trim()
if ($actualRevision -ne $expectedSourceRevision) {
    throw "Expected qtquick3d revision $expectedSourceRevision, found $actualRevision."
}

$sourcePatchState = Get-SourcePatchState -SourceDirectory $sourceDirectory -PatchPath $patchPath
if ($sourcePatchState -eq "Clean") {
    Invoke-External -Command "git.exe" -Arguments @("-C", $sourceDirectory, "apply", "--check", $patchPath)
    Invoke-External -Command "git.exe" -Arguments @("-C", $sourceDirectory, "apply", $patchPath)
} else {
    Write-Host "Cleanup patch is already applied."
}

$verifiedSourcePatchState = Get-SourcePatchState -SourceDirectory $sourceDirectory -PatchPath $patchPath
if ($verifiedSourcePatchState -ne "Patched") {
    throw "The isolated source does not contain the promoted cleanup patch."
}

$sourceForConfigure = Get-ShortPath -PathValue $sourceDirectory
$installForConfigure = Get-ShortPath -PathValue $installDirectory
$vulkanCMakePath = $vulkanHeaderRootResolved.Replace("\", "/")
$configureArguments = @(
    $sourceForConfigure,
    "--",
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DBUILD_TESTING=OFF",
    "-DQT_BUILD_TESTS=OFF",
    "-DQT_BUILD_EXAMPLES=OFF",
    "-DFEATURE_quick3d_assimp=OFF",
    "-DFEATURE_quick3dxr_openxr=OFF",
    "-DCMAKE_INSTALL_PREFIX=$installForConfigure",
    "-DCMAKE_CXX_FLAGS=/I`"$vulkanCMakePath`""
)

Push-Location $buildDirectory
try {
    Invoke-External -Command $configureModule -Arguments $configureArguments
} finally {
    Pop-Location
}

Invoke-External -Command "cmake.exe" -Arguments @(
    "--build", $buildDirectory,
    "--target", "Quick3D", "qquick3dplugin",
    "--parallel", "$Jobs"
)

foreach ($module in @("utils", "runtimerender", "quick3d")) {
    $installScript = Join-Path $buildDirectory "src\$module\cmake_install.cmake"
    if (-not (Test-Path -LiteralPath $installScript -PathType Leaf)) {
        throw "Expected install script was not generated: $installScript"
    }
    Invoke-External -Command "cmake.exe" -Arguments @(
        "-DCMAKE_INSTALL_COMPONENT=Unspecified",
        "-DCMAKE_INSTALL_LOCAL_ONLY=1",
        "-P", $installScript
    )
}

foreach ($relativePath in $runtimeFiles) {
    $runtimePath = Join-Path $installDirectory $relativePath
    if (-not (Test-Path -LiteralPath $runtimePath -PathType Leaf)) {
        throw "The isolated runtime is incomplete; missing $runtimePath"
    }
}

Invoke-External -Command "cmake.exe" -Arguments @(
    "-S", $reproducerSource,
    "-B", $reproducerBuildDirectory,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Debug",
    "-DCMAKE_PREFIX_PATH=$qtRootResolved"
)
Invoke-External -Command "cmake.exe" -Arguments @(
    "--build", $reproducerBuildDirectory,
    "--parallel", "$Jobs"
)

Write-Host "Isolated Qt Quick3D runtime: $installDirectory"
Write-Host "Lifecycle reproducer:        $(Join-Path $reproducerBuildDirectory 'tst_quick3dcleanupqueue.exe')"
Write-Host "The helper does not run the GUI reproducer. See README.md for RED/GREEN commands."
