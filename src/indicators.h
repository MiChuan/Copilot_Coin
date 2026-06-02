#pragma once

#include <vector>

namespace indicators {
std::vector<double> sma(const std::vector<double>& x, int period);
std::vector<double> ema(const std::vector<double>& x, int period);
std::vector<double> rsi(const std::vector<double>& closes, int period);
void macd(const std::vector<double>& closes, std::vector<double>& macdLine, std::vector<double>& signalLine, std::vector<double>& hist, int fast=12, int slow=26, int signal=9);

struct Bollinger { double upper; double middle; double lower; };
Bollinger bollinger(const std::vector<double>& closes, int period=20, double k=2.0);
}
