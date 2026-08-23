<#
.SYNOPSIS
Install the package, check what it produced, uninstall it.

.DESCRIPTION
Building an MSI proves it compiles. It says nothing about whether it installs,
whether the answers given on the command line reach the configuration it
generates, or whether the service it registers can start - and those are the
parts that broke first when the package was finally run by hand.

Installing needs an elevated prompt. GitHub's Windows runners are elevated.
#>
param(
	[Parameter(Mandatory = $true)][string] $Msi
)

$ErrorActionPreference = 'Stop'

$name = 'Zabbix Proxy'
$server = 'zabbix.example.invalid'
$hostname = 'installer-check'
$port = '10077'
$conf = 'C:\Program Files\Zabbix Proxy\zabbix_proxy.conf'
$data = 'C:\ProgramData\Zabbix Proxy'

$elevated = ([Security.Principal.WindowsPrincipal] `
	[Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
	[Security.Principal.WindowsBuiltinRole]::Administrator)

if (-not $elevated) {
	Write-Host 'installer check: needs an elevated prompt'
	exit 2
}

if (Get-Service -Name $name -ErrorAction SilentlyContinue) {
	Write-Host "installer check: '$name' already exists, refusing to touch it"
	exit 2
}

$log = Join-Path ([IO.Path]::GetTempPath()) 'zbx_msi.log'
$failures = @()

function Note($ok, $what, $detail = '') {
	if ($ok) {
		Write-Host "  ok    $what"
	} else {
		Write-Host "  FAIL  $what$(if ($detail) { ": $detail" })"
		$script:failures += "$what$(if ($detail) { ": $detail" })"
	}
}

Write-Host ''
Write-Host 'installer:'

# ---------------------------------------------------------------- install
$p = Start-Process msiexec.exe -Wait -PassThru -ArgumentList @(
	'/i', "`"$Msi`"", '/qn', "SERVER=$server", "HOSTNAME=$hostname",
	"LISTENPORT=$port", '/l*v', "`"$log`"")

if (0 -ne $p.ExitCode) {
	Note $false 'install' "msiexec returned $($p.ExitCode)"

	# The action that failed names itself somewhere in the middle of the log,
	# not at its end, so read the whole thing. What matters: anything a custom
	# action printed, the action that returned 3, and the service errors, which
	# are the two ways this install can fail.
	if (Test-Path $log) {
		Write-Host '  --- from the installer log:'
		Get-Content $log |
			Where-Object {
				$_ -match 'WixQuietExec|returned actual error code|Return value 3|' +
					'Error 19[0-9][0-9]|Product: .*Error|Failed to |cannot '
			} |
			Select-Object -Last 25 |
			ForEach-Object { Write-Host "      $($_.Trim())" }
	}

	# When the service is what failed, the installer only says so; the reason is
	# in the proxy's own log, if it got far enough to open one.
	$proxylog = Join-Path $data 'zabbix_proxy.log'
	if (Test-Path -LiteralPath $proxylog) {
		Write-Host '  --- what the proxy logged:'
		Get-Content -LiteralPath $proxylog -Tail 25 |
			ForEach-Object { Write-Host "      $($_.Trim())" }
	} else {
		Write-Host "  --- no proxy log at $proxylog"
		Write-Host '      it did not get as far as opening one'
	}

	if (Test-Path -LiteralPath $data) {
		Write-Host '  --- what it did leave behind:'
		Get-ChildItem -LiteralPath $data -Force |
			ForEach-Object { Write-Host "      $($_.Name)  $($_.Length)" }
	}

	exit 1
}
Note $true 'install'

# ------------------------------------------------- what it should have made
$svc = Get-Service -Name $name -ErrorAction SilentlyContinue
Note ($null -ne $svc) 'service registered'

if ($svc) {
	$account = (Get-CimInstance Win32_Service -Filter "Name='$name'").StartName
	Note ($account -eq 'NT AUTHORITY\LocalService') 'runs as LocalService' $account
}

# The answers have to survive the trip into the elevated half of the install.
# When they do not, the generated file quietly carries the defaults instead.
if (Test-Path -LiteralPath $conf) {
	Note $true 'configuration generated'
	$text = Get-Content -LiteralPath $conf -Raw
	Note ($text -match "(?m)^Server=$([regex]::Escape($server))$")     'Server carried through'
	Note ($text -match "(?m)^Hostname=$([regex]::Escape($hostname))$") 'Hostname carried through'
	Note ($text -match "(?m)^ListenPort=$([regex]::Escape($port))$")   'ListenPort carried through'
} else {
	Note $false 'configuration generated' "$conf is absent"
}

# The service is started by the installer. It has no server to reach here, which
# it must tolerate; what matters is that it stays up and writes where it should.
Start-Sleep -Seconds 10
$svc = Get-Service -Name $name -ErrorAction SilentlyContinue
Note ($svc -and $svc.Status -eq 'Running') 'service still running' $(if ($svc) { $svc.Status })
Note (Test-Path (Join-Path $data 'zabbix_proxy.log')) 'writes its log under ProgramData'
Note (Test-Path (Join-Path $data 'zabbix_proxy.db'))  'writes its database under ProgramData'

# ---------------------------------------------------------------- uninstall
$p = Start-Process msiexec.exe -Wait -PassThru -ArgumentList @('/x', "`"$Msi`"", '/qn')
Note (0 -eq $p.ExitCode) 'uninstall' "msiexec returned $($p.ExitCode)"
Note ($null -eq (Get-Service -Name $name -ErrorAction SilentlyContinue)) 'service removed'

# The buffer and the log are the operator's; an uninstall must not take them.
Note (Test-Path (Join-Path $data 'zabbix_proxy.db')) 'database left behind by uninstall'

Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue
Remove-Item -Force $log -ErrorAction SilentlyContinue

if ($failures.Count -gt 0) {
	Write-Host ''
	Write-Host 'installer failures:'
	$failures | ForEach-Object { Write-Host "  $_" }
	exit 1
}
exit 0
