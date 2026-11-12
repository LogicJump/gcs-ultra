# gcs-ultra（QGC2）Windows 10 迁移指南

本指南用于把当前 QGC2 源码迁移到新的公开仓库 `LogicJump/gcs-ultra`，并在另一台 Windows 10 x64
工作站上重新配置、编译和测试。原同名 Rust/Cargo 仓库已经删除，备份镜像与元数据由交接流程单独保存，
不得混入新的 QGC 仓库。

## 迁移基线

| 项目 | 要求 |
| --- | --- |
| 仓库 | `https://github.com/LogicJump/gcs-ultra.git` |
| 分支 | `main` |
| 提交记录 | 以后续外部交接清单为准；目标机用 `git log` 核对 |
| 操作系统 | Windows 10 x64；当前验证主机为 `10.0.19045` |
| 编译器 | Visual Studio 2022 的 MSVC v143 14.44 x64 工具集；验证编译器为 `19.44.35221` |
| Qt | Qt 6.12.0，`msvc2022_64` 套件 |
| Python | 推荐 3.12；安全 tar 解包要求 3.10.12+、3.11.4+ 或 3.12+ |
| Python 工具 | 由 `tools/pyproject.toml` 和 `tools/uv.lock` 锁定；uv 至少为 0.12.8 |
| GStreamer | 1.28.8 MSVC x86_64 开发套件 |

工具的安全 tar 解包依赖 PEP 706 的 `tarfile.data_filter`。推荐使用 Python 3.12；若使用较旧分支，
至少需要 Python 3.10.12 或 3.11.4。Python 3.10.11 缺少该能力，不能用于完整工具测试验收。

已建立的全新基线如下。后续阶段提交与 `main` 最终提交不在本文件中固化，避免文档提交产生自引用哈希；由
外部交接清单记录，并在目标机从实际仓库核对：

| 批次 | 提交 |
| --- | --- |
| 全新 QGC 基线 | `1ace5716a69da4c5aa9f67da1e35475de5f5951b` |
| 后续阶段与 `main` 最终提交 | 见外部交接清单；用 `git log --oneline --decorate` 核对 |

Qt 模块以 [`.github/build-config.json`](../../.github/build-config.json) 为准，包括 Qt Quick 3D、Qt Location、
Qt Positioning、Qt Multimedia、Qt Graphs、Qt Sensors、Qt Serial Port、Qt WebSockets 等。不要只安装基础桌面套件。

```text
qtgraphs qtlocation qtpositioning qtspeech qtmultimedia qtserialport
qtimageformats qtshadertools qtconnectivity qtquick3d qtsensors
qtscxml qtwebsockets qthttpserver
```

## 首次发布新仓库

`LogicJump/gcs-ultra` 是为本项目重新创建的空仓库。发布流程必须从不含旧 `.git` 数据的独立目录建立
全新 `main` 历史，不继承当前浅克隆的上游提交历史：

```powershell
git init --initial-branch=main
git remote add origin https://github.com/LogicJump/gcs-ultra.git
git status --short
```

先提交完整、可构建的 QGC 基线，再按生产代码、资源、构建修复、测试和工具等连贯阶段提交。每次提交后记录
哈希；确认最终工作区干净后才允许首次推送。

### 首次推送前关闭 Actions

必须在 GitHub 仓库的 **Settings → Actions → General** 中保持 Actions 禁用，并在首次推送前确认仓库仍显示
0 次工作流运行。源代码中保留现有 `.github/workflows/` 和 `.github/actions/`，因为工具契约测试会读取它们；
保留文件不等于启用 Actions。不要新增或启用工作流。

完成基线和阶段提交后再推送：

```powershell
git status --short
git log --oneline --decorate --max-count=20
git push -u origin main
```

推送后再次确认 Actions 仍为禁用状态且运行次数仍为 0。认证只在目标机通过 Git Credential Manager 或
GitHub CLI 完成；不要把 PAT、SSH 私钥、`.git-credentials` 或认证环境变量放进迁移包。

## 首选迁移：从 GitHub 克隆

首次发布完成后，在目标机执行：

```powershell
git clone https://github.com/LogicJump/gcs-ultra.git
Set-Location gcs-ultra
git switch main
git rev-parse HEAD
git status --short
```

`git rev-parse HEAD` 必须与交接清单中的 `main` 最终提交一致，`git status --short` 应为空。

## 离线后备：Git bundle 与依赖包

完成全部批次提交后，在源机器创建并验证 bundle：

```powershell
git bundle create gcs-ultra-main.bundle main --tags
git bundle verify gcs-ultra-main.bundle
Get-FileHash gcs-ultra-main.bundle -Algorithm SHA256
```

