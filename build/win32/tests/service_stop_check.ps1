<#
.SYNOPSIS
Check that stopping the proxy service is a stop, not a crash.

.DESCRIPTION
Registers the proxy as a service with the real binary, starts it, lets it settle,
then asks the service manager to stop it the way an operator does. A stop the
proxy handles correctly leaves event 7036 "entered the stopped state" in the
System log and nothing else; a stop the proxy mishandles leaves 7031 "terminated
unexpectedly", which triggers the recovery actions and makes Stop-Service fail.

The proxy log is read as well: the stop has to run through the ordinary
shutdown (workers, history cache, "Zabbix Proxy stopped") rather than fall off
the end of the process.

Registering a service needs an elevated prompt. GitHub's Windows runners are
elevated; a developer running this by hand has to be too.
#>
param(
	[Parameter(Mandatory = $true)][string] $Exe,
	# How long the service may take to stop before it is declared stuck.
	[int] $StopTimeout = 90
)

$ErrorActionPreference = 'Stop'
$name = 'Zabbix Proxy'

$elevated = ([Security.Principal.WindowsPrincipal] `
	[Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
	[Security.Principal.WindowsBuiltinRole]::Administrator)

if (-not $elevated) {
	Write-Host 'service stop check: needs an elevated prompt'
	exit 2
}

if (Get-Service -Name $name -ErrorAction SilentlyContinue) {
	Write-Host "service stop check: '$name' already exists on this machine, refusing to touch it"
	exit 2
}

$work = Join-Path ([IO.Path]::GetTempPath()) ("zbx_stop_" + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $work | Out-Null
$conf = Join-Path $work 'zabbix_proxy.conf'
$log = Join-Path $work 'zabbix_proxy.log'
@(
	'Hostname=stop-check'
	'Server=127.0.0.1'
	"LogFile=$log"
	'LogFileSize=0'
	'DebugLevel=4'
	"DBName=$($work -replace '\\','/')/proxy.db"
	'StartPingers=3'
) | Set-Content -Encoding ASCII $conf

$failures = @()

# A process that dies during the stop leaves no trace in the proxy log beyond
# its last line. Ask Windows Error Reporting for a dump of it, and read the
# dump back with cdb when the debugging tools are on the machine.
$dumps = Join-Path ([IO.Path]::GetTempPath()) 'zbx_stop_dumps'
New-Item -ItemType Directory -Force -Path $dumps | Out-Null
$werKey = 'HKLM:\SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps\zabbix_proxy.exe'
$werKeyExisted = Test-Path $werKey
New-Item -Path $werKey -Force | Out-Null
Set-ItemProperty -Path $werKey -Name DumpFolder -Value $dumps -Type ExpandString
Set-ItemProperty -Path $werKey -Name DumpType -Value 2 -Type DWord
Set-ItemProperty -Path $werKey -Name DumpCount -Value 3 -Type DWord

function Read-Dump($dump) {
	$cdb = @(
		'C:\Program Files (x86)\Windows Kits\Debuggersd\cdb.exe'
		'C:\Program Files\Windows Kits\Debuggersd\cdb.exe'
	) | Where-Object { Test-Path $_ } | Select-Object -First 1
	if (-not $cdb) {
		Write-Host '    (cdb.exe not on this machine; the dump is kept for analysis elsewhere)'
		return
	}
	$symdir = Split-Path -Parent $Exe
	& $cdb -z $dump -y "$symdir;srv*https://msdl.microsoft.com/download/symbols" -i $symdir `
		-c '.ecxr; .lines; kv 40; q' 2>&1 |
		Select-String -Pattern 'ExceptionAddress|ExceptionCode|^\s*[0-9a-f]{2} |Child-SP|zabbix_proxy!|KERNELBASE!|ntdll!|ucrtbase' |
		Select-Object -First 70 |
		ForEach-Object { Write-Host "    $($_.Line)" }
}

function Note($ok, $what, $detail) {
	if ($ok) {
		Write-Host "  ok    $what"
	} else {
		Write-Host "  FAIL  $what$(if ($detail) { ": $detail" })"
		$script:failures += "$what$(if ($detail) { ": $detail" })"
	}
}

function Scm-Events($since) {
	# Service Control Manager events about this service since $since. The
	# message text is localised; the event ids are not, so decisions are made
	# on ids and the text is only shown.
	try {
		Get-WinEvent -FilterHashtable @{
			LogName = 'System'; ProviderName = 'Service Control Manager'; StartTime = $since
		} -ErrorAction Stop |
			Where-Object { $_.Message -like "*$name*" } |
			Sort-Object TimeCreated
	} catch {
		@()
	}
}

function Proxy-Processes {
	Get-CimInstance Win32_Process -Filter "Name = 'zabbix_proxy.exe'" |
		Where-Object { $_.CommandLine -like "*$conf*" }
}

Write-Host ''
Write-Host 'service stop:'

$out = & $Exe -c $conf -i 2>&1
if (0 -ne $LASTEXITCODE) {
	Write-Host "  FAIL  install refused: $($out -join ' ')"
	exit 1
}

