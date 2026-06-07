# Copilot_Coin

基于 C++ 的 BTCUSDT 合约量化交易系统，支持多时间框架技术分析、离线回测与 **Binance Demo** 实盘测试。

## 策略概述

### 时间框架

- **日线**：判断趋势方向（BOLL/RSI/SMA）
- **1小时线**：执行交易信号（BOLL/RSI/MACD/ATR/VOL）

### 指标配置

| 指标 | 日线参数 | 1h 参数 | 用途 |
|------|---------|---------|------|
| BOLL | (20, 1.5σ) | (20, 2.0σ) | 趋势方向 / 回调深度 |
| RSI | (14) | (14) | 超买超卖参考 / 回调衰竭判断 |
| SMA | (20) | — | 趋势辅助确认 |
| MACD | — | (12,26,9) | 金叉死叉 + 动能柱 |
| ATR | — | (14) | 动态止损参考 |
| VOL | — | 5周期均值 | 放量/缩量判断 |

### 交易信号

**买入** — 多头趋势健康回调：

1. 日线多头（价格 > BOLL 中轨）
2. SMA20 斜率向上（趋势加速确认）
3. 1h 回调至支撑区（BOLL 下轨 4% 容差 **或** RSI 37-58）
4. MACD 转多（金叉 **或** 柱状体回升）
5. 成交量不缩量

**卖出** — 任一条件触发：

| 条件 | 规则 |
|------|------|
| A: 超买衰竭 | 1h RSI > 70 + 接近 BOLL 上轨 + MACD 转空 |
| B: 趋势翻转 | 日线转空 + 日线 RSI < 35 |
| C: 动量衰竭 | 1h RSI > 65 + MACD 死叉（提前止盈） |

### 风控参数

- 杠杆：2x
- 单笔仓位：总资金 30%（`live.positionPct`）
- TP1：盈利 ≥ 8% 平仓 50%
- TP2：盈利 ≥ 15% 平仓剩余 50%
- SL：亏损 ≥ 2.5% 平仓 80%

## 回测结果

| 指标 | 1h 数据 (2025.3~2026.6) | 1m→1h (2025.3~2026.3) |
|------|--------------------------|-------------------------|
| 收益率 | $1,000 → $3,799,961 | $1,000 → $4,526,103 |
| 最大回撤 | 3.45% | 3.84% |
| 胜率 | 45.2% (19/42) | 40.9% (18/44) |
| 交易次数 | 42 | 44 |
| Sharpe | 5.64 | 5.68 |

## 项目结构

```
Copilot_Coin/
├── src/
│   ├── main.cpp                 # 入口：Demo 实盘 / 回测
│   ├── strategy.cpp/h           # 策略信号引擎
│   ├── indicators.cpp/h       # 技术指标
│   ├── backtest.cpp/h           # 回测引擎
│   ├── executor.cpp/h           # 实盘下单
│   ├── binance_http.cpp/h       # 币安 Futures API（含代理）
│   ├── test_demo_api.cpp        # API 端点连通性测试
│   └── csv_kline_loader.cpp/h   # CSV 加载与聚合
├── data/                        # K 线 CSV（本地，不入库）
├── reports/                     # 回测报表（运行时生成）
├── config.json                  # 本地配置（不入库，含密钥）
├── config_demo_live.json        # Demo 实盘配置模板
├── run_test.ps1                 # 编译 + 回测 (Windows)
├── run_demo_live.ps1            # 编译 + Demo API 测试 + 实盘 (Windows)
├── run_test.sh                  # 编译 + 回测 (Linux)
├── run_demo_live.sh             # 编译 + Demo API 测试 + 实盘 (Linux)
├── test_api_endpoints.ps1       # PowerShell 公开接口测试
├── test_api_connection.sh       # Bash 公开接口测试
└── CMakeLists.txt
```

## 环境准备

### 依赖

| 平台 | 依赖 |
|------|------|
| 通用 | CMake 3.16+, C++17 编译器 |
| Windows | Visual Studio 2022 或兼容 C++ 工具链 |
| Linux | g++ 9+, libcurl4-openssl-dev, libssl-dev |

