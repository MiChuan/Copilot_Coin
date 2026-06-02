#include "indicators.h"
#include <numeric>
#include <cmath>

namespace indicators {

std::vector<double> sma(const std::vector<double>& x, int period) {
	std::vector<double> out;
	if((int)x.size() < period) return out;
	double sum = 0.0;
	for(int i=0;i<period;i++) sum += x[i];
	out.push_back(sum/period);
	for(size_t i=period;i<x.size();i++){
		sum += x[i];
		sum -= x[i-period];
		out.push_back(sum/period);
	}
	return out;
}

std::vector<double> ema(const std::vector<double>& x, int period) {
	std::vector<double> out;
	if(x.empty()) return out;
	double k = 2.0/(period+1);
	double e = x[0];
	out.push_back(e);
	for(size_t i=1;i<x.size();i++){
		e = x[i]*k + e*(1-k);
		out.push_back(e);
	}
	return out;
}

std::vector<double> rsi(const std::vector<double>& closes, int period) {
	std::vector<double> out;
	if((int)closes.size()<=period) return out;
	std::vector<double> gains, losses;
	for(size_t i=1;i<closes.size();i++){
		double diff = closes[i]-closes[i-1];
		gains.push_back(std::max(0.0, diff));
		losses.push_back(std::max(0.0, -diff));
	}
	double avgGain=0, avgLoss=0;
	for(int i=0;i<period;i++){ avgGain += gains[i]; avgLoss += losses[i]; }
	avgGain /= period; avgLoss /= period;
	out.push_back(100 - (100/(1 + (avgGain/avgLoss))));
	for(size_t i=period;i<gains.size();i++){
		avgGain = (avgGain*(period-1) + gains[i]) / period;
		avgLoss = (avgLoss*(period-1) + losses[i]) / period;
		double rs = avgGain / (avgLoss==0?1e-9:avgLoss);
		out.push_back(100 - (100/(1+rs)));
	}
	return out;
}

void macd(const std::vector<double>& closes, std::vector<double>& macdLine, std::vector<double>& signalLine, std::vector<double>& hist, int fast, int slow, int signal) {
	auto emaFast = ema(closes, fast);
	auto emaSlow = ema(closes, slow);
	if(emaFast.size() < emaSlow.size()) return;
	size_t offset = emaFast.size() - emaSlow.size();
	macdLine.clear();
	for(size_t i=0;i<emaSlow.size();i++) macdLine.push_back(emaFast[i+offset] - emaSlow[i]);
	signalLine = ema(macdLine, signal);
	hist.clear();
	size_t sigOffset = macdLine.size() - signalLine.size();
	for(size_t i=0;i<signalLine.size();i++) hist.push_back(macdLine[i+sigOffset] - signalLine[i]);
}

Bollinger bollinger(const std::vector<double>& closes, int period, double k) {
	Bollinger b{0,0,0};
	if((int)closes.size() < period) return b;
	double sum=0;
	for(int i=closes.size()-period;i< (int)closes.size(); i++) sum += closes[i];
	double ma = sum/period;
	double sd=0;
	for(int i=closes.size()-period;i< (int)closes.size(); i++) sd += (closes[i]-ma)*(closes[i]-ma);
	sd = std::sqrt(sd/period);
	b.middle = ma; b.upper = ma + k*sd; b.lower = ma - k*sd;
	return b;
}

}
