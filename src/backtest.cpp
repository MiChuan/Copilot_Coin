#include "backtest.h"
#include "binance_http.h"
#include <iostream>
#include <cmath>

Backtest::Backtest(BinanceHttp *api, Strategy *strategy)
	: api_(api), strategy_(strategy) {}

BacktestResult Backtest::run(int hoursBack, double feePerc, double slippagePerc, double leverage, double initialBalance) {
	BacktestResult res;
	// Approximate number of 4h candles (or just get what we can)
	int candles = hoursBack / 4;
	if(candles < 120) candles = 120;
	int per = 1000;
	std::vector<nlohmann::json> allK;
	int fetched = 0;
	std::cout<<"Fetching "<<candles<<" candles..."<<std::endl;
	while(fetched < candles) {
		int toFetch = std::min(per, candles - fetched);
		std::cout<<"Fetching "<<toFetch<<" candles (total fetched: "<<fetched<<")"<<std::endl;
		auto k = api_->getKlines("BTCUSDT", "4h", toFetch);
		std::cout<<"Got "<<k.size()<<" candles"<<std::endl;
		if(!k.is_array() || k.empty()) {
			std::cout<<"No data received, breaking"<<std::endl;
			break;
		}
		for(auto &it : k) allK.push_back(it);
		fetched += (int)k.size();
		if((int)k.size() < toFetch) break;
	}

	std::cout<<"Total candles loaded: "<<allK.size()<<std::endl;
	if (allK.size() < 120) {
		std::cerr<<"Error: Not enough data loaded (got "<<allK.size()<<", need at least 120)"<<std::endl;
		return res;
	}

	double balance = initialBalance;
	double position = 0.0; // BTC quantity
	double entryPrice = 0.0;
	std::vector<double> equity;
	std::vector<long long> times;
	std::vector<double> returns;

	for(size_t i=120; i<allK.size(); i++){
		try {
			MarketState ms;
			for(size_t j=0; j<=i; j++) {
				try {
					ms.close_4h.push_back(std::stod(allK[j][4].get<std::string>()));
					ms.vol_4h.push_back(std::stod(allK[j][5].get<std::string>()));
				} catch(...) {
					std::cerr<<"Error parsing price/volume at index "<<j<<std::endl;
					continue;
				}
			}

			for(size_t j=0; j<ms.close_4h.size(); j+=6) {
				ms.close_1d.push_back(ms.close_4h[j]);
			}

			auto sig = strategy_->evaluate(ms);
			double price = std::stod(allK[i][4].get<std::string>());
			long long time = allK[i][0].get<long long>();

			// on buy signal
			if(sig.buy && position==0.0) {
				double usdt = balance * 0.8; // use 80% margin
				double effectivePrice = price * (1.0 + slippagePerc);
				double qty = (usdt * leverage) / effectivePrice;
				double entryFee = usdt * feePerc;
				position = qty;
				entryPrice = effectivePrice;
				balance -= entryFee;
				TradeRecord tr; tr.entryTime = time; tr.entryPrice = entryPrice; tr.qty = qty; tr.entryFee = entryFee;
				res.tradesRec.push_back(tr);
			}

			// on sell signal
			if(sig.sell && position>0.0) {
				double effectivePrice = price * (1.0 - slippagePerc);
				double gross = position * effectivePrice;
				double exitFee = gross * feePerc;
				double pnl = (effectivePrice - entryPrice) * position;
				balance += (gross / leverage) + (pnl);
				balance -= exitFee;
				// finalize trade record
				if (!res.tradesRec.empty()) {
					auto &tr = res.tradesRec.back();
					tr.exitTime = time; tr.exitPrice = effectivePrice; tr.exitFee = exitFee; tr.pnl = pnl - tr.entryFee - exitFee;
				}
				position = 0.0; entryPrice = 0.0;
			}

			double netEquity = balance + (position>0.0 ? (position * price / leverage) : 0.0);
			equity.push_back(netEquity); times.push_back(time);
		} catch(const std::exception& e) {
			std::cerr<<"Error processing candle at index "<<i<<": "<<e.what()<<std::endl;
			continue;
		}
	}

	// finalize open position at last price
	if(position>0.0 && !allK.empty()) {
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
			position = 0.0;
		} catch(...) {
			std::cerr<<"Error finalizing open position"<<std::endl;
		}
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
