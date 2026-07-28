from html.parser import HTMLParser
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
SITE = ROOT / "site"
INDEX = SITE / "index.html"
WORKFLOW = ROOT / ".github" / "workflows" / "pages.yml"


class PageParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.images = []
        self.links = []

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        if values.get("id"):
            self.ids.add(values["id"])
        if tag == "img":
            self.images.append(values)
        if tag == "a":
            self.links.append(values)


class GitHubPagesTest(unittest.TestCase):
    def parse_page(self):
        html = INDEX.read_text(encoding="utf-8")
        parser = PageParser()
        parser.feed(html)
        return html, parser

    def test_page_contains_required_sections_and_actions(self):
        html, page = self.parse_page()

        self.assertTrue({"origin", "features", "sync", "install"} <= page.ids)
        hrefs = {link.get("href", "") for link in page.links}
        self.assertIn(
            "https://github.com/PolarisLight/codex-micro-stickc", hrefs
        )
        self.assertIn("styles.css", html)
        self.assertIn("script.js", html)

    def test_local_image_references_exist_and_have_alt_text(self):
        _, page = self.parse_page()

        self.assertGreaterEqual(len(page.images), 4)
        for image in page.images:
            self.assertTrue(image.get("alt"), image)
            source = image.get("src", "")
            self.assertFalse(source.startswith(("http://", "https://")), source)
            self.assertTrue((SITE / source).is_file(), source)

    def test_public_files_do_not_contain_private_machine_details(self):
        forbidden = re.compile(
            r"/Users/|/home/|COM[0-9]+|/dev/(?:cu|tty)\.|"
            r"(?:[0-9A-F]{2}:){5}[0-9A-F]{2}",
            re.IGNORECASE,
        )

        for path in SITE.rglob("*"):
            if path.is_file() and path.suffix in {".html", ".css", ".js", ".txt"}:
                self.assertIsNone(forbidden.search(path.read_text(encoding="utf-8")))

    def test_pages_workflow_deploys_only_site_directory(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")

        self.assertIn("branches: [main]", workflow)
        self.assertIn("actions/configure-pages@v5", workflow)
        self.assertIn("actions/upload-pages-artifact@v3", workflow)
        self.assertIn("actions/deploy-pages@v4", workflow)
        self.assertRegex(workflow, r"path:\s+site(?:\s|$)")


if __name__ == "__main__":
    unittest.main()
