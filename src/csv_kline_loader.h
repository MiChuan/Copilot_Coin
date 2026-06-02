#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class CsvKlineLoader {
public:
	// 从CSV文件加载K线数据
	// Returns: JSON array format compatible with Binance API
	// The CSV should have columns: open_time,open,high,low,close,volume,close_time,quote_volume,trades,taker_buy_base,taker_buy_quote,ignore
	static nlohmann::json loadFromCsv(const std::string& filePath, int limit = 1000);

	// 将CSV数据转换为Binance JSON格式
	static nlohmann::json convertToKlineFormat(
		long long openTime,
		double open,
		double high,
		double low,
		double close,
		double volume,
		long long closeTime,
		double quoteVolume,
		int trades,
		double takerBuyBase,
		double takerBuyQuote
	);

private:
	// 解析CSV一行
	static std::vector<std::string> parseCsvLine(const std::string& line);
	static double parseDouble(const std::string& str);
	static long long parseLongLong(const std::string& str);
};
