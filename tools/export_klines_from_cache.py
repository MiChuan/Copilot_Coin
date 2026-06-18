"""将 cursor_coin 缓存 K 线导出为 Copilot_Coin 回测可读的 Binance JSON 格式。"""
from __future__ import annotations

import json
import sys
from io import StringIO
from pathlib import Path

import pandas as pd


def df_to_binance_json(df: pd.DataFrame) -> list:
    rows = []
    for _, row in df.iterrows():
        open_ms = int(row["open_time"].value // 10**6)
        close_ms = int(row["close_time"].value // 10**6)
        rows.append([
            open_ms,
            f"{row['open']:.8f}",
            f"{row['high']:.8f}",
            f"{row['low']:.8f}",
            f"{row['close']:.8f}",
            f"{row['volume']:.8f}",
            close_ms,
            "0", "0", "0", "0", "0",
        ])
    return rows


def load_cache(cache_path: Path) -> tuple[pd.DataFrame, pd.DataFrame]:
    with open(cache_path, encoding="utf-8") as f:
        raw = json.load(f)
    df_1h = pd.read_json(StringIO(raw["1h"]), orient="split")
    df_1d = pd.read_json(StringIO(raw["1d"]), orient="split")
    df_1h["open_time"] = pd.to_datetime(df_1h["open_time"], utc=True)
    df_1h["close_time"] = pd.to_datetime(df_1h["close_time"], utc=True)
    df_1d["open_time"] = pd.to_datetime(df_1d["open_time"], utc=True)
    df_1d["close_time"] = pd.to_datetime(df_1d["close_time"], utc=True)
    return df_1h, df_1d


def main() -> int:
    cache = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("../cursor_coin/data/klines_BTCUSDT_365d.json")
    out_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else Path("data")
    out_dir.mkdir(parents=True, exist_ok=True)

    df_1h, df_1d = load_cache(cache)
    k1h_path = out_dir / "bt_1h.json"
    k1d_path = out_dir / "bt_1d.json"
    with open(k1h_path, "w", encoding="utf-8") as f:
        json.dump(df_to_binance_json(df_1h), f)
    with open(k1d_path, "w", encoding="utf-8") as f:
        json.dump(df_to_binance_json(df_1d), f)

    print(f"Exported 1h={len(df_1h)} 1d={len(df_1d)}")
    print(f"  {k1h_path}")
    print(f"  {k1d_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
