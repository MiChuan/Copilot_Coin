#include "executor.h"
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

Position Executor::getLocalPosition(const std::string &symbol) {
	std::lock_guard<std::mutex> lg(posMutex_);
	if(positions_.count(symbol)) return positions_[symbol];
	return Position{};
}

void Executor::handleFill(const std::string &symbol, double executedQty, double executedPrice, bool isBuy) {
	std::lock_guard<std::mutex> lg(posMutex_);
	auto &p = positions_[symbol];
	if(isBuy) {
		// update average price
		double totalValue = p.avgPrice * p.qty + executedPrice * executedQty;
		p.qty += executedQty;
		if(p.qty>0) p.avgPrice = totalValue / p.qty;
	} else {
		// sell reduces position
		p.qty -= executedQty;
		if(p.qty <= 0) { p.qty = 0; p.avgPrice = 0; }
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
	// res is array of balances with asset and balance; find USDT
	double available = 0.0;
	if(res.is_array()){
		for(auto &b: res){
			if(b.contains("asset") && b["asset"]=="USDT"){
				if(b.contains("balance")) available = std::stod(b["balance"].get<std::string>());
			}
		}
	}
	return available;
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
	std::ostringstream params;
	params<<"symbol="<<symbol<<"&side=BUY&type=MARKET&quantity="<<qty;
	auto res = api_->postSigned("/fapi/v1/order", params.str());
	// handle partial fill or order response
	if(res.contains("status") && res["status"]=="FILLED") {
		if(res.contains("executedQty") && res.contains("avgPrice")){
			double exQty = std::stod(res["executedQty"].get<std::string>());
			double avgP = std::stod(res["avgPrice"].get<std::string>());
			handleFill(symbol, exQty, avgP, true);
		}
		return res;
	}
	if(res.contains("orderId")){
		long long oid = res["orderId"].get<long long>();
		// poll order
		for(int i=0;i<10;i++){
			auto r2 = api_->getOrder(symbol, oid);
			if(r2.contains("status") && r2["status"]=="FILLED") {
				if(r2.contains("executedQty") && r2.contains("avgPrice")){
					double exQty = std::stod(r2["executedQty"].get<std::string>());
					double avgP = r2.contains("avgPrice") ? std::stod(r2["avgPrice"].get<std::string>()) : std::stod(r2["price"].get<std::string>());
					handleFill(symbol, exQty, avgP, true);
				}
				return r2;
			}
			if(r2.contains("status") && r2["status"]=="PARTIALLY_FILLED") {
				if(r2.contains("executedQty") ){
					double exQty = std::stod(r2["executedQty"].get<std::string>());
					double avgP = r2.contains("avgPrice") ? std::stod(r2["avgPrice"].get<std::string>()) : 0.0;
					// update local position with executedQty
					if(exQty>0) handleFill(symbol, exQty, avgP>0?avgP:std::stod(res.value("price", "0")), true);
				}
				// decide to place another order for remaining qty if needed
				double origQty = std::stod(res["origQty"].get<std::string>());
				double executedQty = r2.contains("executedQty") ? std::stod(r2["executedQty"].get<std::string>()) : 0.0;
				double remain = origQty - executedQty;
				if(remain > 0.0) {
					// try to place a new market order for remaining qty
					std::ostringstream p2; p2<<"symbol="<<symbol<<"&side=BUY&type=MARKET&quantity="<<remain;
					auto r3 = api_->postSigned("/fapi/v1/order", p2.str());
					if(r3.contains("executedQty")){
						double ex2 = std::stod(r3["executedQty"].get<std::string>());
						double avg2 = r3.contains("avgPrice") ? std::stod(r3["avgPrice"].get<std::string>()) : 0.0;
						if(ex2>0) handleFill(symbol, ex2, avg2>0?avg2:std::stod(r3.value("price","0")), true);
					}
				}
				return r2;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
	return res;
}

nlohmann::json Executor::marketSell(const std::string &symbol, double usdtAmount, double leverage) {
	auto k = api_->getKlines(symbol, "1m", 1);
	double price = 0;
	if(k.is_array() && !k.empty()) price = std::stod(k[0][4].get<std::string>());
	if(usdtAmount<=0) {
		double avail = getAvailableUSDT();
		usdtAmount = avail * 0.8; // use 80% of available
	}
	double qty = (usdtAmount * leverage) / price;
	if(!adjustQtyToStepAndMin(symbol, qty)) {
		qty = std::floor(qty*1000.0)/1000.0;
	}
	std::ostringstream params;
	params<<"symbol="<<symbol<<"&side=SELL&type=MARKET&quantity="<<qty;
	auto res = api_->postSigned("/fapi/v1/order", params.str());
	if(res.contains("status") && res["status"]=="FILLED") {
		if(res.contains("executedQty") && res.contains("avgPrice")){
			double exQty = std::stod(res["executedQty"].get<std::string>());
			double avgP = std::stod(res["avgPrice"].get<std::string>());
			handleFill(symbol, exQty, avgP, false);
		}
		return res;
	}
	if(res.contains("orderId")){
		long long oid = res["orderId"].get<long long>();
		// poll order and attempt cancel then fill remaining if necessary
		for(int i=0;i<10;i++){
			auto r2 = api_->getOrder(symbol, oid);
			if(r2.contains("status") && r2["status"]=="FILLED") {
				if(r2.contains("executedQty") && r2.contains("avgPrice")){
					double exQty = std::stod(r2["executedQty"].get<std::string>());
					double avgP = r2.contains("avgPrice") ? std::stod(r2["avgPrice"].get<std::string>()) : std::stod(r2["price"].get<std::string>());
					handleFill(symbol, exQty, avgP, false);
				}
				return r2;
			}
			if(r2.contains("status") && r2["status"]=="PARTIALLY_FILLED") {
				if(r2.contains("executedQty") ){
					double exQty = std::stod(r2["executedQty"].get<std::string>());
					double avgP = r2.contains("avgPrice") ? std::stod(r2["avgPrice"].get<std::string>()) : 0.0;
					if(exQty>0) handleFill(symbol, exQty, avgP>0?avgP:std::stod(res.value("price", "0")), false);
				}
				double origQty = res.contains("origQty") ? std::stod(res["origQty"].get<std::string>()) : 0.0;
				double executedQty = r2.contains("executedQty") ? std::stod(r2["executedQty"].get<std::string>()) : 0.0;
				double remain = origQty - executedQty;
				if(remain > 0.0) {
					// try to cancel open order
					api_->cancelOrder(symbol, oid);
					// place market for remaining
					std::ostringstream p2; p2<<"symbol="<<symbol<<"&side=SELL&type=MARKET&quantity="<<remain;
					auto r3 = api_->postSigned("/fapi/v1/order", p2.str());
					if(r3.contains("executedQty")){
						double ex2 = std::stod(r3["executedQty"].get<std::string>());
						double avg2 = r3.contains("avgPrice") ? std::stod(r3["avgPrice"].get<std::string>()) : 0.0;
						if(ex2>0) handleFill(symbol, ex2, avg2>0?avg2:std::stod(r3.value("price","0")), false);
					}
				}
				return r2;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
	return res;
}

nlohmann::json Executor::marketSellQty(const std::string &symbol, double qty) {
	if(!adjustQtyToStepAndMin(symbol, qty)) {
		qty = std::floor(qty*1000.0)/1000.0;
	}
	std::ostringstream params;
	params<<"symbol="<<symbol<<"&side=SELL&type=MARKET&quantity="<<qty;
	auto res = api_->postSigned("/fapi/v1/order", params.str());
	if(res.contains("status") && res["status"]=="FILLED") {
		if(res.contains("executedQty") && res.contains("avgPrice")){
			double exQty = std::stod(res["executedQty"].get<std::string>());
			double avgP = std::stod(res["avgPrice"].get<std::string>());
			handleFill(symbol, exQty, avgP, false);
		}
		return res;
	}
	if(res.contains("orderId")){
		long long oid = res["orderId"].get<long long>();
		for(int i=0;i<10;i++){
			auto r2 = api_->getOrder(symbol, oid);
			if(r2.contains("status") && r2["status"]=="FILLED") {
				if(r2.contains("executedQty") && r2.contains("avgPrice")){
					double exQty = std::stod(r2["executedQty"].get<std::string>());
					double avgP = r2.contains("avgPrice") ? std::stod(r2["avgPrice"].get<std::string>()) : std::stod(r2["price"].get<std::string>());
					handleFill(symbol, exQty, avgP, false);
				}
				return r2;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
		}
	}
	return res;
}
