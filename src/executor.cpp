#include "executor.h"
#include <iostream>
#include <sstream>
#include <cmath>
#include <thread>
#include <mutex>

Executor::Executor(BinanceHttp *api, double leverage)
	: api_(api), leverage_(leverage) {}

bool Executor::setLeverage(int lev) {
	std::ostringstream params;
	params<<"symbol=BTCUSDT&leverage="<<lev;
	auto res = api_->postSigned("/fapi/v1/leverage", params.str());
	return res.contains("leverage");
}

bool Executor::ensureOneWayMode() {
	auto res = api_->postSigned("/fapi/v1/positionSide/dual", "dualSidePosition=false");
	if (res.contains("code")) {
		const int code = res["code"].get<int>();
		if (code == 200 || code == -4059) {
			std::cout << "[Executor] Position mode: one-way" << std::endl;
			return true;
		}
	}
	std::cerr << "[Executor][Warn] Failed to set one-way mode: " << res.dump() << std::endl;
	return false;
}

double Executor::getExchangePositionQty(const std::string &symbol) {
	auto p = getPosition(symbol);
	if (p.contains("positionAmt")) {
		return std::stod(p["positionAmt"].get<std::string>());
	}
	return 0.0;
}

void Executor::syncLocalPosition(const std::string &symbol) {
	auto p = getPosition(symbol);
	if (!p.contains("positionAmt")) return;
	double qty = std::stod(p["positionAmt"].get<std::string>());
	double avgPrice = 0.0;
	if (p.contains("entryPrice")) {
		avgPrice = std::stod(p["entryPrice"].get<std::string>());
	}
	std::lock_guard<std::mutex> lg(posMutex_);
	auto &local = positions_[symbol];
	local.qty = qty;
	local.avgPrice = qty != 0.0 ? avgPrice : 0.0;
	std::cout << "[Executor] Synced position " << symbol << " qty=" << qty << " avgPrice=" << local.avgPrice << std::endl;
}

Position Executor::getLocalPosition(const std::string &symbol) {
	std::lock_guard<std::mutex> lg(posMutex_);
	if(positions_.count(symbol)) return positions_[symbol];
	return Position{};
}

void Executor::handleFill(const std::string &symbol, double executedQty, double executedPrice, bool isBuy) {
	std::lock_guard<std::mutex> lg(posMutex_);
	auto &p = positions_[symbol];
	if (isBuy) {
		if (p.qty < 0.0) {
			p.qty += executedQty;
			if (p.qty >= 0.0) p.avgPrice = p.qty > 0.0 ? executedPrice : 0.0;
		} else {
			double totalValue = p.avgPrice * p.qty + executedPrice * executedQty;
			p.qty += executedQty;
			if (p.qty > 0.0) p.avgPrice = totalValue / p.qty;
		}
	} else {
		if (p.qty > 0.0) {
			p.qty -= executedQty;
			if (p.qty <= 0.0) {
				p.qty = 0.0;
				p.avgPrice = 0.0;
			}
		} else {
			p.qty -= executedQty;
			if (p.qty == 0.0) p.avgPrice = 0.0;
		}
	}
}

nlohmann::json Executor::getPosition(const std::string &symbol) {
	auto res = api_->getPositionRisk();
	if(res.is_array()){
		for(auto &p: res){ if(p.contains("symbol") && p["symbol"]==symbol) return p; }
	}
	return nlohmann::json::object();
}

bool Executor::adjustQtyToStepAndMin(const std::string &symbol, double &qty) {
	// query exchangeInfo to get lotSize filter
	auto info = api_->getExchangeInfo(symbol);
	if(!info.contains("symbols")) return false;
	for(auto &s: info["symbols"]) {
		if(s.contains("symbol") && s["symbol"]==symbol) {
			if(s.contains("filters")){
				for(auto &f: s["filters"]) {
					if(f.contains("filterType") && f["filterType"]=="LOT_SIZE"){
						std::string stepStr = f["stepSize"].get<std::string>();
						std::string minStr = f["minQty"].get<std::string>();
						// determine decimals for step and min
						auto decimals = [](const std::string &s)->int{
							auto pos = s.find('.');
							if(pos==std::string::npos) return 0;
							return (int)(s.size() - pos - 1);
						};
						int decStep = decimals(stepStr);
						int decMin = decimals(minStr);
						int dec = std::max(decStep, decMin);
						long double factor = std::powl(10.0L, dec);
						// convert strings to integer representations
						long long stepInt = (long long)std::llround(std::stold(stepStr) * factor);
						long long minInt = (long long)std::llround(std::stold(minStr) * factor);
						long long qtyInt = (long long)std::floor(std::stold(std::to_string(qty)) * factor + 1e-9L);
						if(stepInt <= 0) return false;
						long long n = qtyInt / stepInt;
						if(n <= 0) return false;
						long long adjInt = n * stepInt;
						if(adjInt < minInt) return false;
						long double adj = (long double)adjInt / factor;
						qty = (double)adj;
						return true;
					}
				}
			}
		}
	}
	return false;
}

