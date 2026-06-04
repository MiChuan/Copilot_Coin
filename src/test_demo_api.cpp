#include "binance_http.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
int gFail = 0;

bool check(const char* name, const nlohmann::json& j, bool requireObject = false) {
	if (j.empty() || (j.is_object() && j.contains("code"))) {
		std::cerr << "[FAIL] " << name;
		if (!j.empty()) std::cerr << " code=" << j.value("code", -1) << " msg=" << j.value("msg", "");
		std::cerr << std::endl;
		++gFail;
		return false;
	}
	if (requireObject && !j.is_object()) {
		std::cerr << "[FAIL] " << name << " (expected object)" << std::endl;
		++gFail;
		return false;
	}
	if (j.is_array() && j.empty()) {
		std::cerr << "[FAIL] " << name << " (empty array)" << std::endl;
		++gFail;
		return false;
	}
	std::cout << "[OK  ] " << name << std::endl;
	return true;
}

struct ApiConfig {
	std::string apiKey;
	std::string secret;
	bool useDemo = true;
	std::string demoApiBaseUrl;
	std::string demoApiHost;
	std::string httpProxy;
};

ApiConfig loadConfig() {
	nlohmann::json cfg;
	std::ifstream ifs("config.json");
	if (!ifs) throw std::runtime_error("config.json not found");
	ifs >> cfg;

	ApiConfig c;
	c.apiKey = cfg.value("apiKey", "");
	c.secret = cfg.value("secret", "");
	c.useDemo = cfg.value("useDemo", true);
	if (cfg.contains("demoApiBaseUrl")) c.demoApiBaseUrl = cfg["demoApiBaseUrl"].get<std::string>();
	if (cfg.contains("demoApiHost")) c.demoApiHost = cfg["demoApiHost"].get<std::string>();
	if (cfg.contains("httpProxy")) c.httpProxy = cfg["httpProxy"].get<std::string>();
	if (c.apiKey.empty() || c.secret.empty()) throw std::runtime_error("apiKey/secret missing");
	return c;
}
} // namespace

int main() {
	std::cout << "=== Binance Futures API Endpoint Check ===" << std::endl;

	ApiConfig cfg;
	try {
		cfg = loadConfig();
	} catch (const std::exception& e) {
		std::cerr << "[DemoTest] " << e.what() << std::endl;
		return 1;
	}

	BinanceHttp api(cfg.apiKey, cfg.secret, true, cfg.useDemo);
	if (!cfg.demoApiBaseUrl.empty()) api.setDemoBaseUrl(cfg.demoApiBaseUrl, cfg.demoApiHost);
	if (!cfg.httpProxy.empty()) api.setHttpProxy(cfg.httpProxy);

	const std::string symbol = "BTCUSDT";

	// --- 基础信息 ---
	std::cout << "\n-- 基础信息 --" << std::endl;
	auto time = api.getServerTime();
	if (check("/fapi/v1/time", time, true)) {
		long long ts = time.contains("serverTime") ? time["serverTime"].get<long long>() : 0;
		std::cout << "       serverTime=" << ts << std::endl;
	}

	auto exchangeInfo = api.getExchangeInfo(symbol);
	if (check("/fapi/v1/exchangeInfo", exchangeInfo, true)) {
		if (exchangeInfo.contains("symbols") && exchangeInfo["symbols"].is_array() && !exchangeInfo["symbols"].empty()) {
			auto& s = exchangeInfo["symbols"][0];
			std::cout << "       symbol=" << s.value("symbol", "")
			          << " status=" << s.value("status", "") << std::endl;
		}
	}

	// --- 市场数据 ---
	std::cout << "\n-- 市场数据 --" << std::endl;
	auto price = api.getTickerPrice(symbol);
	if (check("/fapi/v1/ticker/price", price, true)) {
		std::cout << "       price=" << price.value("price", "n/a") << std::endl;
	}

	auto depth = api.getDepth(symbol, 10);
	if (check("/fapi/v1/depth", depth, true)) {
		size_t bids = depth.contains("bids") && depth["bids"].is_array() ? depth["bids"].size() : 0;
		size_t asks = depth.contains("asks") && depth["asks"].is_array() ? depth["asks"].size() : 0;
		std::cout << "       bids=" << bids << " asks=" << asks << std::endl;
	}

	auto klines = api.getKlines(symbol, "1h", 10);
	if (check("/fapi/v1/klines", klines)) {
		std::cout << "       bars=" << klines.size()
		          << " lastClose=" << klines.back()[4] << std::endl;
	}

	// --- 账户 v2（需 API Key，本程序使用）---
	std::cout << "\n-- 账户 v2（签名）--" << std::endl;
	auto acc = api.getFuturesAccount();
	if (check("/fapi/v2/account", acc, true)) {
		std::cout << "       totalWalletBalance=" << acc.value("totalWalletBalance", "n/a")
		          << " availableBalance=" << acc.value("availableBalance", "n/a")
		          << " totalUnrealizedProfit=" << acc.value("totalUnrealizedProfit", "n/a") << std::endl;
	}

	auto balance = api.getAccountBalance();
	if (check("/fapi/v2/balance", balance)) {
		int n = 0;
		if (balance.is_array()) {
			for (auto& a : balance) {
				if (!a.contains("asset")) continue;
				double b = a.contains("balance") ? std::stod(a["balance"].get<std::string>()) : 0;
				if (b <= 0) continue;
				std::cout << "       " << a["asset"].get<std::string>()
				          << " balance=" << a.value("balance", "0")
				          << " available=" << a.value("availableBalance", "0") << std::endl;
				if (++n >= 5) break;
			}
		}
	}

	std::cout << "\n=== Summary: ";
	if (gFail == 0) {
		std::cout << "ALL PASSED ===" << std::endl;
		return 0;
	}
	std::cout << gFail << " FAILED ===" << std::endl;
	return 1;
}
