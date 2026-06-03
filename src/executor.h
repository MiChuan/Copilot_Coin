#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "binance_http.h"
#include <unordered_map>
#include <mutex>

struct Position {
	double qty = 0.0; // positive for long
	double avgPrice = 0.0;
};

class Executor {
public:
	Executor(BinanceHttp *api, double leverage=3.0);
	bool setLeverage(int lev);
	double getAvailableUSDT();
	nlohmann::json getPosition(const std::string &symbol);
	bool adjustQtyToStepAndMin(const std::string &symbol, double &qty);
	nlohmann::json marketBuy(const std::string &symbol, double usdtAmount, double leverage);
	nlohmann::json marketSell(const std::string &symbol, double usdtAmount, double leverage);
	nlohmann::json marketSellQty(const std::string &symbol, double qty);
	// in-memory position tracking
	Position getLocalPosition(const std::string &symbol);
	void handleFill(const std::string &symbol, double executedQty, double executedPrice, bool isBuy);
private:
	std::unordered_map<std::string, Position> positions_;
	std::mutex posMutex_;
private:
	BinanceHttp *api_;
	double leverage_;
};
