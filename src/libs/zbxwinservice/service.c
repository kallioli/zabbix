/*
** Copyright (C) 2001-2026 Zabbix SIA
**
** This program is free software: you can redistribute it and/or modify it under the terms of
** the GNU Affero General Public License as published by the Free Software Foundation, version 3.
**
** This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
** without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
** See the GNU Affero General Public License for more details.
**
** You should have received a copy of the GNU Affero General Public License along with this program.
** If not, see <https://www.gnu.org/licenses/>.
**/

#include "zbxwinservice.h"

#include "zbxwin32.h"
#include "zbxstr.h"
#include "zbxcfg.h"
#include "zbxlog.h"

#include <strsafe.h> /* StringCchPrintf */

#define EVENTLOG_REG_PATH TEXT("SYSTEM\\CurrentControlSet\\Services\\EventLog\\")

static	SERVICE_STATUS		serviceStatus;
static	SERVICE_STATUS_HANDLE	serviceHandle;
static	CRITICAL_SECTION	serviceStatus_lock;	/* the handler thread and the stopping thread both report */

#define ZBX_APP_STOPPED	0
#define ZBX_APP_RUNNING	1
/* required for closing application from service */
static int	application_status = ZBX_APP_RUNNING;

static zbx_on_exit_t	zbx_on_exit_cb;

static zbx_get_config_str_f	get_zbx_service_name_cb = NULL;
static zbx_get_config_str_f	get_zbx_event_source_cb = NULL;

int	ZBX_IS_RUNNING(void)
{
	return application_status;
}

void	ZBX_DO_EXIT(void)
{
	application_status = ZBX_APP_STOPPED;
}
#undef ZBX_APP_STOPPED
#undef ZBX_APP_RUNNING

/* free resources allocated by MAIN_ZABBIX_ENTRY() */
void	zbx_free_service_resources(void);

/* How a stop request is carried out. By default (the agent) the control handler tears the program down   */
/* itself through zbx_free_service_resources() and reports the service stopped, while MAIN_ZABBIX_ENTRY() */
/* never returns. A program whose MAIN_ZABBIX_ENTRY() returns once ZBX_IS_RUNNING() is cleared (the      */
/* proxy) selects the other mode: the handler only flags the stop and returns, the main thread runs the   */
/* ordinary shutdown on its way out of MAIN_ZABBIX_ENTRY(), and ServiceEntry() reports the service       */
/* stopped once it is back. The process exits through main() with status 0.                              */
static int	stop_from_main_thread = 0;

#define ZBX_SERVICE_STOP_WAIT_HINT	30000	/* ms the service manager is told a stop may take */

void	zbx_service_stop_from_main_thread(void)
{
	stop_from_main_thread = 1;
}

/******************************************************************************
 *                                                                            *
 * Purpose: tells the service manager that a stop in progress is still        *
 *          making progress                                                   *
 *                                                                            *
 * Comments: meant for the thread carrying out the stop, between its phases;  *
 *           a no-op when the program is not running as a service             *
 *                                                                            *
 ******************************************************************************/
void	zbx_service_report_stop_pending(void)
{
	if (NULL == serviceHandle)
		return;

	EnterCriticalSection(&serviceStatus_lock);
	serviceStatus.dwCurrentState = SERVICE_STOP_PENDING;
	serviceStatus.dwCheckPoint++;
	serviceStatus.dwWaitHint = ZBX_SERVICE_STOP_WAIT_HINT;
	SetServiceStatus(serviceHandle, &serviceStatus);
	LeaveCriticalSection(&serviceStatus_lock);
}

static void	service_report_stopped(void)
{
	EnterCriticalSection(&serviceStatus_lock);
	serviceStatus.dwCurrentState	= SERVICE_STOPPED;
	serviceStatus.dwWaitHint	= 0;
	serviceStatus.dwCheckPoint	= 0;
	serviceStatus.dwWin32ExitCode	= 0;
	SetServiceStatus(serviceHandle, &serviceStatus);
	LeaveCriticalSection(&serviceStatus_lock);
}

static void	parent_signal_handler(int sig)
{
	switch (sig)
	{
		case SIGINT:
		case SIGTERM:
			ZBX_DO_EXIT();
			zabbix_log(LOG_LEVEL_INFORMATION, "Got signal. Exiting ...");

			/* in the foreground the same split applies as for a service stop: the console thread */
			/* only flags the stop, the main thread runs the shutdown on its way out              */
			if (0 == stop_from_main_thread)
				zbx_on_exit_cb(SUCCEED, NULL);
			break;
	}
}

