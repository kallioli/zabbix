# Moved: Zabbix proxy for Windows

The Windows build of the Zabbix proxy that used to live in this fork has its
own repository now:

**https://github.com/kallioli/zabbix-proxy-windows**

Downloads, documentation, issues and new releases are there. The new repository
does not copy the Zabbix source: it holds the pinned upstream version plus the
Windows changes as patches, which makes them easy to read and to carry onto
each new Zabbix release.

The releases published here (`win-proxy-*` tags) stay available but will not be
updated. The `feature/windows-proxy` branch is kept for history; the same code
is the first commit of the new repository.

This repository is otherwise a plain fork of [zabbix/zabbix](https://github.com/zabbix/zabbix).
Unofficial community build, not affiliated with or endorsed by Zabbix SIA.
