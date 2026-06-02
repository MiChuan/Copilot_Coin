#include "mock_data_generator.h"
#include <algorithm>
#include <ctime>

bool MockDataGenerator::initialized_ = false;

void MockDataGenerator::initialize(unsigned int seed) {
	if (!initialized_) {
		if (seed == 0) {
			seed = static_cast<unsigned int>(std::time(nullptr));
		}
		std::srand(seed);
		initialized_ = true;
	}
}

double MockDataGenerator::randomDouble(double min, double max) {
	return min + (max - min) * (std::rand() / (double)RAND_MAX);
}

double MockDataGenerator::randomGaussian(double mean, double stddev) {
	// Box-Muller 转换
	static bool hasSpare = false;
	static double spare;

	if (hasSpare) {
		hasSpare = false;
		return mean + stddev * spare;
	}

	hasSpare = true;
	double u = randomDouble(0.0, 1.0);
	double v = randomDouble(0.0, 1.0);
	double s = std::sqrt(-2.0 * std::log(u)) * std::cos(2.0 * 3.14159265359 * v);
	spare = std::sqrt(-2.0 * std::log(u)) * std::sin(2.0 * 3.14159265359 * v);
	return mean + stddev * s;
}

int MockDataGenerator::getIntervalMinutes(const std::string& interval) {
	if (interval == "1m") return 1;
	if (interval == "5m") return 5;
	if (interval == "15m") return 15;
	if (interval == "30m") return 30;
	if (interval == "1h") return 60;
	if (interval == "4h") return 240;
	if (interval == "1d") return 1440;
	return 240; // 默认4h
}

nlohmann::json MockDataGenerator::createKlineCandle(
	long long timestamp,
	double open,
	double high,
	double low,
	double close,
	double volume
) {
	nlohmann::json kline = nlohmann::json::array();

	kline.push_back(timestamp);                      // [0] 开盘时间
	kline.push_back(std::to_string(open));           // [1] 开盘价
	kline.push_back(std::to_string(high));           // [2] 最高价
	kline.push_back(std::to_string(low));            // [3] 最低价
	kline.push_back(std::to_string(close));          // [4] 收盘价
	kline.push_back(std::to_string(volume));         // [5] 成交量
	kline.push_back(timestamp + getIntervalMinutes("4h") * 60 * 1000);  // [6] 收盘时间
	kline.push_back(std::to_string(volume * close)); // [7] 成交额
	kline.push_back(100);                            // [8] 成交笔数（模拟）
	kline.push_back(std::to_string(volume * 0.6));   // [9] 主买成交量
	kline.push_back(std::to_string(volume * 0.6 * close)); // [10] 主买成交额
	kline.push_back("0");                            // [11] 忽略

	return kline;
}

nlohmann::json MockDataGenerator::generateKlines(
	const std::string& symbol,
	const std::string& interval,
	int limit,
	double startPrice,
	double volatility
) {
	initialize();

	nlohmann::json result = nlohmann::json::array();

	int intervalMinutes = getIntervalMinutes(interval);
	long long currentTime = std::time(nullptr) * 1000; // 当前时间（毫秒）

	// 从过去往未来生成数据（limit个蜡烛）
	currentTime -= (long long)limit * intervalMinutes * 60 * 1000;

	double currentPrice = startPrice;

	for (int i = 0; i < limit; ++i) {
		// 使用几何随机游走模型生成价格
		double logReturn = randomGaussian(0.0, volatility);
		double returnMultiplier = std::exp(logReturn);

		double openPrice = currentPrice;
		currentPrice = openPrice * returnMultiplier;

		// 在这个candle内的高低价
		double range = openPrice * volatility * 0.5;
		double highPrice = std::max(openPrice, currentPrice) + randomDouble(0, range);
		double lowPrice = std::min(openPrice, currentPrice) - randomDouble(0, range);
		double closePrice = currentPrice;

		// 生成成交量（随机）
		double baseVolume = 100.0; // 基础成交量
		double volume = baseVolume * randomDouble(0.5, 2.0);

		// 创建K线
		nlohmann::json kline = createKlineCandle(
			currentTime,
			openPrice,
			highPrice,
			lowPrice,
			closePrice,
			volume
		);

		result.push_back(kline);

		// 移向下一个时间点
		currentTime += (long long)intervalMinutes * 60 * 1000;
	}

	return result;
}