try {
	Start-Service -Name $name -ErrorAction Stop
	$started = Get-Date
	Note $true 'service started'

	# Let the workers come up and do a round of work before pulling the plug,
	# so the stop has something to tear down.
	Start-Sleep -Seconds 15
	Note ((Get-Service -Name $name).Status -eq 'Running') 'service still running after 15 s' `
		(Get-Service -Name $name).Status
	$pidBefore = @(Proxy-Processes | ForEach-Object { $_.ProcessId })
	Note ($pidBefore.Count -eq 1) 'one proxy process' "$($pidBefore.Count) found"

	# ------------------------------------------------------------- the stop
	$t0 = Get-Date
	$stopError = $null
	try {
		Stop-Service -Name $name -ErrorAction Stop
	} catch {
		$stopError = $_.Exception.Message
	}
	$svc = Get-Service -Name $name
	$deadline = $t0.AddSeconds($StopTimeout)
	while ($svc.Status -ne 'Stopped' -and (Get-Date) -lt $deadline) {
		Start-Sleep -Milliseconds 250
		$svc = Get-Service -Name $name
	}
	$stopped = Get-Date
	$elapsed = [int]($stopped - $t0).TotalMilliseconds

	# Give the process a moment to be gone and the event log a moment to catch up.
	Start-Sleep -Seconds 3
	$pidAfter = @(Proxy-Processes | ForEach-Object { $_.ProcessId })
	$events = @(Scm-Events $started)
	$ids = @($events | ForEach-Object { $_.Id })

	Write-Host ''
	Write-Host "  stop asked at $($t0.ToString('HH:mm:ss.fff')), service $($svc.Status) after $elapsed ms"
	if ($stopError) { Write-Host "  Stop-Service said: $stopError" }
	Write-Host '  service manager events:'
	foreach ($e in $events) {
		$text = ($e.Message -split "`r?`n")[0]
		Write-Host ("    {0} {1} {2}" -f $e.TimeCreated.ToString('HH:mm:ss.fff'), $e.Id, $text)
	}
	& sc.exe query "$name" | Select-String 'STATE|EXIT_CODE' | ForEach-Object { Write-Host "    $($_.Line.Trim())" }

	$logText = if (Test-Path $log) { Get-Content $log -Raw } else { '' }
	$logLines = @($logText -split "`r?`n")
	$dumpFiles = @(Get-ChildItem -Path $dumps -Filter '*.dmp' -ErrorAction SilentlyContinue)
	if ($dumpFiles.Count -gt 0) {
		Write-Host ''
		Write-Host '  crash dump(s) written during the stop:'
		foreach ($d in $dumpFiles) {
			Write-Host "    $($d.Name) ($([int]($d.Length / 1MB)) MB)"
			Read-Dump $d.FullName
		}
	}

	Write-Host ''
	Write-Host '  proxy log, from the stop request:'
	$fromStop = $logLines | Select-String -Pattern 'received stop request' -SimpleMatch -List
	$startIdx = if ($fromStop) { $fromStop.LineNumber - 1 } else { [Math]::Max(0, $logLines.Count - 80) }
	$logLines[$startIdx..($logLines.Count - 1)] |
		Where-Object { $_ -and $_ -notmatch ' In | End of |__zbx_|zbx_setproctitle' } |
		Select-Object -First 120 |
		ForEach-Object { Write-Host "    $_" }
	Write-Host ''
	Write-Host '  proxy log, last lines as written:'
	$logLines | Where-Object { $_ } | Select-Object -Last 25 | ForEach-Object { Write-Host "    $_" }
	Write-Host ''

	# ------------------------------------------------------------ verdicts
	Note ($null -eq $stopError) 'Stop-Service succeeds' $stopError
	Note ($svc.Status -eq 'Stopped') 'service reports stopped' "status $($svc.Status) after $elapsed ms"
	Note ($pidAfter.Count -eq 0) 'process is gone' "pid(s) $($pidAfter -join ',') still running"
	Note ($ids -contains 7036) 'event 7036 (entered the stopped state)' 'not logged'
	Note ($ids -notcontains 7031 -and $ids -notcontains 7034) 'no 7031/7034 (terminated unexpectedly)' `
		"ids seen: $($ids -join ',')"
	Note ($logText -match "$name received stop request") "log says '$name received stop request'" `
		$(if ($logText -match 'received stop request') { 'logged under another program name' } else { 'not logged' })
	Note ($logText -match "$name stopped\. Zabbix") "log ends with '$name stopped'" 'not logged'
	Note ($logText -notmatch 'terminated unexpectedly') 'no worker reported as terminated unexpectedly'
	Note ($logText -notmatch 'Unhandled exception') 'no exception during the stop'
	Note ($dumpFiles.Count -eq 0) 'no crash dump written' "$($dumpFiles.Count) in $dumps"
	Note ($elapsed -lt 60000) 'stopped within a minute' "$elapsed ms"
} finally {
	foreach ($p in Proxy-Processes) {
		Write-Host "  cleaning up leftover process $($p.ProcessId)"
		Stop-Process -Id $p.ProcessId -Force -ErrorAction SilentlyContinue
	}
	& $Exe -c $conf -d *> $null
	Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
	if (-not $werKeyExisted) { Remove-Item -Path $werKey -Recurse -Force -ErrorAction SilentlyContinue }
}

if ($failures.Count -gt 0) {
	Write-Host ''
	Write-Host 'service stop failures:'
	$failures | ForEach-Object { Write-Host "  $_" }
	exit 1
}
exit 0
