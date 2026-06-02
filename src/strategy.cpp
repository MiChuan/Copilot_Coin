#include "strategy.h"
#include "indicators.h"
#include <algorithm>

Strategy::Strategy() {}

Signal Strategy::evaluate(const MarketState &s) {
	Signal sig;
	// need enough data
	if(s.close_1d.size()<21 || s.close_4h.size()<50) return sig;
	auto boll = indicators::bollinger(s.close_1d, 20, 2.0);
	double last4 = s.close_4h.back();
	// 判断接近下轨或上轨（用日线）
	bool nearLower = (std::abs(last4 - boll.lower) / boll.lower) < 0.02; // 2%
	bool nearUpper = (std::abs(last4 - boll.upper) / boll.upper) < 0.02;
	// RSI on 4h
	auto rsi4 = indicators::rsi(s.close_4h, 6);
	if(rsi4.empty()) return sig;
	double lastRsi = rsi4.back();
	// MACD on 4h
	std::vector<double> macdLine, signalLine, hist;
	indicators::macd(s.close_4h, macdLine, signalLine, hist);
	bool macdBull = false, macdBear = false;
	if(!hist.empty()) {
		double h = hist.back();
		macdBull = h>0;
		macdBear = h<0;
	}
	// VOL放量: compare last vol to mean of previous 10
	bool volSpike=false;
	if(s.vol_4h.size()>=11) {
		double sum=0; for(size_t i=s.vol_4h.size()-11;i<s.vol_4h.size()-1;i++) sum+=s.vol_4h[i];
		double avg = sum/10.0;
		volSpike = s.vol_4h.back() > avg*1.5;
	}

	if(nearLower && lastRsi<=26 && volSpike && macdBull) {
		sig.buy = true; sig.price = last4; sig.reason = "nearLower & RSI<=26 & vol spike & MACD bull";
	}
	if(nearUpper && lastRsi>=82 && volSpike && macdBear) {
		sig.sell = true; sig.price = last4; sig.reason = "nearUpper & RSI>=82 & vol spike & MACD bear";
	}
	return sig;
}
