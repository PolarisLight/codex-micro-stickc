# Codex Micro StickC GitHub Pages Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and publish a privacy-safe static GitHub Pages landing page for Codex Micro StickC.

**Architecture:** A dependency-free site lives in `site/` and uses committed local image assets. A small Python contract test verifies structure, privacy, asset references, and the Pages workflow. GitHub Actions deploys only `site/` after changes reach `main`.

**Tech Stack:** HTML5, CSS, vanilla JavaScript, Python `unittest`, GitHub Actions Pages.

---

### Task 1: Define the page contract

**Files:**
- Create: `tests/test_github_pages.py`

- [ ] **Step 1: Write the failing test**

```python
class GitHubPagesTest(unittest.TestCase):
    def test_page_contains_required_sections_and_local_assets(self):
        html = INDEX.read_text()
        for section_id in ("origin", "features", "sync", "install"):
            self.assertIn(f'id="{section_id}"', html)
        self.assertNotRegex(html, r"/Users/|COM\\d+|/dev/cu\\.")
```

- [ ] **Step 2: Run test to verify it fails**

Run: `python -m unittest tests.test_github_pages -v`

Expected: FAIL because `site/index.html` and the workflow do not exist.

### Task 2: Implement the static page

**Files:**
- Create: `site/index.html`
- Create: `site/styles.css`
- Create: `site/script.js`
- Create: `site/.nojekyll`
- Create: `site/assets/cover.jpg`
- Create: `site/assets/session-switching.jpg`
- Create: `site/assets/command-control.jpg`
- Create: `site/assets/title-sync.jpg`
- Modify: `README.md`

- [ ] **Step 1: Add semantic page content**

Create a single page with `origin`, `features`, `sync`, and `install` sections, local image references, repository links, and an unofficial-project disclaimer.

- [ ] **Step 2: Add the responsive visual system**

Implement a near-black editorial layout with one orange action color, asymmetric image blocks, mobile breakpoints, visible focus states, and reduced-motion support.

- [ ] **Step 3: Add minimal progressive enhancement**

Use `IntersectionObserver` to reveal story elements only when motion is allowed. Keep all content visible when JavaScript is unavailable.

- [ ] **Step 4: Prepare final images**

Convert the four approved PNG images to quality-controlled JPEG copies in `site/assets/`. Do not include the rejected preview in the deployed directory.

- [ ] **Step 5: Run the contract test**

Run: `python -m unittest tests.test_github_pages -v`

Expected: PASS.

### Task 3: Add GitHub Pages deployment

**Files:**
- Create: `.github/workflows/pages.yml`
- Test: `tests/test_github_pages.py`

- [ ] **Step 1: Extend the failing test**

Require the workflow to trigger on `main`, use official Pages actions, and upload only `site`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `python -m unittest tests.test_github_pages -v`

Expected: FAIL because the workflow does not exist.

- [ ] **Step 3: Add the workflow**

Use `actions/checkout@v4`, `actions/configure-pages@v5`, `actions/upload-pages-artifact@v3`, and `actions/deploy-pages@v4` with Pages permissions and concurrency.

- [ ] **Step 4: Run the test to verify it passes**

Run: `python -m unittest tests.test_github_pages -v`

Expected: PASS.

### Task 4: Verify and publish

**Files:**
- Verify all changed files.

- [ ] **Step 1: Run repository verification**

Run:

```bash
python -m unittest discover -v
python -m py_compile tools/*.py tests/*.py
c++ -std=c++11 -Iinclude tests/title_ui_mode_test.cpp -o /tmp/title_ui_mode_test
/tmp/title_ui_mode_test
c++ -std=c++11 -Iinclude tests/ble_connection_state_test.cpp -o /tmp/ble_connection_state_test
/tmp/ble_connection_state_test
pio run
git diff --check
```

Expected: all tests and builds exit with status 0.

- [ ] **Step 2: Run privacy checks**

Run: `rg -n '/Users/|/home/|COM[0-9]+|/dev/(cu|tty)\\.' site .github README.md`

Expected: no matches.

- [ ] **Step 3: Commit and push the feature branch**

Stage only the approved page, source assets, tests, documentation, workflow, and README changes. Commit and push the existing feature branch.

- [ ] **Step 4: Merge to main**

Fetch the remote, fast-forward `main` to the verified feature branch, run the verification suite on the merged result, and push `main`.

- [ ] **Step 5: Confirm deployment**

Enable GitHub Pages with workflow builds if required, wait for the Pages action to succeed, then verify the public URL returns the new homepage.
