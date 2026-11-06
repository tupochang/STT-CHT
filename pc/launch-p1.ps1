param([switch]$Headless, [ValidateRange(0,60)][int]$StopAfterSeconds = 0)
$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path -Parent $PSScriptRoot
$env:Path = "$workspaceRoot/work/toolchain/portable/bin;$env:Path"
$exe = "$PSScriptRoot/build-runtime/STT2001_PC_Chinese_Paged.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw 'Run pc/build-p1.ps1 first.' }
$arguments = @('--renderer','opengl','--game',('"'+$PSScriptRoot+'/game.toml"'),
    '--bios',('"'+$workspaceRoot+'/reference/psxrecomp/bios/openbios.bin"'),
    '--memcard-dir',('"'+$PSScriptRoot+'/saves-p1"'))
if ($Headless) { $arguments += '--headless' }
# Redirect at process creation: Windows PowerShell 5.1 otherwise turns normal
# stderr diagnostics into terminating NativeCommandError under Stop + 2>&1.
$options = @{
    FilePath = $exe
    ArgumentList = ($arguments -join ' ')
    WorkingDirectory = $PSScriptRoot
    RedirectStandardOutput = "$PSScriptRoot/build-runtime/p1-session.log"
    RedirectStandardError = "$PSScriptRoot/build-runtime/p1-session.stderr.log"
    PassThru = $true
}
if ($Headless) { $options.WindowStyle = 'Hidden' } else { $options.NoNewWindow = $true }
$game = Start-Process @options
# Cache the handle before waiting; Windows PowerShell 5.1 can otherwise expose
# a null ExitCode after Start-Process even when the child exited normally.
$processHandle = $game.Handle
if ($StopAfterSeconds -gt 0 -and -not $game.WaitForExit($StopAfterSeconds * 1000)) {
    Stop-Process -Id $game.Id
    Write-Output "Controlled test stopped after $StopAfterSeconds seconds."
    exit 0
}
$game.WaitForExit()
$game.Refresh()
$exitCode = $game.ExitCode
if ($null -eq $exitCode) { throw 'Unable to read P1 process exit status.' }
if ($exitCode -ne 0) {
    Get-Content -LiteralPath $options.RedirectStandardError -Tail 12
    throw "P1 exited with code $exitCode. See p1-session.stderr.log."
}