static VOID WINAPI	ServiceCtrlHandler(DWORD ctrlCode)
{
	switch (ctrlCode)
	{
		case SERVICE_CONTROL_STOP:
			zabbix_log(LOG_LEVEL_INFORMATION, "%s received stop request.", get_zbx_service_name_cb());
			break;
		case SERVICE_CONTROL_SHUTDOWN:
			zabbix_log(LOG_LEVEL_INFORMATION, "%s received shutdown request.", get_zbx_service_name_cb());
			break;
		default:
			zabbix_log(LOG_LEVEL_DEBUG, "%s received request:%u.", get_zbx_service_name_cb(), ctrlCode);
			break;
	}

	switch (ctrlCode)
	{
		case SERVICE_CONTROL_STOP:
		case SERVICE_CONTROL_SHUTDOWN:
			EnterCriticalSection(&serviceStatus_lock);

			if (SERVICE_STOP_PENDING == serviceStatus.dwCurrentState)
			{
				/* asked again while already stopping: say so, do not start over */
				serviceStatus.dwCheckPoint++;
				SetServiceStatus(serviceHandle, &serviceStatus);
				LeaveCriticalSection(&serviceStatus_lock);
				return;
			}

			serviceStatus.dwCurrentState	= SERVICE_STOP_PENDING;
			serviceStatus.dwCheckPoint	= 0;
			serviceStatus.dwWaitHint	= 0 != stop_from_main_thread ? ZBX_SERVICE_STOP_WAIT_HINT : 4000;
			SetServiceStatus(serviceHandle, &serviceStatus);
			LeaveCriticalSection(&serviceStatus_lock);

			/* notify other threads and allow them to terminate */
			ZBX_DO_EXIT();

			/* the main thread carries out the stop and ServiceEntry() reports it; this handler */
			/* has to be back within the service manager's 30 second limit whatever the shutdown */
			/* takes, so it does not wait here                                                   */
			if (0 != stop_from_main_thread)
				return;

			zbx_free_service_resources();

			service_report_stopped();
			return;
		default:
			/* report the state as it is, which is RUNNING or a stop in progress */
			EnterCriticalSection(&serviceStatus_lock);
			SetServiceStatus(serviceHandle, &serviceStatus);
			LeaveCriticalSection(&serviceStatus_lock);
			return;
	}
}

/******************************************************************************
 *                                                                            *
 * Purpose: runs the program's main entry under the same exception filter as  *
 *          its worker threads, so that a fault on the main thread (for       *
 *          example during the shutdown it carries out) is written to the log *
 *          with a backtrace before the process goes down                     *
 *                                                                            *
 ******************************************************************************/
static void	main_entry_guarded(int flags)
{
	__try
	{
		MAIN_ZABBIX_ENTRY(flags);
	}
	__except (zbx_win_seh_handler(GetExceptionInformation()))
	{
		/* the filter logs and lets the exception continue to the system, so this is not reached */
	}
}

static VOID WINAPI	ServiceEntry(DWORD argc, wchar_t **argv)
{
	wchar_t	*wservice_name;

	ZBX_UNUSED(argc);
	ZBX_UNUSED(argv);

	InitializeCriticalSection(&serviceStatus_lock);

	wservice_name = zbx_utf8_to_unicode(get_zbx_service_name_cb());
	serviceHandle = RegisterServiceCtrlHandler(wservice_name, ServiceCtrlHandler);
	zbx_free(wservice_name);

	/* start service initialization */
	serviceStatus.dwServiceType		= SERVICE_WIN32_OWN_PROCESS;
	serviceStatus.dwCurrentState		= SERVICE_START_PENDING;
	serviceStatus.dwControlsAccepted	= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
	serviceStatus.dwWin32ExitCode		= 0;
	serviceStatus.dwServiceSpecificExitCode	= 0;
	serviceStatus.dwCheckPoint		= 0;
	serviceStatus.dwWaitHint		= 2000;

	SetServiceStatus(serviceHandle, &serviceStatus);

	/* service is running */
	serviceStatus.dwCurrentState	= SERVICE_RUNNING;
	serviceStatus.dwWaitHint	= 0;
	SetServiceStatus(serviceHandle, &serviceStatus);

	main_entry_guarded(0);

	/* only a program that stops from its main thread gets here: its shutdown is complete, the service is */
	/* stopped, and StartServiceCtrlDispatcher() returns in main() so that the process exits with status 0 */
	if (0 != stop_from_main_thread)
		service_report_stopped();
}

