#Requires -Version 7
param(
    [Parameter(Mandatory)][string]$EngineRoot,
    [Parameter(Mandatory)][string]$WorkRoot,
    [int]$ExpectedTests = 19
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$work = [IO.Path]::GetFullPath($WorkRoot)
if (Test-Path -LiteralPath $work) { throw 'Use a new, short WorkRoot directory for each clean build.' }
if (-not (Test-Path -LiteralPath "$EngineRoot/Engine/Build/BatchFiles/RunUAT.bat")) { throw 'Unreal Engine installation not found.' }
New-Item -ItemType Directory -Path $work | Out-Null
$built = Join-Path $work 'Built'
& "$EngineRoot/Engine/Build/BatchFiles/RunUAT.bat" BuildPlugin `
    -Plugin="$repo/SyncShield.uplugin" -Package="$built" -TargetPlatforms=Win64 -Rocket -installed `
    *> "$work/BuildPlugin.log"
if ($LASTEXITCODE -ne 0) { throw "BuildPlugin failed. See $work/BuildPlugin.log" }

# Load the actual built artifact in a Blueprint-only host. No development project
# or pre-existing commercial plugin can satisfy a missing module in this check.
$hostDir = Join-Path $work 'Host'
$pluginDir = Join-Path $hostDir 'Plugins/SyncShield'
New-Item -ItemType Directory -Path $pluginDir -Force | Out-Null
Copy-Item -Path "$built/*" -Destination $pluginDir -Recurse
$project = Join-Path $hostDir 'CoreTest.uproject'
@{ FileVersion = 3; EngineAssociation = '5.7'; Plugins = @(
    @{ Name = 'SyncShield'; Enabled = $true },
    @{ Name = 'Fab'; Enabled = $false }
) } |
    ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $project -Encoding utf8NoBOM
$reportDir = Join-Path $work 'Report'
& "$EngineRoot/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" $project `
    -ExecCmds='Automation RunTests SyncShield; Quit' -unattended -nopause -nullrhi -nosplash `
    -NoLiveCoding -nosound -stdout -FullStdOutLogOutput -ReportExportPath="$reportDir" *> "$work/Automation.log"
if ($LASTEXITCODE -ne 0) { throw "Editor failed. See $work/Automation.log" }
if (-not (Test-Path -LiteralPath "$reportDir/index.json")) { throw 'No automation report: the plugin may not have loaded.' }
$report = Get-Content -LiteralPath "$reportDir/index.json" -Raw | ConvertFrom-Json
$ran = $report.succeeded + $report.succeededWithWarnings + $report.failed
if ($report.failed -gt 0 -or $ran -ne $ExpectedTests -or $report.notRun -gt 0 -or $report.inProcess -gt 0) {
    throw "Unexpected test result: ran=$ran failed=$($report.failed), expected=$ExpectedTests. See $reportDir/index.json"
}
Write-Host "PASS: BuildPlugin and $ran automation tests against the packaged plugin. Reports: $work"
