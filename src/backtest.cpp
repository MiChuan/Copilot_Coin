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

	for (size_t i = 0; i < allK.size(); i++) {
		try {
			double price = std::stod(allK[i][4].get<std::string>());
			double volume = std::stod(allK[i][5].get<std::string>());
			double high = std::stod(allK[i][2].get<std::string>());
			double low = std::stod(allK[i][3].get<std::string>());
			long long time = allK[i][0].get<long long>();

			closes.push_back(price);
			vols.push_back(volume);
			highs.push_back(high);
			lows.push_back(low);
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
				entryTime = time;
				totalEntryFee = entryFee;
				tp1_triggered = false;
				tp2_triggered = false;
				sl_triggered = false;
				balance -= entryFee;
				std::cout << "[Backtest][TradeOpen] time=" << time << " price=" << effectivePrice << " qty=" << qty << " entryFee=" << entryFee << " balance=" << balance << std::endl;
			}

			// ===== 分批止盈检查 =====
			if (position > 0.0) {
				double profitPct = (price - entryPrice) / entryPrice;

				// TP1: 盈利 ≥ 8%，平仓原始仓位的 50%（不超过当前仓位）
				if (!tp1_triggered && profitPct >= 0.08) {
					double closeQty = std::min(originalQty * 0.5, position);
					if (closeQty > 0.0) {
						double exitEffPrice = price * (1.0 - slippagePerc);
						double gross = closeQty * exitEffPrice;
						double exitFee = gross * feePerc;
						double pnl = (exitEffPrice - entryPrice) * closeQty;
						balance += (gross / leverage) + pnl;
						balance -= exitFee;
						position -= closeQty;
						tp1_triggered = true;

						TradeRecord tr;
						tr.entryTime = entryTime;
						tr.exitTime = time;
						tr.entryPrice = entryPrice;
						tr.exitPrice = exitEffPrice;
						tr.qty = closeQty;
						double propEntryFee = (originalQty > 0) ? totalEntryFee * (closeQty / originalQty) : 0.0;
						tr.entryFee = propEntryFee;
						tr.exitFee = exitFee;
						tr.pnl = pnl - propEntryFee - exitFee;
						res.tradesRec.push_back(tr);

						std::cout << "[Backtest][TP1] time=" << time << " profitPct=" << profitPct * 100 << "% closeQty=" << closeQty << " remainQty=" << position << " pnl=" << tr.pnl << std::endl;
					}
				}

				// TP2: 盈利 ≥ 15%，平仓剩余仓位的 50%
				if (!tp2_triggered && position > 0.0 && profitPct >= 0.15) {
					double closeQty = position * 0.5;
					double exitEffPrice = price * (1.0 - slippagePerc);
					double gross = closeQty * exitEffPrice;
					double exitFee = gross * feePerc;
					double pnl = (exitEffPrice - entryPrice) * closeQty;
					balance += (gross / leverage) + pnl;
					balance -= exitFee;
					position -= closeQty;
					tp2_triggered = true;

					TradeRecord tr;
					tr.entryTime = entryTime;
					tr.exitTime = time;
					tr.entryPrice = entryPrice;
					tr.exitPrice = exitEffPrice;
					tr.qty = closeQty;
					double propEntryFee = (originalQty > 0) ? totalEntryFee * (closeQty / originalQty) : 0.0;
					tr.entryFee = propEntryFee;
					tr.exitFee = exitFee;
					tr.pnl = pnl - propEntryFee - exitFee;
					res.tradesRec.push_back(tr);

					std::cout << "[Backtest][TP2] time=" << time << " profitPct=" << profitPct * 100 << "% closeQty=" << closeQty << " remainQty=" << position << " pnl=" << tr.pnl << std::endl;
				}

				// SL: 浮亏 >= 2.5%，平仓当前仓位的 80%
				if (!sl_triggered && profitPct <= -0.025) {
					double closeQty = position * 0.8;
					double exitEffPrice = price * (1.0 - slippagePerc);
					double gross = closeQty * exitEffPrice;
					double exitFee = gross * feePerc;
					double pnl = (exitEffPrice - entryPrice) * closeQty;
					balance += (gross / leverage) + pnl;
					balance -= exitFee;
					position -= closeQty;
					sl_triggered = true;

					TradeRecord tr;
					tr.entryTime = entryTime;
					tr.exitTime = time;
					tr.entryPrice = entryPrice;
					tr.exitPrice = exitEffPrice;
					tr.qty = closeQty;
					double propEntryFee = (originalQty > 0) ? totalEntryFee * (closeQty / originalQty) : 0.0;
					tr.entryFee = propEntryFee;
					tr.exitFee = exitFee;
					tr.pnl = pnl - propEntryFee - exitFee;
					res.tradesRec.push_back(tr);

					std::cout << "[Backtest][SL] time=" << time << " lossPct=" << profitPct * 100 << "% closeQty=" << closeQty << " remainQty=" << position << " pnl=" << tr.pnl << std::endl;
				}
			}

			if (sig.sell && position > 0.0) {
				double effectivePrice = price * (1.0 - slippagePerc);
				double gross = position * effectivePrice;
				double exitFee = gross * feePerc;
				double pnl = (effectivePrice - entryPrice) * position;
				balance += (gross / leverage) + pnl;
				balance -= exitFee;

				TradeRecord tr;
				tr.entryTime = entryTime;
				tr.exitTime = time;
				tr.entryPrice = entryPrice;
				tr.exitPrice = effectivePrice;
				tr.qty = position;
				double propEntryFee = (originalQty > 0) ? totalEntryFee * (position / originalQty) : 0.0;
				tr.entryFee = propEntryFee;
				tr.exitFee = exitFee;
				tr.pnl = pnl - propEntryFee - exitFee;
				res.tradesRec.push_back(tr);

				std::cout << "[Backtest][TradeClose] time=" << time << " price=" << effectivePrice << " gross=" << gross << " pnl=" << pnl << " exitFee=" << exitFee << " balance=" << balance << std::endl;
				position = 0.0;
				originalQty = 0.0;
				entryPrice = 0.0;
				entryTime = 0;
				totalEntryFee = 0.0;
				tp1_triggered = false;
				tp2_triggered = false;
				sl_triggered = false;
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

			TradeRecord tr;
			tr.entryTime = entryTime;
			tr.exitTime = allK.back()[0].get<long long>();
			tr.entryPrice = entryPrice;
			tr.exitPrice = effectivePrice;
			tr.qty = position;
			double propEntryFee = (originalQty > 0) ? totalEntryFee * (position / originalQty) : 0.0;
			tr.entryFee = propEntryFee;
			tr.exitFee = exitFee;
			tr.pnl = pnl - propEntryFee - exitFee;
			res.tradesRec.push_back(tr);

			std::cout << "[Backtest][ForceClose] lastPrice=" << lastPrice << " effectivePrice=" << effectivePrice << " remainQty=" << position << " balance=" << balance << std::endl;
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
