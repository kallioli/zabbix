"""Measure what ICMP checks cost the pinger, with the stand-in server.

The report that motivated this: two production proxies with 27 and 12 ICMP
hosts kept the "utilization of icmp pinger processes over 75%" trigger up,
and going from one pinger to three barely moved the figure. This drives the
real binary with a realistic ICMP population, reads back the proxy's own
busy figure, and times every batch from the debug log, so that a change to
the pinger can be judged on numbers rather than on a theory about them.

Each host carries the three items of the stock ICMP template (icmpping,
icmppingloss, icmppingsec, default parameters, one minute), on its own
address. Most addresses answer (loopback), a few never do (TEST-NET), the way
a real population has a down host or two.

Usage:
    python pinger_load_check.py --exe bin/win64/zabbix_proxy.exe
        [--hosts 30] [--dead 3] [--pingers 1] [--minutes 4]
"""
import argparse
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from datetime import datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import proxy_selftest as st  # noqa: E402

ITEM_TYPE_SIMPLE, ITEM_TYPE_INTERNAL = 3, 5
VALUE_FLOAT, VALUE_UINT64 = 0, 3

BUSY_AVG = "zabbix[process,icmp pinger,avg,busy]"
BUSY_MAX = "zabbix[process,icmp pinger,max,busy]"

CONFIG = st.CONFIG + """\
StartPingers={pingers}
StartPollers=1
StartPollersUnreachable=1
StartHTTPPollers=0
StartDiscoverers=0
StartDBSyncers=1
StartPreprocessors=1
"""


def build_population(schema, trapper_port, hosts, dead):
    """hosts ICMP hosts spread across the minute, the last `dead` of them unreachable."""
    s = schema
    host_rows, iface_rows, item_rows, rt_rows = [], [], [], []
    checks = {}
    for i in range(1, hosts + 1):
        hostid, interfaceid = 50000 + i, 60000 + i * 7  # ids stride 7: nextchecks spread over the minute
        alive = i <= hosts - dead
        ip = f"127.0.0.{i}" if alive else f"192.0.2.{i}"
        host_rows.append({"hostid": hostid, "host": f"icmp-{i:02d}", "name": f"icmp-{i:02d}", "status": 0})
        iface_rows.append({"interfaceid": interfaceid, "hostid": hostid, "main": 1, "type": 1,
                           "useip": 1, "ip": ip, "dns": "", "port": "10050", "available": 1})
        for k, (key, vtype) in enumerate((("icmpping", VALUE_UINT64), ("icmppingloss", VALUE_FLOAT),
                                          ("icmppingsec", VALUE_FLOAT))):
            itemid = 70000 + i * 10 + k
            item_rows.append({"itemid": itemid, "hostid": hostid, "type": ITEM_TYPE_SIMPLE,
                              "key_": key, "params": "", "delay": "1m", "status": 0,
                              "value_type": vtype, "history": "1d", "timeout": "3s",
                              "interfaceid": interfaceid})
            rt_rows.append({"itemid": itemid})
            checks[itemid] = {"key": key, "host": i, "alive": alive}

    # the proxy's own view of the pinger, on the first host
    for k, key in enumerate((BUSY_AVG, BUSY_MAX)):
        itemid = 80001 + k
        item_rows.append({"itemid": itemid, "hostid": 50001, "type": ITEM_TYPE_INTERNAL,
                          "key_": key, "params": "", "delay": "1m", "status": 0,
                          "value_type": VALUE_FLOAT, "history": "1d", "timeout": "3s",
                          "interfaceid": None})
        rt_rows.append({"itemid": itemid})
        checks[itemid] = {"key": key, "host": 1, "alive": True}

    config = {name: s.table(name, []) for name in
              ("host_inventory", "interface_snmp", "item_parameter", "hosts_templates",
               "hostmacro", "item_preproc")}
    config["hosts"] = s.table("hosts", host_rows)
    config["interface"] = s.table("interface", iface_rows)
    config["items"] = s.table("items", item_rows)
    config["item_rtdata"] = s.table("item_rtdata", rt_rows)
    return config, checks


