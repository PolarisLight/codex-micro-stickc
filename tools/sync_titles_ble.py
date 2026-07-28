#!/usr/bin/env python3
"""Send Codex task titles to the StickC over the optional BLE GATT service."""
from __future__ import annotations

import argparse
import asyncio
import json
from pathlib import Path

from tools.sync_titles_usb import read_labels, state_path

SERVICE_UUID = "5f83a25b-442d-4d56-bf3a-3e2f8b21e101"
TITLE_UUID = "5f83a25b-442d-4d56-bf3a-3e2f8b21e102"
CHUNK_SIZE = 48
ACK = b"TITLE_SYNC_OK"


def build_payload(labels):
    message = json.dumps(
        {"labels": list(labels)},
        ensure_ascii=False,
        separators=(",", ":"),
    )
    return (message + "\n").encode("utf-8")


def chunk_payload(payload, size=CHUNK_SIZE):
    return [payload[offset : offset + size] for offset in range(0, len(payload), size)]


def sync_summary(labels):
    return f"{sum(bool(label) for label in labels)}/{len(labels)} labels"


async def sync_once(labels, timeout=12.0):
    from bleak import BleakClient, BleakScanner

    service_uuid = SERVICE_UUID.lower()

    def exposes_title_service(_, advertisement):
        return service_uuid in {
            value.lower() for value in (advertisement.service_uuids or [])
        }

    device = await BleakScanner.find_device_by_filter(
        exposes_title_service,
        timeout=timeout,
    )
    if device is None:
        raise RuntimeError("Codex Micro BLE title service was not found")

    async with BleakClient(device, timeout=timeout) as client:
        for chunk in chunk_payload(build_payload(labels)):
            await client.write_gatt_char(TITLE_UUID, chunk, response=True)
        response = bytes(await client.read_gatt_char(TITLE_UUID))
    if response != ACK:
        raise RuntimeError(
            "BLE title sync was not acknowledged "
            f"(received {response.decode('utf-8', errors='replace')!r})"
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", type=Path, default=state_path())
    parser.add_argument("--timeout", type=float, default=12.0)
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()

    labels = read_labels(args.state)
    asyncio.run(sync_once(labels, args.timeout))
    print(f"Synced over BLE: {sync_summary(labels)}", flush=True)


if __name__ == "__main__":
    main()