在目标机克隆：

```powershell
git clone .\gcs-ultra-main.bundle gcs-ultra
Set-Location gcs-ultra
git switch main
git rev-parse HEAD
```

最终分支提交后，新增 SVG、QML 资源和 Quick3D 工具都会包含在 bundle 中，无需另行复制。若必须在提交完成前
做应急备份，应由打包流程另外保存二进制 Git diff 和未跟踪文件清单；普通 Git bundle 不包含工作区改动。

第三方依赖不要提交到 Git。离线介质应把它们放在独立的 `dependencies/` 目录，并记录文件名、版本、来源、
SHA-256 和目标安装位置。至少准备：

- Qt 6.12.0 `msvc2022_64` 及仓库配置要求的模块；
- GStreamer 安装包 `gstreamer-1.0-msvc-x86_64-1.28.8.exe`；
- uv 0.12.8 或更高的受控安装包，以及与 `tools/uv.lock` 匹配的离线 Python 包缓存；
- Visual Studio 2022 Build Tools 离线布局，包含 MSVC v143 14.44 x64/x86 工具和 Windows SDK；
- Vulkan SDK 或至少包含 `vulkan/vulkan.h` 的头文件目录；
- Qt Quick3D `v6.12.0` 源码种子，包含提交 `a50a2ff960ca58d4b9f87b7e6a7ddae02e620426`，用于完全
  离线地重建隔离补丁运行时。

GStreamer 官方下载地址由仓库生成，Windows x64 包的固定 SHA-256 为：

```text
https://gstreamer.freedesktop.org/data/pkg/windows/1.28.8/msvc/
gstreamer-1.0-msvc-x86_64-1.28.8.exe
e4c41ee6ee9c6c32a45d198463ec08d79bac3bd5fa58adc93d13fe4a08abc9d9
```

下载失败时，仓库会尝试 QGroundControl 的公开 S3 镜像，并使用同一校验值。不要把源机器的已展开缓存目录当作
可移植 SDK。

## 不要复制的内容

不要直接压缩当前 Codex 工作树根目录。特别排除：

- `.git`：当前文件只是指向旧主仓库的 worktree 指针；新的公开仓库必须使用独立的全新历史；
- `build/`、`build-*`：包含生成器状态、编译器路径和源目录绝对路径；
- `tools/.venv/`、`.venv/`：Python 虚拟环境包含解释器和脚本绝对路径；
- `.cache/`、`.ccache/`：CPM、GStreamer、Qt 源码和分析缓存不是稳定交付物；
- `CMakeUserPresets.json`、IDE 用户设置、临时日志和机器专用环境脚本；
- PAT、SSH 私钥、Git 凭据、签名证书及其他认证材料。

最终 Git 分支应包含源代码、测试、CMake 配置、`tools/`、`tools/uv.lock`、翻译、新资源和
`tools/qtquick3d-cleanup/`。依赖安装包与源码 bundle 分开放置。

## 目标机安装

1. 安装 Git、Python 3.12（推荐）以及 Visual Studio 2022 Build Tools。
2. 在 Visual Studio Installer 中选择“使用 C++ 的桌面开发”，并确认 MSVC v143 14.44 x64/x86 工具和
   Windows SDK 已安装。
3. 按 [uv 官方安装说明](https://docs.astral.sh/uv/getting-started/installation/) 安装 uv 0.12.8 或更高版本。
4. 在仓库根目录创建锁定的工具环境：

   ```powershell
   python .\tools\setup\install_python.py dev
   . .\tools\.venv\Scripts\Activate.ps1
   ```

5. 安装 Qt 6.12.0：

   ```powershell
   python .\tools\setup\install_qt.py install --from-config `
       --host windows --arch win64_msvc2022_64 --outdir D:\Qt
   ```

6. 以管理员 PowerShell 安装校验过的 GStreamer 开发套件：

   ```powershell
   python .\tools\setup\install_dependencies --platform windows
   ```

   默认安装位置为 `C:\gstreamer\1.0\msvc_x86_64`。也可以手工安装，然后把
   `GStreamer_ROOT_DIR` 指向不含空格的完整 SDK 路径。

官方平台参考：

- [Visual Studio 2022 系统要求](https://learn.microsoft.com/visualstudio/releases/2022/system-requirements)
- [Qt 6.12 支持的平台](https://doc.qt.io/qt-6.12/supported-platforms.html)
- [GStreamer 下载](https://gstreamer.freedesktop.org/download/)

## 配置、编译和测试

从普通 PowerShell 进入仓库根目录，并显式导入 MSVC 14.44 环境。不要依赖 Visual Studio 当前默认的 v143
小版本：

```powershell
$VsDevCmd = "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
$MsvcEnvironment = & cmd.exe /d /s /c `
    "`"`"$VsDevCmd`" -arch=x64 -host_arch=x64 -vcvars_ver=14.44 >nul && set`""
if ($LASTEXITCODE -ne 0) { throw "无法初始化 MSVC 14.44 环境" }
foreach ($EnvironmentEntry in $MsvcEnvironment) {
    if ($EnvironmentEntry -match '^([^=]+)=(.*)$') {
        [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], "Process")
    }
}