void	zbx_service_start(int flags)
{
	int				ret;
	static SERVICE_TABLE_ENTRY	serviceTable[2];

	if (0 != (flags & ZBX_TASK_FLAG_FOREGROUND))
	{
		main_entry_guarded(flags);
		return;
	}

	serviceTable[0].lpServiceName = zbx_utf8_to_unicode(get_zbx_service_name_cb());
	serviceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)ServiceEntry;
	serviceTable[1].lpServiceName = NULL;
	serviceTable[1].lpServiceProc = NULL;

	ret = StartServiceCtrlDispatcher(serviceTable);
	zbx_free(serviceTable[0].lpServiceName);

	if (0 == ret)
	{
		if (ERROR_FAILED_SERVICE_CONTROLLER_CONNECT == GetLastError())
			zbx_error("use foreground option to run Zabbix agent as console application");
		else
			zbx_error("StartServiceCtrlDispatcher() failed: %s", zbx_strerror_from_system(GetLastError()));
	}
}

static int	svc_OpenSCManager(SC_HANDLE *mgr)
{
	if (NULL != (*mgr = OpenSCManager(NULL, NULL, GENERIC_WRITE)))
		return SUCCEED;

	zbx_error("ERROR: cannot connect to Service Manager: %s", zbx_strerror_from_system(GetLastError()));

	return FAIL;
}

static int	svc_OpenService(SC_HANDLE mgr, SC_HANDLE *service, DWORD desired_access)
{
	wchar_t	*wservice_name;
	int	ret = SUCCEED;

	wservice_name = zbx_utf8_to_unicode(get_zbx_service_name_cb());

	if (NULL == (*service = OpenService(mgr, wservice_name, desired_access)))
	{
		zbx_error("ERROR: cannot open service [%s]: %s",
				get_zbx_service_name_cb(), zbx_strerror_from_system(GetLastError()));
		ret = FAIL;
	}

	zbx_free(wservice_name);

	return ret;
}

static void	svc_get_fullpath(const char *path, wchar_t *fullpath, size_t max_fullpath)
{
	wchar_t	*wpath;

	wpath = zbx_acp_to_unicode(path);
	_wfullpath(fullpath, wpath, max_fullpath);
	zbx_free(wpath);
}

static void	svc_get_command_line(const char *path, unsigned int multiple_agents, wchar_t *cmdLine,
		size_t max_cmdLine, const char *config_file)
{
	wchar_t	path1[MAX_PATH], path2[MAX_PATH];

	svc_get_fullpath(path, path2, MAX_PATH);

	if (NULL == wcsstr(path2, TEXT(".exe")))
		StringCchPrintf(path1, MAX_PATH, TEXT("%s.exe"), path2);
	else
		StringCchPrintf(path1, MAX_PATH, path2);

	if (NULL != config_file)
	{
		svc_get_fullpath(config_file, path2, MAX_PATH);
		StringCchPrintf(cmdLine, max_cmdLine, TEXT("\"%s\" %s--config \"%s\""),
				path1,
				(0 == multiple_agents) ? TEXT("") : TEXT("--multiple-agents "),
				path2);
	}
	else
		StringCchPrintf(cmdLine, max_cmdLine, TEXT("\"%s\""), path1);
}

