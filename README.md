# Copilot_Coin
简易的币安合约量化策略执行机器人（C++）

功能概要
- BTCUSDT 永续合约策略示例：基于日线 BOLL 与 4 小时级别 RSI(6)、MACD、成交量判断买卖信号。
- 支持杠杆、按保证金/仓位计算下单、部分成交处理与精确下单步长对齐。
- 包含回测模块（4h 数据、手续费与滑点模拟、基础统计指标）。

快速开始
1. 准备：安装 CMake 与 Visual Studio（含 C++ 工作负载），确保系统可用 libcurl 与 OpenSSL 开发库。
2. 克隆并构建：
   - mkdir build && cd build
   - cmake ..
   - cmake --build . --config Release
3. 配置：编辑根目录的 config.json，填入你的 Testnet 或真实 API key/secret（建议先用 Testnet）。
4. 回测运行：在仓库根创建空文件 run_backtest.flag，或运行可执行文件并传入 --backtest。可配置参数在 config.json 的 backtest 节点或通过命令行覆盖。
   - 示例：Copilot_Coin.exe --backtest --initialBalance 2000 --feePerc 0.0003 --slippagePerc 0.0006 --leverage 2 --hoursBack 8760
5. 实盘运行：删除 run_backtest.flag，程序进入每分钟轮询模式并按策略下单（请务必先在 Testnet 完整测试）。

重要注意事项（必须阅读）
- 示例代码为教学与框架用途，未包含完整风控与高可用措施。生产环境必须补足：完整错误处理、重试、限频、日志、持久化、监控与报警。
- 下单前请验证 exchangeInfo 中的最小下单量、步长与合约面值，示例代码做了基本对齐处理但不保证覆盖所有情况。
- 回测为简化模型：包含手续费与滑点的近似模拟，但未考虑融资费用、强平、部分成交复杂管理等。
- 强烈建议先在 Binance Futures Testnet 上验证所有行为，确认无误再上主网。

如何贡献
- 请以小步提交（功能切分），并在提交说明中描述变更影响。仓库已包含基础 CMake 配置，可在 Windows 下用 Visual Studio 打开构建。

联系人
- 仓库: https://github.com/MiChuan/Copilot_Coin

