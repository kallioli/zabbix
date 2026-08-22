"""Prove that a built proxy actually collects, by standing in for the server.

Starting and connecting only shows the handshake works. This drives the whole
path a value travels - configuration sync, scheduling, polling, preprocessing,
the history cache and the data sender - and fails if nothing comes back.

Nothing external is needed. The proxy is given two items it can answer by
itself:

  * an internal item, which it reads from its own counters;
  * a simple check against its own trapper port, which goes through a poller
    and the network stack.

The configuration tables are built from create/src/schema.tmpl rather than
written out here. A proxy matches the field list positionally against the
ZBX_PROXY columns and rejects the whole payload on the first mismatch, so a
hand-kept list would rot silently.

Usage:
    python proxy_selftest.py --exe bin/win64/zabbix_proxy.exe [--timeout 90]
"""
import argparse
import json
import os
import re
import shutil
import socketserver
import struct
import subprocess
import sys
import tempfile
import threading
import time
import zlib
from pathlib import Path

PROTOCOL, COMPRESS, LARGE = 0x01, 0x02, 0x04

# A second host costs almost nothing here and covers the case of more than
# one, which the configuration write path treats differently.
HOSTS = {1: (10001, 20001, "win-proxy-selftest"),
         2: (10002, 20002, "win-proxy-selftest-2")}

ITEM_TYPE_SIMPLE, ITEM_TYPE_INTERNAL, ITEM_TYPE_CALCULATED = 3, 5, 15
ITEM_VALUE_TYPE_FLOAT, ITEM_VALUE_TYPE_UINT64 = 0, 3
ZBX_PREPROC_MULTIPLIER, ZBX_PREPROC_SCRIPT = 1, 21

MACRO = "{$TRAPPER_PORT}"


def is_number(v):
    try:
        float(v)
        return True
    except (TypeError, ValueError):
        return False


def equals(expected):
    return (lambda v: is_number(v) and float(v) == expected,
            f"exactly {expected}")


# What the proxy is asked to do, and what each answer would prove. The keys use
# a user macro so that resolving it is part of what gets tested.
CHECKS = [
    {
        "itemid": 30001,
        "proves": "internal poller",
        "type": ITEM_TYPE_INTERNAL,
        "key": "zabbix[wcache,values]",
        "value_type": ITEM_VALUE_TYPE_UINT64,
        "expect": (is_number, "a number"),
    },
    {
        "itemid": 30002,
        "proves": "simple check poller, and user macro resolution",
        "type": ITEM_TYPE_SIMPLE,
        "key": f"net.tcp.service[tcp,127.0.0.1,{MACRO}]",
        "value_type": ITEM_VALUE_TYPE_UINT64,
        "needs_interface": True,
        "expect": equals(1),
    },
    {
        "itemid": 30003,
        "proves": "JavaScript preprocessing, so the embedded engine works",
        "type": ITEM_TYPE_SIMPLE,
        "key": f"net.tcp.service.perf[tcp,127.0.0.1,{MACRO}]",
        "value_type": ITEM_VALUE_TYPE_UINT64,
        "needs_interface": True,
        # Reads the polled value, so a step that never ran cannot pass by luck.
        "preproc": [(ZBX_PREPROC_SCRIPT, "return value >= 0 ? 42 : -1;")],
        "expect": equals(42),
    },
    {
        "itemid": 30004,
        "proves": "multiplier preprocessing, on a second host",
        "host": 2,
        "type": ITEM_TYPE_SIMPLE,
        "key": f"net.tcp.service[tcp,127.0.0.1,{MACRO}]",
        "value_type": ITEM_VALUE_TYPE_UINT64,
        "needs_interface": True,
        # The check reports 1, so seven is the only answer a working step gives.
        "preproc": [(ZBX_PREPROC_MULTIPLIER, "7")],
        "expect": equals(7),
    },
    {
        "itemid": 30005,
        "proves": "the expression evaluator, on a calculated item",
        "type": ITEM_TYPE_CALCULATED,
        "key": "selftest.calculated",
        "params": f"last(//net.tcp.service[tcp,127.0.0.1,{MACRO}]) * 41 + 1",
        "value_type": ITEM_VALUE_TYPE_UINT64,
        "expect": equals(42),
    },
]


