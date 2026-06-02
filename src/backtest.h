#pragma once
#include <nlohmann/json.hpp>
#include "strategy.h"

struct TradeRecord {
	long long entryTime=0;
	long long exitTime=0;
	double entryPrice=0;
	double exitPrice=0;
	double qty=0;
	double entryFee=0;
	double exitFee=0;
	double pnl=0;
};

struct BacktestResult {
	double initial_balance = 1000.0;
	double final_balance = 1000.0;
	int trades = 0;
	double winRate = 0.0;
	double maxDrawdown = 0.0;
	double sharpe = 0.0;
	std::vector<double> equityCurve;
	std::vector<long long> equityTime;
	std::vector<TradeRecord> tradesRec;
};

class Backtest {
public:
	Backtest(class BinanceHttp *api, Strategy *strategy);
	BacktestResult run(int hoursBack, double feePerc=0.0004, double slippagePerc=0.0005, double leverage=3.0, double initialBalance=1000.0);
private:
	BinanceHttp *api_;
	Strategy *strategy_;
};
