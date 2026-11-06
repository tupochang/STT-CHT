param([ValidateSet('Emitters','Generate','Runtime')][string]$Step = 'Emitters')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$workspaceRoot = Split-Path -Parent $projectRoot
$frameworkRoot = Join-Path $workspaceRoot 'reference/psxrecomp'
$toolchainRoot = Join-Path $workspaceRoot 'work/toolchain/portable'
$env:Path = "$toolchainRoot/bin;$env:Path"
$env:PSXRECOMP_TOOLCHAIN_DIR = $toolchainRoot
$env:RETCOMM_TOOLCHAIN_DIR = $toolchainRoot
$emitterBuild = Join-Path $projectRoot 'build-emitters'
function Run-Native([string]$File, [string[]]$Arguments) {
    & $File @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$File failed with exit $LASTEXITCODE" }
}
if ($Step -eq 'Emitters') {
    Run-Native 'cmake' @('-S', "$frameworkRoot/recompiler", '-B', $emitterBuild, '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++',
        '-DPSXRECOMP_ENABLE_CHD=OFF', '-DPSXRECOMP_STATIC_CLI=ON', '-DBUILD_TESTING=OFF')
    Run-Native 'cmake' @('--build', $emitterBuild, '--target', 'psxrecomp-game', 'psxrecomp-bios', '-j2')
} elseif ($Step -eq 'Generate') {
    Push-Location $frameworkRoot
    try { Run-Native "$emitterBuild/psxrecomp-bios.exe" @('--config', 'bios/OpenBIOS.toml') }
    finally { Pop-Location }
    Push-Location $projectRoot
    try { Run-Native "$emitterBuild/psxrecomp-game.exe" @('--config', 'game.toml') }
    finally { Pop-Location }
} else {
    Run-Native 'cmake' @('-S', $projectRoot, '-B', "$projectRoot/build-runtime", '-G', 'Ninja',
        '-DCMAKE_BUILD_TYPE=Release', '-DCMAKE_C_COMPILER=clang', '-DCMAKE_CXX_COMPILER=clang++',
        '-DPSXRECOMP_REQUIRE_GAME_C=ON', '-DPSX_REWIND=OFF')
    Run-Native 'cmake' @('--build', "$projectRoot/build-runtime", '--target', 'psx-runtime', '-j2')
}
