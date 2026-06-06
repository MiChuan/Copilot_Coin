#include <iostream>
#include <cmath>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <optional>
#include <curl/curl.h>
#include "binance_http.h"
#include "strategy.h"
#include "executor.h"
#include "backtest.h"
#include "indicators.h"
#include <nlohmann/json.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
	enum class CommandType {
		Live,
		Backtest
	};

	enum class BacktestMode {
		Run,
		Offline,
		Report
	};

	struct CliOptions {
		CommandType command = CommandType::Live;
		BacktestMode backtestMode = BacktestMode::Run;
		bool offlineMode = false;
		bool useDemo = false;
		std::string csvPath;
		std::string demoApiBaseUrl;
		std::string demoApiHost;
		std::string httpProxy;
		double initialBalance = 1000.0;
		double feePerc = 0.0004;
		double slippagePerc = 0.0005;
		double leverage = 3.0;
		double positionPct = 0.3;
		long long recvWindowMs = 60000;
		int timeSyncIntervalSeconds = 300;
		int pollIntervalSeconds = 60;
		int hoursBack = 24 * 365;
	};

	CliOptions parseCli(int argc, char** argv) {
		CliOptions opt;
		std::ifstream btFlag("run_backtest.flag");
		if (btFlag) opt.command = CommandType::Backtest;

		auto parseBacktestMode = [&](const std::string& token) {
			if (token == "offline" || token == "test-offline" || token == "--offline" || token == "--test-offline") {
				opt.backtestMode = BacktestMode::Offline;
				opt.offlineMode = true;
				opt.command = CommandType::Backtest;
				return true;
			}
			if (token == "report" || token == "report-backtest" || token == "--report-backtest") {
				opt.backtestMode = BacktestMode::Report;
				opt.offlineMode = true;
				opt.command = CommandType::Backtest;
				return true;
			}
			if (token == "run" || token == "--run") {
				opt.backtestMode = BacktestMode::Run;
				opt.command = CommandType::Backtest;
				return true;
			}
			return false;
		};

		for (int i = 1; i < argc; ++i) {
			std::string a = argv[i];
			if (a == "backtest" || a == "--backtest") {
				opt.command = CommandType::Backtest;
				if (i + 1 < argc) {
					std::string next = argv[i + 1];
					if (!next.empty() && next[0] != '-' && parseBacktestMode(next)) {
						++i;
					}
				}
			} else if (parseBacktestMode(a)) {
				continue;
			} else if (a == "--offline-mode") {
				opt.offlineMode = true;
			} else if (a == "--csv" && i + 1 < argc) {
				opt.csvPath = argv[++i];
			} else if (a == "--csvPath" && i + 1 < argc) {
				opt.csvPath = argv[++i];
			} else if (a == "--initialBalance" && i + 1 < argc) {
				opt.initialBalance = std::stod(argv[++i]);
			} else if (a == "--feePerc" && i + 1 < argc) {
				opt.feePerc = std::stod(argv[++i]);
			} else if (a == "--slippagePerc" && i + 1 < argc) {
				opt.slippagePerc = std::stod(argv[++i]);
			} else if (a == "--leverage" && i + 1 < argc) {
				opt.leverage = std::stod(argv[++i]);
			} else if (a == "--recvWindow" && i + 1 < argc) {
				opt.recvWindowMs = std::stoll(argv[++i]);
			} else if (a == "--timeSyncInterval" && i + 1 < argc) {
				opt.timeSyncIntervalSeconds = std::stoi(argv[++i]);
			} else if (a == "--pollInterval" && i + 1 < argc) {
				opt.pollIntervalSeconds = std::stoi(argv[++i]);
			} else if (a == "--hoursBack" && i + 1 < argc) {
				opt.hoursBack = std::stoi(argv[++i]);
			}
		}
		return opt;
	}

	int parseHoursBackValue(const nlohmann::json& value, int fallback) {
		try {
			if (value.is_number_integer()) {
				return value.get<int>();
			}
			if (value.is_number()) {
				return static_cast<int>(value.get<double>());
			}
			if (value.is_string()) {
				const std::string s = value.get<std::string>();
				if (s == "24*365") return 24 * 365;
				return std::stoi(s);
			}
		} catch (...) {
			std::cerr << "[Main][Warn] Invalid backtest.hoursBack value, using fallback=" << fallback << std::endl;
		}
		return fallback;
	}

	void applyConfigOverrides(const nlohmann::json& cfg, CliOptions& opt, std::string& apiKey, std::string& secret) {
		if (cfg.contains("apiKey")) apiKey = cfg.value("apiKey", apiKey);
		if (cfg.contains("secret")) secret = cfg.value("secret", secret);
		if (cfg.contains("useDemo")) opt.useDemo = cfg.value("useDemo", false);
		if (cfg.contains("demoApiBaseUrl")) opt.demoApiBaseUrl = cfg.value("demoApiBaseUrl", opt.demoApiBaseUrl);
		if (cfg.contains("demoApiHost")) opt.demoApiHost = cfg.value("demoApiHost", opt.demoApiHost);
		if (cfg.contains("httpProxy")) opt.httpProxy = cfg.value("httpProxy", opt.httpProxy);
		if (cfg.contains("live")) {
			auto l = cfg["live"];
			opt.leverage = l.value("leverage", opt.leverage);
			opt.positionPct = l.value("positionPct", opt.positionPct);
			opt.recvWindowMs = l.value("recvWindowMs", opt.recvWindowMs);
			opt.timeSyncIntervalSeconds = l.value("timeSyncIntervalSeconds", opt.timeSyncIntervalSeconds);
			opt.pollIntervalSeconds = l.value("pollIntervalSeconds", opt.pollIntervalSeconds);
		}
		if (cfg.contains("backtest")) {
			auto b = cfg["backtest"];
			if (opt.command == CommandType::Backtest) {
				opt.initialBalance = b.value("initialBalance", opt.initialBalance);
				opt.feePerc = b.value("feePerc", opt.feePerc);
				opt.slippagePerc = b.value("slippagePerc", opt.slippagePerc);
				opt.leverage = b.value("leverage", opt.leverage);
				if (b.contains("hoursBack")) {
					opt.hoursBack = parseHoursBackValue(b["hoursBack"], opt.hoursBack);
				}
				opt.csvPath = b.value("csvPath", opt.csvPath);
				std::string mode = b.value("mode", std::string("run"));
				if (mode == "offline") {
					opt.backtestMode = BacktestMode::Offline;
					opt.offlineMode = true;
				} else if (mode == "report") {
					opt.backtestMode = BacktestMode::Report;
					opt.offlineMode = true;
				}
			}
		}
	}

	std::string commandName(CommandType command) {
		switch (command) {
			case CommandType::Backtest: return "backtest";
			default: return "live";
		}
	}

	std::string backtestModeName(BacktestMode mode) {
		switch (mode) {
			case BacktestMode::Offline: return "offline";
			case BacktestMode::Report: return "report";
			default: return "run";
		}
	}

	void writeReports(const BacktestResult& r) {
		std::filesystem::create_directories("reports");
		const std::string reportBase = "reports/bt_";
		const std::string equityCsvReport = reportBase + "equity.csv";
		const std::string summaryJsonReport = reportBase + "summary.json";
		const std::string tradesCsvReport = reportBase + "trades.csv";
		const std::string tradesJsonReport = reportBase + "trades.json";

		std::ofstream csv(equityCsvReport);
		csv << "time,equity\n";
		for (size_t i = 0; i < r.equityCurve.size(); ++i) {
			csv << r.equityTime[i] << "," << std::fixed << std::setprecision(8) << r.equityCurve[i] << "\n";
		}

		std::ofstream tcsv(tradesCsvReport);
		tcsv << "entryTime,exitTime,entryPrice,exitPrice,qty,entryFee,exitFee,pnl\n";
		for (const auto& t : r.tradesRec) {
			tcsv << t.entryTime << "," << t.exitTime << "," << std::fixed << std::setprecision(8)
				<< t.entryPrice << "," << t.exitPrice << "," << t.qty << "," << t.entryFee << "," << t.exitFee << "," << t.pnl << "\n";
		}

		nlohmann::json summary;
		summary["initial_balance"] = r.initial_balance;
		summary["final_balance"] = r.final_balance;
		summary["trades"] = r.trades;
		summary["win_rate"] = r.winRate;
		summary["max_drawdown"] = r.maxDrawdown;
		summary["sharpe"] = r.sharpe;
		summary["equity_curve_csv"] = equityCsvReport;
		summary["trades_csv"] = tradesCsvReport;
		summary["equity_curve"] = nlohmann::json::array();
		for (size_t i = 0; i < r.equityCurve.size(); ++i) {
			summary["equity_curve"].push_back({{"time", r.equityTime[i]}, {"equity", r.equityCurve[i]}});
		}
		std::ofstream js(summaryJsonReport);
		js << std::setw(2) << summary << std::endl;

		nlohmann::json tradesJson = nlohmann::json::array();
		for (const auto& t : r.tradesRec) {
			tradesJson.push_back({
				{"entryTime", t.entryTime}, {"exitTime", t.exitTime}, {"entryPrice", t.entryPrice},
				{"exitPrice", t.exitPrice}, {"qty", t.qty}, {"entryFee", t.entryFee},
				{"exitFee", t.exitFee}, {"pnl", t.pnl}
			});
		}
		std::ofstream tj(tradesJsonReport);
		tj << std::setw(2) << tradesJson << std::endl;

		std::cout << "[Report] Equity CSV saved to: " << equityCsvReport << std::endl;
		std::cout << "[Report] Summary JSON saved to: " << summaryJsonReport << std::endl;
		std::cout << "[Report] Trades CSV saved to: " << tradesCsvReport << std::endl;
		std::cout << "[Report] Trades JSON saved to: " << tradesJsonReport << std::endl;
	}
}

