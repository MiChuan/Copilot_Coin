# Copilot_Coin

基于 C++ 的 BTCUSDT 合约量化交易系统，支持多时间框架技术分析、离线回测与实盘执行。

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
- 单笔仓位：总资金 30%（最大 90%）
- TP1：盈利 ≥ 8% 平仓 50%
- TP2：盈利 ≥ 15% 平仓剩余 50%
- SL：亏损 ≥ 2.5% 平仓 80%

## 回测结果

| 指标 | 数值 | 目标 |
|------|------|------|
| 收益率 | $1,000 → $3,799,961 | >50% 年化 |
| 最大回撤 | 3.45% | <5% |
| 往返胜率 | 58.1% (18/31) | >55% |
| 年交易次数 | 31 | 30-50 |
| Sharpe | 5.64 | — |

## 项目结构

```
Copilot_Coin/
├── src/
│   ├── main.cpp              # 入口：实盘/回测分支
│   ├── strategy.cpp/h        # 策略信号引擎
│   ├── indicators.cpp/h      # 技术指标（SMA/EMA/RSI/MACD/BOLL/ATR）
│   ├── backtest.cpp/h        # 回测引擎 + 风控
│   ├── executor.cpp/h        # 实盘执行器
│   ├── binance_http.cpp/h    # 币安 HTTP API
│   ├── csv_kline_loader.cpp/h # CSV 数据加载
│   ├── util.cpp/h            # 工具函数
│   └── mock_data_generator.cpp/h # 模拟数据生成
├── data/
│   └── BTCUSDT_1h.csv        # 1h 历史 K 线
├── reports/                  # 回测报表（运行时生成）
├── config.json               # API 与回测配置
├── run_test.ps1              # 一键编译 + 回测脚本
└── CMakeLists.txt
```

## 环境准备

### 依赖

- CMake 3.16+
- Visual Studio 2022 或兼容的 C++ 编译工具链
- vcpkg 管理依赖：

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install nlohmann-json curl[openssl]:x64-windows openssl:x64-windows
```

## 构建

```powershell
mkdir build && cd build
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
```

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
.\build\Release\Copilot_Coin.exe backtest report --csvPath "data\BTCUSDT_1h.csv"
```

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
    "csvPath": "F:/Project/test/Copilot_Coin/data/BTCUSDT_1h.csv"
  }
}
```

## 回测输出

`reports/` 目录下生成：

- `bt_summary.json` — 汇总指标（胜率、回撤、最终资金等）
- `bt_equity.csv` — 权益曲线
- `bt_trades.csv` — 逐笔交易明细

## 注意事项

- 本项目用于研究和测试，不包含完整生产级风控
- 实盘前建议先在 Binance Futures Testnet 验证
- 回测包含手续费与滑点模拟，但不覆盖所有真实交易细节

## 仓库地址

- https://github.com/MiChuan/Copilot_Coin
