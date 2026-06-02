#include "csv_kline_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

std::vector<std::string> CsvKlineLoader::parseCsvLine(const std::string& line) {
	std::vector<std::string> fields;
	std::stringstream ss(line);
	std::string field;

	while (std::getline(ss, field, ',')) {
		// Trim whitespace safely, even for empty fields
		const auto first = field.find_first_not_of(" \t\r\n");
		if (first == std::string::npos) {
			field.clear();
		} else {
			const auto last = field.find_last_not_of(" \t\r\n");
			field = field.substr(first, last - first + 1);
		}
		fields.push_back(field);
	}

	return fields;
}

double CsvKlineLoader::parseDouble(const std::string& str) {
	try {
		return std::stod(str);
	} catch(...) {
		return 0.0;
	}
}

long long CsvKlineLoader::parseLongLong(const std::string& str) {
	try {
		return std::stoll(str);
	} catch(...) {
		return 0;
	}
}

nlohmann::json CsvKlineLoader::convertToKlineFormat(
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
) {
	nlohmann::json kline = nlohmann::json::array();

	kline.push_back(openTime);                    // [0] Open time
	kline.push_back(std::to_string(open));        // [1] Open price
	kline.push_back(std::to_string(high));        // [2] High price
	kline.push_back(std::to_string(low));         // [3] Low price
	kline.push_back(std::to_string(close));       // [4] Close price
	kline.push_back(std::to_string(volume));      // [5] Base asset volume
	kline.push_back(closeTime);                   // [6] Close time
	kline.push_back(std::to_string(quoteVolume)); // [7] Quote asset volume
	kline.push_back(trades);                      // [8] Number of trades
	kline.push_back(std::to_string(takerBuyBase)); // [9] Taker buy base
	kline.push_back(std::to_string(takerBuyQuote)); // [10] Taker buy quote
	kline.push_back("0");                         // [11] Ignore

	return kline;
}

nlohmann::json CsvKlineLoader::aggregateKlines(const nlohmann::json& source, int intervalHours) {
	nlohmann::json result = nlohmann::json::array();
	if (!source.is_array() || source.empty() || intervalHours <= 1) {
		return source;
	}

	const int baseHours = 1;
	const int groupSize = intervalHours / baseHours;
	if (groupSize <= 1) {
		return source;
	}

	for (size_t i = 0; i + groupSize <= source.size(); i += groupSize) {
		const auto& first = source[i];
		const auto& last = source[i + groupSize - 1];

		long long openTime = first[0].get<long long>();
		long long closeTime = last[6].get<long long>();
		double open = std::stod(first[1].get<std::string>());
		double close = std::stod(last[4].get<std::string>());
		double high = std::stod(first[2].get<std::string>());
		double low = std::stod(first[3].get<std::string>());
		double volume = 0.0;
		double quoteVolume = 0.0;
		int trades = 0;
		double takerBuyBase = 0.0;
		double takerBuyQuote = 0.0;

		for (int j = 0; j < groupSize; ++j) {
			const auto& k = source[i + j];
			double kHigh = std::stod(k[2].get<std::string>());
			double kLow = std::stod(k[3].get<std::string>());
			double kVolume = std::stod(k[5].get<std::string>());
			double kQuoteVolume = std::stod(k[7].get<std::string>());
			int kTrades = k[8].get<int>();
			double kTakerBuyBase = std::stod(k[9].get<std::string>());
			double kTakerBuyQuote = std::stod(k[10].get<std::string>());

			high = std::max(high, kHigh);
			low = std::min(low, kLow);
			volume += kVolume;
			quoteVolume += kQuoteVolume;
			trades += kTrades;
			takerBuyBase += kTakerBuyBase;
			takerBuyQuote += kTakerBuyQuote;
		}

		result.push_back(convertToKlineFormat(
			openTime, open, high, low, close, volume,
			closeTime, quoteVolume, trades, takerBuyBase, takerBuyQuote
		));
	}

	return result;
}

nlohmann::json CsvKlineLoader::loadFromCsv(const std::string& filePath, int limit) {
	nlohmann::json result = nlohmann::json::array();
	std::ifstream file(filePath);

	if (!file.is_open()) {
		std::cerr << "Error: Cannot open CSV file: " << filePath << std::endl;
		return result;
	}

	std::string line;

	if (!std::getline(file, line)) {
		std::cerr << "Error: CSV file is empty" << std::endl;
		return result;
	}

	std::vector<std::vector<std::string>> allData;
	while (std::getline(file, line)) {
		if (line.empty()) continue;

		auto fields = parseCsvLine(line);
		if (fields.size() < 12) {
			std::cerr << "Warning: Skipping line with insufficient fields" << std::endl;
			continue;
		}

		allData.push_back(fields);
	}

	int startIdx = std::max(0, (int)allData.size() - limit);
	std::cout << "CSV Loader: Loaded " << allData.size() << " records, using last " << (allData.size() - startIdx) << std::endl;

	for (size_t i = startIdx; i < allData.size(); ++i) {
		const auto& fields = allData[i];
		try {
			long long openTime = parseLongLong(fields[0]);
			double open = parseDouble(fields[1]);
			double high = parseDouble(fields[2]);
			double low = parseDouble(fields[3]);
			double close = parseDouble(fields[4]);
			double volume = parseDouble(fields[5]);
			long long closeTime = parseLongLong(fields[6]);
			double quoteVolume = parseDouble(fields[7]);
			int trades = std::stoi(fields[8]);
			double takerBuyBase = parseDouble(fields[9]);
			double takerBuyQuote = parseDouble(fields[10]);

			result.push_back(convertToKlineFormat(openTime, open, high, low, close, volume, closeTime, quoteVolume, trades, takerBuyBase, takerBuyQuote));
		} catch (const std::exception& e) {
			std::cerr << "Error parsing CSV line: " << e.what() << std::endl;
		}
	}

	return result;
}
