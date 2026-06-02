#pragma once

#include <string>
#include <nlohmann/json.hpp>

class BinanceHttp {
public:
	BinanceHttp(const std::string &apiKey, const std::string &secret, bool useTestnet=true);
	nlohmann::json getKlines(const std::string &symbol, const std::string &interval, int limit);
	nlohmann::json getAccountBalance();
	nlohmann::json getPositionRisk();
	nlohmann::json getExchangeInfo(const std::string &symbol);
	nlohmann::json getOrder(const std::string &symbol, long long orderId);
	nlohmann::json cancelOrder(const std::string &symbol, long long orderId);
	nlohmann::json postSigned(const std::string &path, const std::string &params);
	nlohmann::json getPublic(const std::string &path, const std::string &params);
	nlohmann::json getSigned(const std::string &path, const std::string &params);
	nlohmann::json signedRequest(const std::string &path, const std::string &params, const std::string &method = "POST");
private:
	std::string apiKey_;
	std::string secret_;
	std::string baseUrl_;
	std::string doRequest(const std::string &url, const std::string &method, const std::string &body, const std::string &headers);
};
