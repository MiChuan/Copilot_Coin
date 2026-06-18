#include "backtest.h"
#include "binance_http.h"
#include <iostream>
#include <cmath>

namespace {

struct DailyBar {
	long long closeTime = 0;
	double close = 0.0;
};

std::vector<DailyBar> parseDailyBars(const nlohmann::json& klines1d) {
	std::vector<DailyBar> bars;
	if (!klines1d.is_array()) return bars;
	bars.reserve(klines1d.size());
	for (const auto& k : klines1d) {
		DailyBar d;
		d.closeTime = k[6].get<long long>();
		d.close = std::stod(k[4].get<std::string>());
		bars.push_back(d);
	}
	return bars;
}

double netEquity(double balance, double position, double price, double entryPrice) {
	if (position <= 0.0) return balance;
	return balance + position * (price - entryPrice);
}

void closePartial(
	double& balance,
	double& position,
	double entryPrice,
	double originalQty,
	double totalEntryFee,
	double closeQty,
	double exitEffPrice,
	double feePerc,
	long long entryTime,
	long long exitTime,
	const std::string& tag,
	BacktestResult& res
) {
	double gross = closeQty * exitEffPrice;
	double exitFee = gross * feePerc;
	double pnl = (exitEffPrice - entryPrice) * closeQty;
	balance += pnl - exitFee;
	position -= closeQty;

	TradeRecord tr;
	tr.entryTime = entryTime;
	tr.exitTime = exitTime;
	tr.entryPrice = entryPrice;
	tr.exitPrice = exitEffPrice;
	tr.qty = closeQty;
	double propEntryFee = (originalQty > 0) ? totalEntryFee * (closeQty / originalQty) : 0.0;
	tr.entryFee = propEntryFee;
	tr.exitFee = exitFee;
	tr.pnl = pnl - propEntryFee - exitFee;
	res.tradesRec.push_back(tr);

	std::cout << "[Backtest][" << tag << "] time=" << exitTime
		<< " closeQty=" << closeQty << " remainQty=" << position
		<< " pnl=" << tr.pnl << " balance=" << balance << std::endl;
}

} // namespace

Backtest::Backtest(BinanceHttp *api, Strategy *strategy)
	: api_(api), strategy_(strategy) {}

BacktestResult Backtest::run(int hoursBack, double feePerc, double slippagePerc, double leverage, double initialBalance) {
	int candles = hoursBack;
	if (candles < 120) candles = 120;
	std::cout << "[Backtest] Fetching " << candles << " 1h candles" << std::endl;
	auto allK = api_->getKlines("BTCUSDT", "1h", candles);
	auto k1d = api_->getKlines("BTCUSDT", "1d", candles / 24 + 60);
	std::cout << "[Backtest] Total candles loaded: 1h=" << allK.size() << " 1d=" << k1d.size() << std::endl;
	return runOnKlines(allK, feePerc, slippagePerc, leverage, initialBalance, k1d);
}

BacktestResult Backtest::runFrom1hKlines(
	const nlohmann::json& klines,
	double feePerc,
	double slippagePerc,
	double leverage,
	double initialBalance,
	const nlohmann::json& klines1d
) {
	std::cout << "[Backtest] Running from in-memory 1h klines: " << klines.size();
	if (klines1d.is_array() && !klines1d.empty()) {
		std::cout << " with 1d klines: " << klines1d.size();
	}
	std::cout << std::endl;
	return runOnKlines(klines, feePerc, slippagePerc, leverage, initialBalance, klines1d);
}

