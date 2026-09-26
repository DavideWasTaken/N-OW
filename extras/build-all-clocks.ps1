$ErrorActionPreference = 'Stop'

$project = Split-Path -Parent $PSScriptRoot
$arduinoCliCommand = Get-Command arduino-cli -ErrorAction SilentlyContinue
if ($arduinoCliCommand) {
  $arduinoCli = $arduinoCliCommand.Source
} else {
  $wingetPackages = Join-Path $env:LOCALAPPDATA 'Microsoft\WinGet\Packages'
  $arduinoCli = Get-ChildItem -Path $wingetPackages `
    -Filter arduino-cli.exe -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object FullName -Like '*ArduinoSA.IDE.stable*' |
    Select-Object -First 1 -ExpandProperty FullName
}
$fqbn = 'esp32:esp32:esp32s3:CDCOnBoot=cdc,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,UploadSpeed=115200'
$profiles = @(
  @{ Clock = 1; Speed = 1; Buttons = 'ON'; Sync = 'MASTER' },
  @{ Clock = 2; Speed = 2; Buttons = 'OFF'; Sync = 'RECEIVER' },
  @{ Clock = 3; Speed = 8; Buttons = 'OFF'; Sync = 'RECEIVER' },
  @{ Clock = 4; Speed = 40; Buttons = 'OFF'; Sync = 'RECEIVER' },
  @{ Clock = 5; Speed = 250; Buttons = 'OFF'; Sync = 'RECEIVER' }
)

if (-not $arduinoCli -or -not (Test-Path -LiteralPath $arduinoCli)) {
  throw 'arduino-cli was not found on PATH or in a standard Arduino IDE WinGet installation.'
}

$profilesRoot = Join-Path $project 'build-profiles'
$workRoot = Join-Path $profilesRoot '_work'
New-Item -ItemType Directory -Force -Path $profilesRoot | Out-Null
New-Item -ItemType Directory -Force -Path $workRoot | Out-Null
$manifest = @()

foreach ($profile in $profiles) {
  $name = "clock-$($profile.Clock)-$($profile.Speed)x"
  $output = Join-Path $profilesRoot $name
  $buildPath = Join-Path $workRoot $name
  $log = Join-Path $profilesRoot "$name-compile.log"
  New-Item -ItemType Directory -Force -Path $output | Out-Null
  New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

  & $arduinoCli compile --clean --fqbn $fqbn `
    --build-property "compiler.cpp.extra_flags=-DNOW_CLOCK_PROFILE=$($profile.Clock)" `
    --build-path $buildPath `
    --output-dir $output $project *> $log
  if ($LASTEXITCODE -ne 0) {
    Get-Content -LiteralPath $log
    throw "Compile failed for $name"
  }
  Copy-Item -LiteralPath (Join-Path $buildPath 'build.options.json') `
    -Destination (Join-Path $output 'build.options.json') -Force

  $merged = Join-Path $output 'NOW_Timer.ino.merged.bin'
  if (-not (Test-Path -LiteralPath $merged)) {
    throw "Merged firmware missing for $name"
  }
  $file = Get-Item -LiteralPath $merged
  $manifest += [pscustomobject]@{
    Clock = $profile.Clock
    Speed = "$($profile.Speed)x"
    Buttons = $profile.Buttons
    Sync = $profile.Sync
    Firmware = "$name\NOW_Timer.ino.merged.bin"
    Bytes = $file.Length
    SHA256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $merged).Hash
  }
}

$manifest | ConvertTo-Json | Set-Content `
  -LiteralPath (Join-Path $profilesRoot 'manifest.json') -Encoding utf8
$manifest | Format-Table -AutoSize
