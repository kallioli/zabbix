<#
.SYNOPSIS
Install one version of the package, upgrade it with the next, take it apart.

.DESCRIPTION
An upgrade is where an operator finds out what the packager assumed. The
configuration is written by a custom action that skips a file already there,
and the buffer lives outside the install folder - both are meant to survive a
new version, and neither had been put to the test.

What is checked: the old product is replaced rather than left beside the new
one, edits made to the configuration are still there afterwards, the buffer
database survives, the sample configuration is refreshed, and the service is
still registered and can run.

Installing needs an elevated prompt. GitHub's Windows runners are elevated.
#>
param(
	[Parameter(Mandatory = $true)][string] $OldMsi,
	[Parameter(Mandatory = $true)][string] $NewMsi,
	[Parameter(Mandatory = $true)][string] $NewVersion
)

$ErrorActionPreference = 'Stop'

$name = 'Zabbix Proxy'
$server = 'zabbix.example.invalid'
$hostname = 'upgrade-check'
$port = '10078'
$folder = 'C:\Program Files\Zabbix Proxy'
$conf = Join-Path $folder 'zabbix_proxy.conf'
$sample = Join-Path $folder 'zabbix_proxy.conf.example'
$data = 'C:\ProgramData\Zabbix Proxy'
$marker = '# kept across the upgrade'

$elevated = ([Security.Principal.WindowsPrincipal] `
	[Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
	[Security.Principal.WindowsBuiltinRole]::Administrator)

if (-not $elevated) {
	Write-Host 'upgrade check: needs an elevated prompt'
	exit 2
}

if (Get-Service -Name $name -ErrorAction SilentlyContinue) {
	Write-Host "upgrade check: '$name' already exists, refusing to touch it"
	exit 2
}

# An uninstall leaves the configuration behind on purpose - it is the
# operator's file, not the package's. That makes it someone else's leftover
# here, and this check has to start from nothing.
Remove-Item -Recurse -Force $folder -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue

$log = Join-Path ([IO.Path]::GetTempPath()) 'zbx_msi_upgrade.log'
$failures = @()

function Note($ok, $what, $detail = '') {
	if ($ok) {
		Write-Host "  ok    $what"
	} else {
		Write-Host "  FAIL  $what$(if ($detail) { ": $detail" })"
		$script:failures += "$what$(if ($detail) { ": $detail" })"
	}
}

# Win32_Product would answer this, and would reconfigure every package on the
# machine to do it. The uninstall keys hold the same answer for nothing.
function Installed {
	Get-ChildItem 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\*' |
		ForEach-Object { Get-ItemProperty $_.PSPath } |
		Where-Object { $_.DisplayName -eq $name }
}

function Msiexec($msi, $extra) {
	$args = @('/i', "`"$msi`"", '/qn') + $extra + @('/l*v', "`"$log`"")
	$p = Start-Process msiexec.exe -Wait -PassThru -ArgumentList $args
	if (0 -ne $p.ExitCode -and (Test-Path $log)) {
		Write-Host '  --- from the installer log:'
		Get-Content $log |
			Where-Object { $_ -match 'CustomAction|returned 3|Service|Error' } |
			Select-Object -Last 25 |
			ForEach-Object { Write-Host "      $($_.Trim())" }
	}
	return $p.ExitCode
}

Write-Host ''
Write-Host 'upgrade:'

# ------------------------------------------------------------- the old one
$code = Msiexec $OldMsi @("SERVER=$server", "HOSTNAME=$hostname", "LISTENPORT=$port")
Note (0 -eq $code) 'first version installs' "msiexec returned $code"

$before = @(Installed)
Note (1 -eq $before.Count) 'one product registered' "found $($before.Count)"
$oldVersion = if ($before.Count -ge 1) { $before[0].DisplayVersion } else { '?' }

