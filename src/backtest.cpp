#include "backtest.h"
#include "binance_http.h"
#include <iostream>
#include <cmath>

Backtest::Backtest(BinanceHttp *api, Strategy *strategy)
	: api_(api), strategy_(strategy) {}

BacktestResult Backtest::run(int hoursBack, double feePerc, double slippagePerc, double leverage, double initialBalance) {
	int candles = hoursBack;
	if (candles < 120) candles = 120;
	std::cout << "[Backtest] Fetching " << candles << " 1h candles" << std::endl;
	auto allK = api_->getKlines("BTCUSDT", "1h", candles);
	std::cout << "[Backtest] Total candles loaded: " << allK.size() << std::endl;
	return runOnKlines(allK, feePerc, slippagePerc, leverage, initialBalance);
}

BacktestResult Backtest::runFrom1hKlines(const nlohmann::json& klines, double feePerc, double slippagePerc, double leverage, double initialBalance) {
	std::cout << "[Backtest] Running from in-memory 1h klines: " << klines.size() << std::endl;
	return runOnKlines(klines, feePerc, slippagePerc, leverage, initialBalance);
}

BacktestResult Backtest::runOnKlines(const nlohmann::json& allK, double feePerc, double slippagePerc, double leverage, double initialBalance) {
	BacktestResult res;
	std::cout << "[Backtest] Params: fee=" << feePerc
		<< " slippage=" << slippagePerc
		<< " leverage=" << leverage
		<< " initialBalance=" << initialBalance << std::endl;

	if (!allK.is_array()) {
		std::cerr << "[Backtest][Error] Kline payload is not an array" << std::endl;
		return res;
	}
	if (allK.empty()) {
		std::cerr << "[Backtest][Error] Kline payload is empty" << std::endl;
		return res;
	}
	if (allK.size() < 120) {
		std::cerr << "[Backtest][Error] Not enough data loaded: got " << allK.size() << ", need at least 120" << std::endl;
		return res;
	}

	double balance = initialBalance;
	double position = 0.0;
	double entryPrice = 0.0;
	std::vector<double> equity;
	std::vector<long long> times;
	std::vector<double> returns;
	std::vector<double> closes;
	std::vector<double> vols;
	std::vector<double> dailyCloses;

	for (size_t i = 0; i < allK.size(); i++) {
		try {
			double price = std::stod(allK[i][4].get<std::string>());
			double volume = std::stod(allK[i][5].get<std::string>());
			long long time = allK[i][0].get<long long>();

			closes.push_back(price);
			vols.push_back(volume);
			if ((i + 1) % 24 == 0) {
				dailyCloses.push_back(price);
			}

			if (i < 3 || i + 1 == allK.size() || (i + 1) % 100 == 0) {
				std::cout << "[Backtest][Candle] i=" << i
					<< " time=" << time
					<< " price=" << price
					<< " closeCount=" << closes.size()
					<< " dayCount=" << dailyCloses.size()
					<< " volCount=" << vols.size()
					<< " pos=" << position
					<< " bal=" << balance << std::endl;
			}

			if (closes.size() < 120 || dailyCloses.size() < 21 || vols.size() < 50) {
				double netEquity = balance + (position > 0.0 ? (position * price / leverage) : 0.0);
				equity.push_back(netEquity);
				times.push_back(time);
				continue;
			}

			MarketState ms;
			ms.close_4h = closes;
			ms.vol_4h = vols;
			ms.close_1d = dailyCloses;

			auto sig = strategy_->evaluate(ms);
			if (!sig.reason.empty()) {
				std::cout << "[Backtest][Signal] i=" << i << " buy=" << sig.buy << " sell=" << sig.sell << " reason=" << sig.reason << std::endl;
			}

			if (sig.buy && position == 0.0) {
				double usdt = balance * 0.8;
				double effectivePrice = price * (1.0 + slippagePerc);
				double qty = (usdt * leverage) / effectivePrice;
				double entryFee = usdt * feePerc;
				position = qty;
				entryPrice = effectivePrice;
				balance -= entryFee;
				TradeRecord tr; tr.entryTime = time; tr.entryPrice = entryPrice; tr.qty = qty; tr.entryFee = entryFee;
				res.tradesRec.push_back(tr);
				std::cout << "[Backtest][TradeOpen] time=" << time << " price=" << effectivePrice << " qty=" << qty << " entryFee=" << entryFee << " balance=" << balance << std::endl;
			}

			if (sig.sell && position > 0.0) {
				double effectivePrice = price * (1.0 - slippagePerc);
				double gross = position * effectivePrice;
				double exitFee = gross * feePerc;
				double pnl = (effectivePrice - entryPrice) * position;
				balance += (gross / leverage) + pnl;
				balance -= exitFee;
				if (!res.tradesRec.empty()) {
					auto &tr = res.tradesRec.back();
					tr.exitTime = time; tr.exitPrice = effectivePrice; tr.exitFee = exitFee; tr.pnl = pnl - tr.entryFee - exitFee;
				}
				std::cout << "[Backtest][TradeClose] time=" << time << " price=" << effectivePrice << " gross=" << gross << " pnl=" << pnl << " exitFee=" << exitFee << " balance=" << balance << std::endl;
				position = 0.0;
				entryPrice = 0.0;
			}

			double netEquity = balance + (position > 0.0 ? (position * price / leverage) : 0.0);
			equity.push_back(netEquity);
			times.push_back(time);
		} catch (const std::exception& e) {
			std::cerr << "[Backtest][Error] Candle index " << i << " failed: " << e.what() << std::endl;
		} catch (...) {
			std::cerr << "[Backtest][Error] Candle index " << i << " failed with unknown exception" << std::endl;
		}
	}

	if (position > 0.0 && !allK.empty()) {
		try {
			double lastPrice = std::stod(allK.back()[4].get<std::string>());
			double effectivePrice = lastPrice * (1.0 - slippagePerc);
			double gross = position * effectivePrice;
			double exitFee = gross * feePerc;
			double pnl = (effectivePrice - entryPrice) * position;
			balance += (gross / leverage) + pnl;
			balance -= exitFee;
			if (!res.tradesRec.empty()) {
				auto &tr = res.tradesRec.back();
				tr.exitTime = allK.back()[0].get<long long>(); tr.exitPrice = effectivePrice; tr.exitFee = exitFee; tr.pnl = pnl - tr.entryFee - exitFee;
			}
			std::cout << "[Backtest][ForceClose] lastPrice=" << lastPrice << " effectivePrice=" << effectivePrice << " balance=" << balance << std::endl;
		} catch (const std::exception& e) {
			std::cerr << "[Backtest][Error] finalizing open position failed: " << e.what() << std::endl;
		}
	}

	res.final_balance = balance;
	res.initial_balance = initialBalance;
	res.trades = (int)res.tradesRec.size();
	int wins = 0; for (auto &t : res.tradesRec) if (t.pnl > 0) wins++;
	res.winRate = res.trades > 0 ? (double)wins / res.trades : 0.0;
	res.equityCurve = equity;
	res.equityTime = times;
	double peak = -1e18, mdd = 0;
	for (auto &v : equity) { if (v > peak) peak = v; double dd = (peak - v) / peak; if (dd > mdd) mdd = dd; }
	res.maxDrawdown = mdd;
	for (size_t i = 1; i < equity.size(); i++) returns.push_back((equity[i] - equity[i-1]) / equity[i-1]);
	double mean = 0; for (auto &r : returns) mean += r; if (!returns.empty()) mean /= returns.size();
	double var = 0; for (auto &r : returns) var += (r - mean) * (r - mean); if (!returns.empty()) var /= returns.size();
	double stddev = std::sqrt(var);
	res.sharpe = (stddev > 0) ? (mean / stddev * std::sqrt(24.0 * 365.0)) : 0.0;
	std::cout << "[Backtest][Done] trades=" << res.trades << " winRate=" << res.winRate << " maxDD=" << res.maxDrawdown << " sharpe=" << res.sharpe << " finalBalance=" << res.final_balance << std::endl;
	return res;
}