class QuietSession(st.Session):
    """The selftest session, without a line per value: 90 values a minute is too many to read."""

    def __init__(self, config, checks):
        self.config = config
        self.checks = checks
        self.keys = {itemid: c["key"] for itemid, c in checks.items()}
        self.revision = 1
        self.served = 0
        self.alive = set(self.keys)
        self.values = {itemid: [] for itemid in self.keys}
        self.configs = 0
        self.datas = 0
        self.lock = threading.Lock()

    def on_data(self, message):
        with self.lock:
            self.datas += 1
            for entry in message.get("history data", []):
                itemid = int(entry.get("itemid", 0))
                if itemid in self.values:
                    self.values[itemid].append((entry.get("clock"), entry.get("value")))
        return {"response": "success", "upload": "enabled"}


LOG_LINE = re.compile(r"^\s*(\d+):(\d{8}):(\d{6})\.(\d{3}) (.*)$")


def parse_batches(log_path):
    """(start, end, hosts) for every zbx_ping() call, from a DebugLevel=4 log."""
    open_calls, batches = {}, []
    for line in log_path.read_text(encoding="utf-8", errors="replace").splitlines():
        m = LOG_LINE.match(line)
        if not m:
            continue
        tid, day, hms, ms, text = m.groups()
        t = datetime.strptime(day + hms, "%Y%m%d%H%M%S").timestamp() + int(ms) / 1000.0
        if text.startswith("In zbx_ping() hosts_count:"):
            open_calls[tid] = (t, int(text.rsplit(":", 1)[1]))
        elif text.startswith("End of zbx_ping()") and tid in open_calls:
            start, n = open_calls.pop(tid)
            batches.append((start, t, n))
    return batches