nlohmann-json 通过 CMake FetchContent 自动下载，无需手动安装。

#### Windows (vcpkg)

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install nlohmann-json curl[openssl]:x64-windows openssl:x64-windows
```

#### Linux (apt)

```bash
sudo apt-get install -y cmake g++ libcurl4-openssl-dev libssl-dev
```

### 网络代理（可选）

访问 `demo-fapi.binance.com` 或 testnet 若需代理，在 `config.json` 设置：

```json
"httpProxy": "http://127.0.0.1:10808"
```

Windows 环境变量：

```powershell
$env:HTTP_PROXY = "http://127.0.0.1:10808"
$env:HTTPS_PROXY = "http://127.0.0.1:10808"
```

Linux 环境变量：

```bash
export HTTP_PROXY="http://127.0.0.1:10808"
export HTTPS_PROXY="http://127.0.0.1:10808"
```

## 构建

### Windows

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

产物：

- `build/Release/Copilot_Coin.exe` — 主程序
- `build/Release/test_demo_api.exe` — API 连通性测试
- `build/Release/test_csv.exe` — CSV 加载测试

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

产物：

- `build/Copilot_Coin` — 主程序
- `build/test_demo_api` — API 连通性测试
- `build/test_csv` — CSV 加载测试

---

## 测试操作步骤

### 前置条件

| 项目 | 要求 |
|------|------|
| 编译 | 已完成 Release 构建 |
| Demo API Key | 在 [demo.binance.com](https://demo.binance.com) 创建，启用合约权限 |
| 持仓模式 | **单向持仓**（One-way Mode） |
| 保证金模式 | 联合保证金（Multi-Assets）可选，程序已支持 USDT+USDC |
| 网络 | 国内需配置代理（见下方） |

> Demo / Testnet / 主网 API Key **不可混用**。

---

### 步骤 1：配置

1. 编辑 `config_demo_live.json`，填入 Demo 站的 `apiKey` / `secret`（勿提交真实密钥）。
2. 如需代理，添加 `"httpProxy": "http://127.0.0.1:10808"`。
3. 运行脚本时会自动复制为 `config.json`（已在 `.gitignore`，不会入库）。

`live` 段推荐配置：

```json
"live": {
  "leverage": 2.0,
  "positionPct": 0.3,
  "recvWindowMs": 60000,
  "timeSyncIntervalSeconds": 300,
  "pollIntervalSeconds": 60
}
```

| 字段 | 说明 |
|------|------|
| `pollIntervalSeconds` | 实盘轮询间隔（秒），默认 60 = 每分钟查一次 |
| `recvWindowMs` | 签名请求时间窗口（毫秒） |
| `timeSyncIntervalSeconds` | 后台与服务器时间同步间隔（秒） |

---

### 步骤 2：回测测试

**离线 CSV 回测（生成报表）：**

Windows：

```powershell
# 确保存在 run_backtest.flag 或使用 backtest 参数
echo. > run_backtest.flag
.\run_test.ps1 -Release -Report
```

Linux：

```bash
# 确保存在 run_backtest.flag 或使用 backtest 参数
touch run_backtest.flag
./run_test.sh --csv data/BTCUSDT_1h.csv --report
```

**在线拉 K 线回测：**

Windows：

```powershell
.\build\Release\Copilot_Coin.exe backtest
```

Linux：

```bash
./build/Copilot_Coin backtest
```

成功标志：终端输出 `Backtest trades=...`，`backtest.mode=report` 时在 `reports/` 生成 CSV/JSON。

---

### 步骤 3：Demo API 连通性测试（必做）

仅测 API，不下单：

Windows：

```powershell
.\run_demo_live.ps1 -TestOnly
```

Linux：

```bash
./run_demo_live.sh config_demo_live.json Release --test-only
```

或手动：

Windows：

```powershell
Copy-Item config_demo_live.json config.json -Force
.\build\Release\test_demo_api.exe
```

Linux：

```bash
cp config_demo_live.json config.json
./build/test_demo_api
```

**预期输出：**

```
=== Summary: ALL PASSED ===
[OK  ] /fapi/v1/time
[OK  ] /fapi/v2/account
       totalWalletBalance=...
