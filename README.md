# Copilot_Coin
简易的币安合约量化策略执行机器人（C++）

功能概要
- BTCUSDT 永续合约策略示例：基于日线 BOLL 与 4 小时级别 RSI(6)、MACD、成交量判断买卖信号。
- 支持杠杆、按保证金/仓位计算下单、部分成交处理与精确下单步长对齐。
- 包含回测模块（4h 数据、手续费与滑点模拟、基础统计指标）。

构建与运行说明

1) 环境准备
- 安装 CMake（>= 3.16 推荐）与 Visual Studio（含 C++ 工作负载）。
- 推荐使用 vcpkg 管理依赖：
  - git clone https://github.com/microsoft/vcpkg.git
  - .\vcpkg\bootstrap-vcpkg.bat
  - .\vcpkg\vcpkg.exe install nlohmann-json curl[openssl]:x64-windows openssl:x64-windows

2) 本地构建（Windows / PowerShell）
- 在仓库根：
  - Remove-Item -Recurse -Force build  # 可选：清理
  - mkdir build; cd build
  - cmake .. -A x64 -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
  - cmake --build . --config Debug
- 可执行文件位置： build\Debug\Copilot_Coin.exe

3) 配置（config.json）
- 在仓库根编辑 config.json：
{
  "apiKey": "YOUR_API_KEY",
  "secret": "YOUR_SECRET",
  "backtest": {
    "initialBalance": 1000.0,
    "feePerc": 0.0004,
    "slippagePerc": 0.0005,
    "leverage": 3.0,
    "hoursBack": 8760
  }
}
- 建议先在 Binance Futures Testnet 创建 API Key 并填入，以避免真实资金风险。

4) 回测运行
- 推荐使用 `run_test.ps1` 一键完成编译 + 回测：
  - Debug：`.\run_test.ps1 -Report -OpenResults`
  - Release：`.\run_test.ps1 -Release -Report -OpenResults`
  - 运行前清理旧报表：`.\run_test.ps1 -Clean -Report -OpenResults`
- 主命令 + 二级子命令：
  - `backtest run`：标准回测
  - `backtest offline`：离线 CSV 回测并输出报表
  - `backtest report`：离线回测 + 生成 `reports/` 下的 CSV / JSON 报表
- 方式 A（直接运行离线 CSV）：
  - 使用本地一年 BTCUSDT 1 小时数据：
    .\\build\\Debug\\Copilot_Coin.exe backtest report --csvPath "F:\\Project\\test\\Copilot_Coin\\data\\BTCUSDT_1h.csv"
  - 或使用：`backtest offline --csvPath ...`
- 方式 B（传统参数兼容）：
  - 直接运行并传入覆盖参数：
    .\\build\\Debug\\Copilot_Coin.exe backtest run --initialBalance 2000 --feePerc 0.0003 --slippagePerc 0.0006 --leverage 2 --hoursBack 8760
- 输出：程序会打印回测摘要（交易次数、起止资金、胜率、最大回撤、夏普等），并在 `reports/` 下生成：
  - `bt_equity.csv`
  - `bt_summary.json`
  - `bt_trades.csv`
  - `bt_trades.json`

5) 实盘（实时）运行
- 确认 config.json 填入正确的 apiKey/secret（Testnet 或主网）。
- 删除 run_backtest.flag（若存在）： Remove-Item run_backtest.flag
- 运行可执行文件，程序将每分钟拉取 K 线并根据策略判断下单。


重要注意事项（必须阅读）
- 示例代码为教学与框架用途，未包含完整风控与高可用措施。生产环境必须补足：完整错误处理、重试、限频、日志、持久化、监控与报警。
- 下单前请验证 exchangeInfo 中的最小下单量、步长与合约面值，示例代码做了基本对齐处理但不保证覆盖所有情况。
- 回测为简化模型：包含手续费与滑点的近似模拟，但未考虑融资费用、强平、部分成交复杂管理等。
- 强烈建议先在 Binance Futures Testnet 上验证所有行为，确认无误再上主网。

如何贡献
- 请以小步提交（功能切分），并在提交说明中描述变更影响。仓库已包含基础 CMake 配置，可在 Windows 下用 Visual Studio 打开构建。

联系人
- 仓库: https://github.com/MiChuan/Copilot_Coin

