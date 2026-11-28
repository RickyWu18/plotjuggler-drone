<#
.SYNOPSIS
Build the PJ4 drone plugins (PowerShell counterpart of build.sh).

.DESCRIPTION
  ./build.ps1                         Release build, all plugins  -> build/all/
  ./build.ps1 <plugin> [<plugin>...]  Only the named plugins (dirs under plugins/)
                                      -> build/<plugin>[+<plugin>...]/
  ./build.ps1 -Debug [...]            Debug build                 -> <the above>/debug/
  ./build.ps1 -SdkPath DIR [...]      Build against the unreleased plotjuggler_sdk checked out at DIR
                                      (conan create as plotjuggler_sdk/<DIR/VERSION>@local/dev)
Plugins land in <build dir>/bin/.
#>
$ErrorActionPreference = 'Stop'
$Root = $PSScriptRoot
$Usage = 'Usage: ./build.ps1 [-Debug] [-SdkPath DIR] [plugin ...]'

# Arguments are parsed by hand (like build.sh): a param() block would bind the first plugin name positionally,
# and a [Parameter] attribute would add PowerShell's own -Debug common parameter.
$Debug = $false
$SdkPath = ''
$Plugin = @()
for ($i = 0; $i -lt $args.Count; $i++) {
    switch -Regex ($args[$i]) {
        '^-Debug$' { $Debug = $true; break }
        '^-SdkPath$' {
            if ($i + 1 -ge $args.Count) { throw $Usage }
            $i++
            $SdkPath = $args[$i]
            break
        }
        '^-' { throw $Usage }
        default { $Plugin += $args[$i] }
    }
}

function Invoke-Native {
    # Run a native command and stop on a non-zero exit code (PowerShell does not do this by itself).
    param([string]$Exe, [string[]]$Arguments)
    # Windows PowerShell 5.1 turns anything a native tool writes to stderr into an error under 'Stop'.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $Exe @Arguments } finally { $ErrorActionPreference = $previous }
    if ($LASTEXITCODE -ne 0) { throw "$Exe failed with exit code $LASTEXITCODE" }
}

$BuildType = if ($Debug) { 'Debug' } else { 'Release' }

foreach ($p in $Plugin) {
    if (-not (Test-Path (Join-Path $Root "plugins/$p/CMakeLists.txt"))) { throw "Unknown plugin: $p" }
}

if ($SdkPath) {
    if (-not ((Test-Path (Join-Path $SdkPath 'conanfile.py')) -and (Test-Path (Join-Path $SdkPath 'VERSION')))) {
        throw "Not a plotjuggler_sdk checkout (conanfile.py/VERSION missing): $SdkPath"
    }
    $SdkPath = (Resolve-Path $SdkPath).Path
}

$PjPlugins = ''
$Scope = 'all'
if ($Plugin.Count -gt 0) {
    $PjPlugins = $Plugin -join ';'
    $Scope = $Plugin -join '+'
}
$BuildDir = Join-Path $Root "build/$Scope"
if ($Debug) { $BuildDir = Join-Path $BuildDir 'debug' }

# Read by conanfile.py (fetch only the selected plugins' deps) and passed to CMake below.
$env:BUILD_TYPE = $BuildType
$env:PJ_PLUGINS = $PjPlugins
Remove-Item Env:PJ_SDK_REF -ErrorAction SilentlyContinue

if ($SdkPath) {
    # Local SDK: build it into the cache under @local/dev so it never shadows a released package.
    $SdkVer = (Get-Content (Join-Path $SdkPath 'VERSION') -Raw).Trim()
    $env:PJ_SDK_REF = "plotjuggler_sdk/$SdkVer@local/dev"
    Write-Host "build.ps1: building $($env:PJ_SDK_REF) from $SdkPath"
    Invoke-Native conan @('create', $SdkPath, '--user=local', '--channel=dev',
        '-s', "build_type=$BuildType", '-s', 'compiler.cppstd=20',
        '--build=plotjuggler_sdk/*', '--build=missing')
}

# Otherwise plotjuggler_sdk/<SDK_VERSION> is resolved from the plotjuggler-conan remote by `conan install`.
Invoke-Native conan @('install', $Root, "--output-folder=$BuildDir", '--build=missing',
    '-s', "build_type=$BuildType", '-s', 'compiler.cppstd=20')

$CmakeArgs = @('-S', $Root, '-B', $BuildDir,
    "-DCMAKE_TOOLCHAIN_FILE=$(Join-Path $BuildDir 'conan_toolchain.cmake')",
    "-DCMAKE_BUILD_TYPE=$BuildType", "-DPJ_PLUGINS=$PjPlugins")

if (Get-Command ccache -ErrorAction SilentlyContinue) {
    $CmakeArgs += @('-DCMAKE_C_COMPILER_LAUNCHER=ccache', '-DCMAKE_CXX_COMPILER_LAUNCHER=ccache')
}
# Conan's toolchain targets the Visual Studio generator on Windows; Ninja elsewhere (pwsh on Linux/macOS).
$OnWindows = ($PSVersionTable.PSVersion.Major -lt 6) -or $IsWindows
if (-not $OnWindows -and (Get-Command ninja -ErrorAction SilentlyContinue)) {
    $CmakeArgs = @('-G', 'Ninja') + $CmakeArgs
}

Invoke-Native cmake $CmakeArgs
Invoke-Native cmake @('--build', $BuildDir, '--config', $BuildType, '-j', [Environment]::ProcessorCount)
