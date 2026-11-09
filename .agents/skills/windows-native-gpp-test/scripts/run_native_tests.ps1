[CmdletBinding()]
param(
    [switch]$Install,
    [string[]]$Environment = @()
)

$ErrorActionPreference = 'Stop'
$gccDirectory = 'C:\msys64\ucrt64\bin'
$gccExecutable = Join-Path $gccDirectory 'g++.exe'
$bashExecutable = 'C:\msys64\usr\bin\bash.exe'

function Resolve-Gpp {
    $command = Get-Command g++ -ErrorAction SilentlyContinue
    if ($null -ne $command) { return $command.Source }
    if (Test-Path -LiteralPath $gccExecutable) { return $gccExecutable }
    return $null
}

$gpp = Resolve-Gpp
if ($null -eq $gpp -and $Install) {
    winget install --id MSYS2.MSYS2 --exact --source winget --silent `
        --accept-package-agreements --accept-source-agreements --disable-interactivity
    if ($LASTEXITCODE -ne 0) { throw "MSYS2 installation failed: $LASTEXITCODE" }
    & $bashExecutable -lc 'pacman -S --needed --noconfirm mingw-w64-ucrt-x86_64-gcc'
    if ($LASTEXITCODE -ne 0) { throw "UCRT64 GCC installation failed: $LASTEXITCODE" }

    $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
    $entries = @($userPath -split ';' | Where-Object { $_ })
    if ($entries -notcontains $gccDirectory) {
        [Environment]::SetEnvironmentVariable(
            'Path', (($entries + $gccDirectory) -join ';'), 'User')
    }
    $gpp = Resolve-Gpp
}

if ($null -eq $gpp) {
    throw 'Windows native g++ is unavailable. Request approval, then rerun with -Install.'
}

$env:PATH = "$gccDirectory;$env:PATH"
& $gpp --version | Select-Object -First 1

if ($Environment.Count -eq 0) {
    Write-Output 'Native g++ is ready. No PlatformIO environment was requested.'
    exit 0
}

foreach ($name in $Environment) {
    if ($name -notmatch '^[a-z0-9][a-z0-9-]*$') {
        throw "Invalid PlatformIO environment name: $name"
    }
}

$platformio = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\platformio.exe'
if (-not (Test-Path -LiteralPath $platformio)) {
    $platformioCommand = Get-Command pio -ErrorAction SilentlyContinue
    if ($null -eq $platformioCommand) { throw 'PlatformIO executable was not found.' }
    $platformio = $platformioCommand.Source
}

$arguments = @('run')
foreach ($name in $Environment) { $arguments += @('-e', $name) }
& $platformio @arguments
if ($LASTEXITCODE -ne 0) { throw "PlatformIO native build failed: $LASTEXITCODE" }

foreach ($name in $Environment) {
    $program = Join-Path ".pio\build\$name" 'program.exe'
    if (-not (Test-Path -LiteralPath $program)) {
        throw "Native test executable was not generated: $program"
    }
    & $program
    if ($LASTEXITCODE -ne 0) {
        throw "Native assertions failed for ${name}: $LASTEXITCODE"
    }
    Write-Output "Native assertions passed: $name"
}
