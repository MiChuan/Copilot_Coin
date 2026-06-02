#include <iostream>
#include <thread>
#include <chrono>
#include <fstream>
#include "binance_http.h"
#include "strategy.h"
#include "executor.h"
#include "backtest.h"
#include <nlohmann/json.hpp>

int main(int argc, char** argv){
	// load config
	std::string apiKey="";
	std::string secret="";
	bool offlineMode = false;
	std::string csvDataPath = "";
	nlohmann::json cfg;
	std::ifstream ifs("config.json");
	if(ifs){ ifs>>cfg; apiKey = cfg.value("apiKey", ""); secret = cfg.value("secret", ""); }
	BinanceHttp api(apiKey, secret, true);
	Strategy strat;
	Executor exe(&api, 3.0);
	exe.setLeverage(3);

	// support backtest mode (via flag file or command-line)
	bool runBacktest = false;
	std::ifstream btFlag("run_backtest.flag"); if(btFlag) runBacktest = true;
	// default/backtest config from file
	double initBal = 1000.0; double fee=0.0004, slip=0.0005, lev=3.0; int hours = 24*365;
	if(cfg.contains("backtest")){
		auto b = cfg["backtest"];
		initBal = b.value("initialBalance", initBal);
		fee = b.value("feePerc", fee);
		slip = b.value("slippagePerc", slip);
		lev = b.value("leverage", lev);
		hours = b.value("hoursBack", hours);
	}
	// parse command-line overrides
	for(int i=1;i<argc;i++){
		std::string a = argv[i];
		if(a=="backtest" || a=="--backtest") { runBacktest = true; }
		else if(a=="--offline") { offlineMode = true; }
		else if(a=="--csv" && i+1<argc) { csvDataPath = argv[++i]; }
		else if(a=="--initialBalance" && i+1<argc) { initBal = std::stod(argv[++i]); }
		else if(a=="--feePerc" && i+1<argc) { fee = std::stod(argv[++i]); }
		else if(a=="--slippagePerc" && i+1<argc) { slip = std::stod(argv[++i]); }
		else if(a=="--leverage" && i+1<argc) { lev = std::stod(argv[++i]); }
		else if(a=="--hoursBack" && i+1<argc) { hours = std::stoi(argv[++i]); }
	}

	// Set offline mode and CSV path
	if (offlineMode) {
		api.setOfflineMode(true);
	}
	if (!csvDataPath.empty()) {
		api.setCsvDataPath(csvDataPath);
	}

	if(runBacktest) {
		std::cout<<"Running backtest..."<<std::endl;
		std::cout<<"Config: initialBalance="<<initBal<<" fee="<<fee<<" slip="<<slip<<" lev="<<lev<<" hours="<<hours<<std::endl;
		if(offlineMode) std::cout<<"[Offline Mode] Using mock data"<<std::endl;
		if(!csvDataPath.empty()) std::cout<<"[CSV Mode] Using CSV file: "<<csvDataPath<<std::endl;
		try {
			Strategy strategy;
			Backtest bt(&api, &strategy);
			auto r = bt.run(hours, fee, slip, lev, initBal);
			std::cout<<"Backtest trades="<<r.trades<<" start="<<r.initial_balance<<" end="<<r.final_balance<<" winRate="<<r.winRate<<" maxDD="<<r.maxDrawdown<<" sharpe="<<r.sharpe<<std::endl;
		} catch(const std::exception& e) {
			std::cerr<<"Backtest error: "<<e.what()<<std::endl;
		}
		return 0;
	}

	while(true) {
		MarketState s;
		auto k4 = api.getKlines("BTCUSDT", "4h", 500);
		auto kd = api.getKlines("BTCUSDT", "1d", 500);
		if(k4.is_array()){
			for(auto &k: k4){ s.close_4h.push_back(std::stod(k[4].get<std::string>())); s.vol_4h.push_back(std::stod(k[5].get<std::string>())); }
		}
		if(kd.is_array()){
			for(auto &k: kd) s.close_1d.push_back(std::stod(k[4].get<std::string>()));
		}
		auto sig = strat.evaluate(s);
		if(sig.buy){
			double usable = 100.0; // placeholder: should query balance
			auto resp = exe.marketBuy("BTCUSDT", usable*0.8, 3.0);
			std::cout<<"Buy exec: "<<resp.dump()<<" reason="<<sig.reason<<std::endl;
		}
		if(sig.sell){
			double usable = 100.0;
			auto resp = exe.marketSell("BTCUSDT", usable*0.8, 3.0);
			std::cout<<"Sell exec: "<<resp.dump()<<" reason="<<sig.reason<<std::endl;
		}
		std::this_thread::sleep_for(std::chrono::minutes(1));
	}
	return 0;
}
