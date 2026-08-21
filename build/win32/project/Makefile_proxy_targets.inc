all: $(PREBUILD) $(PROGNAME) $(POSTBUILD)

clean: $(POSTCLEAN)
# break this down because del has limit for command-line args length (NMAKE : fatal error U1077: 'del' : return code '0x1')
	@<<zbx_del_obj.bat
	del $(OBJS) 2>NUL
<<
	del $(RESOURCE_RES) $(MESSAGES_H) $(PROGNAME) messages.rc 2>NUL
	del MSG00001.bin $(TARGETNAME).$(TARGETEXT).manifest $(PDBS) $(TARGETNAME).$(TARGETEXT).idb 2>NUL

# the proxy links 350+ objects, which overflows the command line - use a response file
$(PROGNAME): $(MESSAGES_H) $(OBJS) $(RESOURCE_RES)
	$(LINKER) @<<zbx_link_proxy.rsp
$(OBJS) $(RESOURCE_RES) $(LFLAGS) /OUT:$(PROGNAME) $(LIBS)
<<
	$(MT) /outputresource:"$(PROGNAME);#1" /manifest $(TARGETNAME).$(TARGETEXT).manifest /nologo

$(MESSAGES_H): messages.mc
	$(MC) -U -h ".\\" -r ".\\" messages.mc

$(RESOURCE_RES): $(RESOURCE_RC) $(RESOURCE_H) $(DESC_H)
	$(RC) $(RFLAGS) /d "NDEBUG" /d "_VC80_UPGRADE=0x0600" /l 0x419 /fo"$(RESOURCE_RES)" $(RESOURCE_RC)

