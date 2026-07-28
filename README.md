<p align="center">
  <img src="assets/readme/hero.svg" alt="Codex Micro for M5StickC — 原版 M5StickC 本地任务控制器" width="100%">
</p>

<p align="center">
  <a href="#中文说明">中文</a>
  ·
  <a href="#english-summary">English</a>
  ·
  <a href="https://github.com/PolarisLight/codex-micro-stickc/releases">Releases</a>
  ·
  <a href="LICENSE">MIT License</a>
</p>

# Codex Micro for M5StickC

<a id="中文说明"></a>

把闲置的原版 **M5StickC** 变成一个袖珍 Codex 任务屏和实体控制器：一眼查看六个任务的状态，用正面按键选择任务，再用顶部按键打开任务或执行映射操作。

*Turn an original **M5StickC** into a pocket-sized Codex task display and physical controller. See six task states at a glance, select a task with the front button, and open it or invoke a mapped action with the top button.*

当前源码还能自动同步真实任务标题：插线时优先使用 USB，拔线后由独立的加密 BLE 通道接管。标题始终在本地读取和传输，不经过云端中继。

*The current source also synchronizes real task titles automatically. It prefers USB while connected and falls back to an independent encrypted BLE channel after the cable is removed. Titles are read and transported locally, without a cloud relay.*

> [!WARNING]
> 这是一个非官方兼容项目，与 OpenAI、Work Louder 或 M5Stack 无隶属或支持关系。项目使用了未公开的设备协议，未来的 ChatGPT Desktop 更新可能导致兼容性变化。
>
> *This is an independent compatibility project. It is not affiliated with or supported by OpenAI, Work Louder, or M5Stack. It uses an undocumented device protocol that may change with a future ChatGPT Desktop update.*

## 快速开始 / Quick start

### 1. 编译并刷写 / Build and flash