double Executor::getAvailableUSDT() {
	auto res = api_->getAccountBalance();
	double available = 0.0;
	if (res.is_array()) {
		for (auto& b : res) {
			if (!b.contains("asset")) continue;
			const std::string asset = b["asset"].get<std::string>();
			if (asset != "USDT" && asset != "USDC") continue;
			double bal = 0.0;
			if (b.contains("availableBalance")) {
				bal = std::stod(b["availableBalance"].get<std::string>());
			} else if (b.contains("balance")) {
				bal = std::stod(b["balance"].get<std::string>());
			}
			available += bal;
		}
	}
	return available;
}

double Executor::getWalletBalance() {
	auto acc = api_->getFuturesAccount();
	if (acc.contains("totalWalletBalance")) {
		return std::stod(acc["totalWalletBalance"].get<std::string>());
	}
	if (acc.contains("totalMarginBalance")) {
		return std::stod(acc["totalMarginBalance"].get<std::string>());
	}
	return getAvailableUSDT();
}

nlohmann::json Executor::placeMarketOrder(const std::string &symbol, const std::string &side, double qty, bool reduceOnly) {
	if (!adjustQtyToStepAndMin(symbol, qty)) {
		qty = std::floor(qty * 1000.0) / 1000.0;
	}
	if (qty <= 0.0) {
		return nlohmann::json{{"code", -1}, {"msg", "invalid quantity"}};
	}
	std::ostringstream params;
	params << "symbol=" << symbol
		<< "&side=" << side
		<< "&type=MARKET"
		<< "&quantity=" << qty
		<< "&positionSide=BOTH";
	if (reduceOnly) params << "&reduceOnly=true";
	auto res = api_->postSigned("/fapi/v1/order", params.str());
	const bool isBuy = side == "BUY";
	if (res.contains("status") && res["status"] == "FILLED") {
		applyFillFromOrder(symbol, res, isBuy);
		return res;
	}
	if (res.contains("orderId")) {
		long long oid = res["orderId"].get<long long>();
		for (int i = 0; i < 10; i++) {
			auto r2 = api_->getOrder(symbol, oid);
			if (r2.contains("status") && (r2["status"] == "FILLED" || r2["status"] == "PARTIALLY_FILLED")) {
				if (r2.contains("executedQty")) {
					double exQty = std::stod(r2["executedQty"].get<std::string>());
					if (exQty > 0.0) applyFillFromOrder(symbol, r2, isBuy);
				}
				if (r2["status"] == "FILLED") return r2;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
	return res;
}

void Executor::applyFillFromOrder(const std::string &symbol, const nlohmann::json &res, bool isBuy) {
	if (!res.contains("executedQty")) return;
	double exQty = std::stod(res["executedQty"].get<std::string>());
	if (exQty <= 0.0) return;
	double avgP = 0.0;
	if (res.contains("avgPrice")) {
		avgP = std::stod(res["avgPrice"].get<std::string>());
	} else if (res.contains("price")) {
		avgP = std::stod(res["price"].get<std::string>());
	}
	handleFill(symbol, exQty, avgP, isBuy);
	syncLocalPosition(symbol);
}

nlohmann::json Executor::marketCloseLongQty(const std::string &symbol, double qty) {
	return placeMarketOrder(symbol, "SELL", qty, true);
}

nlohmann::json Executor::marketCloseShortQty(const std::string &symbol, double qty) {
	return placeMarketOrder(symbol, "BUY", qty, true);
}

nlohmann::json Executor::marketBuy(const std::string &symbol, double usdtAmount, double leverage) {
	// use market order: calculate quantity in BTC: qty = usdtAmount * leverage / price
	// get price from klines 1 limit
	auto k = api_->getKlines(symbol, "1m", 1);
	double price = 0;
	if(k.is_array() && !k.empty()) price = std::stod(k[0][4].get<std::string>());
	if(usdtAmount<=0) {
		double avail = getAvailableUSDT();
		usdtAmount = avail * 0.8; // use 80% of available
	}
	double qty = (usdtAmount * leverage) / price;
	// adjust qty to step and min
	if(!adjustQtyToStepAndMin(symbol, qty)) {
		// fallback: round down to 3 decimals
		qty = std::floor(qty*1000.0)/1000.0;
	}
	return placeMarketOrder(symbol, "BUY", qty, false);
}
