#pragma once

#include <vector>
#include <nlohmann/json.hpp>

struct MarketState {
	std::vector<double> close_1m;
	std::vector<double> close_4h;
	std::vector<double> close_1d;
	std::vector<double> vol_4h;
};

struct Signal {
	bool buy = false;
	bool sell = false;
	double price = 0.0;
	std::string reason;
};

class Strategy {
public:
	Strategy();
	Signal evaluate(const MarketState &s);
};