# --------------------------------------------------------------- the schema

class Schema:
    """The subset of create/src/schema.tmpl this test needs."""

    def __init__(self, tmpl):
        self.tables = {}
        table = None
        for line in Path(tmpl).read_text(encoding="utf-8", errors="replace").splitlines():
            parts = [p.strip() for p in line.split("|")]
            if parts[0] == "TABLE" and len(parts) >= 3:
                table = {"pk": parts[2], "fields": []}
                self.tables[parts[1]] = table
            elif parts[0] == "FIELD" and table is not None and len(parts) >= 6:
                table["fields"].append({"name": parts[1], "default": parts[3],
                                        "null": parts[4], "flags": parts[5]})
            elif not line.strip():
                table = None

    def fields(self, name):
        """Primary key, then every ZBX_PROXY column, in schema order."""
        t = self.tables[name]
        out = [t["pk"]]
        out += [f["name"] for f in t["fields"]
                if f["name"] != t["pk"] and "ZBX_PROXY" in re.split(r"[,\s]+", f["flags"])]
        return out

    def _default(self, table, field):
        for f in self.tables[table]["fields"]:
            if f["name"] == field:
                d = f["default"]
                if d == "":
                    return None if f["null"] == "NULL" else ""
                d = d.strip("'")
                return int(d) if re.fullmatch(r"-?\d+", d) else d
        return None

    def table(self, name, rows):
        fields = self.fields(name)
        for r in rows:
            unknown = set(r) - set(fields)
            if unknown:
                raise KeyError(f"{name}: not sent to proxies: {sorted(unknown)}")
        return {"fields": fields,
                "data": [[r.get(f, self._default(name, f)) for f in fields] for r in rows]}


def build_config(schema, trapper_port, checks):
    s = schema
    # Sending hosts commits to sending its companions: the proxy refuses the
    # whole payload if one is absent, even empty. It fills in the httptest
    # family itself. hostmacro drags hosts_templates along the same way.
    config = {name: s.table(name, []) for name in
              ("host_inventory", "interface_snmp", "item_parameter", "hosts_templates")}

    # Each item carries a runtime-data row on the server side. Without one the
    # proxy stores the item but never schedules it.
    config["item_rtdata"] = s.table("item_rtdata",
                                    [{"itemid": c["itemid"]} for c in checks])

    used = sorted({c.get("host", 1) for c in checks})

    config["hosts"] = s.table("hosts", [
        {"hostid": HOSTS[h][0], "host": HOSTS[h][2], "name": HOSTS[h][2], "status": 0}
        for h in used])

    config["interface"] = s.table("interface", [
        {"interfaceid": HOSTS[h][1], "hostid": HOSTS[h][0], "main": 1, "type": 1,
         "useip": 1, "ip": "127.0.0.1", "dns": "", "port": str(trapper_port),
         "available": 1}
        for h in used])

    # The port reaches the item keys through this, so resolving it is part of
    # what the run proves.
    config["hostmacro"] = s.table("hostmacro", [
        {"hostmacroid": 40000 + h, "hostid": HOSTS[h][0], "macro": MACRO,
         "value": str(trapper_port), "type": 0, "automatic": 0}
        for h in used])

    config["items"] = s.table("items", [
        {"itemid": c["itemid"], "hostid": HOSTS[c.get("host", 1)][0], "type": c["type"],
         "key_": c["key"], "params": c.get("params", ""), "delay": "5s",
         "status": 0, "value_type": c["value_type"], "history": "1d",
         "timeout": "3s",
         "interfaceid": HOSTS[c.get("host", 1)][1] if c.get("needs_interface") else None}
        for c in checks])

    preproc = []
    for c in checks:
        for step, (ptype, params) in enumerate(c.get("preproc", []), start=1):
            preproc.append({"item_preprocid": c["itemid"] * 10 + step,
                            "itemid": c["itemid"], "step": step, "type": ptype,
                            "params": params, "error_handler": 0,
                            "error_handler_params": ""})
    config["item_preproc"] = s.table("item_preproc", preproc)

    return config