def main():
    here = Path(__file__).resolve()
    repo = here.parents[3]
    p = argparse.ArgumentParser()
    p.add_argument("--exe", default=str(repo / "bin" / "win64" / "zabbix_proxy.exe"))
    p.add_argument("--schema", default=str(repo / "create" / "src" / "schema.tmpl"))
    p.add_argument("--hosts", type=int, default=30)
    p.add_argument("--dead", type=int, default=3, help="how many of the hosts never answer")
    p.add_argument("--pingers", type=int, default=1)
    p.add_argument("--minutes", type=int, default=4, help="how long to let it run")
    p.add_argument("--server-port", type=int, default=10061)
    p.add_argument("--trapper-port", type=int, default=10051)
    p.add_argument("--keep", action="store_true")
    args = p.parse_args()

    exe = Path(args.exe)
    if not exe.is_file():
        print(f"no proxy binary at {exe}", file=sys.stderr)
        return 2

    workdir = Path(tempfile.mkdtemp(prefix="zbx_pinger_"))
    conf = workdir / "zabbix_proxy.conf"
    conf.write_text(CONFIG.format(server_port=args.server_port, trapper_port=args.trapper_port,
                                  debug_level=4, workdir=workdir.as_posix(), pingers=args.pingers),
                    encoding="ascii")

    schema = st.Schema(args.schema)
    config, checks = build_population(schema, args.trapper_port, args.hosts, args.dead)
    session = QuietSession(config, checks)

    print(f"pinger load: {args.hosts} hosts ({args.dead} unreachable), 3 ICMP items each, "
          f"StartPingers={args.pingers}, {args.minutes} min", flush=True)

    proxy, t_start = None, time.time()
    try:
        with st.Server(("127.0.0.1", args.server_port), st.Handler) as srv:
            srv.session = session
            threading.Thread(target=srv.serve_forever, daemon=True).start()
            console = open(workdir / "console.log", "wb")
            proxy = subprocess.Popen([str(exe), "-f", "-c", str(conf)],
                                     stdout=console, stderr=subprocess.STDOUT)
            deadline = time.monotonic() + args.minutes * 60
            while time.monotonic() < deadline:
                if proxy.poll() is not None:
                    print(f"proxy exited early with code {proxy.returncode}", flush=True)
                    break
                time.sleep(1)
            srv.shutdown()
    finally:
        if proxy is not None and proxy.poll() is None:
            proxy.terminate()
            try:
                proxy.wait(timeout=15)
            except subprocess.TimeoutExpired:
                proxy.kill()

    failures = []

    # ------------------------------------------------- what the proxy says
    busy = session.values[80001]
    print("\nproxy's own figure, zabbix[process,icmp pinger,avg,busy], one value a minute:")
    for clock, value in busy:
        print(f"    {datetime.fromtimestamp(int(clock)).strftime('%H:%M:%S')}  {float(value):6.2f} %")
    settled = [float(v) for c, v in busy[2:]]  # the first two minutes include start-up and scheduling
    if settled:
        avg_busy = sum(settled) / len(settled)
        print(f"    average once settled: {avg_busy:.1f} %")
    else:
        avg_busy = None
        failures.append("no busy figure came back after the warm-up")

    # -------------------------------------------- what the log says it did
    batches = parse_batches(workdir / "zabbix_proxy.log")
    if batches:
        t0 = min(b[0] for b in batches)
        t1 = max(b[1] for b in batches)
        span = max(t1 - t0, 1.0)
        in_ping = sum(e - s for s, e, n in batches)
        sizes = [n for s, e, n in batches]
        durs = [e - s for s, e, n in batches]
        print(f"\nzbx_ping() calls over {span:.0f} s: {len(batches)} "
              f"({60 * len(batches) / span:.1f} a minute)")
        print(f"    hosts per call: min {min(sizes)}, avg {sum(sizes) / len(sizes):.1f}, max {max(sizes)}")
        print(f"    seconds per call: min {min(durs):.2f}, avg {sum(durs) / len(durs):.2f}, max {max(durs):.2f}")
        print(f"    time inside zbx_ping(): {100 * in_ping / span / args.pingers:.1f} % of {args.pingers} pinger(s)")
        # the template defaults: 3 packets one second apart, 500 ms to wait for the last one;
        # a run that lasts longer is waiting for a dead host at every round instead of once
        if max(durs) > 2.0 + 0.5 + 0.7:
            failures.append(f"a zbx_ping() call took {max(durs):.2f} s, more than count, interval and timeout allow")
    else:
        failures.append("no zbx_ping() call found in the log")

    # ------------------------------------------------- what came back
    by_key = {"icmpping": {True: [], False: []}, "icmppingloss": {True: [], False: []},
              "icmppingsec": {True: [], False: []}}
    for itemid, c in checks.items():
        if c["key"] in by_key:
            by_key[c["key"]][c["alive"]] += [float(v) for _, v in session.values[itemid]]
    print("\nvalues, reachable hosts / unreachable hosts:")
    for key, d in by_key.items():
        up, down = d[True], d[False]
        f = lambda xs: (f"n={len(xs)} min {min(xs):.4g} max {max(xs):.4g}" if xs else "nothing")
        print(f"    {key:13s} {f(up):40s} | {f(down)}")
    if not by_key["icmpping"][True] or any(v != 1 for v in by_key["icmpping"][True]):
        failures.append("icmpping on a reachable host did not always give 1")
    if args.dead and (not by_key["icmpping"][False] or any(v != 0 for v in by_key["icmpping"][False])):
        failures.append("icmpping on an unreachable host did not always give 0")
    if args.dead and any(v != 100 for v in by_key["icmppingloss"][False]):
        failures.append("icmppingloss on an unreachable host was not 100")

    # ------------------------------------------------------------ verdict
    print()
    limit = 10.0
    if avg_busy is not None:
        ok = avg_busy < limit
        print(f"  {'ok  ' if ok else 'FAIL'}  pinger busy {avg_busy:.1f} % (limit {limit:.0f} %) "
              f"for {args.hosts} hosts on {args.pingers} pinger(s)")
        if not ok:
            failures.append(f"pinger busy {avg_busy:.1f} %")
    for line in failures:
        print(f"  FAIL  {line}")

    if args.keep:
        print(f"\nwork directory kept at {workdir}")
    else:
        shutil.rmtree(workdir, ignore_errors=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
