param(
    [Parameter(Mandatory = $true)]
    [string]$VofaDirectory
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Dll = Join-Path $ProjectRoot "dist\ConfigurableEngine.dll"
$Config = Join-Path $ProjectRoot "configurable_engine.json"
$PluginHelp = Join-Path $ProjectRoot "ConfigurableEngine.json"
$Target = Join-Path $VofaDirectory "plugins\dataengines"

if (-not (Test-Path -LiteralPath $Dll)) { throw "$Dll was not found. Run build.ps1 first." }
if (-not (Test-Path -LiteralPath $Config)) { throw "$Config was not found." }
if (-not (Test-Path -LiteralPath $PluginHelp)) { throw "$PluginHelp was not found." }
if (-not (Test-Path -LiteralPath $VofaDirectory)) { throw "VOFA+ directory does not exist: $VofaDirectory" }

New-Item -ItemType Directory -Force -Path $Target | Out-Null
Copy-Item -LiteralPath $Dll -Destination (Join-Path $Target "ConfigurableEngine.dll") -Force
Copy-Item -LiteralPath $Config -Destination (Join-Path $Target "configurable_engine.json") -Force
Copy-Item -LiteralPath $PluginHelp -Destination (Join-Path $Target "ConfigurableEngine.json") -Force
Write-Host "Installed to $Target. Restart VOFA+."
