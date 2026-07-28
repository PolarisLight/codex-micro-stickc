<p align="center">
  <img src="assets/readme/hero.svg" alt="Codex Micro for M5StickC — 原版 M5StickC 本地任务控制器" width="100%">
</p>

<p align="center">
  <strong>简体中文</strong>
  ·
  <a href="README_EN.md">English</a>
  ·
  <a href="https://polarislight.github.io/codex-micro-stickc/">Project Page</a>
  ·
  <a href="https://github.com/PolarisLight/codex-micro-stickc/releases">Releases</a>
  ·
  <a href="LICENSE">MIT License</a>
</p>

# Codex Micro for M5StickC

把闲置的原版 **M5StickC** 变成一个袖珍 Codex 任务屏和实体控制器：一眼查看六个任务的状态，用正面按键选择任务，再用顶部按键打开任务或执行映射操作。

当前源码还能自动同步真实任务标题：插线时优先使用 USB，拔线后由独立的加密 BLE 通道接管。标题始终在本地读取和传输，不经过云端中继。

> [!WARNING]
> 这是一个非官方兼容项目，与 OpenAI、Work Louder 或 M5Stack 无隶属或支持关系。项目使用了未公开的设备协议，未来的 ChatGPT Desktop 更新可能导致兼容性变化。

## 快速开始

### 1. 编译并刷写

