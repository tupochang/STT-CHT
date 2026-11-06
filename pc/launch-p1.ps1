$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path -Parent $PSScriptRoot
$env:Path = "$workspaceRoot/work/toolchain/portable/bin;$env:Path"
$exe = "$PSScriptRoot/build-runtime/STT2001_P1_NativeText.exe"
if (-not (Test-Path -LiteralPath $exe)) { throw 'Run pc/build-p1.ps1 first.' }
& $exe --renderer opengl --game "$PSScriptRoot/game.toml" --bios "$workspaceRoot/reference/psxrecomp/bios/openbios.bin" --memcard-dir "$PSScriptRoot/saves-p1" 2>&1 | Tee-Object -FilePath "$PSScriptRoot/build-runtime/p1-session.log"
