# Firmware Title UI Fallback Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Start every firmware boot in the reference-style agent status UI and enter title UI only after a valid USB title packet arrives.

**Architecture:** A tiny pure helper models the boot-session latch and status assignment semantics. `main_stickc.cpp` keeps the existing single-task panel but substitutes a fitted `AGENT N` label until serial parsing activates title mode.

**Tech Stack:** Arduino C++17, M5Unified, ArduinoJson, PlatformIO

---

### Task 1: Add and integrate the session latch

**Files:**
- Create: `include/TitleUiMode.h`
- Modify: `src/main_stickc.cpp`

- [ ] **Step 1: Add the pure transition helper and compile-time tests**

```cpp
#pragma once

constexpr bool nextTitleUiActive(bool active, bool validLabelsPacket) {
  return active || validLabelsPacket;
}

static_assert(!nextTitleUiActive(false, false), "boot stays status-only");
static_assert(nextTitleUiActive(false, true), "first packet enables titles");
static_assert(nextTitleUiActive(true, false), "title mode stays latched");
```

- [ ] **Step 2: Build to establish the helper compiles**

Run: `.venv312/bin/pio run`
Expected: `SUCCESS`

- [ ] **Step 3: Integrate the latch**

Include `TitleUiMode.h`, initialize `bool titleSyncActive = false`, and set:

```cpp
titleSyncActive = nextTitleUiActive(titleSyncActive, true);
```

only inside the successfully parsed `labels` packet branch. Persisted labels may
still be loaded for later use, but must not change the boot value.

### Task 2: Add the status-only renderer

**Files:**
- Modify: `src/main_stickc.cpp`
- Modify: `README.md`

- [ ] **Step 1: Implement the reference-style task placeholder**

Reuse the existing single-task title/status-card layout. Before title sync,
render `AGENT N` directly in the title region with `Font2` on narrow panels and
`Font4` where it fits. Treat the slot as assigned for status rendering so the
fallback never displays `UNASSIGNED`.

- [ ] **Step 2: Select the renderer**

In `drawScreen()`:

```cpp
if (titleSyncActive) drawTitleInRegion(...);
else drawAgentPlaceholder(...);
```

Keep the existing status card, footer, A/B behavior, and orientation layout.

- [ ] **Step 3: Document the fallback**

State that every boot begins in status-only mode, cached titles are not shown,
and the first valid title-sync packet enables title mode until reboot.

### Task 3: Verify and publish

**Files:**
- Verify all files above

- [ ] **Step 1: Run host tests**

Run: `PYTHONPATH=. .venv312/bin/python -m unittest discover -v`
Expected: all tests pass.

- [ ] **Step 2: Build firmware**

Run: `.venv312/bin/pio run`
Expected: `SUCCESS`.

- [ ] **Step 3: Flash and inspect both modes**

Stop the title service, flash, and confirm the existing task panel shows
`AGENT N` with no task title or `UNASSIGNED`. Restart title sync and confirm
`TITLE_SYNC_OK` followed by the existing title UI.

- [ ] **Step 4: Commit and update the existing PR**

Commit the firmware, README, helper, and validation changes, then push
`codex/cross-platform-title-sync-service`.