{..\..\..\src\libs\zbxagentget}.c{..\..\..\src\libs\zbxagentget}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxalgo}.c{..\..\..\src\libs\zbxalgo}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxasynchttppoller}.c{..\..\..\src\libs\zbxasynchttppoller}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxasyncpoller}.c{..\..\..\src\libs\zbxasyncpoller}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxaudit}.c{..\..\..\src\libs\zbxaudit}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxautoreg}.c{..\..\..\src\libs\zbxautoreg}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxavailability}.c{..\..\..\src\libs\zbxavailability}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxbincommon}.c{..\..\..\src\libs\zbxbincommon}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcacheconfig}.c{..\..\..\src\libs\zbxcacheconfig}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcachehistory}.c{..\..\..\src\libs\zbxcachehistory}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcachevalue}.c{..\..\..\src\libs\zbxcachevalue}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcfg}.c{..\..\..\src\libs\zbxcfg}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcommon}.c{..\..\..\src\libs\zbxcommon}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcomms}.c{..\..\..\src\libs\zbxcomms}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcommshigh}.c{..\..\..\src\libs\zbxcommshigh}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcompress}.c{..\..\..\src\libs\zbxcompress}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxconnector}.c{..\..\..\src\libs\zbxconnector}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcrypto}.c{..\..\..\src\libs\zbxcrypto}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxcurl}.c{..\..\..\src\libs\zbxcurl}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdb}.c{..\..\..\src\libs\zbxdb}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdbhigh}.c{..\..\..\src\libs\zbxdbhigh}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdbschema}.c{..\..\..\src\libs\zbxdbschema}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdbsyncer}.c{..\..\..\src\libs\zbxdbsyncer}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdbupgrade}.c{..\..\..\src\libs\zbxdbupgrade}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdbwrap}.c{..\..\..\src\libs\zbxdbwrap}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdiag}.c{..\..\..\src\libs\zbxdiag}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdiscoverer}.c{..\..\..\src\libs\zbxdiscoverer}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxdiscovery}.c{..\..\..\src\libs\zbxdiscovery}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxembed}.c{..\..\..\src\libs\zbxembed}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxescalations}.c{..\..\..\src\libs\zbxescalations}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxeval}.c{..\..\..\src\libs\zbxeval}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxevent}.c{..\..\..\src\libs\zbxevent}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxexec}.c{..\..\..\src\libs\zbxexec}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxexport}.c{..\..\..\src\libs\zbxexport}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxexpr}.c{..\..\..\src\libs\zbxexpr}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxexpression}.c{..\..\..\src\libs\zbxexpression}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxfile}.c{..\..\..\src\libs\zbxfile}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxfile\win32}.c{..\..\..\src\libs\zbxfile\win32}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxgetopt}.c{..\..\..\src\libs\zbxgetopt}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxhash}.c{..\..\..\src\libs\zbxhash}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxhistory}.c{..\..\..\src\libs\zbxhistory}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxhttp}.c{..\..\..\src\libs\zbxhttp}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxhttppoller}.c{..\..\..\src\libs\zbxhttppoller}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxicmpping}.c{..\..\..\src\libs\zbxicmpping}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxinterface}.c{..\..\..\src\libs\zbxinterface}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxip}.c{..\..\..\src\libs\zbxip}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxipcservice}.c{..\..\..\src\libs\zbxipcservice}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxjson}.c{..\..\..\src\libs\zbxjson}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxkvs}.c{..\..\..\src\libs\zbxkvs}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxlog}.c{..\..\..\src\libs\zbxlog}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxmodules}.c{..\..\..\src\libs\zbxmodules}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxmutexs}.c{..\..\..\src\libs\zbxmutexs}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxnum}.c{..\..\..\src\libs\zbxnum}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxodbc}.c{..\..\..\src\libs\zbxodbc}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxparam}.c{..\..\..\src\libs\zbxparam}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxpgservice}.c{..\..\..\src\libs\zbxpgservice}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxpinger}.c{..\..\..\src\libs\zbxpinger}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxpoller}.c{..\..\..\src\libs\zbxpoller}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxpreproc}.c{..\..\..\src\libs\zbxpreproc}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxpreprocbase}.c{..\..\..\src\libs\zbxpreprocbase}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxprof}.c{..\..\..\src\libs\zbxprof}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxprometheus}.c{..\..\..\src\libs\zbxprometheus}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxproxybuffer}.c{..\..\..\src\libs\zbxproxybuffer}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxregexp}.c{..\..\..\src\libs\zbxregexp}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxrtc}.c{..\..\..\src\libs\zbxrtc}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxscripts}.c{..\..\..\src\libs\zbxscripts}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxself}.c{..\..\..\src\libs\zbxself}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxserialize}.c{..\..\..\src\libs\zbxserialize}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxshmem}.c{..\..\..\src\libs\zbxshmem}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxsnmptrapper}.c{..\..\..\src\libs\zbxsnmptrapper}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxstats}.c{..\..\..\src\libs\zbxstats}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxstr}.c{..\..\..\src\libs\zbxstr}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxsysinfo}.c{..\..\..\src\libs\zbxsysinfo}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxsysinfo\alias}.c{..\..\..\src\libs\zbxsysinfo\alias}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxsysinfo\common}.c{..\..\..\src\libs\zbxsysinfo\common}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxsysinfo\simple}.c{..\..\..\src\libs\zbxsysinfo\simple}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxsysinfo\win32}.c{..\..\..\src\libs\zbxsysinfo\win32}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxtagfilter}.c{..\..\..\src\libs\zbxtagfilter}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxtasks}.c{..\..\..\src\libs\zbxtasks}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxthreads}.c{..\..\..\src\libs\zbxthreads}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxtime}.c{..\..\..\src\libs\zbxtime}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxtimekeeper}.c{..\..\..\src\libs\zbxtimekeeper}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxtrapper}.c{..\..\..\src\libs\zbxtrapper}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxtrends}.c{..\..\..\src\libs\zbxtrends}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxvariant}.c{..\..\..\src\libs\zbxvariant}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxvault}.c{..\..\..\src\libs\zbxvault}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxversion}.c{..\..\..\src\libs\zbxversion}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxvmware}.c{..\..\..\src\libs\zbxvmware}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxwin}.c{..\..\..\src\libs\zbxwin}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxwin32}.c{..\..\..\src\libs\zbxwin32}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxwinservice}.c{..\..\..\src\libs\zbxwinservice}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\libs\zbxxml}.c{..\..\..\src\libs\zbxxml}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\autoreg}.c{..\..\..\src\zabbix_proxy\autoreg}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\cachehistory}.c{..\..\..\src\zabbix_proxy\cachehistory}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\datasender}.c{..\..\..\src\zabbix_proxy\datasender}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\diag}.c{..\..\..\src\zabbix_proxy\diag}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\discovery}.c{..\..\..\src\zabbix_proxy\discovery}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\housekeeper}.c{..\..\..\src\zabbix_proxy\housekeeper}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\poller}.c{..\..\..\src\zabbix_proxy\poller}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\preproc}.c{..\..\..\src\zabbix_proxy\preproc}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\proxyconfig}.c{..\..\..\src\zabbix_proxy\proxyconfig}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\proxyconfigwrite}.c{..\..\..\src\zabbix_proxy\proxyconfigwrite}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\rtc}.c{..\..\..\src\zabbix_proxy\rtc}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\stats}.c{..\..\..\src\zabbix_proxy\stats}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\taskmanager}.c{..\..\..\src\zabbix_proxy\taskmanager}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{..\..\..\src\zabbix_proxy\trapper}.c{..\..\..\src\zabbix_proxy\trapper}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)

{$(PROJECTDIR)}.c{$(PROJECTDIR)}.o:
	$(CC) $? /Fo"$@" $(CFLAGS)
