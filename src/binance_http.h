#pragma once

#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <nlohmann/json.hpp>

class BinanceHttp {
public:
	BinanceHttp(const std::string &apiKey, const std::string &secret, bool useTestnet=true, bool useDemo=false);
	~BinanceHttp();
	void setDemoBaseUrl(const std::string& url, const std::string& hostHeader = "");
	void setHttpProxy(const std::string& proxy);
	void setRecvWindow(long long recvWindowMs);
	void startTimeSyncLoop(int intervalSeconds = 300);
	void stopTimeSyncLoop();
	void syncServerTime();

	// Set offline mode
	void setOfflineMode(bool offline);
	bool isOfflineMode() const;

	// Set CSV file path for offline data
	void setCsvDataPath(const std::string& path);
	// Set CSV source interval (e.g. "1m"), used to aggregate to target interval
	void setCsvSourceInterval(const std::string& interval);

	nlohmann::json getServerTime();
	nlohmann::json getTickerPrice(const std::string &symbol);
	nlohmann::json getDepth(const std::string &symbol, int limit = 10);
	nlohmann::json getKlines(const std::string &symbol, const std::string &interval, int limit);
	nlohmann::json getAccountBalance();
	nlohmann::json getFuturesAccount();
	nlohmann::json getPositionRisk();
	nlohmann::json getExchangeInfo(const std::string &symbol);
	nlohmann::json getOrder(const std::string &symbol, long long orderId);
	nlohmann::json cancelOrder(const std::string &symbol, long long orderId);
	nlohmann::json postSigned(const std::string &path, const std::string &params);
	nlohmann::json getPublic(const std::string &path, const std::string &params);
	nlohmann::json getSigned(const std::string &path, const std::string &params);
	nlohmann::json signedRequest(const std::string &path, const std::string &params, const std::string &method = "POST");
	std::string directRequest(const std::string &url);
	static std::string testDirectRequest(const std::string &url, const std::string &hostHeader);
private:
	std::string apiKey_;
	std::string secret_;
	std::string baseUrl_;
	std::string hostHeader_;
	bool offlineMode_;
	std::string csvDataPath_;
	std::string csvSourceInterval_;
	std::string httpProxy_;
	long long timeOffsetMs_ = 0;
	bool timeSynced_ = false;
	long long recvWindowMs_ = 60000;
	std::atomic<bool> stopTimeSync_{false};
	std::thread timeSyncThread_;
	std::mutex timeMutex_;
	std::string buildSignedQuery(const std::string& params);
	std::string doRequest(const std::string &url, const std::string &method, const std::string &body, const std::string &headers);
	bool isTimestampError(const std::string& response) const;
	void resyncTimeIfNeeded();
};
