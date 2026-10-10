# Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
param([string]$QtPrefix = '', [switch]$Package, [string]$PackageDir = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$QtPrefix) { $QtPrefix = Join-Path $projectRoot '.tools\Qt\6.8.3\mingw_64' }
if (!(Test-Path -LiteralPath (Join-Path $QtPrefix 'lib\cmake\Qt6'))) {
    throw 'Qt 6 not found. Pass -QtPrefix <your Qt kit directory>.'
}
$compilerBin = Join-Path $projectRoot '.tools\Qt\Tools\mingw1310_64\bin'
if (Test-Path -LiteralPath $compilerBin) { $env:PATH = "$compilerBin;$env:PATH" }
$env:PATH = "$(Join-Path $QtPrefix 'bin');$env:PATH"
$buildPath = Join-Path $projectRoot 'build'
& cmake -S $projectRoot -B $buildPath -G 'MinGW Makefiles' "-DCMAKE_PREFIX_PATH=$QtPrefix" '-DCMAKE_BUILD_TYPE=Release'
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
& cmake --build $buildPath --parallel 4
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
& ctest --test-dir $buildPath --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
if ($Package) {
    $projectDefinition = Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
    if ($projectDefinition -notmatch 'project\(OrangeStudy VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Project version not found.' }
    $releaseVersion = $Matches[1]
    $releasePath = if ($PackageDir) { [System.IO.Path]::GetFullPath($PackageDir) } else { Join-Path $projectRoot "release\OrangeStudy-$releaseVersion" }
    New-Item -ItemType Directory -Force $releasePath | Out-Null
    Copy-Item -LiteralPath (Join-Path $buildPath 'OrangeStudy.exe'), (Join-Path $buildPath 'orange-cli.exe') -Destination $releasePath
    & (Join-Path $QtPrefix 'bin\windeployqt.exe') --release --no-translations --no-opengl-sw (Join-Path $releasePath 'OrangeStudy.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Qt deployment failed.' }
    Copy-Item -LiteralPath (Join-Path $projectRoot 'LICENSE'), (Join-Path $projectRoot 'NOTICE'), (Join-Path $projectRoot 'README.md'), (Join-Path $projectRoot 'THIRD_PARTY_NOTICES.md') -Destination $releasePath
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\licenses') -Destination (Join-Path $releasePath 'third-party-licenses') -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\MicroTeX\LICENSE') -Destination (Join-Path $releasePath 'third-party-licenses\MicroTeX-MIT.txt')
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\tinyxml2\LICENSE.txt') -Destination (Join-Path $releasePath 'third-party-licenses\tinyxml2-zlib.txt')
    Copy-Item -LiteralPath (Join-Path $projectRoot 'third_party\MicroTeX\res\fonts\licences') -Destination (Join-Path $releasePath 'third-party-licenses\math-fonts') -Recurse -Force
    $qtSourcePath = Join-Path $projectRoot '.tools\sources\qtbase-everywhere-src-6.8.3.tar.xz'
    if (Test-Path -LiteralPath $qtSourcePath) {
        $checksumPath = "$qtSourcePath.sha256"
        if (!(Test-Path -LiteralPath $checksumPath)) { throw 'Qt source checksum file is missing.' }
        $expectedHash = ((Get-Content -LiteralPath $checksumPath -Raw).Trim() -split '\s+')[0]
        if ((Get-FileHash -LiteralPath $qtSourcePath -Algorithm SHA256).Hash -ne $expectedHash) {
            throw 'Qt source checksum mismatch; source archive may be incomplete.'
        }
        $sourcesPath = Join-Path $releasePath 'third-party-sources'
        New-Item -ItemType Directory -Force $sourcesPath | Out-Null
        Copy-Item -LiteralPath $qtSourcePath, $checksumPath -Destination $sourcesPath
    }
    $portableLauncher = "@echo off`r`nstart `"Orange Study`" /D `"%~dp0`" `"%~dp0OrangeStudy.exe`" --data-dir `"%~dp0data`"`r`n"
    [System.IO.File]::WriteAllText((Join-Path $releasePath 'Start.cmd'), $portableLauncher, [System.Text.Encoding]::ASCII)
    Write-Host "Packaged: $releasePath"
}
