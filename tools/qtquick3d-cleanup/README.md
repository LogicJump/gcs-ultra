# Qt 6.12 Quick3D cleanup queue fix

This directory builds a patched Qt Quick3D runtime beside the installed Qt SDK. It never overwrites Qt, QGroundControl, or the Vulkan SDK.

The target is Qt Quick3D `v6.12.0` at [revision `a50a2ff960ca58d4b9f87b7e6a7ddae02e620426`](https://github.com/qt/qtquick3d/tree/a50a2ff960ca58d4b9f87b7e6a7ddae02e620426). The regression follows Qt commit [`82231d2552aecbafbb347b5c66023bfbfe3ad719`](https://github.com/qt/qtquick3d/commit/82231d2552aecbafbb347b5c66023bfbfe3ad719), which keeps `QQuick3DWindowAttachment` alive for the lifetime of its `QQuickWindow`.

## Failure and correction

When the last `View3D` is destroyed, `evaluateEol()` removes its scene manager from `sceneManagers` and queues it in `sceneManagerCleanupQueue`. With no live `View3D`, a later Quick3D synchronization may never drain that queue. Window invalidation processes only the live managers, and attachment destruction then deletes the queued manager after the render context and window are unavailable. Qt emits:

```text
Pending resource cleanup queue not empty, but no RCI or window to clean it up!
```

[`patches/qtquick3d-6.12-cleanup-queue.patch`](patches/qtquick3d-6.12-cleanup-queue.patch) adds seven lines to `QQuick3DWindowAttachment::onInvalidated()`. It deletes queued managers on the render thread before the existing resource collection, matching the cleanup ordering already used by `synchronize()`.

The public-API regression in [`reproducer/`](reproducer/) creates a real `QQuickView`, renders a `Loader` containing a cube `View3D`, unloads the final viewport, renders the empty Qt Quick scene, and destroys the window. `QTest::failOnWarning()` turns the exact diagnostic into a test failure. On Windows it also prints the loaded PE paths and, when `QT_QUICK3D_OVERRIDE_ROOT` is set, verifies that all four Quick3D modules came from the isolated prefix.

## Prerequisites

- Windows 10 or later.
- The MSVC toolset used by the Qt kit, CMake, Ninja, and Git.
- Qt `6.12.0` MSVC Debug libraries with Qt Quick and Qt Quick3D.
- Vulkan headers containing `vulkan/vulkan.h`.
- Roughly 500 Ninja build steps for the Debug `Quick3D` and `qquick3dplugin` targets.

The installed Qt kit used during validation advertised Vulkan support but did not include the Vulkan headers. Pass their `Include` directory explicitly, or set `VULKAN_SDK` and let the helper use `$env:VULKAN_SDK\Include`.

## Build an isolated runtime

Use a fresh PowerShell. `-MsvcEnvScript` may name `VsDevCmd.bat` or a `vcvars*.bat` file. Omit it when the shell already has an initialized MSVC environment. `VsDevCmd.bat` defaults to `-arch=x64 -host_arch=x64`; use `-MsvcEnvArguments` to select the toolset that matches the Qt kit, for example `-arch=x64 -host_arch=x64 -vcvars_ver=14.44`.

```powershell
$QtRoot = "D:\Qt\6.12.0\msvc2022_64"
$WorkRoot = "D:\qt-work\qtquick3d-cleanup"

& .\tools\qtquick3d-cleanup\build.ps1 `
    -QtRoot $QtRoot `
    -WorkRoot $WorkRoot `
    -MsvcEnvScript "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat" `
    -MsvcEnvArguments "-arch=x64 -host_arch=x64 -vcvars_ver=14.44" `
    -VulkanHeaderRoot "D:\VulkanSDK\Include" `
    -Jobs 8
```

When `-SourceSeed` is omitted, the helper shallow-clones the official Qt repository at tag `v6.12.0`. To avoid a network fetch, seed it from an existing Git checkout that contains the expected revision:

```powershell
& .\tools\qtquick3d-cleanup\build.ps1 `
    -QtRoot $QtRoot `
    -WorkRoot $WorkRoot `
    -SourceSeed "D:\src\qtquick3d" `
    -VulkanHeaderRoot "D:\VulkanSDK\Include"
```

The helper verifies the exact Qt version and source revision, checks that an existing isolated runtime is not loaded by another process, applies the patch idempotently, configures from the isolated build directory, builds only `Quick3D` and `qquick3dplugin`, installs their runtime closure, and builds the reproducer. If the repository path contains spaces, it uses the Windows 8.3 path for Qt's batch configure wrapper. Supply a space-free `-WorkRoot` if 8.3 names are disabled.

Use `-ValidateOnly` to check paths, tool availability, Qt version, MSVC initialization, and Vulkan headers without fetching or changing source and build files.

## Prove RED and GREEN

The helper deliberately does not launch a GUI test. Run these commands only when no other serialized GUI test owns the desktop.

First run against the installed, unpatched Qt runtime:

```powershell
$TestExe = "$WorkRoot\reproducer-build\tst_quick3dcleanupqueue.exe"
& $TestExe -o "$WorkRoot\red.txt,txt"
```

The Qt `6.12.0` RED result is `2 passed, 1 failed`, caused by the exact pending-cleanup warning.

Then overlay the isolated runtime for this PowerShell process and rerun:

```powershell
$InstallPrefix = "$WorkRoot\install"
$env:PATH = "$InstallPrefix\bin;$env:PATH"
$env:QML2_IMPORT_PATH = "$InstallPrefix\qml;$QtRoot\qml"
$env:QT_QUICK3D_OVERRIDE_ROOT = $InstallPrefix

& $TestExe -o "$WorkRoot\green.txt,txt"
```

The GREEN result is `3 passed, 0 failed`. The output must report all four modules below from `$InstallPrefix`:

<!-- spellchecker:off -->

```text
bin/Qt6Quick3DUtilsd.dll
bin/Qt6Quick3DRuntimeRenderd.dll
bin/Qt6Quick3Dd.dll
qml/QtQuick3D/qquick3dplugind.dll
```

<!-- spellchecker:on -->

Start a new PowerShell process to remove the overlay. Do not copy these files over the installed Qt SDK.

## Validation record

The reproducer was validated on Windows with Qt `6.12.0` and MSVC `14.44`:

- Stock Qt: `2 passed, 1 failed` with the exact cleanup warning.
- Isolated patched runtime: `3 passed, 0 failed`.
- The patched run loaded all four modules listed above from the isolated install prefix.

The source tree, build tree, install prefix, and runtime overlay are all disposable. No global SDK file is modified.
