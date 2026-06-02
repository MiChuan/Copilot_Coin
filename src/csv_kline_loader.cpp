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
		// Trim whitespace
		field.erase(0, field.find_first_not_of(" \t\r\n"));
		field.erase(field.find_last_not_of(" \t\r\n") + 1);
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

nlohmann::json CsvKlineLoader::loadFromCsv(const std::string& filePath, int limit) {
	nlohmann::json result = nlohmann::json::array();
	std::ifstream file(filePath);

	if (!file.is_open()) {
		std::cerr << "Error: Cannot open CSV file: " << filePath << std::endl;
		return result;
	}

	std::string line;
	int lineCount = 0;

	// Skip header line
	if (!std::getline(file, line)) {
		std::cerr << "Error: CSV file is empty" << std::endl;
		return result;
	}

	// Read all data lines first to reverse them (get newest data first)
	std::vector<std::vector<std::string>> allData;
	while (std::getline(file, line) && allData.size() < limit * 2) {
		if (line.empty()) continue;

		auto fields = parseCsvLine(line);
		if (fields.size() < 12) {
			std::cerr << "Warning: Skipping line with insufficient fields" << std::endl;
			continue;
		}

		allData.push_back(fields);
	}

	file.close();

	// Take the last 'limit' entries (most recent data)
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

			nlohmann::json kline = convertToKlineFormat(
				openTime, open, high, low, close, volume,
				closeTime, quoteVolume, trades, takerBuyBase, takerBuyQuote
			);

			result.push_back(kline);
		} catch (const std::exception& e) {
			std::cerr << "Error parsing CSV line: " << e.what() << std::endl;
			continue;
		}
	}

	return result;
}