# ----------------------------------------------------------------- the wire

def recv_exactly(conn, n):
    buf = b""
    while len(buf) < n:
        chunk = conn.recv(n - len(buf))
        if not chunk:
            raise EOFError("peer closed mid-frame")
        buf += chunk
    return buf


def read_frame(conn):
    header = recv_exactly(conn, 5)
    if header[:4] != b"ZBXD":
        raise ValueError(f"not a ZBXD frame: {header!r}")
    flags = header[4]
    width, fmt = (8, "<Q") if flags & LARGE else (4, "<I")
    length = struct.unpack(fmt, recv_exactly(conn, width))[0]
    recv_exactly(conn, width)
    payload = recv_exactly(conn, length)
    if flags & COMPRESS:
        payload = zlib.decompress(payload)
    return json.loads(payload.decode("utf-8"))


def send_frame(conn, obj):
    payload = json.dumps(obj).encode("utf-8")
    body = zlib.compress(payload)
    header = b"ZBXD" + bytes([PROTOCOL | COMPRESS])
    header += struct.pack("<I", len(body)) + struct.pack("<I", len(payload))
    conn.sendall(header + body)


# ---------------------------------------------------------------- the state

class Session:
    def __init__(self, config, checks):
        self.config = config
        self.checks = {c["itemid"]: c for c in checks}
        self.keys = {c["itemid"]: c["key"] for c in checks}
        self.revision = 1
        self.values = {itemid: [] for itemid in self.keys}
        self.configs = 0
        self.datas = 0
        self.lock = threading.Lock()

    def on_config(self, message):
        with self.lock:
            self.configs += 1
            n = self.configs
        seen = int(message.get("config_revision", 0))
        if seen >= self.revision:
            return {"config_revision": self.revision}
        print(f"<- proxy config #{n}: sending {len(self.keys)} items", flush=True)
        return {"config_revision": self.revision, "full_sync": 1, "data": self.config}

    def on_data(self, message):
        with self.lock:
            self.datas += 1
        for entry in message.get("history data", []):
            itemid = int(entry.get("itemid", 0))
            with self.lock:
                if itemid in self.values:
                    self.values[itemid].append(entry.get("value"))
                    print(f"<- {self.keys[itemid]} = {entry.get('value')}", flush=True)
        return {"response": "success", "upload": "enabled"}

    def complete(self):
        with self.lock:
            return all(self.values[i] for i in self.keys)

    def report(self):
        print(f"\nconfig requests: {self.configs}   data requests: {self.datas}\n", flush=True)
        failures = 0
        for itemid, check in self.checks.items():
            got = self.values[itemid]
            predicate, described = check["expect"]
            if not got:
                verdict, detail = "FAIL", f"nothing collected, wanted {described}"
            elif not predicate(got[-1]):
                verdict, detail = "FAIL", f"got {got[-1]!r}, wanted {described}"
            else:
                verdict, detail = "ok", f"= {got[-1]}"
            if verdict == "FAIL":
                failures += 1
            print(f"  {verdict:4s}  {check['proves']:52s} {detail}", flush=True)
        if failures:
            print(f"\n{failures} of {len(self.checks)} checks failed", flush=True)
        else:
            print(f"\nall {len(self.checks)} checks passed", flush=True)
        return 1 if failures else 0


class Handler(socketserver.BaseRequestHandler):
    def handle(self):
        try:
            message = read_frame(self.request)
        except (EOFError, ValueError, zlib.error, json.JSONDecodeError) as e:
            print(f"  ! malformed frame: {e}", flush=True)
            return
        request = message.get("request", "?")
        session = self.server.session
        if request == "proxy config":
            reply = session.on_config(message)
        elif request == "proxy data":
            reply = session.on_data(message)
        else:
            reply = {"response": "failed", "info": f"unsupported request '{request}'"}
        try:
            send_frame(self.request, reply)
        except OSError:
            pass


