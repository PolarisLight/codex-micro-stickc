import json
import io
import sqlite3
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import Mock

from tools.sync_titles_usb import read_labels, send


class ReadLabelsTests(unittest.TestCase):
    def make_state(self, directory, persisted):
        path = Path(directory) / "state.json"
        path.write_text(
            json.dumps({"electron-persisted-atom-state": persisted}),
            encoding="utf-8",
        )
        return path

    def make_thread_db(self, directory, rows):
        path = Path(directory) / "state_5.sqlite"
        with sqlite3.connect(path) as connection:
            connection.execute(
                """
                CREATE TABLE threads (
                    id TEXT PRIMARY KEY,
                    updated_at INTEGER NOT NULL,
                    updated_at_ms INTEGER,
                    title TEXT NOT NULL,
                    thread_source TEXT,
                    archived INTEGER NOT NULL DEFAULT 0
                )
                """
            )
            connection.executemany(
                """
                INSERT INTO threads
                    (id, updated_at, updated_at_ms, title, thread_source, archived)
                VALUES (?, ?, ?, ?, ?, ?)
                """,
                rows,
            )
        return path

    def test_custom_source_reads_explicit_assignments(self):
        with tempfile.TemporaryDirectory() as directory:
            state = self.make_state(
                directory,
                {
                    "codex-micro-agent-source": "custom",
                    "codex-micro-custom-agent-assignments": {
                        "AG00": {"title": "First"},
                        "AG02": {"title": "Third"},
                    },
                },
            )

            self.assertEqual(
                read_labels(state),
                ["First", "", "Third", "", "", ""],
            )

    def test_recent_is_the_default_and_reads_display_safe_titles(self):
        with tempfile.TemporaryDirectory() as directory:
            state = self.make_state(directory, {})
            database = self.make_thread_db(
                directory,
                [
                    ("old", 1, None, "Old", "user", 0),
                    ("archived", 9, None, "Archived", "user", 1),
                    ("subagent", 8, None, "Hidden", "subagent", 0),
                    ("multi", 7, None, "\n  Recent title  \nfull prompt", "user", 0),
                    ("long", 6, None, "中" * 100, None, 0),
                ],
            )

            self.assertEqual(
                read_labels(state, database),
                ["Recent title", "中" * 80, "Old", "", "", ""],
            )

    def test_custom_assignments_win_when_source_setting_is_absent(self):
        with tempfile.TemporaryDirectory() as directory:
            state = self.make_state(
                directory,
                {
                    "codex-micro-custom-agent-assignments": {
                        "AG00": {"title": "Rebound task"},
                    },
                },
            )
            database = self.make_thread_db(
                directory,
                [("recent", 1, None, "Recent task", "user", 0)],
            )

            self.assertEqual(
                read_labels(state, database),
                ["Rebound task", "", "", "", "", ""],
            )


class SendTests(unittest.TestCase):
    def make_link(self):
        link = Mock()
        link.readline.return_value = b"TITLE_SYNC_OK\n"
        return link

    def test_titles_are_redacted_by_default(self):
        output = io.StringIO()
        with redirect_stdout(output):
            send(self.make_link(), ["Private task", "", "", "", "", ""])

        self.assertEqual(output.getvalue().strip(), "Synced: 1/6 labels")
        self.assertNotIn("Private task", output.getvalue())

    def test_show_titles_is_opt_in(self):
        output = io.StringIO()
        with redirect_stdout(output):
            send(
                self.make_link(),
                ["Visible task", "", "", "", "", ""],
                show_titles=True,
            )

        self.assertIn("Visible task", output.getvalue())


if __name__ == "__main__":
    unittest.main()
