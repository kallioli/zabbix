<#
.SYNOPSIS
Check that the proxy installs a service the service manager will start.

.DESCRIPTION
The proxy asks CreateService() for a startup type derived from task flags. If
nothing sets them the request is SERVICE_DISABLED, the service installs without
complaint, and every later start fails with 0x422. Nothing in the collection
tests goes near this, so it needs its own check.

Registering a service needs an elevated prompt. GitHub's Windows runners are
elevated; a developer running this by hand has to be too.
#>
param(
	[Parameter(Mandatory = $true)][string] $Exe
)

$ErrorActionPreference = 'Stop'
$name = 'Zabbix Proxy'

$elevated = ([Security.Principal.WindowsPrincipal] `
	[Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
	[Security.Principal.WindowsBuiltinRole]::Administrator)

if (-not $elevated) {
	Write-Host 'service check: needs an elevated prompt'
	exit 2
}

if (Get-Service -Name $name -ErrorAction SilentlyContinue) {
	Write-Host "service check: '$name' already exists on this machine, refusing to touch it"
	exit 2
}

$work = Join-Path ([IO.Path]::GetTempPath()) ("zbx_svc_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$conf = Join-Path $work 'zabbix_proxy.conf'
@(
	'Hostname=svc-check'
	'Server=127.0.0.1'
	'LogType=console'
	"DBName=$($work -replace '\\','/')/proxy.db"
) | Set-Content -Encoding ASCII $conf

$failures = @()

function Startup-Type {
	# sc.exe is the only thing that reports the raw type, and its output is localised
	# past the first field - so read the number, not the words after it.
	$line = (& sc.exe qc "$name" | Select-String 'START_TYPE')
	if ($line -match 'START_TYPE\s*:\s*(\d+)') { return [int]$Matches[1] }
	return -1
}

function Try-Install($label, $extra, $expected, $expectedName) {
	$out = & $Exe -c $conf -i @extra 2>&1
	if (0 -ne $LASTEXITCODE) {
		Write-Host "  FAIL  $label : install refused: $($out -join ' ')"
		$script:failures += "$label : install refused"
		return
	}

	$type = Startup-Type
	& $Exe -c $conf -d *> $null

	if ($type -eq $expected) {
		Write-Host "  ok    $label : START_TYPE $type ($expectedName)"
	} else {
		$seen = switch ($type) { 2 {'AUTO_START'} 3 {'DEMAND_START'} 4 {'DISABLED'} default {'?'} }
		Write-Host "  FAIL  $label : START_TYPE $type ($seen), expected $expected ($expectedName)"
		$script:failures += "$label : START_TYPE $type au lieu de $expected"
	}
}

Write-Host ''
Write-Host 'service registration:'

# 2 = SERVICE_AUTO_START, 3 = SERVICE_DEMAND_START, 4 = SERVICE_DISABLED
Try-Install 'default'      @()                  2 'AUTO_START'
Try-Install '-S manual'    @('-S','manual')     3 'DEMAND_START'
Try-Install '-S automatic' @('-S','automatic')  2 'AUTO_START'
Try-Install '-S disabled'  @('-S','disabled')   4 'DISABLED'

Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue

if ($failures.Count -gt 0) {
	Write-Host ''
	Write-Host 'service registration failures:'
	$failures | ForEach-Object { Write-Host "  $_" }
	exit 1
}
exit 0
