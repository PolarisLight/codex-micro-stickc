import unittest

from tools.sync_titles_ble import build_payload, chunk_payload, sync_summary


class PayloadTests(unittest.TestCase):
    def test_payload_is_newline_delimited_and_chunks_are_bounded(self):
        payload = build_payload(["一" * 40, "two", "", "", "", ""])
        chunks = chunk_payload(payload)

        self.assertTrue(payload.endswith(b"\n"))
        self.assertEqual(b"".join(chunks), payload)
        self.assertTrue(all(0 < len(chunk) <= 48 for chunk in chunks))

    def test_default_summary_redacts_titles(self):
        summary = sync_summary(["Private", "", "", "", "", ""])

        self.assertEqual(summary, "1/6 labels")
        self.assertNotIn("Private", summary)


if __name__ == "__main__":
    unittest.main()