class Server(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


# ----------------------------------------------------------------- the test

CONFIG = """\
ProxyMode=0
Server=127.0.0.1:{server_port}
Hostname=win-proxy-selftest
ListenPort={trapper_port}
ListenIP=127.0.0.1
DBName={workdir}/proxy.db
LogType=file
LogFile={workdir}/zabbix_proxy.log
LogFileSize=0
DebugLevel=3
ProxyConfigFrequency=10
DataSenderFrequency=1
"""


def main():
    here = Path(__file__).resolve()
    repo = here.parents[3]

    p = argparse.ArgumentParser()
    p.add_argument("--exe", default=str(repo / "bin" / "win64" / "zabbix_proxy.exe"))
    p.add_argument("--schema", default=str(repo / "create" / "src" / "schema.tmpl"))
    p.add_argument("--timeout", type=int, default=90)
    p.add_argument("--server-port", type=int, default=10061)
    p.add_argument("--trapper-port", type=int, default=10051)
    p.add_argument("--keep", action="store_true", help="leave the work directory behind")
    p.add_argument("--only", help="run only the checks whose description contains this")
    args = p.parse_args()

    exe = Path(args.exe)
    if not exe.is_file():
        print(f"no proxy binary at {exe}", file=sys.stderr)
        return 2

    workdir = Path(tempfile.mkdtemp(prefix="zbx_selftest_"))
    conf = workdir / "zabbix_proxy.conf"
    conf.write_text(CONFIG.format(server_port=args.server_port,
                                  trapper_port=args.trapper_port,
                                  workdir=workdir.as_posix()), encoding="ascii")

    checks = [c for c in CHECKS if not args.only or args.only in c["proves"]]
    if not checks:
        print(f"nothing matches --only {args.only!r}", file=sys.stderr)
        return 2

    config = build_config(Schema(args.schema), args.trapper_port, checks)
    session = Session(config, checks)

    proxy = None
    try:
        with Server(("127.0.0.1", args.server_port), Handler) as srv:
            srv.session = session
            threading.Thread(target=srv.serve_forever, daemon=True).start()
            print(f"stand-in server on 127.0.0.1:{args.server_port}", flush=True)

            proxy = subprocess.Popen([str(exe), "-f", "-c", str(conf)],
                                     stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
            print(f"proxy started, pid {proxy.pid}", flush=True)

            deadline = time.monotonic() + args.timeout
            while time.monotonic() < deadline:
                if proxy.poll() is not None:
                    print(f"proxy exited early with code {proxy.returncode}", flush=True)
                    break
                if session.complete():
                    print("\nevery item reported", flush=True)
                    break
                time.sleep(0.5)
            srv.shutdown()
    finally:
        if proxy is not None and proxy.poll() is None:
            proxy.terminate()
            try:
                proxy.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proxy.kill()

    status = session.report()

    log = workdir / "zabbix_proxy.log"
    if status and log.is_file():
        lines = log.read_text(encoding="utf-8", errors="replace").splitlines()
        # The tail is usually thread start-up noise. What matters is whatever
        # the proxy complained about, wherever in the run that happened.
        trouble = re.compile(r"cannot |failed|fatal|unsupported|not running|"
                             r"unexpected|Assertion|SHOULD NEVER", re.I)
        noise = re.compile(r"(In|End of) zbx|error_handler|_error|error=")
        hits = [l for l in lines if trouble.search(l) and not noise.search(l)]
        if hits:
            print(f"\ncomplaints in {log.name}:", flush=True)
            for line in list(dict.fromkeys(hits))[:20]:
                print("  " + line[:220], flush=True)
        print(f"\nlast lines of {log.name}:", flush=True)
        for line in lines[-15:]:
            print("  " + line[:220], flush=True)

    if args.keep:
        print(f"\nwork directory kept at {workdir}", flush=True)
    else:
        shutil.rmtree(workdir, ignore_errors=True)

    return status


if __name__ == "__main__":
    sys.exit(main())