if (-not (Test-Path -LiteralPath $conf)) {
	Note $false 'configuration generated' "$conf is absent"
} else {
	Note $true 'configuration generated'
	# Stand in for everything an operator adds after the install: if this
	# survives, so does the rest of the file.
	Add-Content -LiteralPath $conf -Value $marker -Encoding ASCII
}

# Give it a buffer to lose. Starting is also what proves the old version was
# a working install rather than a set of files.
try {
	Start-Service -Name $name -ErrorAction Stop
	Start-Sleep -Seconds 10
	Note $true 'first version runs'
} catch {
	Note $false 'first version runs' $_.Exception.Message
}

$db = Join-Path $data 'zabbix_proxy.db'
Note (Test-Path -LiteralPath $db) 'buffer database created'
$dbStamp = if (Test-Path -LiteralPath $db) { (Get-Item -LiteralPath $db).CreationTimeUtc } else { $null }

# ------------------------------------------------------------- the new one
# Deliberately left running. An operator upgrading a proxy does not stop it
# first, and if the package cannot handle that it should say so here.
$code = Msiexec $NewMsi @()
Note (0 -eq $code) 'second version installs over the first' "msiexec returned $code"

$after = @(Installed)
Note (1 -eq $after.Count) 'still one product registered' "found $($after.Count)"
if ($after.Count -ge 1) {
	Note ($after[0].DisplayVersion -eq $NewVersion) 'version is the new one' `
		"$oldVersion -> $($after[0].DisplayVersion), expected $NewVersion"
}

# The point of the whole exercise.
if (Test-Path -LiteralPath $conf) {
	$kept = (Get-Content -LiteralPath $conf -Raw) -match [regex]::Escape($marker)
	Note $kept 'configuration edits survive the upgrade'
	if (-not $kept) {
		Write-Host '  --- what the configuration holds now:'
		Get-Content -LiteralPath $conf | ForEach-Object { Write-Host "      $_" }
	}
} else {
	Note $false 'configuration edits survive the upgrade' "$conf is gone"
}

Note (Test-Path -LiteralPath $db) 'buffer database survives the upgrade'
if ($dbStamp -and (Test-Path -LiteralPath $db)) {
	Note ((Get-Item -LiteralPath $db).CreationTimeUtc -eq $dbStamp) `
		'buffer database is the same file' 'it was recreated'
}
Note (Test-Path -LiteralPath $sample) 'sample configuration is still shipped'

$svc = Get-Service -Name $name -ErrorAction SilentlyContinue
Note ($null -ne $svc) 'service still registered'
Write-Host "  note  service is $(if ($svc) { $svc.Status } else { 'absent' }) after the upgrade"

if ($svc -and $svc.Status -ne 'Running') {
	try { Start-Service -Name $name -ErrorAction Stop } catch {}
	Start-Sleep -Seconds 8
	$svc = Get-Service -Name $name -ErrorAction SilentlyContinue
}
Note ($svc -and $svc.Status -eq 'Running') 'service runs on the new version' `
	$(if ($svc) { $svc.Status })

Stop-Service -Name $name -ErrorAction SilentlyContinue

# ------------------------------------------------------------- take it apart
$p = Start-Process msiexec.exe -Wait -PassThru -ArgumentList @('/x', "`"$NewMsi`"", '/qn')
Note (0 -eq $p.ExitCode) 'uninstall' "msiexec returned $($p.ExitCode)"
Note ($null -eq (Get-Service -Name $name -ErrorAction SilentlyContinue)) 'service removed'
Note (0 -eq @(Installed).Count) 'nothing left registered'
Note (Test-Path -LiteralPath $db) 'database left behind by uninstall'

Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $folder -ErrorAction SilentlyContinue
Remove-Item -Force $log -ErrorAction SilentlyContinue

if ($failures.Count -gt 0) {
	Write-Host ''
	Write-Host 'upgrade failures:'
	$failures | ForEach-Object { Write-Host "  $_" }
	exit 1
}
exit 0
