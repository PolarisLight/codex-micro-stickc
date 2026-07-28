import unittest
from unittest.mock import AsyncMock

from tools.sync_titles_auto import should_sync, sync_with_fallback


class SchedulingTests(unittest.TestCase):
    def test_assignment_change_syncs_immediately(self):
        self.assertTrue(
            should_sync(
                ["Rebound task", "", "", "", "", ""],
                ["Old task", "", "", "", "", ""],
                now=11.0,
                last_sync=10.0,
            )
        )

    def test_unchanged_assignments_refresh_after_device_reboot_window(self):
        labels = ["Same task", "", "", "", "", ""]

        self.assertFalse(
            should_sync(labels, labels, now=39.0, last_sync=10.0)
        )
        self.assertTrue(
            should_sync(labels, labels, now=40.0, last_sync=10.0)
        )


class TransportTests(unittest.IsolatedAsyncioTestCase):
    async def test_usb_is_preferred_when_available(self):
        usb = AsyncMock()
        ble = AsyncMock()

        transport = await sync_with_fallback(
            ["Task", "", "", "", "", ""],
            usb_sync=usb,
            ble_sync=ble,
        )

        self.assertEqual(transport, "USB")
        usb.assert_awaited_once()
        ble.assert_not_awaited()

    async def test_ble_is_used_when_usb_is_unavailable(self):
        usb = AsyncMock(side_effect=RuntimeError("no serial device"))
        ble = AsyncMock()

        transport = await sync_with_fallback(
            ["Task", "", "", "", "", ""],
            usb_sync=usb,
            ble_sync=ble,
        )

        self.assertEqual(transport, "BLE")
        ble.assert_awaited_once()


if __name__ == "__main__":
    unittest.main()