static int	svc_install_event_source(const char *path)
{
	HKEY	hKey;
	DWORD	dwTypes = EVENTLOG_ERROR_TYPE | EVENTLOG_WARNING_TYPE | EVENTLOG_INFORMATION_TYPE;
	wchar_t	execName[MAX_PATH];
	wchar_t	regkey[256], *wevent_source;

	svc_get_fullpath(path, execName, MAX_PATH);

	wevent_source = zbx_utf8_to_unicode(get_zbx_event_source_cb());
	StringCchPrintf(regkey, ARRSIZE(regkey), EVENTLOG_REG_PATH TEXT("System\\%s"), wevent_source);
	zbx_free(wevent_source);

	if (ERROR_SUCCESS != RegCreateKeyEx(HKEY_LOCAL_MACHINE, regkey, 0, NULL, REG_OPTION_NON_VOLATILE,
			KEY_SET_VALUE, NULL, &hKey, NULL))
	{
		zbx_error("unable to create registry key: %s", zbx_strerror_from_system(GetLastError()));
		return FAIL;
	}

	RegSetValueEx(hKey, TEXT("TypesSupported"), 0, REG_DWORD, (BYTE *)&dwTypes, sizeof(DWORD));
	RegSetValueEx(hKey, TEXT("EventMessageFile"), 0, REG_EXPAND_SZ, (BYTE *)execName,
			(DWORD)(wcslen(execName) + 1) * sizeof(wchar_t));
	RegCloseKey(hKey);

	zbx_error("event source [%s] installed successfully", get_zbx_event_source_cb());

	return SUCCEED;
}

static DWORD	svc_start_type_get(unsigned int flags) {
	if (0 == (flags & ZBX_TASK_FLAG_SERVICE_ENABLED))
		return SERVICE_DISABLED;

	if (0 != (flags & ZBX_TASK_FLAG_SERVICE_AUTOSTART))
		return SERVICE_AUTO_START;
	else
		return SERVICE_DEMAND_START;
}