安装 [PlatformIO](https://platformio.org/)，用可传输数据的 USB-C 线连接 StickC，然后运行：

```sh
git clone https://github.com/PolarisLight/codex-micro-stickc.git
cd codex-micro-stickc
pio run --target upload
```

当前源码包含自动 USB → BLE 标题服务。如果只需要已经发布的基础固件，可以下载 [v0.8.0 Release](https://github.com/PolarisLight/codex-micro-stickc/releases/tag/v0.8.0)。

### 2. 蓝牙配对与分配

1. 与名为 **Codex Micro** 的蓝牙设备配对。
2. 打开 **ChatGPT Desktop → Settings → Codex Micro**。
3. 为六个 Agent Keys 分配任务。
4. 将命令槽映射为 Approve、Decline、Mic/PTT、Send、Fast 或 Fork 等操作。

如果电脑仍缓存旧固件的 HID 描述符，请在蓝牙设置中忘记 **Codex Micro**，重启 StickC 后重新配对。

### 3. 启用自动任务标题

需要 Python 3.10 或更高版本。macOS 和 Windows 使用同一组当前用户命令，不需要管理员权限：

```sh
python tools/title_sync_service.py install
python tools/title_sync_service.py status
```

卸载服务：

```sh
python tools/title_sync_service.py uninstall
```

安装器会在用户应用数据目录创建隔离环境，并在 macOS 注册 launchd 服务、在 Windows 注册计划任务。

## 功能概览

| 功能 | 说明 |
| --- | --- |
| **六个实时任务槽** | 显示运行、待处理、完成和错误状态 |
| **实体控制** | 选择并打开任务，执行可重新映射的 Codex Micro 操作 |
| **自动标题** | 解析本地任务分配，并在换绑或重启后自动刷新 |
| **双传输通道** | USB 优先，拔线后回退到独立的加密 BLE 标题通道 |
| **横竖屏界面** | 使用内置 IMU 自动切换纵向和横向 UI |
| **省电与电量估算** | 只关闭屏幕、不暂停 BLE，并结合电流积分与慢速电压校正估算电量 |

## 标题同步原理

<p align="center">
  <img src="assets/readme/transport.svg" alt="Codex Micro 双通道本地架构：BLE HID 负责状态与按键，本地 USB 或加密 BLE 负责任务标题" width="100%">
</p>

ChatGPT 的 Codex Micro 协议会发送槽位颜色和效果，但不会发送任务名称。本地标题服务补上了这一部分：

1. 以只读方式读取 Codex 的本地分配状态和任务数据库。
2. 支持默认的 **Recent** 来源和显式 **Custom** 分配。
3. 每秒检测一次换绑，每 30 秒刷新一次未变化的标题。
4. USB 可用时直接通过串口写入 StickC。
5. USB 断开后回退到独立的加密 BLE GATT 特征。

标题特征不会复用、占用或写入 ChatGPT 使用的厂商 HID 通道。日志默认隐藏任务名称，也不会打印蓝牙标识符。

每次启动先显示 `AGENT N`。收到第一个有效标题包后，本次启动才切换到真实任务标题，因此设备不会展示上次启动残留的旧标题。

> [!NOTE]
> 当前支持 **Recent** 和 **Custom**；尚未同步 **Pinned** 和 **Priority**。macOS 已验证自动 BLE 回退；Windows 已验证 USB，同步服务的 Windows BLE 路径仍待测试。

## 按键

| 输入 | 任务页 | 命令页 |
| --- | --- | --- |
| 正面 **A** 短按 | 下一个任务 | 下一个命令 |
| 正面 **A** 长按 | 切换到命令页 | 切换到任务页 |
| 顶部 **B** 按下并松开 | 打开当前任务 | 执行当前命令 |
| 侧面电源键短按 | 唤醒或关闭屏幕 | 唤醒或关闭屏幕 |
| 旋转设备 | 切换横竖屏 | 切换横竖屏 |

屏幕休眠时，第一次按 A/B 只负责唤醒；再次按下才执行操作，避免唤醒时误切任务。

### 状态颜色

| 颜色 | 大致含义 |
| --- | --- |
| 蓝色 | 运行中 |
| 橙色 | 需要操作或批准 |
| 绿色 | 就绪或已完成 |
| 红色 | 错误 |
| 空槽 | 未分配任务 |

具体颜色和灯效由 ChatGPT Desktop 下发，未来版本可能改变。

## 省电与电量

为了避免破坏 Codex Micro 的活动连接，固件不会让 ESP32 或 BLE 控制器进入轻睡眠或深睡眠，而是在显示层省电：

| 电源 | 屏幕行为 |
| --- | --- |
| 电池 | 20 秒后变暗，60 秒后关闭 |
| USB/VBUS | 120 秒后变暗，保持显示 |

电量不再直接采用波动明显的瞬时电压百分比。估算器基于原装 95 mAh 电池进行充放电电流积分，仅使用平滑、负载补偿后的电压进行缓慢纠偏，并跨重启保存结果。

首次启动仍从电压估值开始；完整充电并正常放电一轮后会更准确。它仍然是软件估算，而不是专用电量计。

## 开发与诊断

### 环境要求

- 原版 M5StickC（ESP32-PICO-D4，80 × 160 屏幕）
- 可传输数据的 USB-C 线
- Windows 或 macOS、ChatGPT Desktop 与 Codex Micro 支持
- [PlatformIO](https://platformio.org/) 与 Python 3.10+

```sh
pio run
pio run --target upload
pio device monitor
python -m unittest discover -s tests -v
```

串口监视器速率为 `115200`。成功启动后会输出 `CODEX_MICRO_READY`。如果自动复位刷写不可靠，可使用 esptool 以 `115200` 波特率将应用镜像写入地址 `0x10000`。

### 手动检查标题同步

日常使用推荐后台服务。以下前台命令用于排查端口发现或蓝牙权限：

```sh
python -m pip install pyserial bleak==3.0.2
python tools/sync_titles_auto.py --once
python tools/sync_titles_usb.py
python tools/sync_titles_ble.py --once
```

只有在明确需要让调试输出包含任务名称时才使用 `python tools/sync_titles_usb.py --show-titles`。仅当自动发现无法区分多个串口设备时，才手动指定 `--port COM3` 或 `--port /dev/cu.usbserial-*`。

在 macOS 上，运行服务的 Python 可能会请求 **System Settings → Privacy & Security → Bluetooth** 权限。如果旧配对记录看不到新的标题服务，请忘记 **Codex Micro**、重启 StickC 并重新配对。

## 兼容性

| 功能 | macOS | Windows |
| --- | :---: | :---: |
| Codex Micro BLE HID | 已测试 | 已测试 |
| USB 标题同步 | 已测试 | 已测试 |
| 后台服务 | launchd | Task Scheduler |
| 自动 BLE 标题回退 | 已测试 | 尚未验证 |

## 当前限制

- 目前只验证了原版 M5StickC；Plus 系列可能需要修改屏幕与引脚配置。
- 内置麦克风不会传输到电脑，Mic/PTT 操作仍使用电脑麦克风。
- 原版 StickC 电池很小，在保持活动 BLE 连接时续航仍然有限。
- 厂商协议未公开，ChatGPT Desktop 更新后可能失效。
- BLE 稳定性会受到电池状态、供电路径、主机无线电和旧配对缓存影响。建议保留一份已知可用的 Release 固件用于恢复。

<details>
<summary><strong>项目结构</strong></summary>

```text
include/BatteryEstimator.h   混合电量估算器
include/CodexMicroBle.h      BLE 传输与共享状态
src/CodexMicroBle.cpp        HID、RPC 分帧和加密标题 GATT 服务
src/main_stickc.cpp          UI、按键、IMU、标题同步与电源行为
tools/sync_titles_auto.py    自动 USB → BLE 标题同步
tools/sync_titles_ble.py     单次 BLE 标题同步
tools/sync_titles_usb.py     本地任务分配与标题读取
tools/title_sync_service.py  macOS/Windows 后台服务管理器
platformio.ini               可复现的 PlatformIO 构建配置
```

</details>

## 致谢

本项目移植自 [`imliubo/codex-micro-4-core2`](https://github.com/imliubo/codex-micro-4-core2)，沿用了其最初的 BLE HID 与 Codex Micro 协议实现，并保留原始版权和 MIT 许可证。

## 许可证

MIT。参见 [LICENSE](LICENSE) 与 [NOTICE.md](NOTICE.md)。
