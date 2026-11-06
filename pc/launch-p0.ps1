$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path -Parent $PSScriptRoot
$env:Path = "$workspaceRoot/work/toolchain/portable/bin;$env:Path"
$exe = Join-Path $PSScriptRoot 'build-runtime/STT2001_P0_Japanese_baseline.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw 'Build P0 Runtime first.' }
& $exe --game "$PSScriptRoot/game.toml" --bios "$workspaceRoot/reference/psxrecomp/bios/openbios.bin" --memcard-dir "$PSScriptRoot/saves"
