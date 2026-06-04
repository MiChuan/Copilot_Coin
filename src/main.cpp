#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <optional>
#include "binance_http.h"
#include "strategy.h"
#include "executor.h"
#include "backtest.h"
#include <nlohmann/json.hpp>

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
		std::string csvPath;
		double initialBalance = 1000.0;
		double feePerc = 0.0004;
		double slippagePerc = 0.0005;
		double leverage = 3.0;
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
		if (cfg.contains("backtest")) {
			auto b = cfg["backtest"];
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
		<< " command=" << commandName(opt.command)
		<< " mode=" << backtestModeName(opt.backtestMode)
		<< " offline=" << opt.offlineMode
		<< " csvPath=" << opt.csvPath
		<< " initialBalance=" << opt.initialBalance
		<< " fee=" << opt.feePerc
		<< " slippage=" << opt.slippagePerc
		<< " leverage=" << opt.leverage
		<< " hoursBack=" << opt.hoursBack << std::endl;

	BinanceHttp api(apiKey, secret, true);
	Strategy strat;
	Executor exe(&api, 2.0);
	exe.setLeverage(2);
	std::cout << "[Main] BinanceHttp/Strategy/Executor initialized" << std::endl;

	if (opt.offlineMode) {
		std::cout << "[Main] Enabling offline mode" << std::endl;
		api.setOfflineMode(true);
	}
	if (!opt.csvPath.empty()) {
		std::cout << "[Main] Setting CSV path: " << opt.csvPath << std::endl;
		api.setCsvDataPath(opt.csvPath);
	}
	// Set CSV source interval for 1m->1h aggregation
	if (cfg.contains("backtest") && cfg["backtest"].contains("csvInterval")) {
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

	// TP / SL 状态追踪
	double tpOriginalQty = 0.0;
	bool tp1Live = false;
	bool tp2Live = false;
	bool slLive = false;

	while(true) {
		MarketState s;
		auto k4 = api.getKlines("BTCUSDT", "4h", 500);
		auto kd = api.getKlines("BTCUSDT", "1d", 500);
		if(k4.is_array()){
			for(auto &k: k4){
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

		// ===== 分批止盈检查 =====
		Position pos = exe.getLocalPosition("BTCUSDT");
		if(pos.qty > 0.0 && pos.avgPrice > 0.0) {
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
						auto resp = exe.marketSellQty("BTCUSDT", closeQty);
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
						auto resp = exe.marketSellQty("BTCUSDT", closeQty);
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
						auto resp = exe.marketSellQty("BTCUSDT", closeQty);
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
			double usable = 100.0;
			auto resp = exe.marketBuy("BTCUSDT", usable*0.3, 2.0);
			std::cout<<"Buy exec: "<<resp.dump()<<" reason="<<sig.reason<<std::endl;
			// 重置止盈/止损状态，等待下一轮更新
			tpOriginalQty = 0.0;
			tp1Live = false;
			tp2Live = false;
			slLive = false;
		}
		if(sig.sell){
			double usable = 100.0;
			auto resp = exe.marketSell("BTCUSDT", usable*0.3, 2.0);
			std::cout<<"Sell exec: "<<resp.dump()<<" reason="<<sig.reason<<std::endl;
			tpOriginalQty = 0.0;
			tp1Live = false;
			tp2Live = false;
			slLive = false;
		}
		std::this_thread::sleep_for(std::chrono::minutes(1));
	}
	return 0;
}
