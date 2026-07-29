#!/usr/bin/env python3
"""Keep Codex task titles synchronized over USB with automatic BLE fallback."""
from __future__ import annotations

import argparse
import asyncio
import sys
import time
from pathlib import Path

try:
    from tools.sync_titles_ble import sync_once as sync_ble_once
    from tools.sync_titles_ble import sync_summary
    from tools.sync_titles_usb import (
        choose_port,
        open_serial,
        read_labels,
        send,
        state_path,
    )
except ModuleNotFoundError:
    from sync_titles_ble import sync_once as sync_ble_once
    from sync_titles_ble import sync_summary
    from sync_titles_usb import choose_port, open_serial, read_labels, send, state_path

REFRESH_SECONDS = 300.0


def should_sync(labels, previous, now, last_sync, refresh=REFRESH_SECONDS):
    return labels != previous or now - last_sync >= refresh


async def sync_with_fallback(labels, usb_sync, ble_sync):
    try:
        await usb_sync(labels)
        return "USB"
    except Exception:
        try:
            await ble_sync(labels)
            return "BLE"
        except Exception as error:
            raise RuntimeError(
                "no USB or BLE title transport is currently available"
            ) from error


class UsbTransport:
    def __init__(self, port=None):
        self.port = port
        self.link = None

    def _sync(self, labels):
        try:
            if self.link is None or not self.link.is_open:
                self.link = open_serial(choose_port(self.port))
                time.sleep(2)
            send(self.link, labels)
        except Exception:
            if self.link is not None:
                try:
                    self.link.close()
                except Exception:
                    pass
            self.link = None
            raise

    async def __call__(self, labels):
        await asyncio.to_thread(self._sync, labels)


async def run(path, port=None, once=False):
    previous = None
    last_sync = 0.0
    usb = UsbTransport(port)

    while True:
        try:
            labels = read_labels(path)
            now = time.monotonic()
            if should_sync(labels, previous, now, last_sync):
                transport = await sync_with_fallback(
                    labels,
                    usb_sync=usb,
                    ble_sync=sync_ble_once,
                )
                if transport == "BLE":
                    print(
                        f"Synced via BLE: {sync_summary(labels)}",
                        flush=True,
                    )
                previous = labels
                last_sync = time.monotonic()
                if once:
                    return
            await asyncio.sleep(1)
        except KeyboardInterrupt:
            return
        except Exception as error:
            print(f"Title sync waiting: {error}", file=sys.stderr, flush=True)
            if once:
                raise
            await asyncio.sleep(3)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port")
    parser.add_argument("--state", type=Path, default=state_path())
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    asyncio.run(run(args.state, args.port, args.once))


if __name__ == "__main__":
    main()
