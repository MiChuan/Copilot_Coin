# Copilot_Coin

[![C++](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C.svg)](https://cmake.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows-0078D6.svg)](https://visualstudio.microsoft.com/)
[![Binance](https://img.shields.io/badge/Binance-USDT--M%20Futures-F0B90B.svg)](https://www.binance.com/)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![GitHub stars](https://img.shields.io/github/stars/MiChuan/Copilot_Coin?style=social)](https://github.com/MiChuan/Copilot_Coin/stargazers)

本项目基于 MIT License 开源发布，欢迎在遵守许可证条款的前提下自由使用、修改与再发布。

## 目录导航

- [项目简介](#introduction)
- [项目亮点](#highlights)
- [策略概述](#strategy)
- [回测结果](#backtest-results)
- [技术架构](#architecture)
- [目录结构](#structure)
- [环境准备](#requirements)
- [构建](#build)
- [运行](#run)
- [数据获取](#data-acquisition)
- [配置文件](#config)
- [回测输出](#backtest-output)
- [常见问题](#faq)
- [开源声明与版权归属](#open-source-statement--copyright)
- [免责声明](#disclaimer)

<a id="introduction"></a>
## 项目简介

Copilot_Coin 是一套基于 C++17 实现的 **BTCUSDT 永续合约量化交易系统**，支持多时间框架技术分析、离线回测与实盘执行。系统以日线判断趋势方向、1 小时线执行交易信号，内置完整的指标计算、信号引擎、风控参数与回测引擎，并提供 Binance USDT-M 合约 HTTP API 接入能力。

项目面向量化交易学习与研究场景，所有回测过程均模拟手续费与滑点，可一键生成回测报表，便于策略验证与参数调优。

<a id="highlights"></a>
## 项目亮点

- **多时间框架策略**：日线定趋势、1h 找信号，兼顾趋势跟踪与回调介入。
- **指标体系完整**：内置 SMA / EMA / RSI / MACD / BOLL / ATR 等技术指标，配置灵活。
- **回测与实盘双模式**：支持 CSV 历史数据离线回测与 Binance 实盘执行，统一入口管理。
- **数据管道配套**：提供 Python 脚本从 Binance 拉取 K 线数据并合并为 CSV，开箱即用。
- **结果可复现**：一键脚本完成编译、回测与报表生成，回测结果含手续费与滑点模拟。

<a id="strategy"></a>
## 策略概述

### 时间框架

- **日线**：判断趋势方向（BOLL / RSI / SMA）
- **1 小时线**：执行交易信号（BOLL / RSI / MACD / ATR / VOL）

### 指标配置

| 指标 | 日线参数 | 1h 参数 | 用途 |
|------|---------|---------|------|
| BOLL | (20, 1.5σ) | (20, 2.0σ) | 趋势方向 / 回调深度 |
| RSI | (14) | (14) | 超买超卖参考 / 回调衰竭判断 |
| SMA | (20) | — | 趋势辅助确认 |
| MACD | — | (12,26,9) | 金叉死叉 + 动能柱 |
| ATR | — | (14) | 动态止损参考 |
| VOL | — | 5 周期均值 | 放量 / 缩量判断 |

### 交易信号

**买入** — 多头趋势健康回调：

1. 日线多头（价格 > BOLL 中轨）
2. SMA20 斜率向上（趋势加速确认）
3. 1h 回调至支撑区（BOLL 下轨 4% 容差 **或** RSI 37–58）
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
- 单笔仓位：总资金 30%（最大 90%）
- TP1：盈利 ≥ 8% 平仓 50%
- TP2：盈利 ≥ 15% 平仓剩余 50%
- SL：亏损 ≥ 2.5% 平仓 80%

<a id="backtest-results"></a>
## 回测结果

| 指标 | 1h 数据 (2025.3~2026.6) | 1m→1h (2025.3~2026.3) |
|------|--------------------------|-------------------------|
| 收益率 | $1,000 → $3,799,961 | $1,000 → $4,526,103 |
| 最大回撤 | 3.45% | 3.84% |
| 胜率 | 45.2% (19/42) | 40.9% (18/44) |
| 交易次数 | 42 | 44 |
| Sharpe | 5.64 | 5.68 |

> 说明：以上结果基于历史数据的离线回测，仅用于策略研究，不构成投资建议，也不代表未来表现。

<a id="architecture"></a>
## 技术架构

项目由 C++ 核心与 Python 数据管道两部分组成：

- **C++ 核心**（CMake + vcpkg 依赖管理）：策略信号引擎、技术指标库、回测引擎、实盘执行器与 Binance HTTP API 客户端
- **数据管道**（Python）：从 Binance 拉取 K 线数据，下载、合并为回测所需的 CSV 文件
- **运行模式**：通过命令行参数切换回测（backtest）与实盘执行，回测报表自动输出至 `reports/`

<a id="structure"></a>
## 目录结构

```text
Copilot_Coin/
├── src/                          # C++ 源码
│   ├── main.cpp                  # 入口：实盘 / 回测分支
│   ├── strategy.cpp/h            # 策略信号引擎
│   ├── indicators.cpp/h          # 技术指标（SMA/EMA/RSI/MACD/BOLL/ATR）
│   ├── backtest.cpp/h            # 回测引擎 + 风控
│   ├── executor.cpp/h            # 实盘执行器
│   ├── binance_http.cpp/h        # 币安 HTTP API
│   ├── csv_kline_loader.cpp/h    # CSV 数据加载
│   ├── util.cpp/h                # 工具函数
│   ├── mock_data_generator.cpp/h # 模拟数据生成
│   └── test_csv.cpp              # CSV 加载器测试
├── data/                         # K 线数据（运行时生成 / 下载，不入库）
├── reports/                      # 回测报表（运行时生成，不入库）
├── download_um_klines.py         # 按时间范围下载 K 线（默认 1h）
├── download_um_klines_1m.py      # 下载近一年 1m 数据
├── download_um_klines_1m_full.py # 完整 1m 数据下载（支持代理与限速）
├── merge_klines.py               # 解压并合并月度 K 线为单个 CSV
├── config.json                   # API 与回测配置（示例，勿提交真实密钥）
├── run_test.ps1                  # 一键编译 + 回测脚本
├── CMakeLists.txt                # CMake 构建脚本
└── LICENSE                       # MIT 许可证
```

<a id="requirements"></a>
## 环境准备

### 依赖

- CMake 3.16+
- Visual Studio 2022 或兼容的 C++17 编译工具链
- vcpkg 管理依赖：

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install nlohmann-json curl[openssl]:x64-windows openssl:x64-windows
```

<a id="build"></a>
## 构建

```powershell
mkdir build && cd build
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

构建成功后，`build/Release/` 下会生成 `Copilot_Coin.exe` 与 `test_csv.exe`（CSV 加载器测试程序）。

<a id="run"></a>
## 运行

### 一键编译 + 回测

```powershell
.\run_test.ps1 -Release -Report
```

参数说明：

| 参数 | 作用 |
|------|------|
| `-Release` | Release 配置 |
| `-SkipBuild` | 跳过编译 |
| `-Report` | 生成回测报表 |
| `-Clean` | 清理旧报表 |
| `-OpenResults` | 自动打开结果 |

### CLI 命令

```powershell
# 使用 1h CSV
.\build\Release\Copilot_Coin.exe backtest report --csvPath "data\BTCUSDT_1h.csv"

# 使用 1m CSV（需在 config.json 设置 csvInterval: "1m"）
.\build\Release\Copilot_Coin.exe backtest report --csvPath "data\BTCUSDT_1m_20250301_20260302.csv"
```

<a id="data-acquisition"></a>
## 数据获取

仓库中的 `data/` 目录默认不随仓库分发，可通过以下 Python 脚本自行获取 K 线数据：

- `download_um_klines.py`：按时间范围下载 Binance USDT-M 合约 K 线（默认 1h，写入 `./data`）
- `download_um_klines_1m.py`：下载近一年 1 分钟 K 线数据
- `download_um_klines_1m_full.py`：完整 1m 数据下载，支持代理与请求限速（避免被 API 限流）
- `merge_klines.py`：解压并合并 Binance 月度 K 线归档（zip）为单个 CSV 文件

数据下载后，在 `config.json` 中指定 `csvPath` 与 `csvInterval` 即可开始回测。

<a id="config"></a>
## 配置文件

`config.json`：

```json
{
  "apiKey": "YOUR_API_KEY",
  "secret": "YOUR_SECRET",
  "backtest": {
    "initialBalance": 1000.0,
    "feePerc": 0.0004,
    "slippagePerc": 0.0005,
    "leverage": 2.0,
    "hoursBack": 8760,
    "mode": "report",
    "csvPath": "data/BTCUSDT_1h.csv",
    "csvInterval": "1m"
  }
}
```

| 字段 | 说明 |
|------|------|
| `csvPath` | CSV 数据文件路径 |
| `csvInterval` | 源数据粒度，如 `"1m"`、`"1h"`，回测会自动聚合成 1h |

<a id="backtest-output"></a>
## 回测输出

`reports/` 目录下生成：

- `bt_summary.json` — 汇总指标（胜率、回撤、最终资金等）
- `bt_equity.csv` — 权益曲线
- `bt_trades.csv` — 逐笔交易明细

<a id="faq"></a>
## 常见问题

### 1. CMake 配置或编译失败，找不到依赖

请确认已通过 vcpkg 安装 `nlohmann-json`、`curl[openssl]` 与 `openssl`，且 `-DCMAKE_TOOLCHAIN_FILE` 指向本机 vcpkg 的构建系统脚本。

### 2. 回测提示找不到 CSV 文件

`data/` 目录不随仓库分发，请先运行数据下载脚本生成 K 线 CSV，或检查 `config.json` 中的 `csvPath` 是否与本地路径一致。

### 3. 1m 数据如何参与回测

将 `csvInterval` 设为 `"1m"` 并指定 1m 粒度的 CSV 路径即可，回测引擎会自动将 1m K 线聚合为 1h。

### 4. 实盘模式如何配置

在 `config.json` 中填入 Binance API Key 与 Secret，并通过 CLI 进入实盘分支；实盘涉及真实资金，请先在 Binance Futures Testnet 完成验证。

### 5. 配置文件中的路径与示例不一致

示例中的 `csvPath` 为相对路径，本地配置可能因机器不同而使用绝对路径，请按实际数据位置调整。

<a id="open-source-statement--copyright"></a>
## 开源声明与版权归属

本项目基于 MIT License 开源发布（详见 [LICENSE](LICENSE)）。在保留原始署名与许可证声明的前提下，使用者可以自由复制、修改、分发、再发布，或用于个人学习与二次开发。

若将本项目代码用于课程设计、毕业设计展示或二次开发，建议保留原作者信息、仓库地址及许可说明，以尊重原始创作贡献。

<a id="disclaimer"></a>
## 免责声明

- 本项目用于研究和测试，不包含完整生产级风控。
- 回测包含手续费与滑点模拟，但不覆盖所有真实交易细节（如市场深度、网络延迟、极端行情等）。
- 实盘前建议先在 Binance Futures Testnet 验证。
- 加密货币交易存在高风险，历史回测收益不代表未来表现，本项目不构成任何投资建议。