```

若失败：检查 API Key 来源、代理、防火墙。

---

### 步骤 4：Demo 实盘测试

Windows：

```powershell
.\run_demo_live.ps1
```

Linux：

```bash
./run_demo_live.sh
```

脚本会依次：编译 → API 测试 → 启动 live 循环。

**不要** 带 `backtest` 参数；若存在 `run_backtest.flag`，脚本会自动删除（否则会误进回测模式）。

**预期启动日志：**

```
[Main] CLI parsed: command=live ...
[Executor] Position mode: one-way
[BinanceHttp] Server time synced, offsetMs=...
[Main] Live poll interval: 60s (1h RSI + strategy)
```

**每轮轮询日志（约每分钟）：**

```
[Live] Poll rsi1h=45.2 pos=0 buy=0 sell=1 reason=SELL|trendFlip|RSId=15
[Live] Sell skipped: no long position (qty=0) reason=...
```

| 日志 | 含义 |
|------|------|
| `Poll rsi1h=` | 基于最新 1h K 线计算的 RSI(14) |
| `pos=0` | 当前无持仓 |
| `Sell skipped: no long position` | 策略发出卖信号但无多仓，**正常**（只做多，不平空开空） |
| `Buy exec: ... "status":"FILLED"` | 开多成功 |
| `Sell exec: ... "reduceOnly":true` | 平多成功 |

停止程序：终端 **Ctrl+C**。

在 [Demo 合约钱包](https://demo.binance.com/en/my/wallet/account/futures) 查看余额与持仓。

---

### 步骤 5：公开接口快测（可选）

不依赖 API Key：

Windows：

```powershell
.\test_api_endpoints.ps1
.\test_api_endpoints.ps1 -BaseUrl "https://demo-fapi.binance.com"
```

Linux：

```bash
./test_api_connection.sh
```

---

### 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 进入 `backtest mode` 而非 live | 存在 `run_backtest.flag` 或传了 `backtest` 参数 | 删除 flag 或不带参数运行 |
| `-4061 position side` | 账户为双向持仓 | 改为单向持仓，或重启程序（会自动调用 API 设置） |
| `-1021 timestamp` | 本地时间与服务器偏差 | 已内置时间同步；可调大 `recvWindowMs` |
| 有 SELL 信号但不下单 | 无多仓可平 | 正常；等 BUY 信号且空仓时才会开多 |
| 启动时 `Closing stray short` | 上次误开空仓 | 程序自动 reduceOnly 平仓，一次性行为 |

---

## 运行（快捷命令）

### 回测

Windows：

```powershell
.\run_test.ps1 -Release -Report
# 或
.\build\Release\Copilot_Coin.exe backtest
```

Linux：

```bash
./run_test.sh --csv data/BTCUSDT_1h.csv --report
# 或
./build/Copilot_Coin backtest
```

`config.json` 中 `backtest.mode` 为 `report` 时生成 `reports/` 报表；`csvInterval: "1m"` 时自动聚合为 1h。

### Demo 实盘

Windows：

```powershell
.\run_demo_live.ps1 -TestOnly   # 仅测 API
.\run_demo_live.ps1             # API 测试 + 实盘循环
```

Linux：

```bash
./run_demo_live.sh config_demo_live.json Release --test-only  # 仅测 API
./run_demo_live.sh                                          # API 测试 + 实盘循环
```

CLI 可选参数：

```bash
# Linux
./build/Copilot_Coin --pollInterval 60 --recvWindow 60000 --timeSyncInterval 300

