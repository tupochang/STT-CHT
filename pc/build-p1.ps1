$ErrorActionPreference = 'Stop'
$workspaceRoot = Split-Path -Parent $PSScriptRoot
$env:Path = "$workspaceRoot/work/toolchain/portable/bin;$env:Path"
& py -3 "$PSScriptRoot/p1/export_dialogue.py"
if ($LASTEXITCODE -ne 0) { throw 'P1 data export failed' }
& cmake -S $PSScriptRoot -B "$PSScriptRoot/build-runtime" -DSTT_P1=ON -DPSX_REWIND=OFF
if ($LASTEXITCODE -ne 0) { throw 'P1 configure failed' }
& cmake --build "$PSScriptRoot/build-runtime" --target psx-runtime -j2
if ($LASTEXITCODE -ne 0) { throw 'P1 build failed' }
