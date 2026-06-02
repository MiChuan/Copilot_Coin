#include "backtest.h"
#include "binance_http.h"
#include <iostream>
#include <cmath>

Backtest::Backtest(BinanceHttp *api, Strategy *strategy)
	: api_(api), strategy_(strategy) {}

BacktestResult Backtest::run(int hoursBack, double feePerc, double slippagePerc, double leverage) {
	BacktestResult res;
	// Approximate number of 4h candles
	int candles = hoursBack / 4;
	if(candles < 120) candles = 120;
	int per = 1000;
	std::vector<nlohmann::json> allK;
	int fetched = 0;
	while(fetched < candles) {
		int toFetch = std::min(per, candles - fetched);
		auto k = api_->getKlines("BTCUSDT", "4h", toFetch);
		if(!k.is_array() || k.empty()) break;
		for(auto &it: k) allK.push_back(it);
		fetched += (int)k.size();
		if((int)k.size() < toFetch) break;
	}

	double balance = initialBalance;
	double position = 0.0; // BTC quantity
	double entryPrice = 0.0;
	std::vector<double> equity;
	std::vector<long long> times;
	std::vector<double> returns;

	for(size_t i=120;i<allK.size();i++){
		MarketState ms;
		for(size_t j=0;j<=i;j++) ms.close_4h.push_back(std::stod(allK[j][4].get<std::string>()));
		for(size_t j=0;j<=i;j++) ms.vol_4h.push_back(std::stod(allK[j][5].get<std::string>()));
		for(size_t j=0;j<ms.close_4h.size();j+=6) ms.close_1d.push_back(ms.close_4h[j]);
		auto sig = strategy_->evaluate(ms);
		double price = std::stod(allK[i][4].get<std::string>());
		long long time = allK[i][0].get<long long>();

		// on buy signal
		if(sig.buy && position==0.0) {
			double usdt = balance * 0.8; // use 80% margin
			double effectivePrice = price * (1.0 + slippagePerc);
			double qty = (usdt * leverage) / effectivePrice;
			double entryFee = usdt * feePerc; // charge fee on margin for simplicity
			position = qty;
			entryPrice = effectivePrice;
			balance -= entryFee; // pay fee
			TradeRecord tr; tr.entryTime = time; tr.entryPrice = entryPrice; tr.qty = qty; tr.entryFee = entryFee;
			res.tradesRec.push_back(tr);
		}

		// on sell signal -> close
		if(sig.sell && position>0.0) {
			double effectivePrice = price * (1.0 - slippagePerc);
			double gross = position * effectivePrice;
			double exitFee = gross * feePerc;
			double pnl = (effectivePrice - entryPrice) * position * (1.0); // simplified
			// return margin + pnl after fees
			balance += (gross / leverage) + (pnl);
			balance -= exitFee;
			// finalize trade record
			auto &tr = res.tradesRec.back();
			tr.exitTime = time; tr.exitPrice = effectivePrice; tr.exitFee = exitFee; tr.pnl = pnl - tr.entryFee - exitFee;
			position = 0.0; entryPrice = 0.0;
		}

		double netEquity = balance + (position>0.0 ? (position * price / leverage) : 0.0);
		equity.push_back(netEquity); times.push_back(time);
	}

	// finalize open position at last price
	if(position>0.0) {
		double lastPrice = std::stod(allK.back()[4].get<std::string>());
		double effectivePrice = lastPrice * (1.0 - slippagePerc);
		double gross = position * effectivePrice;
		double exitFee = gross * feePerc;
		double pnl = (effectivePrice - entryPrice) * position;
		balance += (gross / leverage) + pnl;
		balance -= exitFee;
		auto &tr = res.tradesRec.back();
		tr.exitTime = allK.back()[0].get<long long>(); tr.exitPrice = effectivePrice; tr.exitFee = exitFee; tr.pnl = pnl - tr.entryFee - exitFee;
		position = 0.0;
	}

	res.final_balance = balance;
	res.initial_balance = initialBalance;
	res.trades = (int)res.tradesRec.size();
	// compute win rate, equity curve
	int wins=0; for(auto &t: res.tradesRec) if(t.pnl>0) wins++;
	res.winRate = res.trades>0 ? (double)wins/res.trades : 0.0;
	res.equityCurve = equity; res.equityTime = times;
	// max drawdown
	double peak = -1e18, mdd=0;
	for(auto &v: equity){ if(v>peak) peak=v; double dd = (peak - v)/peak; if(dd>mdd) mdd=dd; }
	res.maxDrawdown = mdd;
	// sharpe (simplified) using returns
	for(size_t i=1;i<equity.size();i++) returns.push_back((equity[i]-equity[i-1])/equity[i-1]);
	double mean=0; for(auto &r: returns) mean+=r; if(!returns.empty()) mean/=returns.size();
	double var=0; for(auto &r: returns) var+=(r-mean)*(r-mean); if(!returns.empty()) var/=returns.size();
	double stddev = std::sqrt(var);
	res.sharpe = (stddev>0) ? (mean / stddev * std::sqrt(252.0*6.0)) : 0.0; // scaled
	return res;
}