# Windows
.\build\Release\Copilot_Coin.exe --pollInterval 60 --recvWindow 60000 --timeSyncInterval 300
```

| 参数 | 说明 |
|------|------|
| `backtest [run\|offline\|report]` | 进入回测模式，可指定子模式 |
| `--csvPath <path>` | CSV 文件路径（回测） |
| `--initialBalance <num>` | 初始资金（默认 1000） |
| `--feePerc <num>` | 手续费率（默认 0.0004） |
| `--slippagePerc <num>` | 滑点率（默认 0.0005） |
| `--leverage <num>` | 杠杆（默认 3.0） |
| `--recvWindow <ms>` | 签名请求时间窗口（默认 60000） |
| `--timeSyncInterval <sec>` | 服务器时间同步间隔（默认 300） |
| `--pollInterval <sec>` | 实盘轮询间隔（默认 60） |
| `--hoursBack <num>` | 回测回溯小时数（默认 8760） |

> **不要** 带 `backtest` 参数运行 Demo 实盘。

### Linux 公开接口快测

```bash
./test_api_connection.sh
```

## API 端点

| 类型 | 端点 | 说明 |
|------|------|------|
| 公开 | `/fapi/v1/time` | 服务器时间 |
| 公开 | `/fapi/v1/exchangeInfo` | 交易规则 |
| 公开 | `/fapi/v1/ticker/price` | 最新价 |
| 公开 | `/fapi/v1/depth` | 深度 |
| 公开 | `/fapi/v1/klines` | K 线 |
| 签名 | `/fapi/v2/account` | 账户信息（程序使用） |
| 签名 | `/fapi/v2/balance` | 资产余额（程序使用） |

Demo 环境 Base URL：`https://demo-fapi.binance.com`

## 配置文件

参考 `config_demo_live.json`（勿将含真实密钥的 `config.json` 提交到 Git）：

```json
{
  "apiKey": "YOUR_DEMO_API_KEY",
  "secret": "YOUR_DEMO_SECRET",
  "useDemo": true,
  "demoApiBaseUrl": "https://demo-fapi.binance.com",
  "demoApiHost": "",
  "httpProxy": "http://127.0.0.1:10808",
  "live": {
    "leverage": 2.0,
    "positionPct": 0.3,
    "recvWindowMs": 60000,
    "timeSyncIntervalSeconds": 300,
    "pollIntervalSeconds": 60
  },
  "backtest": {
    "initialBalance": 1000.0,
    "feePerc": 0.0004,
    "slippagePerc": 0.0005,
    "leverage": 2.0,
    "hoursBack": 8760,
    "mode": "report",
    "csvPath": "data/BTCUSDT_1m_20250301_20260302.csv",
    "csvInterval": "1m"
  }
}
```

| 字段 | 说明 |
|------|------|
| `useDemo` | `true` 使用 Demo Futures API |
| `httpProxy` | HTTP/HTTPS 代理地址 |
| `live.positionPct` | 单笔占用钱包余额比例 |
| `live.leverage` | 合约杠杆 |
| `live.pollIntervalSeconds` | 实盘轮询间隔（秒），默认 60 |
| `live.recvWindowMs` | 签名请求 recvWindow（毫秒） |
| `live.timeSyncIntervalSeconds` | 后台时间同步间隔（秒） |
| `backtest.csvPath` | 回测 CSV 路径 |
| `backtest.csvInterval` | 源粒度，`1m` 会聚合为 1h |
| `backtest.mode` | `report` / `offline` / `run`（仅回测生效） |

## 回测输出

`reports/` 目录：

- `bt_summary.json` — 汇总指标
- `bt_equity.csv` — 权益曲线
- `bt_trades.csv` — 交易明细

## 注意事项

- 本项目用于研究与测试，非生产级风控。
- Demo 与 Testnet、主网 API Key **不可混用**。
- 实盘循环：**每分钟**拉取最新 1h/1d K 线，计算 1h RSI 与策略信号；轮询间隔由 `live.pollIntervalSeconds` 控制（默认 60 秒）。
- 实盘为**只做多**：BUY 仅空仓开多，SELL 仅持多平仓（`reduceOnly`），不会开空仓。
- 启动时会同步交易所持仓，并自动平掉误开的空仓。
- `config.json` 已在 `.gitignore` 中，请勿提交真实密钥。

## 仓库地址

- https://github.com/MiChuan/Copilot_Coin
