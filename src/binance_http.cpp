#include "binance_http.h"
#include "util.h"
#include "mock_data_generator.h"
#include "csv_kline_loader.h"
#include <curl/curl.h>
#include <sstream>
#include <iostream>

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
	((std::string*)userp)->append(static_cast<char*>(contents), size * nmemb);
	return size * nmemb;
}

nlohmann::json BinanceHttp::getSigned(const std::string &path, const std::string &params) {
	long long ts = util::current_timestamp_ms();
	std::ostringstream qs;
	if(!params.empty()) qs<<params<<"&";
	qs<<"timestamp="<<ts;
	std::string signature = util::hmac_sha256_hex(secret_, qs.str());
	qs<<"&signature="<<signature;
	std::ostringstream url;
	url<<baseUrl_<<path<<"?"<<qs.str();
	std::string headers = "X-MBX-APIKEY: ";
	headers += apiKey_;
	std::string res = doRequest(url.str(), "GET", "", headers);
	try { return nlohmann::json::parse(res); } catch(...) { return nlohmann::json::object(); }
}

nlohmann::json BinanceHttp::getAccountBalance() {
	// Futures balance endpoint
	return getSigned("/fapi/v2/balance", "");
}

nlohmann::json BinanceHttp::signedRequest(const std::string &path, const std::string &params, const std::string &method) {
	long long ts = util::current_timestamp_ms();
	std::ostringstream qs;
	if(!params.empty()) qs<<params<<"&";
	qs<<"timestamp="<<ts;
	std::string signature = util::hmac_sha256_hex(secret_, qs.str());
	qs<<"&signature="<<signature;
	std::ostringstream url;
	url<<baseUrl_<<path;
	std::string headers = "X-MBX-APIKEY: "; headers += apiKey_;
	std::string qstring = qs.str();
	std::string res;
	if(method=="GET" || method=="DELETE") {
		res = doRequest(url.str()+"?"+qstring, method, "", headers);
	} else {
		res = doRequest(url.str(), method, qstring, headers);
	}
	try { return nlohmann::json::parse(res); } catch(...) { return nlohmann::json::object(); }
}

nlohmann::json BinanceHttp::getPositionRisk() {
	return getSigned("/fapi/v2/positionRisk", "");
}

nlohmann::json BinanceHttp::getExchangeInfo(const std::string &symbol) {
	std::ostringstream oss;
	oss<<"/fapi/v1/exchangeInfo";
	if(!symbol.empty()) oss<<"?symbol="<<symbol;
	return getPublic(oss.str(), "");
}

nlohmann::json BinanceHttp::getOrder(const std::string &symbol, long long orderId) {
	std::ostringstream params; params<<"symbol="<<symbol<<"&orderId="<<orderId;
	return signedRequest("/fapi/v1/order", params.str(), "GET");
}

nlohmann::json BinanceHttp::cancelOrder(const std::string &symbol, long long orderId) {
	std::ostringstream params; params<<"symbol="<<symbol<<"&orderId="<<orderId;
	return signedRequest("/fapi/v1/order", params.str(), "DELETE");
}

BinanceHttp::BinanceHttp(const std::string &apiKey, const std::string &secret, bool useTestnet)
	: apiKey_(apiKey), secret_(secret), offlineMode_(false), csvDataPath_(""), csvSourceInterval_("") {
#ifdef SIMULATION
	baseUrl_ = "https://testnet.binancefuture.com";
#else
	baseUrl_ = useTestnet ? "https://testnet.binancefuture.com" : "https://fapi.binance.com";
#endif
}

void BinanceHttp::setOfflineMode(bool offline) {
	offlineMode_ = offline;
	if (offline) {
		std::cout << "[BinanceHttp] Offline mode enabled" << std::endl;
	} else {
		std::cout << "[BinanceHttp] Offline mode disabled" << std::endl;
	}
}

bool BinanceHttp::isOfflineMode() const {
	return offlineMode_;
}

void BinanceHttp::setCsvDataPath(const std::string& path) {
	csvDataPath_ = path;
	std::cout << "[BinanceHttp] CSV data path set to: " << path << std::endl;
}

void BinanceHttp::setCsvSourceInterval(const std::string& interval) {
	csvSourceInterval_ = interval;
	std::cout << "[BinanceHttp] CSV source interval set to: " << interval << std::endl;
}

std::string BinanceHttp::doRequest(const std::string &url, const std::string &method, const std::string &body, const std::string &headers) {
	CURL *curl = curl_easy_init();
	std::string readBuffer;
	if(curl) {
		struct curl_slist *chunk = NULL;
		if(!headers.empty()) {
			chunk = curl_slist_append(chunk, headers.c_str());
		}
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
		// 禁用 SSL 证书验证（仅用于调试）
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
		// 设置超时（60秒）
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
		if(!headers.empty()) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
		if(method == "POST") {
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		} else if(method == "DELETE") {
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
			if(!body.empty()) curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
		} else if(method == "GET") {
			// nothing extra
		}
		CURLcode res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			std::cerr<<"curl_easy_perform() failed: "<<curl_easy_strerror(res)<<std::endl;
		}
		if(chunk) curl_slist_free_all(chunk);
		curl_easy_cleanup(curl);
	}
	return readBuffer;
}