. .\tools\.venv\Scripts\Activate.ps1
$env:QT_DIR = "D:\Qt\6.12.0\msvc2022_64"
$env:QT_ROOT_DIR = $env:QT_DIR
$env:JOBS = "8"
$GStreamerRoot = "C:/gstreamer/1.0/msvc_x86_64"

$env:GST_PLUGIN_SYSTEM_PATH_1_0 = Join-Path $GStreamerRoot "lib/gstreamer-1.0"
$env:GST_PLUGIN_SCANNER_1_0 = Join-Path $GStreamerRoot "libexec/gstreamer-1.0/gst-plugin-scanner.exe"
$GStreamerProxyPath = Join-Path $GStreamerRoot "lib/libproxy"
$RuntimePaths = @(
    (Join-Path $env:QT_DIR "bin")
    (Join-Path $GStreamerRoot "bin")
)
if (Test-Path -LiteralPath $GStreamerProxyPath -PathType Container) {
    $RuntimePaths = @($GStreamerProxyPath) + $RuntimePaths
}
$env:PATH = ($RuntimePaths + @($env:PATH)) -join [IO.Path]::PathSeparator

just info
python .\tools\configure.py --preset default --qt-root $env:QT_DIR -- `
    -DQGC_TEST_ONSCREEN=ON `
    "-DGStreamer_ROOT_DIR=$GStreamerRoot"
just build
```

`tools/configure.py` 和 `just build` 使用仓库的 `default` Ninja 预设，在 `build/` 生成 Debug 测试版本。
`QGC_TEST_ONSCREEN=ON` 是本次 Windows 原生 UI 验证使用的配置；该选项默认是 `OFF`，遗漏后 CTest 会给测试
附加 offscreen 平台，不能复现本次原生窗口结果。显式传入 `GStreamer_ROOT_DIR` 后，配置完成时应核对
`build/CMakeCache.txt` 中的 `QGC_GST_RESOLVED_ROOT` 与 `$GStreamerRoot` 一致。

先完成下文的 Qt Quick3D 隔离补丁并在同一测试 PowerShell 中启用运行时覆盖，再运行完整测试。`just test`
运行 `Unit|Integration`，默认排除 `Flaky|Network`。单个测试可用：

```powershell
ctest --test-dir build --build-config Debug --output-on-failure -R PlanViewUITest
```

工具版本由 uv 锁定环境提供，不要复制源机器的 `tools/.venv`。当前源机器锁定环境解析为 CMake 3.31.10、
Ninja 1.13.0 和 just 1.55.1；目标机应以最终分支中的 `tools/uv.lock` 为准。

### Lint 状态说明

最终改动文件的显式 prek 检查已通过。当前 Windows 主机上的仓库全量 `just lint` 曾因外部
`clang-format.exe` 子进程停滞而无法完成，且钩子可能临时改写无关文件；这不是目标机配置成功与否的判据。
目标机应先在干净分支上运行改动范围检查，再尝试全量检查：

新克隆的 `main` 与 `origin/main` 指向同一提交，以 `origin/main` 为基准会得到空改动列表；这里使用
上述已记录的导入基线，并将两端提交传给 prek，以便格式检查沿用相同改动范围。

```powershell
$base = '1ace5716a69da4c5aa9f67da1e35475de5f5951b'
git merge-base --is-ancestor $base HEAD
if ($LASTEXITCODE -ne 0) { throw "当前提交不包含已记录的导入基线" }
& uv run --frozen --project tools --group precommit prek run `
    --from-ref $base --to-ref HEAD

just lint
```

不要通过跳过钩子或修改超时来制造通过结果。若全量检查再次停滞，保留日志并单独调查工具进程。

## Qt Quick3D 6.12 隔离补丁

