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

	# A rollback removes the folders and files the install had made, the proxy's
	# log among them, so the evidence is gone by the time we look. Run it once
	# more with rollback disabled, purely to keep what it leaves behind.
	# The service manager records why it gave up, and the event log survives the
	# rollback that takes everything else away. 7000 carries the error, 7009 a
	# timeout, 7024 the exit code the service itself returned.
	Write-Host '  --- what the service manager recorded:'
	try {
		Get-WinEvent -FilterHashtable @{
			LogName = 'System'; ProviderName = 'Service Control Manager'
			StartTime = (Get-Date).AddMinutes(-10)
			Id = 7000, 7009, 7024, 7031, 7034
		} -ErrorAction Stop |
			Where-Object { $_.Message -match 'Zabbix Proxy' } |
			Select-Object -First 4 |
			ForEach-Object {
				Write-Host "      [$($_.Id)] $(($_.Message -replace '\s+', ' ').Trim())"
			}
	} catch {
		Write-Host '      nothing recorded'
	}

	Write-Host '  --- retrying with rollback disabled, to keep the evidence'
	Start-Process msiexec.exe -Wait -ArgumentList @(
		'/i', "`"$Msi`"", '/qn', "SERVER=$server", "HOSTNAME=$hostname",
		"LISTENPORT=$port", 'DISABLEROLLBACK=1') | Out-Null

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

	# Starting it by hand says what the service manager only counted as a
	# timeout, since a console run has somewhere to put an early error.
	$exe = 'C:\Program Files\Zabbix Proxy\zabbix_proxy.exe'
	if (Test-Path -LiteralPath $exe) {
		Write-Host '  --- starting it by hand, in the foreground:'
		$out = Join-Path ([IO.Path]::GetTempPath()) 'zbx_fg.txt'
		$q = Start-Process $exe -Wait -PassThru -NoNewWindow `
			-ArgumentList @('-f', '-c', "`"$conf`"") `
			-RedirectStandardOutput $out -RedirectStandardError "$out.err"
		foreach ($f in @($out, "$out.err")) {
			if ((Test-Path $f) -and (Get-Item $f).Length -gt 0) {
				Get-Content $f | Select-Object -First 10 |
					ForEach-Object { Write-Host "      $($_.Trim())" }
			}
		}
		Write-Host "      exit code $($q.ExitCode)"
	}

	Start-Process msiexec.exe -Wait -ArgumentList @('/x', "`"$Msi`"", '/qn') | Out-Null
	Remove-Item -Recurse -Force $data -ErrorAction SilentlyContinue
	exit 1
}
Note $true 'install'

# ------------------------------------------------- what it should have made
$svc = Get-Service -Name $name -ErrorAction SilentlyContinue
Note ($null -ne $svc) 'service registered'

if ($svc) {
	$account = (Get-CimInstance Win32_Service -Filter "Name='$name'").StartName
	Note ($account -eq 'LocalSystem') 'runs as LocalSystem' $account
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

# The installer registers the service without starting it, so start it here. It
# has no server to reach, which it must tolerate; what matters is that it comes
# up, stays up, and writes where it was told to.
try {
	Start-Service -Name $name -ErrorAction Stop
	Note $true 'service starts'
} catch {
	Note $false 'service starts' $_.Exception.Message
}

Start-Sleep -Seconds 10
$svc = Get-Service -Name $name -ErrorAction SilentlyContinue
Note ($svc -and $svc.Status -eq 'Running') 'service still running' $(if ($svc) { $svc.Status })
Note (Test-Path (Join-Path $data 'zabbix_proxy.log')) 'writes its log under ProgramData'
Note (Test-Path (Join-Path $data 'zabbix_proxy.db'))  'writes its database under ProgramData'

# When it did not come up, its own log is where the reason is - and nothing
# rolls back now, so the log is still there to read.
if (-not ($svc -and $svc.Status -eq 'Running')) {
	$proxylog = Join-Path $data 'zabbix_proxy.log'
	if (Test-Path -LiteralPath $proxylog) {
		Write-Host '  --- what the proxy logged:'
		Get-Content -LiteralPath $proxylog -Tail 20 |
			ForEach-Object { Write-Host "      $($_.Trim())" }
	}
	Write-Host '  --- what the service manager recorded:'
	Get-WinEvent -FilterHashtable @{
		LogName = 'System'; ProviderName = 'Service Control Manager'
		StartTime = (Get-Date).AddMinutes(-5); Id = 7000, 7009, 7024, 7031, 7034
	} -ErrorAction SilentlyContinue |
		Where-Object { $_.Message -match 'Zabbix Proxy' } |
		Select-Object -First 3 |
		ForEach-Object { Write-Host "      [$($_.Id)] $(($_.Message -replace '\s+', ' ').Trim())" }
}

Stop-Service -Name $name -ErrorAction SilentlyContinue

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