static int	svc_delayed_autostart_config(SC_HANDLE service, unsigned int flags)
{
	const OSVERSIONINFOEX		*vi;
	SERVICE_DELAYED_AUTO_START_INFO	scdasi;

	scdasi.fDelayedAutostart = (0 != (flags & ZBX_TASK_FLAG_SERVICE_AUTOSTART_DELAYED) ? TRUE : FALSE);

	/* SERVICE_CONFIG_DELAYED_AUTO_START_INFO is supported on Windows Server 2008/Vista and onwards */
	if (NULL == (vi = zbx_win_getversion()))
	{
		if (TRUE == scdasi.fDelayedAutostart)
		{
			zbx_error("cannot retrieve system version to check if delayed auto-start can be configured");
			return FAIL;
		}

		return SUCCEED;
	}

	if (6 > vi->dwMajorVersion)
	{
		if (TRUE == scdasi.fDelayedAutostart)
		{
			zbx_error("delayed auto-start can be configured on Windows Server 2008/Vista and onwards");
			return FAIL;
		}

		return SUCCEED;
	}

	if (0 == ChangeServiceConfig2(service, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &scdasi))
	{
		zbx_error("failed to configure service delayed auto-start %s: %s",
				(TRUE == scdasi.fDelayedAutostart ? "TRUE" : "FALSE"),
				zbx_strerror_from_system(GetLastError()));
		return FAIL;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: creates and installs Zabbix agent Windows service                 *
 *                                                                            *
 * Parameters: path        - [IN] path to Zabbix agent 2 binary file          *
 *             config_file - [IN] path to Zabbix agent 2 config file          *
 *             flags       - [IN] flags defined by command line options       *
 *                                                                            *
 * Return value: SUCCEED - installed and mandatory configuration set          *
 *               FAIL    - failed to install or configure service             *
 *                                                                            *
 ******************************************************************************/
int	ZabbixCreateService(const char *path, const char *config_file, unsigned int flags)
{
	SC_HANDLE		mgr, service;
	SERVICE_DESCRIPTION	sd;
	wchar_t			cmdLine[MAX_PATH];
	wchar_t			*wservice_name;
	DWORD			code, dwStartType;
	int			ret = SUCCEED;

	if (FAIL == svc_OpenSCManager(&mgr))
		return FAIL;

	svc_get_command_line(path, flags & ZBX_TASK_FLAG_MULTIPLE_AGENTS, cmdLine, MAX_PATH, config_file);

	wservice_name = zbx_utf8_to_unicode(get_zbx_service_name_cb());

	dwStartType = svc_start_type_get(flags);


	if (NULL == (service = CreateService(mgr, wservice_name, wservice_name, GENERIC_READ, SERVICE_WIN32_OWN_PROCESS,
			dwStartType, SERVICE_ERROR_NORMAL, cmdLine, NULL, NULL, NULL, NULL, NULL)))
	{
		if (ERROR_SERVICE_EXISTS == (code = GetLastError()))
			zbx_error("ERROR: service [%s] already exists", get_zbx_service_name_cb());
		else
		{
			zbx_error("ERROR: cannot create service [%s]: %s", get_zbx_service_name_cb(),
					zbx_strerror_from_system(code));
		}

		ret = FAIL;
	}
	else
	{
		zbx_error("service [%s] installed successfully", get_zbx_service_name_cb());
		CloseServiceHandle(service);

		/* update the service description */
		if (SUCCEED == svc_OpenService(mgr, &service, SERVICE_CHANGE_CONFIG))
		{
			sd.lpDescription = TEXT("Provides system monitoring");
			if (0 == ChangeServiceConfig2(service, SERVICE_CONFIG_DESCRIPTION, &sd))
			{
				zbx_error("service description update failed: %s",
						zbx_strerror_from_system(GetLastError()));
			}

			if (SERVICE_AUTO_START == dwStartType)
				ret = svc_delayed_autostart_config(service, flags);

			CloseServiceHandle(service);
		}
	}

	zbx_free(wservice_name);

	CloseServiceHandle(mgr);

	if (SUCCEED == ret)
		ret = svc_install_event_source(path);

	return ret;
}

static int	svc_RemoveEventSource()
{
	wchar_t	regkey[256];
	wchar_t	*wevent_source;
	int	ret = FAIL;

	wevent_source = zbx_utf8_to_unicode(get_zbx_event_source_cb());
	StringCchPrintf(regkey, ARRSIZE(regkey), EVENTLOG_REG_PATH TEXT("System\\%s"), wevent_source);
	zbx_free(wevent_source);

	if (ERROR_SUCCESS == RegDeleteKey(HKEY_LOCAL_MACHINE, regkey))
	{
		zbx_error("event source [%s] uninstalled successfully", get_zbx_event_source_cb());
		ret = SUCCEED;
	}
	else
	{
		zbx_error("unable to uninstall event source [%s]: %s",
				get_zbx_event_source_cb(), zbx_strerror_from_system(GetLastError()));
	}

	return ret;
}

int	ZabbixRemoveService(void)
{
	SC_HANDLE	mgr, service;
	int		ret = FAIL;

	if (FAIL == svc_OpenSCManager(&mgr))
		return ret;

	if (SUCCEED == svc_OpenService(mgr, &service, DELETE))
	{
		if (0 != DeleteService(service))
		{
			zbx_error("service [%s] uninstalled successfully", get_zbx_service_name_cb());
			ret = SUCCEED;
		}
		else
		{
			zbx_error("ERROR: cannot remove service [%s]: %s",
					get_zbx_service_name_cb(), zbx_strerror_from_system(GetLastError()));
		}

		CloseServiceHandle(service);
	}

	CloseServiceHandle(mgr);

	if (SUCCEED == ret)
		ret = svc_RemoveEventSource();

	return ret;
}

int	ZabbixStartService(void)
{
	SC_HANDLE	mgr, service;
	int		ret = FAIL;

	if (FAIL == svc_OpenSCManager(&mgr))
		return ret;

	if (SUCCEED == svc_OpenService(mgr, &service, SERVICE_START))
	{
		if (0 != StartService(service, 0, NULL))
		{
			zbx_error("service [%s] started successfully", get_zbx_service_name_cb());
			ret = SUCCEED;
		}
		else
		{
			zbx_error("ERROR: cannot start service [%s]: %s",
					get_zbx_service_name_cb(), zbx_strerror_from_system(GetLastError()));
		}

		CloseServiceHandle(service);
	}

	CloseServiceHandle(mgr);

	return ret;
}

int	ZabbixStopService(void)
{
	SC_HANDLE	mgr, service;
	SERVICE_STATUS	status;
	int		ret = FAIL;

	if (FAIL == svc_OpenSCManager(&mgr))
		return ret;

	if (SUCCEED == svc_OpenService(mgr, &service, SERVICE_STOP))
	{
		if (0 != ControlService(service, SERVICE_CONTROL_STOP, &status))
		{
			zbx_error("service [%s] stopped successfully", get_zbx_service_name_cb());
			ret = SUCCEED;
		}
		else
		{
			zbx_error("ERROR: cannot stop service [%s]: %s",
					get_zbx_service_name_cb(), zbx_strerror_from_system(GetLastError()));
		}

		CloseServiceHandle(service);
	}

	CloseServiceHandle(mgr);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: changes service startup type for installed service                *
 *                                                                            *
 * Parameters: flags - [IN] flags specifying service startup type to set      *
 *                                                                            *
 * Return value: SUCCEED - successfully set                                   *
 *               FAIL    - failed to set                                      *
 *                                                                            *
 ******************************************************************************/
int	zbx_service_startup_type_change(unsigned int flags)
{
	int		ret = SUCCEED;
	DWORD		dwStartType;
	SC_HANDLE	mgr, service;

	if (FAIL == svc_OpenSCManager(&mgr))
		return FAIL;

	if (SUCCEED != svc_OpenService(mgr, &service, SERVICE_CHANGE_CONFIG))
	{
		zbx_error("failed to set service startup type, failed to open service: %s",
				zbx_strerror_from_system(GetLastError()));
		ret = FAIL;
		goto close_mgr;
	}

	dwStartType = svc_start_type_get(flags);

	if (0 == ChangeServiceConfig(service, SERVICE_NO_CHANGE, dwStartType, SERVICE_NO_CHANGE, NULL, NULL,
			NULL, NULL, NULL, NULL, NULL))
	{
		zbx_error("failed to set service startup type: %s",
				zbx_strerror_from_system(GetLastError()));
		ret = FAIL;
		goto close_service;
	}

	if (SERVICE_AUTO_START == dwStartType)
		ret = svc_delayed_autostart_config(service, flags);
close_service:
	CloseServiceHandle(service);
close_mgr:
	CloseServiceHandle(mgr);

	if (SUCCEED == ret)
		zbx_error("service startup-type configured successfully");

	return ret;
}

void	zbx_set_parent_signal_handler(zbx_on_exit_t zbx_on_exit_cb_arg)
{
	zbx_on_exit_cb = zbx_on_exit_cb_arg;
	signal(SIGINT, parent_signal_handler);
	signal(SIGTERM, parent_signal_handler);
}

/******************************************************************************
 *                                                                            *
 * Purpose: set callback variables                                            *
 *                                                                            *
 * Parameters: get_zbx_service_name_f - [IN]                                  *
 *             get_zbx_event_source_f - [IN]                                  *
 *                                                                            *
 ******************************************************************************/
void	zbx_service_init(zbx_get_config_str_f get_zbx_service_name_f, zbx_get_config_str_f get_zbx_event_source_f)
{
	get_zbx_service_name_cb = get_zbx_service_name_f;
	get_zbx_event_source_cb = get_zbx_event_source_f;
}

/******************************************************************************
 *                                                                            *
 * Purpose: sets service startup type flags from command line option argument *
 *                                                                            *
 * Parameters: optarg - [IN]                                                  *
 *             flags  - [OUT]                                                 *
 *                                                                            *
 * Return value: SUCCEED - successfully set                                   *
 *               FAIL    - unknown argument                                   *
 *                                                                            *
 ******************************************************************************/
int	zbx_service_startup_flags_set(const char *optarg, unsigned int *flags) {
	*flags &= ~(ZBX_TASK_FLAG_SERVICE_ENABLED | ZBX_TASK_FLAG_SERVICE_AUTOSTART |
			ZBX_TASK_FLAG_SERVICE_AUTOSTART_DELAYED);

	if (0 == strcmp(optarg, ZBX_SERVICE_STARTUP_AUTOMATIC))
		*flags |= ZBX_TASK_FLAG_SERVICE_ENABLED | ZBX_TASK_FLAG_SERVICE_AUTOSTART;
	else if (0 == strcmp(optarg, ZBX_SERVICE_STARTUP_DELAYED))
	{
		*flags |= ZBX_TASK_FLAG_SERVICE_ENABLED | ZBX_TASK_FLAG_SERVICE_AUTOSTART |
			ZBX_TASK_FLAG_SERVICE_AUTOSTART_DELAYED;
	}
	else if (0 == strcmp(optarg, ZBX_SERVICE_STARTUP_MANUAL))
		*flags |= ZBX_TASK_FLAG_SERVICE_ENABLED;
	else if (0 != strcmp(optarg, ZBX_SERVICE_STARTUP_DISABLED))
	{
		zbx_error("unknown startup-type option argument, allowed values: %s, %s, %s, %s",
				ZBX_SERVICE_STARTUP_AUTOMATIC, ZBX_SERVICE_STARTUP_DELAYED, ZBX_SERVICE_STARTUP_MANUAL,
				ZBX_SERVICE_STARTUP_DISABLED);
		return FAIL;
	}

	return SUCCEED;
}
