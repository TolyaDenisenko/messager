<#
 Собирает проект, раскладывает Qt-библиотеки и строит MSI агента (WiX 3.x).
 Пример:  .\installer\build-msi.ps1 -SharedKey "МойСекретныйКлюч"
 Если ключ не указан — генерируется случайный и сохраняется в dist\sharedkey.txt.
#>
param(
    [string]$SharedKey,
    [string]$WixDir   = "$PSScriptRoot\wix314",       # папка с candle.exe / light.exe / heat.exe
    [string]$QtDir    = "C:\Qt\6.11.2\mingw_64",
    [string]$MingwDir = "C:\Qt\Tools\mingw1310_64",
    [string]$NinjaDir = "C:\Qt\Tools\Ninja"
)
$ErrorActionPreference = 'Stop'
$root  = Split-Path $PSScriptRoot
$build = Join-Path $root 'build'
$dist  = Join-Path $root 'dist'
$env:PATH = "$MingwDir\bin;$NinjaDir;$QtDir\bin;$env:PATH"

if (-not $SharedKey) {
    $b = New-Object byte[] 18
    [Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($b)
    $SharedKey = [Convert]::ToBase64String($b).Replace('+','A').Replace('/','B')
}

Write-Host '== Сборка' -ForegroundColor Cyan
cmake -S $root -B $build -G Ninja -Wno-dev -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtDir" -DCMAKE_CXX_COMPILER="$MingwDir\bin\g++.exe"
if ($LASTEXITCODE) { throw 'cmake configure failed' }
cmake --build $build
if ($LASTEXITCODE) { throw 'cmake build failed' }

Write-Host '== Подготовка файлов' -ForegroundColor Cyan
Remove-Item $dist -Recurse -Force -ErrorAction SilentlyContinue
$payload = Join-Path $dist 'agent-payload'
$sender  = Join-Path $dist 'LanMessengerSender'
New-Item -ItemType Directory $payload, $sender | Out-Null

# Агент: UI-хелпер + Qt-библиотеки (Widgets, Network, платформа) в одной папке со службой
Copy-Item "$build\agentui\LanMessengerUI.exe" $payload
Copy-Item "$build\agent\LanMessengerAgent.exe" $payload
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$payload\LanMessengerUI.exe"
windeployqt --release --no-translations "$payload\LanMessengerAgent.exe"
$agentExe = Join-Path $dist 'LanMessengerAgent.exe'
Move-Item "$payload\LanMessengerAgent.exe" $agentExe   # служба описана в Product.wxs отдельным компонентом

# Отправитель для ПК (портативная папка)
Copy-Item "$build\sender\LanMessengerSender.exe" $sender
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw "$sender\LanMessengerSender.exe"

Write-Host '== MSI' -ForegroundColor Cyan
foreach ($t in 'heat','candle','light') {
    if (-not (Test-Path "$WixDir\$t.exe")) { throw "Не найден $WixDir\$t.exe — скачайте WiX Toolset 3.14 (binaries) и распакуйте в $WixDir" }
}
$obj = Join-Path $dist 'obj'; New-Item -ItemType Directory $obj | Out-Null
& "$WixDir\heat.exe" dir $payload -cg QtFiles -dr INSTALLFOLDER -srd -sreg -gg -g1 -sfrag -var var.PayloadDir -out "$obj\payload.wxs"
if ($LASTEXITCODE) { throw 'heat failed' }
& "$WixDir\candle.exe" -nologo -arch x64 -ext WixFirewallExtension "-dSharedKey=$SharedKey" "-dPayloadDir=$payload" "-dAgentExe=$agentExe" -out "$obj\" "$PSScriptRoot\Product.wxs" "$obj\payload.wxs"
if ($LASTEXITCODE) { throw 'candle failed' }
$msi = Join-Path $dist 'LanMessengerAgent.msi'
& "$WixDir\light.exe" -nologo -sval -ext WixFirewallExtension -out $msi "$obj\Product.wixobj" "$obj\payload.wixobj"
if ($LASTEXITCODE) { throw 'light failed' }

Set-Content (Join-Path $dist 'sharedkey.txt') $SharedKey
Write-Host "`nГотово: $msi" -ForegroundColor Green
Write-Host "Ключ (введите его в отправителе): $SharedKey"
Write-Host "Отправитель для ПК: $sender"
