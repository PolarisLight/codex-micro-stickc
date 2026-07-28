#!/usr/bin/env python3
"""Keep M5StickC labels synchronized with Codex task assignments."""
from __future__ import annotations
import argparse, json, os, sqlite3, sys, time
from pathlib import Path
import serial
from serial.tools import list_ports

STATE_KEY = "codex-micro-custom-agent-assignments"
SOURCE_KEY = "codex-micro-agent-source"

def state_path():
    return Path(os.environ.get("USERPROFILE", str(Path.home()))) / ".codex" / ".codex-global-state.json"

def thread_db_path():
    return Path.home() / ".codex" / "sqlite" / "state_5.sqlite"

def choose_port(requested):
    if requested: return requested
    ports = list(list_ports.comports())
    if len(ports) == 1: return ports[0].device
    likely = [p.device for p in ports if (p.vid == 0x0403 and p.pid == 0x6001) or any(x in (p.device + " " + (p.description or "")).lower() for x in ("usbserial", "usbmodem", "ftdi", "uart", "m5"))]
    if len(likely) == 1: return likely[0]
    raise RuntimeError("Cannot choose StickC serial port; pass --port COM3")

def compact_title(value, limit=80):
    first = next((line.strip() for line in str(value).splitlines() if line.strip()), "")
    return first[:limit]

def read_recent_labels(path):
    if not path.exists():
        return [""] * 6
    with sqlite3.connect(f"file:{path}?mode=ro", uri=True) as connection:
        rows = connection.execute(
            """
            SELECT title
            FROM threads
            WHERE archived = 0
              AND title <> ''
              AND COALESCE(thread_source, 'user') <> 'subagent'
            ORDER BY COALESCE(updated_at_ms, updated_at * 1000) DESC, id DESC
            LIMIT 6
            """
        ).fetchall()
    labels = [compact_title(row[0]) for row in rows]
    return labels + [""] * (6 - len(labels))

def read_labels(path, database=None):
    data = json.loads(path.read_text(encoding="utf-8"))
    persisted = data.get("electron-persisted-atom-state", {})
    source = persisted.get(SOURCE_KEY)
    slots = persisted.get(STATE_KEY, {}) or {}
    if source == "recent" or (source is None and not any(slots.values())):
        return read_recent_labels(database or thread_db_path())
    return [str((slots.get(f"AG{i:02d}") or {}).get("title") or "").strip() for i in range(6)]

def open_serial(port):
    link = serial.Serial()
    link.port, link.baudrate, link.timeout, link.write_timeout = port, 115200, .25, 2
    link.dtr = False
    link.rts = False
    link.open()
    return link

def send(link, labels, show_titles=False):
    line = json.dumps({"labels": labels}, ensure_ascii=False, separators=(",", ":")) + "\n"
    link.write(line.encode("utf-8")); link.flush()
    deadline, acknowledged = time.monotonic() + 1.5, False
    while time.monotonic() < deadline:
        response = link.readline().decode("utf-8", errors="ignore").strip()
        if response == "TITLE_SYNC_OK":
            acknowledged = True
            break
    prefix = "Synced" if acknowledged else "Sent (awaiting device acknowledgement)"
    if show_titles:
        detail = " | ".join(x or "(unassigned)" for x in labels)
    else:
        detail = f"{sum(bool(x) for x in labels)}/{len(labels)} labels"
    print(prefix + ":", detail, flush=True)

def run(port, path, once, show_titles=False):
    previous, last_send, link = None, 0.0, None
    while True:
        try:
            labels = read_labels(path)
            if link is None or not link.is_open:
                selected = choose_port(port); link = open_serial(selected)
                print("Connected to StickC; watching Codex state", flush=True)
                time.sleep(2); previous = None
            now = time.monotonic()
            if labels != previous or now - last_send >= 60:
                send(link, labels, show_titles); previous, last_send = labels, now
                if once: return
            if link.in_waiting:
                link.read(link.in_waiting)
            time.sleep(1)
        except KeyboardInterrupt: return
        except Exception as error:
            print(f"Title sync waiting: {error}", file=sys.stderr, flush=True)
            if link:
                try: link.close()
                except Exception: pass
            link = None
            if once: raise
            time.sleep(3)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--state", type=Path, default=state_path())
    parser.add_argument("--once", action="store_true")
    parser.add_argument(
        "--show-titles",
        action="store_true",
        help="include task titles in console output (off by default)",
    )
    args = parser.parse_args()
    run(args.port, args.state, args.once, args.show_titles)

if __name__ == "__main__": main()
