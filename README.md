# Copilot_Coin

一个基于 C++ 的 BTCUSDT 合约量化交易示例项目，支持实盘执行、离线 CSV 回测以及自动生成测试报表。

## 项目概览

本项目包含以下核心能力：

- BTCUSDT 永续合约策略示例
- 基于手续费、滑点和杠杆的回测模拟
- 支持本地 `1h` CSV 历史数据回测
- 支持回测结果导出为 `CSV` / `JSON` 报表
- 支持 `run_test.ps1` 一键编译 + 运行测试

## 目录结构

- `src/`：核心源码
- `data/`：离线历史数据
- `reports/`：回测输出报表（运行时生成）
- `run_test.ps1`：一键编译并运行离线测试
- `config.json`：运行与回测配置

## 环境准备

### 依赖

- CMake 3.16+
- Visual Studio 2022 或兼容的 C++ 编译工具链
- 推荐使用 `vcpkg` 管理依赖：

```powershell
git clone https://github.com/microsoft/vcpkg.git
.\vcpkg\bootstrap-vcpkg.bat
.\vcpkg\vcpkg.exe install nlohmann-json curl[openssl]:x64-windows openssl:x64-windows
```

## 构建

在仓库根目录执行：

```powershell
mkdir build
cd build
cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Debug
```

编译成功后，可执行文件通常位于：

```powershell
build\Debug\Copilot_Coin.exe
```

## 配置文件

`config.json` 用于保存 API 配置和回测参数：

```json
{
  "apiKey": "YOUR_API_KEY",
  "secret": "YOUR_SECRET",
  "backtest": {
    "initialBalance": 1000.0,
    "feePerc": 0.0004,
    "slippagePerc": 0.0005,
    "leverage": 3.0,
    "hoursBack": 8760,
    "mode": "report",
    "csvPath": "F:/Project/test/Copilot_Coin/data/BTCUSDT_1h.csv"
  }
}
```

### 参数说明

- `apiKey` / `secret`：币安 API 配置
- `initialBalance`：初始资金
- `feePerc`：手续费比例
- `slippagePerc`：滑点比例
- `leverage`：杠杆倍数
- `hoursBack`：回测时间长度（小时）
- `mode`：`run` / `offline` / `report`
- `csvPath`：离线 CSV 数据路径

## 运行方式

### 1. 一键编译 + 测试

推荐使用 `run_test.ps1`：

```powershell
.\run_test.ps1 -Report -OpenResults
```

常用参数：

- `-Release`：使用 Release 配置
- `-Clean`：运行前清理旧的 `reports/`
- `-SkipBuild`：跳过编译，直接运行已有可执行文件
- `-Report`：生成回测报表
- `-OpenResults`：运行结束后自动打开结果文件

示例：

```powershell
.\run_test.ps1 -Release -Clean -Report -OpenResults
```

### 2. 主命令 + 二级子命令

当前 CLI 结构如下：

- `backtest run`：标准回测
- `backtest offline`：离线 CSV 回测
- `backtest report`：离线 CSV 回测并生成报表

示例：

```powershell
.\build\Debug\Copilot_Coin.exe backtest report --csvPath "F:\Project\test\Copilot_Coin\data\BTCUSDT_1h.csv"
```

## 回测输出

运行 `backtest offline` 或 `backtest report` 后，会在 `reports/` 下生成：

- `bt_equity.csv`：收益曲线
- `bt_summary.json`：汇总指标
- `bt_trades.csv`：逐笔交易明细
- `bt_trades.json`：逐笔交易明细 JSON

### 查看结果

- `bt_summary.json`：适合快速查看胜率、最大回撤、最终资金等核心指标
- `bt_equity.csv`：适合用 Excel 或脚本画收益曲线
- `bt_trades.csv`：适合分析每一笔交易

## 实盘运行

如果要运行实时策略：

1. 配置 `config.json`
2. 确认没有启用回测模式文件 `run_backtest.flag`
3. 启动可执行文件

程序会按实时行情循环拉取数据并根据策略做出交易判断。

## 注意事项

- 本项目示例代码主要用于研究和测试，不包含完整生产级风控。
- 实盘前建议先在 Binance Futures Testnet 上验证。
- 回测逻辑包含手续费与滑点模拟，但不覆盖所有真实交易细节。
- 请确认历史数据的时间顺序与字段格式正确。

## 贡献

欢迎以小步提交方式进行扩展，并在提交信息中说明用途与影响。

## 仓库地址

- https://github.com/MiChuan/Copilot_Coin
