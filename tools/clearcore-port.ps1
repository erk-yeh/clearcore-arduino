#Requires -Version 5.1
<#
.SYNOPSIS
    Print the serial port address of the attached Teknic ClearCore.

.DESCRIPTION
    The ClearCore re-enumerates on a different COM port after an upload
    (observed hopping COM3 -> COM4), so hardcoding a port in tasks.json goes
    stale after the first flash. This resolves it fresh from arduino-cli's own
    board discovery each time.

    Writes the port address to stdout and nothing else, so it can be used in
    PowerShell command substitution:

        arduino-cli monitor -p (./tools/clearcore-port.ps1) -c baudrate=115200

    Exits non-zero with a message on stderr if the board can't be resolved
    unambiguously.

.PARAMETER Fqbn
    Board to match. Note the lowercase board-ID segment -- 'ClearCore:sam:ClearCore'
    will silently match nothing.
#>
[CmdletBinding()]
param(
    [string]$Fqbn = 'ClearCore:sam:clearcore'
)

$ErrorActionPreference = 'Stop'

$json = arduino-cli board list --format json
if ($LASTEXITCODE -ne 0) {
    Write-Error "arduino-cli board list failed (exit $LASTEXITCODE)."
    exit 1
}

# @() forces an array even when exactly one port matches, so .Count is reliable.
$ports = @(
    ($json | ConvertFrom-Json).detected_ports |
        Where-Object { $_.matching_boards.fqbn -contains $Fqbn } |
        ForEach-Object { $_.port.address }
)

if ($ports.Count -eq 0) {
    Write-Error @"
No board matching '$Fqbn' found.
If it is sitting in bootloader mode it shows up as a UF2 device with no FQBN --
press reset to return it to application mode. Otherwise check the USB cable.
Run 'arduino-cli board list' to see what is actually attached.
"@
    exit 1
}

if ($ports.Count -gt 1) {
    Write-Error "Multiple ClearCore boards found ($($ports -join ', ')). Pass -p explicitly."
    exit 1
}

$ports[0]
