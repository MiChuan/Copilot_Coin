#pragma once

#include <vector>
#include <nlohmann/json.hpp>
#include <ctime>
#include <cstdlib>
#include <cmath>

class MockDataGenerator {
public:
	// 生成模拟K线数据
	// symbol: 交易对
	// interval: 时间间隔 ("4h", "1d")
	// limit: 返回蜡烛数量
	// startPrice: 起始价格（默认50000）
	// volatility: 波动率（默认0.02 = 2%）
	static nlohmann::json generateKlines(
		const std::string& symbol,
		const std::string& interval,
		int limit,
		double startPrice = 50000.0,
		double volatility = 0.02
	);

	// 生成单根K线数据（Binance格式）
	// timestamp: 开盘时间戳 (ms)
	// open, high, low, close, volume: OHLCV数据
	static nlohmann::json createKlineCandle(
		long long timestamp,
		double open,
		double high,
		double low,
		double close,
		double volume
	);

	// 获取间隔（分钟数）
	static int getIntervalMinutes(const std::string& interval);

	// 初始化随机数生成器
	static void initialize(unsigned int seed = 0);

private:
	static bool initialized_;

	// 随机数生成函数
	static double randomGaussian(double mean = 0.0, double stddev = 1.0);
	static double randomDouble(double min, double max);
};