nlohmann::json BinanceHttp::getKlines(const std::string &symbol, const std::string &interval, int limit) {
	std::cout << "[BinanceHttp] getKlines symbol=" << symbol << " interval=" << interval << " limit=" << limit
		<< " csvPathSet=" << (!csvDataPath_.empty()) << " offlineMode=" << offlineMode_ << std::endl;
	// CSV mode: load from CSV file
	if (!csvDataPath_.empty()) {
		std::cout << "[BinanceHttp][CSV] Loading data from: " << csvDataPath_ << std::endl;
		int sourceLimit = limit;
		if (!csvSourceInterval_.empty()) {
			// 1m source: scale up limit to load enough raw bars for aggregation
			if (interval == "1h") sourceLimit = limit * 60;
			else if (interval == "4h") sourceLimit = limit * 240;
			else if (interval == "1d") sourceLimit = limit * 1440;
			else sourceLimit = limit;
		} else {
			if (interval == "4h") sourceLimit = limit * 4;
			else if (interval == "1d") sourceLimit = limit * 24;
		}
		std::cout << "[BinanceHttp][CSV] sourceLimit=" << sourceLimit << std::endl;
		auto data = CsvKlineLoader::loadFromCsv(csvDataPath_, sourceLimit);
		std::cout << "[BinanceHttp][CSV] loaded rows=" << data.size() << std::endl;
		if (!csvSourceInterval_.empty()) {
			// Aggregate from source interval to target interval
			if (interval == "1h") {
				std::cout << "[BinanceHttp][CSV] Aggregating 1m->1h (group 60)" << std::endl;
				auto agg = CsvKlineLoader::aggregateKlines(data, 60);
				std::cout << "[BinanceHttp][CSV] returning 1h data rows=" << agg.size() << std::endl;
				return agg;
			}
			if (interval == "4h") {
				std::cout << "[BinanceHttp][CSV] Aggregating 1m->4h (group 240)" << std::endl;
				auto agg = CsvKlineLoader::aggregateKlines(data, 240);
				std::cout << "[BinanceHttp][CSV] returning 4h data rows=" << agg.size() << std::endl;
				return agg;
			}
			if (interval == "1d") {
				std::cout << "[BinanceHttp][CSV] Aggregating 1m->1d (group 1440)" << std::endl;
				auto agg = CsvKlineLoader::aggregateKlines(data, 1440);
				std::cout << "[BinanceHttp][CSV] returning 1d data rows=" << agg.size() << std::endl;
				return agg;
			}
			std::cout << "[BinanceHttp][CSV] returning raw data rows=" << data.size() << std::endl;
			return data;
		}
		// Existing aggregation logic (for 1h source)
		if (interval == "1h") {
			std::cout << "[BinanceHttp][CSV] returning 1h data rows=" << data.size() << std::endl;
			return data;
		}
		if (interval == "4h") {
			auto agg = CsvKlineLoader::aggregateKlines(data, 4);
			std::cout << "[BinanceHttp][CSV] returning 4h data rows=" << agg.size() << std::endl;
			return agg;
		}
		if (interval == "1d") {
			auto agg = CsvKlineLoader::aggregateKlines(data, 24);
			std::cout << "[BinanceHttp][CSV] returning 1d data rows=" << agg.size() << std::endl;
			return agg;
		}
		std::cout << "[BinanceHttp][CSV] returning raw data rows=" << data.size() << std::endl;
		return data;
	}

	// Offline mode: use mock data
	if (offlineMode_) {
		std::cout << "[BinanceHttp][Mock] Generating " << limit << " candles of " << interval << std::endl;
		return MockDataGenerator::generateKlines(symbol, interval, limit, 50000.0, 0.02);
	}

	// Online mode: fetch from Binance API
	std::cout << "[BinanceHttp][Online] Fetching from Binance REST API" << std::endl;
	std::ostringstream oss;
	oss << baseUrl_ << "/fapi/v1/klines?symbol=" << symbol << "&interval=" << interval << "&limit=" << limit;
	std::string res = doRequest(oss.str(), "GET", "", "");
	try {
		return nlohmann::json::parse(res);
	} catch(...) {
		std::cerr << "[BinanceHttp][Error] Failed to parse API response" << std::endl;
		return nlohmann::json::array();
	}
}

nlohmann::json BinanceHttp::getPublic(const std::string &path, const std::string &params) {
	std::ostringstream oss;
	oss<<baseUrl_<<path;
	if(!params.empty()) oss<<"?"<<params;
	std::string res = doRequest(oss.str(), "GET", "", "");
	try { return nlohmann::json::parse(res); } catch(...) { return nlohmann::json::array(); }
}

nlohmann::json BinanceHttp::postSigned(const std::string &path, const std::string &params) {
	long long ts = util::current_timestamp_ms();
	std::ostringstream qs;
	if(!params.empty()) qs<<params<<"&";
	qs<<"timestamp="<<ts;
	std::string signature = util::hmac_sha256_hex(secret_, qs.str());
	qs<<"&signature="<<signature;
	std::ostringstream url;
	url<<baseUrl_<<path;
	std::string headers = "X-MBX-APIKEY: ";
	headers += apiKey_;
	std::string body = qs.str();
	std::string res = doRequest(url.str(), "POST", body, headers);
	try { return nlohmann::json::parse(res); } catch(...) { return nlohmann::json::object(); }
}