Qt 6.12.0 的最后一个 `View3D` 销毁路径存在上游资源清理队列问题。仓库中的
[`tools/qtquick3d-cleanup/`](../qtquick3d-cleanup/) 提供补丁、公开 API 复现程序和隔离构建助手。
它不会覆盖全局 Qt SDK，也不是普通 QGC 编译步骤。

若目标机尚无 Vulkan 头文件，可先以管理员 PowerShell 安装仓库固定的 Vulkan SDK：

```powershell
python .\tools\setup\install_dependencies --platform windows --skip-gstreamer --vulkan
```

```powershell
$QtRoot = "D:\Qt\6.12.0\msvc2022_64"
$Quick3DWork = "D:\qt-work\qtquick3d-cleanup"

& .\tools\qtquick3d-cleanup\build.ps1 `
    -QtRoot $QtRoot `
    -WorkRoot $Quick3DWork `
    -MsvcEnvScript `
        "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" `
    -MsvcEnvArguments "-arch=x64 -host_arch=x64 -vcvars_ver=14.44" `
    -VulkanHeaderRoot "C:\VulkanSDK\latest\Include" `
    -Jobs 8
```

完全离线时添加 `-SourceSeed <qtquick3d-v6.12.0-checkout>`。先用 `-ValidateOnly` 检查 Qt、MSVC、Vulkan
头文件和路径。构建完成后，按
[`tools/qtquick3d-cleanup/README.md`](../qtquick3d-cleanup/README.md) 的 RED/GREEN 步骤验证模块来源。
不要把隔离 DLL 复制到 Qt 安装目录；新 PowerShell 进程会清除临时运行时覆盖环境。

在运行 QGC UI 测试前，把验证通过的隔离运行时覆盖到当前 PowerShell，并保持 GStreamer 的 `bin`、插件
扫描器和 `lib/libproxy` 路径为上一节设置的值：

```powershell
$Quick3DInstall = Join-Path $Quick3DWork "install"
$env:PATH = "$Quick3DInstall\bin;$env:PATH"
$env:QML2_IMPORT_PATH = "$Quick3DInstall\qml;$env:QT_DIR\qml"
$env:QT_QUICK3D_OVERRIDE_ROOT = $Quick3DInstall

$env:QT_FORCE_STDERR_LOGGING = "1"
$env:JOBS = "1"
just test
```

`QT_FORCE_STDERR_LOGGING=1` 让 Windows QtTest 失败详情进入终端及重定向日志；仅设置 `QGC_TEST_VERBOSE` 不足。

不要为这次原生 Windows 验证设置 `QSG_RHI_BACKEND`；Qt 默认使用 D3D11，已修补的 Quick3D DLL 通过上述
进程级覆盖加载。原生 UI 测试在本次 Windows 10 工作站上并行运行会相互争用桌面与图形资源，出现已通过
用例的伪超时，因此完整 `just test` 必须设置 `JOBS=1` 串行执行；这与前面的 `JOBS=8` 并行编译设置相互
独立。个别 CTest 若由仓库显式设置其他渲染后端，仍以该测试自身的环境属性为准。测试结束后关闭这个
PowerShell，即可同时清除 Quick3D 覆盖和运行时 PATH 调整。

## 交接检查清单

- [ ] 新仓库 Actions 在首次推送前已禁用，推送后仍保持 0 次运行。
- [ ] 全新基线和阶段提交已完成，`main` 及每个提交哈希已写入交接清单。
- [ ] GitHub `main` 已推送，或已从独立仓库创建并验证 Git bundle。
- [ ] 上游 `.github` 工作流源码仅作为工具契约输入保留，没有新增或启用工作流。
- [ ] `git status --short` 为空，五个新增工具栏资源已纳入提交。
- [ ] 源码包不含 worktree `.git` 指针、构建目录、虚拟环境、缓存或凭据。
- [ ] 依赖清单记录版本、下载来源、SHA-256、安装位置和许可证文件。
- [ ] 目标机使用 Windows 10 x64、MSVC v143 14.44 和 Qt 6.12.0 `msvc2022_64`。
- [ ] `tools/configure.py`、`just build` 和相关测试已在目标机重新运行。
- [ ] Debug 测试配置包含 `QGC_TEST_ONSCREEN=ON`，GStreamer 运行时 PATH 包含 SDK 的 `bin` 和
      `lib/libproxy`（若存在）。
- [ ] Quick3D 补丁只通过隔离目录构建和验证，没有修改全局 Qt SDK。
- [ ] 原生 D3D11 UI 测试从隔离 Quick3D 前缀加载四个已验证模块，没有复制 DLL 到 Qt SDK。
- [ ] 全量 lint 的已知限制与改动范围 lint 的实际结果已如实记录。
