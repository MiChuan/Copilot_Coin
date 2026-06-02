#include <iostream>
#include "csv_kline_loader.h"

int main() {
	std::cout << "Starting CSV test..." << std::endl;
	std::string csvPath = "F:\\Project\\test\\Copilot_Coin\\data\\BTCUSDT_1h.csv";

	std::cout << "Loading CSV from: " << csvPath << std::endl;

	try {
		auto data = CsvKlineLoader::loadFromCsv(csvPath, 100);
		std::cout << "Successfully loaded " << data.size() << " candles" << std::endl;

		if (data.size() > 0) {
			std::cout << "First candle: " << data[0].dump() << std::endl;
			std::cout << "Last candle: " << data[data.size()-1].dump() << std::endl;
		}
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	std::cout << "Test completed successfully!" << std::endl;
	return 0;
}
