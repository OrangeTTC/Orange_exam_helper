# Copyright 2026 Orange. SPDX-License-Identifier: Apache-2.0
# JSON 导入与界面共用项目根目录的 data 题库。
param([Parameter(Mandatory=$true)][string]$JsonFile)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$cliPath = Join-Path $projectRoot 'release\OrangeStudy-1.4.1\orange-cli.exe'
if (!(Test-Path -LiteralPath $cliPath)) { throw 'Run scripts/build.ps1 -Package first.' }
& $cliPath --data-dir (Join-Path $projectRoot 'data') import $JsonFile
if ($LASTEXITCODE -ne 0) { throw 'Question import failed; see the error above.' }
