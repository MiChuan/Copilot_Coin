#include "strategy.h"
#include "indicators.h"
#include <algorithm>

Strategy::Strategy() {}

Signal Strategy::evaluate(const MarketState &s) {
	Signal sig;
	if(s.close_1d.size()<22 || s.close_4h.size()<50) return sig;

	// =============================================================
	// 【日线指标 — 趋势判断】
	// =============================================================

	// 1. BOLL(20, 1.5) — 趋势方向
	auto bollD = indicators::bollinger(s.close_1d, 20, 1.5);
	// 2. RSI(14) — 超买超卖参考
	auto rsiD14 = indicators::rsi(s.close_1d, 14);
	if(rsiD14.empty()) return sig;
	// 3. SMA(20) — 辅助趋势
	auto sma20d = indicators::sma(s.close_1d, 20);
	if(sma20d.empty()) return sig;

	double lastClose1d = s.close_1d.back();
	bool bullTrend = lastClose1d > bollD.middle;
	bool bearTrend = lastClose1d < bollD.middle;
	double rsiDVal = rsiD14.back();

	// =============================================================
	// 【1小时线指标 — 交易执行】
	// =============================================================

	double last1h = s.close_4h.back();

	// 1. BOLL(20, 2.0) — 回调深度/止盈位置
	auto boll1h = indicators::bollinger(s.close_4h, 20, 2.0);
	double bollLower = boll1h.lower;
	double bollUpper = boll1h.upper;
	bool nearLower1h      = (bollLower > 0) && ((std::abs(last1h - bollLower) / bollLower) < 0.025);
	bool nearLower1h_wide = (bollLower > 0) && ((std::abs(last1h - bollLower) / bollLower) < 0.04);
	bool nearUpper1h      = (bollUpper > 0) && ((std::abs(last1h - bollUpper) / bollUpper) < 0.025);

	// 2. RSI(14) — 回调/衰竭判断
	auto rsi1h14 = indicators::rsi(s.close_4h, 14);
	if(rsi1h14.empty()) return sig;
	double rsi1h = rsi1h14.back();

	// 3. MACD(12, 26, 9) — 金叉死叉 + 柱状体动能
	std::vector<double> dif, dea, hist;
	indicators::macd(s.close_4h, dif, dea, hist);
	bool macdCrossUp = false, macdCrossDn = false;
	bool histPositive = false, histNegative = false;
	bool histRising = false, histFalling = false;
	if(dif.size() >= 2 && dea.size() >= 2) {
		macdCrossUp = dif.back() > dea.back() && dif[dif.size()-2] <= dea[dea.size()-2];
		macdCrossDn = dif.back() < dea.back() && dif[dif.size()-2] >= dea[dea.size()-2];
	}
	if(!hist.empty()) {
		histPositive = hist.back() > 0;
		histNegative = hist.back() < 0;
		if(hist.size() >= 2) {
			histRising  = hist.back() > hist[hist.size()-2];
			histFalling = hist.back() < hist[hist.size()-2];
		}
	}

	// 4. ATR(14) — 动态止损参考
	auto atr14 = indicators::atr(s.high_4h, s.low_4h, s.close_4h, 14);
	double atrVal = atr14.empty() ? 0.0 : atr14.back();
	double atrPct = atrVal > 0 ? (atrVal / last1h * 100.0) : 0.0;

	// 5. VOL — 放量/缩量 (当前 vs 前5周期均值)
	bool volSpike = false, volDry = false, volNormal = false;
	if(s.vol_4h.size() >= 6) {
		double sum5 = 0;
		for(size_t i = s.vol_4h.size()-6; i < s.vol_4h.size()-1; i++)
			sum5 += s.vol_4h[i];
		double avg5 = sum5 / 5.0;
		double lastVol = s.vol_4h.back();
		volSpike = lastVol > avg5 * 1.2;
		volDry   = lastVol < avg5 * 0.8;
		volNormal = !volSpike && !volDry;
	}

	// =============================================================
	// 【交易信号生成】
	// =============================================================

	// ----- BUY: 多头趋势健康回调买入 -----
	// 规范: 多头趋势中 RSI 40-60 = 健康回调; 1h BOLL下轨 = 支撑
	bool buySig = false;
	if(bullTrend) {
		// 日线趋势强度: SMA20斜率向上
		bool trendStrong = (sma20d.size() >= 2) && (sma20d.back() > sma20d[sma20d.size()-2]);
		// 价格在支撑区: 靠近1h BOLL下轨(4%容差) 或 RSI处于健康回调区(37-58)
		bool atSupport = nearLower1h_wide || (rsi1h >= 37.0 && rsi1h <= 58.0);
		// MACD转多: 金叉 或 柱状体回升
		bool macdBullish = macdCrossUp || histRising;
		// 量能: 不缩量即可 (缩量反弹不可靠)
		bool volOk = !volDry;

		if(trendStrong && atSupport && macdBullish && volOk) {
			buySig = true;
			sig.reason = "BUY|pullback|RSI1h=" + std::to_string((int)rsi1h)
				+ "|RSId=" + std::to_string((int)rsiDVal)
				+ "|MACD" + std::string(macdCrossUp?"cross":"histUp");
		}
	}

	// ----- SELL: 卖出/离场 -----
	bool sellSig = false;

	// 条件A: 1h RSI超买衰竭 (>70) + 接近BOLL上轨 + MACD转空
	if(!sellSig) {
		bool overbought = rsi1h > 70.0 && nearUpper1h;
		bool macdBearish = macdCrossDn || histFalling;
		if(overbought && macdBearish) {
			sellSig = true;
			sig.reason = "SELL|overbought|RSI1h=" + std::to_string((int)rsi1h);
		}
	}

	// 条件B: 日线转空 + 日线RSI弱势 < 35
	if(!sellSig && bearTrend && rsiDVal < 35.0) {
		sellSig = true;
		sig.reason = "SELL|trendFlip|RSId=" + std::to_string((int)rsiDVal);
	}

	// 条件C: 动量衰竭止盈 (RSI>65 + MACD死叉) — 在超买前锁定利润
	if(!sellSig && rsi1h > 65.0 && macdCrossDn) {
		sellSig = true;
		sig.reason = "SELL|momFade|RSI1h=" + std::to_string((int)rsi1h);
	}

	if(buySig)  { sig.buy  = true; sig.price = last1h; }
	if(sellSig) { sig.sell = true; sig.price = last1h; }

	return sig;
}
