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
├── run_test.ps1                 # 编译 + 回测
├── run_demo_live.ps1            # 编译 + Demo API 测试 + 实盘
├── test_api_endpoints.ps1       # PowerShell 公开接口测试
└── CMakeLists.txt
```

## 环境准备

### 依赖

- CMake 3.16+
- Visual Studio 2022 或兼容 C++ 工具链
- vcpkg：

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install nlohmann-json curl[openssl]:x64-windows openssl:x64-windows
```

### 网络代理（可选）

访问 `demo-fapi.binance.com` 或 testnet 若需代理，在 `config.json` 设置：

```json
"httpProxy": "http://127.0.0.1:7897"
```

或在 PowerShell 中：

```powershell
$env:HTTP_PROXY = "http://127.0.0.1:7897"
$env:HTTPS_PROXY = "http://127.0.0.1:7897"
```

## 构建

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

产物：

- `build/Release/Copilot_Coin.exe` — 主程序
- `build/Release/test_demo_api.exe` — API 测试

## 运行

### 回测

```powershell
.\run_test.ps1 -Release -Report
```

或：

```powershell
.\build\Release\Copilot_Coin.exe backtest
```

`config.json` 中 `backtest.mode` 为 `report` 时生成 `reports/` 报表；`csvInterval: "1m"` 时自动聚合为 1h。

### Demo 实盘（[demo.binance.com](https://demo.binance.com)）

1. 在 **Demo 站**（非主站）创建 API Key，启用合约权限。
2. 复制 `config_demo_live.json` 为 `config.json`，填入 `apiKey` / `secret`。
3. 测试 API：

```powershell
.\run_demo_live.ps1 -TestOnly
# 或
.\build\Release\test_demo_api.exe
```

4. 启动策略循环（会按信号下单）：

```powershell
.\run_demo_live.ps1
# 或
.\build\Release\Copilot_Coin.exe
```

> **不要** 带 `backtest` 参数，否则进入回测模式。

### PowerShell 公开接口快测

```powershell
.\test_api_endpoints.ps1
.\test_api_endpoints.ps1 -BaseUrl "https://testnet.binancefuture.com"
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
  "httpProxy": "http://127.0.0.1:7897",
  "live": {
    "leverage": 2.0,
    "positionPct": 0.3
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
- 实盘循环使用 **1h K 线**，与回测一致；轮询间隔 60 分钟。
- `config.json` 已在 `.gitignore` 中，请勿提交真实密钥。

## 仓库地址

- https://github.com/MiChuan/Copilot_Coin