安装 [PlatformIO](https://platformio.org/)，用可传输数据的 USB-C 线连接 StickC，然后运行：

*Install PlatformIO, connect the StickC with a data-capable USB-C cable, then run:*

```sh
git clone https://github.com/PolarisLight/codex-micro-stickc.git
cd codex-micro-stickc
pio run --target upload
```

当前源码包含自动 USB → BLE 标题服务。如果只需要已经发布的基础固件，可以下载 [v0.8.0 Release](https://github.com/PolarisLight/codex-micro-stickc/releases/tag/v0.8.0)。

*The current source contains the automatic USB → BLE title service. For the established base firmware, use the v0.8.0 release.*

### 2. 蓝牙配对与分配 / Pair and assign

1. 与名为 **Codex Micro** 的蓝牙设备配对。<br>*Pair the Bluetooth device named **Codex Micro**.*
2. 打开 **ChatGPT Desktop → Settings → Codex Micro**。<br>*Open **ChatGPT Desktop → Settings → Codex Micro**.*
3. 为六个 Agent Keys 分配任务。<br>*Assign tasks to the six Agent Keys.*
4. 将命令槽映射为 Approve、Decline、Mic/PTT、Send、Fast 或 Fork 等操作。<br>*Map command slots to actions such as Approve, Decline, Mic/PTT, Send, Fast, or Fork.*

如果电脑仍缓存旧固件的 HID 描述符，请在蓝牙设置中忘记 **Codex Micro**，重启 StickC 后重新配对。

*If the host still caches an older HID descriptor, forget **Codex Micro**, restart the StickC, and pair again.*

### 3. 启用自动任务标题 / Enable automatic task titles

需要 Python 3.10 或更高版本。macOS 和 Windows 使用同一组当前用户命令，不需要管理员权限：

*Python 3.10+ is required. The same current-user commands work on macOS and Windows without administrator access:*

```sh
python tools/title_sync_service.py install
python tools/title_sync_service.py status
```

卸载服务 / Remove the service:

```sh
python tools/title_sync_service.py uninstall
```

安装器会在用户应用数据目录创建隔离环境，并在 macOS 注册 launchd 服务、在 Windows 注册计划任务。

*The installer creates an isolated environment in the user's application-data directory and registers a launchd service on macOS or a Task Scheduler entry on Windows.*

## 功能概览 / What you get

| 功能 / Capability | 说明 / What it does |
| --- | --- |
| **六个实时任务槽**<br>Six live task slots | 显示运行、待处理、完成和错误状态。<br>Shows Running, Action, Ready, and Error states supplied by the host. |
| **实体控制**<br>Physical control | 选择并打开任务，执行可重新映射的 Codex Micro 操作。<br>Selects and opens tasks, then invokes remappable Codex Micro actions. |
| **自动标题**<br>Automatic titles | 解析本地任务分配，并在换绑或重启后自动刷新。<br>Resolves local assignments and refreshes after changes or restarts. |
| **双传输通道**<br>Cable-aware transport | USB 优先，拔线后回退到独立的加密 BLE 标题通道。<br>Prefers USB and falls back to a separate encrypted BLE title channel. |
| **横竖屏界面**<br>Adaptive layout | 使用内置 IMU 自动切换纵向和横向 UI。<br>Uses the built-in IMU to switch between portrait and landscape UI. |
| **省电与电量估算**<br>Power and battery | 只关闭屏幕、不暂停 BLE，并结合电流积分与慢速电压校正估算电量。<br>Saves display power without suspending BLE and combines current integration with slow voltage correction. |

## 标题同步原理 / How title sync works

<p align="center">
  <img src="assets/readme/transport.svg" alt="Codex Micro 双通道本地架构：BLE HID 负责状态与按键，本地 USB 或加密 BLE 负责任务标题" width="100%">
</p>

ChatGPT 的 Codex Micro 协议会发送槽位颜色和效果，但不会发送任务名称。本地标题服务补上了这一部分：

*ChatGPT's Codex Micro protocol supplies slot colors and effects, but not task names. The local title service fills that gap:*

1. 以只读方式读取 Codex 的本地分配状态和任务数据库。<br>*Reads Codex's local assignment state and task database in read-only mode.*
2. 支持默认的 **Recent** 来源和显式 **Custom** 分配。<br>*Follows the default **Recent** source or explicit **Custom** assignments.*
3. 每秒检测一次换绑，每 30 秒刷新一次未变化的标题。<br>*Detects assignment changes once per second and refreshes unchanged titles every 30 seconds.*
4. USB 可用时直接通过串口写入 StickC。<br>*Writes directly to the StickC over USB serial when available.*
5. USB 断开后回退到独立的加密 BLE GATT 特征。<br>*Falls back to a separate encrypted BLE GATT characteristic after USB disappears.*

标题特征不会复用、占用或写入 ChatGPT 使用的厂商 HID 通道。日志默认隐藏任务名称，也不会打印蓝牙标识符。

*The title characteristic does not reuse, take ownership of, or write to the vendor HID channel used by ChatGPT. Logs redact task names by default and do not print Bluetooth identifiers.*

每次启动先显示 `AGENT N`。收到第一个有效标题包后，本次启动才切换到真实任务标题，因此设备不会展示上次启动残留的旧标题。

*Every boot begins with `AGENT N`. Real titles appear only after the first valid title packet, preventing stale cached names from being displayed.*

> [!NOTE]
> 当前支持 **Recent** 和 **Custom**；尚未同步 **Pinned** 和 **Priority**。macOS 已验证自动 BLE 回退；Windows 已验证 USB，同步服务的 Windows BLE 路径仍待测试。
>
> *Recent and Custom are supported. Pinned and Priority are not yet mirrored. Automatic BLE fallback is validated on macOS; Windows is validated over USB, while its BLE path still needs testing.*

## 按键 / Controls

| 输入 / Input | 任务页 / Tasks | 命令页 / Commands |
| --- | --- | --- |
| 正面 **A** 短按<br>Front **A**, short press | 下一个任务<br>Next task | 下一个命令<br>Next command |
| 正面 **A** 长按<br>Front **A**, hold | 切换到命令页<br>Switch to Commands | 切换到任务页<br>Switch to Tasks |
| 顶部 **B** 按下并松开<br>Top-right **B**, press/release | 打开当前任务<br>Open selected task | 执行当前命令<br>Run selected command |
| 侧面电源键短按<br>Side power, short press | 唤醒或关闭屏幕<br>Wake or turn off display | 唤醒或关闭屏幕<br>Wake or turn off display |
| 旋转设备<br>Rotate device | 切换横竖屏<br>Switch orientation | 切换横竖屏<br>Switch orientation |

屏幕休眠时，第一次按 A/B 只负责唤醒；再次按下才执行操作，避免唤醒时误切任务。

*While the display is asleep, the first A/B press only wakes it. Press again to perform the action, preventing accidental changes during wake-up.*

### 状态颜色 / Status colors

| 颜色 / Color | 大致含义 / Approximate meaning |
| --- | --- |
| 蓝色 / Blue | 运行中 / Running |
| 橙色 / Orange | 需要操作或批准 / Action or approval required |
| 绿色 / Green | 就绪或已完成 / Ready or completed |
| 红色 / Red | 错误 / Error |
| 空槽 / Empty | 未分配任务 / No task assigned |

具体颜色和灯效由 ChatGPT Desktop 下发，未来版本可能改变。

*Exact colors and effects are supplied by ChatGPT Desktop and may change between versions.*

## 省电与电量 / Power and battery

为了避免破坏 Codex Micro 的活动连接，固件不会让 ESP32 或 BLE 控制器进入轻睡眠或深睡眠，而是在显示层省电：

*To preserve the active Codex Micro connection, the firmware does not put the ESP32 or BLE controller into light or deep sleep. Power is saved at the display level instead:*

| 电源 / Power | 屏幕行为 / Display behavior |
| --- | --- |
| 电池 / Battery | 20 秒后变暗，60 秒后关闭 / Dim after 20 s, off after 60 s |
| USB/VBUS | 120 秒后变暗，保持显示 / Dim after 120 s, remain on |

电量不再直接采用波动明显的瞬时电压百分比。估算器基于原装 95 mAh 电池进行充放电电流积分，仅使用平滑、负载补偿后的电压进行缓慢纠偏，并跨重启保存结果。

*Battery percentage no longer comes directly from noisy instantaneous voltage. The estimator integrates charge and discharge current against the original 95 mAh cell, uses smoothed load-compensated voltage only for slow correction, and persists its result across restarts.*

首次启动仍从电压估值开始；完整充电并正常放电一轮后会更准确。它仍然是软件估算，而不是专用电量计。

*The first boot still starts from a voltage estimate. Accuracy improves after a full charge and normal discharge cycle. This remains a software estimate, not a dedicated fuel gauge.*

## 开发与诊断 / Build and diagnostics

### 环境要求 / Requirements

- 原版 M5StickC（ESP32-PICO-D4，80 × 160 屏幕）<br>*Original M5StickC (ESP32-PICO-D4, 80 × 160 display)*
- 可传输数据的 USB-C 线<br>*Data-capable USB-C cable*
- Windows 或 macOS、ChatGPT Desktop 与 Codex Micro 支持<br>*Windows or macOS with ChatGPT Desktop and Codex Micro support*
- [PlatformIO](https://platformio.org/) 与 Python 3.10+

```sh
pio run
pio run --target upload
pio device monitor
python -m unittest discover -s tests -v
```

串口监视器速率为 `115200`。成功启动后会输出 `CODEX_MICRO_READY`。如果自动复位刷写不可靠，可使用 esptool 以 `115200` 波特率将应用镜像写入地址 `0x10000`。

*The serial monitor runs at `115200` baud and a successful boot prints `CODEX_MICRO_READY`. If automatic upload reset is unreliable, use esptool at `115200` baud to flash the application image at address `0x10000`.*

### 手动检查标题同步 / Manual title-sync diagnostics

日常使用推荐后台服务。以下前台命令用于排查端口发现或蓝牙权限：

*The background service is recommended for daily use. These foreground commands help diagnose discovery or Bluetooth permissions:*

```sh
python -m pip install pyserial bleak==3.0.2
python tools/sync_titles_auto.py --once
python tools/sync_titles_usb.py
python tools/sync_titles_ble.py --once
```

只有在明确需要让调试输出包含任务名称时才使用 `python tools/sync_titles_usb.py --show-titles`。仅当自动发现无法区分多个串口设备时，才手动指定 `--port COM3` 或 `--port /dev/cu.usbserial-*`。

*Use `python tools/sync_titles_usb.py --show-titles` only when task names are explicitly needed in foreground debug output. Specify `--port COM3` or `--port /dev/cu.usbserial-*` only when automatic discovery cannot choose between multiple serial devices.*

在 macOS 上，运行服务的 Python 可能会请求 **System Settings → Privacy & Security → Bluetooth** 权限。如果旧配对记录看不到新的标题服务，请忘记 **Codex Micro**、重启 StickC 并重新配对。

*On macOS, the service runtime may request permission under **System Settings → Privacy & Security → Bluetooth**. If an older pairing cannot see the new title service, forget **Codex Micro**, restart the StickC, and pair again.*

## 兼容性 / Compatibility

| 功能 / Area | macOS | Windows |
| --- | :---: | :---: |
| Codex Micro BLE HID | 已测试 / Tested | 已测试 / Tested |
| USB 标题同步 / USB title sync | 已测试 / Tested | 已测试 / Tested |
| 后台服务 / Background service | launchd | Task Scheduler |
| 自动 BLE 标题回退 / Automatic BLE fallback | 已测试 / Tested | 尚未验证 / Not yet validated |

## 当前限制 / Current limitations

- 目前只验证了原版 M5StickC；Plus 系列可能需要修改屏幕与引脚配置。<br>*Only the original M5StickC is currently validated; Plus variants may require display and pin changes.*
- 内置麦克风不会传输到电脑，Mic/PTT 操作仍使用电脑麦克风。<br>*The built-in microphone is not streamed to the computer; Mic/PTT still activates the host microphone.*
- 原版 StickC 电池很小，在保持活动 BLE 连接时续航仍然有限。<br>*The original StickC battery is very small, so runtime remains limited while maintaining active BLE.*
- 厂商协议未公开，ChatGPT Desktop 更新后可能失效。<br>*The vendor protocol is undocumented and may break after a ChatGPT Desktop update.*
- BLE 稳定性会受到电池状态、供电路径、主机无线电和旧配对缓存影响。建议保留一份已知可用的 Release 固件用于恢复。<br>*BLE reliability depends on battery condition, power path, host radio, and cached pairing state. Keep a known-good release image for recovery.*

<details>
<summary><strong>项目结构 / Project layout</strong></summary>

```text
include/BatteryEstimator.h   Hybrid battery state-of-charge estimator
include/CodexMicroBle.h      BLE transport and shared state
src/CodexMicroBle.cpp        HID, RPC framing, and encrypted title GATT service
src/main_stickc.cpp          UI, controls, IMU, title sync, and power behavior
tools/sync_titles_auto.py    Automatic USB-to-BLE title synchronizer
tools/sync_titles_ble.py     One-shot BLE title synchronizer
tools/sync_titles_usb.py     Local assignment and title reader
tools/title_sync_service.py  macOS/Windows background service manager
platformio.ini               Reproducible PlatformIO build
```

</details>

<a id="english-summary"></a>

## English summary

Codex Micro for M5StickC is an unofficial, local-first controller for the original M5StickC. It exposes Codex Micro status and controls through BLE HID, displays six assigned task states, supports portrait and landscape layouts, and provides remappable task and command actions.

The optional title service reads local Codex assignment state in read-only mode, prefers USB serial, and falls back to a separate encrypted BLE GATT characteristic. It supports Recent and Custom task sources, redacts titles in logs by default, and does not use a cloud relay. The bilingual sections above contain the complete setup, control, power, compatibility, and troubleshooting reference.

## 致谢 / Credits

本项目移植自 [`imliubo/codex-micro-4-core2`](https://github.com/imliubo/codex-micro-4-core2)，沿用了其最初的 BLE HID 与 Codex Micro 协议实现，并保留原始版权和 MIT 许可证。

*This port is derived from `imliubo/codex-micro-4-core2`, which provided the original BLE HID and Codex Micro protocol implementation. The original copyright and MIT license are preserved.*

## 许可证 / License

MIT。参见 [LICENSE](LICENSE) 与 [NOTICE.md](NOTICE.md)。

*MIT. See [LICENSE](LICENSE) and [NOTICE.md](NOTICE.md).*
