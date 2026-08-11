param(
    [ValidateSet("x64", "x86")]
    [string]$Architecture = "x64",
    [string]$QtRoot = $env:QT_ROOT,
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

if ($QtRoot) {
    $Qmake = Join-Path $QtRoot "bin\qmake.exe"
} else {
    $QmakeCommand = Get-Command qmake.exe -ErrorAction SilentlyContinue
    if ($QmakeCommand) {
        $Qmake = $QmakeCommand.Source
        $QtRoot = Split-Path -Parent (Split-Path -Parent $Qmake)
    }
}

if (-not $Qmake -or -not (Test-Path -LiteralPath $Qmake)) {
    throw "qmake.exe was not found. Pass -QtRoot, set QT_ROOT, or add the selected Qt kit's bin directory to PATH."
}

$VsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path -LiteralPath $VsWhere)) {
    throw "vswhere.exe was not found."
}

$VsCandidates = @(& $VsWhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
$Toolsets = foreach ($Candidate in $VsCandidates) {
    $ToolsRoot = Join-Path $Candidate "VC\Tools\MSVC"
    if (-not (Test-Path -LiteralPath $ToolsRoot)) { continue }
    foreach ($Toolset in (Get-ChildItem -LiteralPath $ToolsRoot -Directory)) {
        $Compiler = Join-Path $Toolset.FullName "bin\Hostx64\x64\cl.exe"
        if (Test-Path -LiteralPath $Compiler) {
            [PSCustomObject]@{ VsPath = $Candidate; Version = $Toolset.Name }
        }
    }
}
$SelectedToolset = $Toolsets | Where-Object { $_.Version -like "14.16.*" } | Select-Object -First 1
if (-not $SelectedToolset) {
    $SelectedToolset = $Toolsets | Where-Object { $_.Version -like "14.2*" } |
        Sort-Object Version -Descending | Select-Object -First 1
}
if (-not $SelectedToolset) { throw "No MSVC 2017/2019 compiler toolset was found." }
$VsPath = $SelectedToolset.VsPath
$VcVarsVersion = ($SelectedToolset.Version -split '\.')[0..1] -join '.'
$VsDevCmd = Join-Path $VsPath "Common7\Tools\VsDevCmd.bat"
$BuildRoot = Join-Path $ProjectRoot "build\$Architecture"
$PluginBuild = Join-Path $BuildRoot "plugin"
$TestBuild = Join-Path $BuildRoot "tests"
$SmokeBuild = Join-Path $BuildRoot "smoke"
New-Item -ItemType Directory -Force -Path $PluginBuild, $TestBuild, $SmokeBuild | Out-Null

function Invoke-MsvcBuild([string]$WorkingDirectory, [string]$ProjectFile) {
    $MsvcArchitecture = if ($Architecture -eq "x64") { "amd64" } else { "x86" }
    $commands = @(
        "call `"$VsDevCmd`" -arch=$MsvcArchitecture -host_arch=amd64 -vcvars_ver=$VcVarsVersion",
        "cd /d `"$WorkingDirectory`"",
        "`"$Qmake`" `"$ProjectFile`" CONFIG+=release CONFIG-=debug",
        "nmake /NOLOGO"
    ) -join " && "
    & cmd.exe /d /s /c $commands
    if ($LASTEXITCODE -ne 0) { throw "Build failed with exit code $LASTEXITCODE" }
}

Write-Host "Using MSVC toolset $($SelectedToolset.Version) from $VsPath"
Invoke-MsvcBuild $PluginBuild (Join-Path $ProjectRoot "configurable_engine.pro")

if ($RunTests) {
    Invoke-MsvcBuild $TestBuild (Join-Path $ProjectRoot "tests\parser_tests.pro")
    Invoke-MsvcBuild $SmokeBuild (Join-Path $ProjectRoot "tests\plugin_smoke.pro")
    $TestExe = Join-Path $ProjectRoot "dist\parser_tests.exe"
    $SmokeExe = Join-Path $ProjectRoot "dist\plugin_smoke.exe"
    $PluginDll = Join-Path $ProjectRoot "dist\ConfigurableEngine.dll"
    $ConfigJson = Join-Path $ProjectRoot "configurable_engine.json"
    $env:PATH = "$(Join-Path $QtRoot 'bin');$env:PATH"
    & $TestExe
    if ($LASTEXITCODE -ne 0) { throw "Tests failed with exit code $LASTEXITCODE" }
    & $SmokeExe $PluginDll $ConfigJson
    if ($LASTEXITCODE -ne 0) { throw "Plugin smoke test failed with exit code $LASTEXITCODE" }
}

Write-Host "Build complete: $(Join-Path $ProjectRoot 'dist\ConfigurableEngine.dll')"
