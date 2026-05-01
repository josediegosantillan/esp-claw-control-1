# Edge Agent Guide

## How It Works

The main entry point is `application/edge_agent/main/main.c`.

After the device boots, the overall flow is:

1. Initialize NVS and load device settings
2. Mount FATFS at `/fatfs`
3. Initialize Wi-Fi and the local HTTP configuration service
4. Enter `app_claw_start()`
5. Initialize the event router, memory, skills, and capabilities
6. Initialize and start `claw_core`
7. Start the CLI and begin handling requests and events

The current runtime depends on the following local directories:

- `/fatfs/sessions`: session history
- `/fatfs/memory/MEMORY.md`: long-term memory
- `/fatfs/skills`: skill documents and manifest
- `/fatfs/scripts`: Lua scripts
- `/fatfs/router_rules/router_rules.json`: automation rules
- `/fatfs/inbox`: message attachment storage

The current app integrates the following capabilities:

- `cap_im_qq`
- `cap_im_tg`
- `cap_files`
- `cap_lua`
- `cap_mcp_client`
- `cap_mcp_server`
- `cap_skill_mgr`
- `cap_time`
- `cap_llm_inspect`
- `cap_web_search`

## Quick Start

### Prerequisites

- ESP-IDF is installed and exported
- `ESP-IDF v5.5.4` is recommended
- Current board in this repo: `esp32_S3_DevKitC_1`

```bash
. <your-esp-idf-path>/export.sh
```

On Windows PowerShell, ensure the Board Manager extension is visible and force UTF-8 output:

```powershell
$env:IDF_PATH='C:\esp\v5.5.4\esp-idf'
$env:IDF_TOOLS_PATH='C:\Espressif\tools'
$env:IDF_PYTHON_ENV_PATH='C:\Espressif\tools\python\v5.5.4\venv'
$env:IDF_EXTRA_ACTIONS_PATH='C:\path\to\edge_agent\managed_components\espressif__esp_board_manager'
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='utf-8'
```

### Configuration

To make `esp-board-manager` easier to use, first install the helper package with `pip install esp-bmgr-assist`. You only need to do this once in a given ESP-IDF environment.

1. Generate board support files:

```powershell
cd application/edge_agent
idf.py bmgr -l
idf.py bmgr -c ./boards -b esp32_S3_DevKitC_1
```

`idf.py bmgr` is the preferred workflow. `idf.py gen-bmgr-config` still exists as a legacy alias, but this repo should use `bmgr` as the default command.

2. Configure Wi-Fi, LLM, IM, search engine, and related parameters:

The key demo settings include:

- Wi-Fi SSID / Password
- LLM API Key / Provider / Model
- QQ App ID / App Secret
- Telegram Bot Token
- Brave / Tavily Search Key
- Timezone

Key Notes:

- IM bot token: available from Telegram [@BotFather](https://t.me/BotFather) or [QQ Bot](https://q.qq.com/qqbot/openclaw/login.html)
- LLM API key: available from [Anthropic Console](https://console.anthropic.com), [OpenAI Platform](https://platform.openai.com), or [Alibaba Cloud Bailian](https://bailian.console.aliyun.com/#/api-key)

You can adjust compile-time default values through `menuconfig`:

```powershell
idf.py menuconfig
```

Recommended project defaults:

- keep `LVGL` demos disabled
- keep `LVGL` examples disabled
- take flash offsets from the current build artifacts, not from old notes

3. Build and flash:

```powershell
Remove-Item -Recurse -Force .\build -ErrorAction SilentlyContinue
idf.py build
idf.py flash monitor
```

Current verified build characteristics:

- target: `esp32s3`
- board: `esp32_S3_DevKitC_1`
- flash size: `16MB`
- partition table: `partitions_16MB.csv`

For exact manual flashing arguments, use:

- `build/flasher_args.json`
- `build/flash_project_args`

The current verified layout is:

```text
0x0      bootloader/bootloader.bin
0x8000   partition_table/partition-table.bin
0xf000   ota_data_initial.bin
0x20000  edge_agent.bin
0x620000 storage.bin
```

If you need the full recovery and maintenance workflow, see `PROJECT_RECOVERY_AND_IMPROVEMENT_GUIDE.md`.