int main(int argc, char** argv){
#ifdef _WIN32
	// 设置控制台输出为 UTF-8
	SetConsoleOutputCP(CP_UTF8);
#endif
	// 初始化 curl（必须在多线程使用前调用）
	curl_global_init(CURL_GLOBAL_ALL);
	std::cout << "[Main] Program start" << std::endl;
	std::cout << "[Main] argc=" << argc << std::endl;
	for (int i = 0; i < argc; ++i) {
		std::cout << "[Main] argv[" << i << "]=" << argv[i] << std::endl;
	}

	std::string apiKey;
	std::string secret;
	nlohmann::json cfg;
	std::ifstream ifs("config.json");
	if (ifs) {
		std::cout << "[Main] Loading config.json" << std::endl;
		ifs >> cfg;
		std::cout << "[Main] config loaded: keys=" << cfg.size() << std::endl;
	} else {
		std::cout << "[Main] config.json not found, using defaults" << std::endl;
	}

	CliOptions opt = parseCli(argc, argv);
	std::cout << "[Main] CLI parsed: command=" << commandName(opt.command)
		<< " mode=" << backtestModeName(opt.backtestMode)
		<< " offline=" << opt.offlineMode
		<< " csvPath=" << opt.csvPath
		<< " initialBalance=" << opt.initialBalance
		<< " fee=" << opt.feePerc
		<< " slippage=" << opt.slippagePerc
		<< " leverage=" << opt.leverage
		<< " hoursBack=" << opt.hoursBack << std::endl;
	applyConfigOverrides(cfg, opt, apiKey, secret);
	std::cout << "[Main] Config applied: apiKeySet=" << (!apiKey.empty())
		<< " secretSet=" << (!secret.empty())
		<< " useDemo=" << opt.useDemo
		<< " command=" << commandName(opt.command)
		<< " mode=" << backtestModeName(opt.backtestMode)
		<< " offline=" << opt.offlineMode
		<< " csvPath=" << opt.csvPath
		<< " initialBalance=" << opt.initialBalance
		<< " fee=" << opt.feePerc
		<< " slippage=" << opt.slippagePerc
		<< " leverage=" << opt.leverage
		<< " recvWindowMs=" << opt.recvWindowMs
		<< " timeSyncIntervalSeconds=" << opt.timeSyncIntervalSeconds
		<< " pollIntervalSeconds=" << opt.pollIntervalSeconds
		<< " hoursBack=" << opt.hoursBack << std::endl;

	BinanceHttp api(apiKey, secret, true, opt.useDemo);
	if (opt.useDemo && !opt.demoApiBaseUrl.empty()) {
		api.setDemoBaseUrl(opt.demoApiBaseUrl, opt.demoApiHost);
	}
	if (!opt.httpProxy.empty()) {
		api.setHttpProxy(opt.httpProxy);
	}
	api.setRecvWindow(opt.recvWindowMs);
	const bool needsTimeSync = opt.command != CommandType::Backtest || !opt.offlineMode;
	if (needsTimeSync) {
		api.syncServerTime();
		api.startTimeSyncLoop(opt.timeSyncIntervalSeconds);
	}
	Strategy strat;
	Executor exe(&api, opt.leverage);
	exe.setLeverage(static_cast<int>(opt.leverage));
	std::cout << "[Main] BinanceHttp/Strategy/Executor initialized (leverage=" << opt.leverage << ")" << std::endl;

	if (opt.command != CommandType::Backtest || !opt.offlineMode) {
		exe.ensureOneWayMode();
		exe.syncLocalPosition("BTCUSDT");
		double startupPos = exe.getExchangePositionQty("BTCUSDT");
		if (startupPos < 0.0) {
			const double shortQty = -startupPos;
			std::cout << "[Main] Closing stray short position qty=" << shortQty << std::endl;
			auto closeResp = exe.marketCloseShortQty("BTCUSDT", shortQty);
			std::cout << "[Main] Close short: " << closeResp.dump() << std::endl;
			exe.syncLocalPosition("BTCUSDT");
		}
	}

	if (opt.command == CommandType::Backtest && opt.offlineMode) {
		std::cout << "[Main] Enabling offline mode" << std::endl;
		api.setOfflineMode(true);
	}
	if (opt.command == CommandType::Backtest && !opt.csvPath.empty()) {
		std::cout << "[Main] Setting CSV path: " << opt.csvPath << std::endl;
		api.setCsvDataPath(opt.csvPath);
	}
	if (opt.command == CommandType::Backtest && cfg.contains("backtest") && cfg["backtest"].contains("csvInterval")) {
		std::string csvInterval = cfg["backtest"]["csvInterval"].get<std::string>();
		std::cout << "[Main] Setting CSV source interval: " << csvInterval << std::endl;
		api.setCsvSourceInterval(csvInterval);
	}

	if (opt.command == CommandType::Backtest) {
		std::cout << "[Main] Running backtest mode=" << backtestModeName(opt.backtestMode) << std::endl;
		std::cout << "[Main] Backtest config: initialBalance=" << opt.initialBalance
			<< " fee=" << opt.feePerc
			<< " slip=" << opt.slippagePerc
			<< " lev=" << opt.leverage
			<< " hours=" << opt.hoursBack << std::endl;
		if (opt.offlineMode) std::cout << "[Main] Offline mode active" << std::endl;
		if (!opt.csvPath.empty()) std::cout << "[Main] CSV mode active: " << opt.csvPath << std::endl;
		try {
			Backtest bt(&api, &strat);
			std::cout << "[Main] Fetching 1h klines for backtest..." << std::endl;
			auto k1h = api.getKlines("BTCUSDT", "1h", opt.hoursBack);
			std::cout << "[Main] Kline payload size returned: " << k1h.size() << std::endl;
			std::cout << "[Main] Entering Backtest::runFrom1hKlines" << std::endl;
			auto r = bt.runFrom1hKlines(k1h, opt.feePerc, opt.slippagePerc, opt.leverage, opt.initialBalance);
			std::cout << "[Main] Backtest returned" << std::endl;
			std::cout << "Backtest trades=" << r.trades << " start=" << r.initial_balance << " end=" << r.final_balance
				<< " winRate=" << r.winRate << " maxDD=" << r.maxDrawdown << " sharpe=" << r.sharpe << std::endl;
			std::cout << "Equity Curve:" << std::endl;
			for (size_t i = 0; i < r.equityCurve.size(); ++i) {
				std::cout << r.equityTime[i] << "," << r.equityCurve[i] << std::endl;
			}
			if (opt.backtestMode == BacktestMode::Offline || opt.backtestMode == BacktestMode::Report) {
				std::cout << "[Main] Writing reports..." << std::endl;
				writeReports(r);
			}
		} catch (const std::exception& e) {
			std::cerr << "[Main][BacktestError] " << e.what() << std::endl;
		} catch (...) {
			std::cerr << "[Main][BacktestError] unknown exception" << std::endl;
		}
		return 0;
	}

	std::cout << "[Main] Demo live mode — fetching account from demo-fapi.binance.com" << std::endl;
	auto acc = api.getFuturesAccount();
	if (acc.contains("totalWalletBalance")) {
		std::cout << "[Main] Wallet balance: " << acc["totalWalletBalance"].get<std::string>() << " USDT" << std::endl;
		std::cout << "[Main] Available balance: " << acc.value("availableBalance", "n/a") << std::endl;
		std::cout << "[Main] Unrealized PNL: " << acc.value("totalUnrealizedProfit", "n/a") << std::endl;
	} else if (!acc.empty()) {
		std::cout << "[Main] Account response: " << acc.dump() << std::endl;
	} else {
		std::cerr << "[Main][Warn] Could not read futures account. Check API key (from demo.binance.com) and network." << std::endl;
	}
	double walletBal = exe.getWalletBalance();
	double availBal = exe.getAvailableUSDT();
	std::cout << "[Main] Margin wallet=" << walletBal << " available(USDT+USDC)=" << availBal
		<< " positionPct=" << opt.positionPct << std::endl;

	std::cout << "[Main] Live poll interval: " << opt.pollIntervalSeconds << "s (1h RSI + strategy)" << std::endl;

	// TP / SL 状态追踪
	double tpOriginalQty = 0.0;
	bool tp1Live = false;
	bool tp2Live = false;
	bool slLive = false;

	while(true) {
		MarketState s;
		// 与回测一致：1h 执行 + 日线趋势（close_4h 字段名沿用，实际为 1h K 线）
		auto k1h = api.getKlines("BTCUSDT", "1h", 500);
		auto kd = api.getKlines("BTCUSDT", "1d", 500);
		if(k1h.is_array()){
			for(auto &k: k1h){
				s.close_4h.push_back(std::stod(k[4].get<std::string>()));
				s.vol_4h.push_back(std::stod(k[5].get<std::string>()));
				s.high_4h.push_back(std::stod(k[2].get<std::string>()));
				s.low_4h.push_back(std::stod(k[3].get<std::string>()));
			}
		}
		if(kd.is_array()){
			for(auto &k: kd) s.close_1d.push_back(std::stod(k[4].get<std::string>()));
		}
		auto sig = strat.evaluate(s);

		double rsi1h = 0.0;
		auto rsi1hSeries = indicators::rsi(s.close_4h, 14);
		if (!rsi1hSeries.empty()) rsi1h = rsi1hSeries.back();
		const double posQtyNow = exe.getExchangePositionQty("BTCUSDT");
		std::cout << "[Live] Poll rsi1h=" << std::fixed << std::setprecision(1) << rsi1h
			<< " pos=" << posQtyNow
			<< " buy=" << sig.buy << " sell=" << sig.sell;
		if (!sig.reason.empty()) std::cout << " reason=" << sig.reason;
		std::cout << std::endl;

		double orderMargin = walletBal * opt.positionPct;
		if (orderMargin <= 0.0) orderMargin = availBal * opt.positionPct;

		// ===== 分批止盈检查（仅有多仓时） =====
		const double exchangePos = exe.getExchangePositionQty("BTCUSDT");
		Position pos = exe.getLocalPosition("BTCUSDT");
		if (exchangePos > 0.0) {
			if (pos.qty <= 0.0 || std::abs(pos.qty - exchangePos) > 1e-8) {
				exe.syncLocalPosition("BTCUSDT");
				pos = exe.getLocalPosition("BTCUSDT");
			}
			auto k1 = api.getKlines("BTCUSDT", "1m", 1);
			if(k1.is_array() && !k1.empty()) {
				double curPrice = std::stod(k1[0][4].get<std::string>());
				double profitPct = (curPrice - pos.avgPrice) / pos.avgPrice;

				// 首次开仓时记录原始仓位
				if(tpOriginalQty <= 0.0) {
					tpOriginalQty = pos.qty;
					tp1Live = false;
					tp2Live = false;
					slLive = false;
				}

				// TP1: 盈利 >= 8%，平仓原始仓位的 50%
				if(!tp1Live && profitPct >= 0.08) {
					double closeQty = tpOriginalQty * 0.5;
					if(closeQty > 0.0 && closeQty <= pos.qty) {
						std::cout << "[Live][TP1] profitPct=" << profitPct*100 << "% closeQty=" << closeQty << std::endl;
						auto resp = exe.marketCloseLongQty("BTCUSDT", closeQty);
						std::cout << "[Live][TP1] exec: " << resp.dump() << std::endl;
						tp1Live = true;
					}
				}

				// TP2: 盈利 >= 15%，平仓剩余仓位的 50%
				if(!tp2Live && profitPct >= 0.15) {
					Position pos2 = exe.getLocalPosition("BTCUSDT");
					double closeQty = pos2.qty * 0.5;
					if(closeQty > 0.0) {
						std::cout << "[Live][TP2] profitPct=" << profitPct*100 << "% closeQty=" << closeQty << " remainQty=" << pos2.qty << std::endl;
						auto resp = exe.marketCloseLongQty("BTCUSDT", closeQty);
						std::cout << "[Live][TP2] exec: " << resp.dump() << std::endl;
						tp2Live = true;
					}
				}

				// SL: 浮亏 >= 2.5%，平仓当前仓位的 80%
				if(!slLive && profitPct <= -0.025) {
					Position posSL = exe.getLocalPosition("BTCUSDT");
					double closeQty = posSL.qty * 0.8;
					if(closeQty > 0.0) {
						std::cout << "[Live][SL] lossPct=" << profitPct*100 << "% closeQty=" << closeQty << " remainQty=" << posSL.qty << std::endl;
						auto resp = exe.marketCloseLongQty("BTCUSDT", closeQty);
						std::cout << "[Live][SL] exec: " << resp.dump() << std::endl;
						slLive = true;
					}
				}
			}
		} else {
			// 无持仓时重置止盈/止损状态
			tpOriginalQty = 0.0;
			tp1Live = false;
			tp2Live = false;
			slLive = false;
		}

		if(sig.buy){
			const double posQty = exe.getExchangePositionQty("BTCUSDT");
			if(posQty <= 0.0) {
				if(posQty < 0.0) {
					std::cout << "[Live] Buy skipped: short position open (qty=" << posQty << ")" << std::endl;
				} else {
					auto resp = exe.marketBuy("BTCUSDT", orderMargin, opt.leverage);
					std::cout<<"Buy exec: "<<resp.dump()<<" margin="<<orderMargin<<" reason="<<sig.reason<<std::endl;
					tpOriginalQty = 0.0;
					tp1Live = false;
					tp2Live = false;
					slLive = false;
				}
			} else {
				std::cout << "[Live] Buy skipped: already long (qty=" << posQty << ")" << std::endl;
			}
		}
		if(sig.sell){
			const double posQty = exe.getExchangePositionQty("BTCUSDT");
			if(posQty > 0.0) {
				auto resp = exe.marketCloseLongQty("BTCUSDT", posQty);
				std::cout<<"Sell exec: "<<resp.dump()<<" closeQty="<<posQty<<" reason="<<sig.reason<<std::endl;
				tpOriginalQty = 0.0;
				tp1Live = false;
				tp2Live = false;
				slLive = false;
			} else {
				std::cout << "[Live] Sell skipped: no long position (qty=" << posQty << ") reason=" << sig.reason << std::endl;
			}
		}
		walletBal = exe.getWalletBalance();
		availBal = exe.getAvailableUSDT();
		std::this_thread::sleep_for(std::chrono::seconds(opt.pollIntervalSeconds));
	}
	return 0;
}