BacktestResult Backtest::runOnKlines(
	const nlohmann::json& allK,
	double feePerc,
	double slippagePerc,
	double leverage,
	double initialBalance,
	const nlohmann::json& klines1d
) {
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

	const auto dailyBars = parseDailyBars(klines1d);
	const bool useRealDaily = !dailyBars.empty();
	if (useRealDaily) {
		std::cout << "[Backtest] Using real 1d klines: " << dailyBars.size() << " bars" << std::endl;
	} else {
		std::cout << "[Backtest][Warn] No 1d klines provided, falling back to synthetic daily (every 24x1h)" << std::endl;
	}

	double balance = initialBalance;
	double position = 0.0;
	double entryPrice = 0.0;
	double originalQty = 0.0;
	double totalEntryFee = 0.0;
	long long entryTime = 0;
	bool tp1_triggered = false;
	bool tp2_triggered = false;
	bool sl_triggered = false;
	std::vector<double> equity;
	std::vector<long long> times;
	std::vector<double> returns;
	std::vector<double> closes;
	std::vector<double> vols;
	std::vector<double> highs;
	std::vector<double> lows;
	std::vector<double> dailyCloses;
	size_t dailyIdx = 0;

	for (size_t i = 0; i < allK.size(); i++) {
		try {
			double price = std::stod(allK[i][4].get<std::string>());
			double volume = std::stod(allK[i][5].get<std::string>());
			double high = std::stod(allK[i][2].get<std::string>());
			double low = std::stod(allK[i][3].get<std::string>());
			long long closeTime = allK[i][6].get<long long>();

			closes.push_back(price);
			vols.push_back(volume);
			highs.push_back(high);
			lows.push_back(low);

			if (useRealDaily) {
				while (dailyIdx < dailyBars.size() && dailyBars[dailyIdx].closeTime <= closeTime) {
					dailyCloses.push_back(dailyBars[dailyIdx].close);
					dailyIdx++;
				}
			} else if ((i + 1) % 24 == 0) {
				dailyCloses.push_back(price);
			}

			if (i < 3 || i + 1 == allK.size() || (i + 1) % 100 == 0) {
				std::cout << "[Backtest][Candle] i=" << i
					<< " time=" << closeTime
					<< " price=" << price
					<< " closeCount=" << closes.size()
					<< " dayCount=" << dailyCloses.size()
					<< " volCount=" << vols.size()
					<< " pos=" << position
					<< " bal=" << balance << std::endl;
			}

			if (closes.size() < 120 || dailyCloses.size() < 21 || vols.size() < 50) {
				equity.push_back(netEquity(balance, position, price, entryPrice));
				times.push_back(closeTime);
				continue;
			}

			MarketState ms;
			ms.close_4h = closes;
			ms.vol_4h = vols;
			ms.high_4h = highs;
			ms.low_4h = lows;
			ms.close_1d = dailyCloses;

			auto sig = strategy_->evaluate(ms);
			if (!sig.reason.empty()) {
				std::cout << "[Backtest][Signal] i=" << i << " buy=" << sig.buy << " sell=" << sig.sell << " reason=" << sig.reason << std::endl;
			}

			if (sig.buy && position == 0.0) {
				double usdt = balance * 0.3;
				double effectivePrice = price * (1.0 + slippagePerc);
				double qty = (usdt * leverage) / effectivePrice;
				double entryFee = usdt * feePerc;
				position = qty;
				originalQty = qty;
				entryPrice = effectivePrice;
				entryTime = closeTime;
				totalEntryFee = entryFee;
				tp1_triggered = false;
				tp2_triggered = false;
				sl_triggered = false;
				balance -= entryFee;
				std::cout << "[Backtest][TradeOpen] time=" << closeTime << " price=" << effectivePrice << " qty=" << qty << " entryFee=" << entryFee << " balance=" << balance << std::endl;
			}

			if (position > 0.0) {
				double profitPct = (price - entryPrice) / entryPrice;

				if (!tp1_triggered && profitPct >= 0.08) {
					double closeQty = std::min(originalQty * 0.5, position);
					if (closeQty > 0.0) {
						double exitEffPrice = price * (1.0 - slippagePerc);
						closePartial(balance, position, entryPrice, originalQty, totalEntryFee, closeQty, exitEffPrice, feePerc, entryTime, closeTime, "TP1", res);
						tp1_triggered = true;
					}
				}

				if (!tp2_triggered && position > 0.0 && profitPct >= 0.15) {
					double closeQty = position * 0.5;
					double exitEffPrice = price * (1.0 - slippagePerc);
					closePartial(balance, position, entryPrice, originalQty, totalEntryFee, closeQty, exitEffPrice, feePerc, entryTime, closeTime, "TP2", res);
					tp2_triggered = true;
				}

				if (!sl_triggered && profitPct <= -0.025) {
					double closeQty = position * 0.8;
					double exitEffPrice = price * (1.0 - slippagePerc);
					closePartial(balance, position, entryPrice, originalQty, totalEntryFee, closeQty, exitEffPrice, feePerc, entryTime, closeTime, "SL", res);
					sl_triggered = true;
				}
			}

			if (sig.sell && position > 0.0) {
				double effectivePrice = price * (1.0 - slippagePerc);
				closePartial(balance, position, entryPrice, originalQty, totalEntryFee, position, effectivePrice, feePerc, entryTime, closeTime, "TradeClose", res);
				position = 0.0;
				originalQty = 0.0;
				entryPrice = 0.0;
				entryTime = 0;
				totalEntryFee = 0.0;
				tp1_triggered = false;
				tp2_triggered = false;
				sl_triggered = false;
			}

			equity.push_back(netEquity(balance, position, price, entryPrice));
			times.push_back(closeTime);
		} catch (const std::exception& e) {
			std::cerr << "[Backtest][Error] Candle index " << i << " failed: " << e.what() << std::endl;
		} catch (...) {
			std::cerr << "[Backtest][Error] Candle index " << i << " failed with unknown exception" << std::endl;
		}
	}

	if (position > 0.0 && !allK.empty()) {
		try {
			double lastPrice = std::stod(allK.back()[4].get<std::string>());
			long long lastCloseTime = allK.back()[6].get<long long>();
			double effectivePrice = lastPrice * (1.0 - slippagePerc);
			closePartial(balance, position, entryPrice, originalQty, totalEntryFee, position, effectivePrice, feePerc, entryTime, lastCloseTime, "ForceClose", res);
		} catch (const std::exception& e) {
			std::cerr << "[Backtest][Error] finalizing open position failed: " << e.what() << std::endl;
		}
	}

	res.final_balance = balance;
	res.initial_balance = initialBalance;
	res.trades = (int)res.tradesRec.size();
	int wins = 0;
	for (auto &t : res.tradesRec) if (t.pnl > 0) wins++;
	res.winRate = res.trades > 0 ? (double)wins / res.trades : 0.0;
	res.equityCurve = equity;
	res.equityTime = times;
	double peak = -1e18, mdd = 0;
	for (auto &v : equity) { if (v > peak) peak = v; double dd = (peak - v) / peak; if (dd > mdd) mdd = dd; }
	res.maxDrawdown = mdd;
	for (size_t i = 1; i < equity.size(); i++) returns.push_back((equity[i] - equity[i-1]) / equity[i-1]);
	double mean = 0;
	for (auto &r : returns) mean += r;
	if (!returns.empty()) mean /= returns.size();
	double var = 0;
	for (auto &r : returns) var += (r - mean) * (r - mean);
	if (!returns.empty()) var /= returns.size();
	double stddev = std::sqrt(var);
	res.sharpe = (stddev > 0) ? (mean / stddev * std::sqrt(24.0 * 365.0)) : 0.0;
	std::cout << "[Backtest][Done] trades=" << res.trades << " winRate=" << res.winRate << " maxDD=" << res.maxDrawdown << " sharpe=" << res.sharpe << " finalBalance=" << res.final_balance << std::endl;
	return res;
}
