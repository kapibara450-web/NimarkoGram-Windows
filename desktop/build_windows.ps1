$ErrorActionPreference = 'Stop'

$desktopRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $desktopRoot

function Find-CMake {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    )
    return $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

$cmake = Find-CMake
if ([string]::IsNullOrWhiteSpace($cmake)) {
    throw 'CMake не найден. Установи Visual Studio 2022 с workload Desktop development with C++ или отдельный CMake.'
}
Write-Host "Using CMake: $cmake"

$vcpkgRoot = $env:VCPKG_ROOT
if ([string]::IsNullOrWhiteSpace($vcpkgRoot) -and (Test-Path 'C:\vcpkg\scripts\buildsystems\vcpkg.cmake')) {
    $vcpkgRoot = 'C:\vcpkg'
}
if ([string]::IsNullOrWhiteSpace($vcpkgRoot)) {
    Write-Warning 'VCPKG_ROOT is not set. TDLib may not find OpenSSL and zlib.'
}

if (-not [string]::IsNullOrWhiteSpace($vcpkgRoot)) {
    & "$vcpkgRoot\vcpkg.exe" install openssl:x64-windows zlib:x64-windows qtbase:x64-windows qtmultimedia:x64-windows
}

$qtRoot = $env:QT_ROOT
if ([string]::IsNullOrWhiteSpace($qtRoot)) {
    $qtCandidates = @(
        'C:\Qt\6.8.3\msvc2022_64',
        'C:\Qt\6.8.2\msvc2022_64',
        'C:\Qt\6.7.3\msvc2022_64',
        'C:\Qt\6.6.3\msvc2019_64'
    )
    if (-not [string]::IsNullOrWhiteSpace($vcpkgRoot)) {
        $qtCandidates += (Join-Path $vcpkgRoot 'installed\x64-windows\tools\qt6')
    }
    $qtRoot = $qtCandidates | Where-Object { Test-Path (Join-Path $_ 'bin\windeployqt.exe') } | Select-Object -First 1
}
if ([string]::IsNullOrWhiteSpace($qtRoot)) {
    throw 'Qt не найден. Установи Qt MSVC 64-bit или задай QT_ROOT, например C:\Qt\6.8.3\msvc2022_64.'
}
$env:PATH = "$qtRoot\bin;$env:PATH"

$generator = 'Visual Studio 17 2022'
$generatorArgs = @('-G', $generator, '-A', 'x64')
if (Get-Command ninja -ErrorAction SilentlyContinue) {
    $generatorArgs = @('-G', 'Ninja')
}

$configureArgs = @(
    '-S', '.',
    '-B', 'build',
    '-DCMAKE_BUILD_TYPE=Release',
    "-DCMAKE_PREFIX_PATH=$qtRoot"
) + $generatorArgs
if (-not [string]::IsNullOrWhiteSpace($vcpkgRoot)) {
    $configureArgs += "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake"
}

$tdlibRoot = $env:TDLIB_ROOT
if (-not [string]::IsNullOrWhiteSpace($tdlibRoot)) {
    $configureArgs += "-DNIMARKOGRAM_TDLIB_SOURCE_DIR=$tdlibRoot"
} else {
    $configureArgs += '-DNIMARKOGRAM_FETCH_TDLIB=ON'
}

Write-Host "Configuring with $generator..."
& $cmake @configureArgs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'Building...'
& $cmake --build build --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$windeployQt = Get-Command windeployqt -ErrorAction SilentlyContinue
if ($null -eq $windeployQt) {
    throw 'windeployqt не найден. Добавь папку Qt bin в PATH или задай QT_ROOT.'
}

Write-Host 'Deploying Qt runtime...'
windeployqt --release build/NimarkoGram.exe

$archive = Join-Path $desktopRoot 'NimarkoGram-windows-portable.zip'
if (Test-Path $archive) { Remove-Item $archive -Force }
Compress-Archive -Path 'build/*' -DestinationPath $archive -Force
Write-Host "Created $archive"
