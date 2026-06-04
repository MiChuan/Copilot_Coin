# download_um_klines_1m.py
import requests
import csv
import os
from datetime import datetime, timedelta

def download_um_klines(symbol="BTCUSDT", interval="1m", start_date=None, end_date=None, output_dir="./data"):
    """
    下载 Binance U本位合约 1分钟K线数据
    """
    os.makedirs(output_dir, exist_ok=True)

    base_url = "https://fapi.binance.com/fapi/v1/klines"

    # 如果没有指定日期，默认下载最近一年的数据
    if start_date is None:
        start_date = (datetime.now() - timedelta(days=365)).strftime("%Y-%m-%d")
    if end_date is None:
        end_date = datetime.now().strftime("%Y-%m-%d")

    # 日期转毫秒时间戳
    start_ms = int(datetime.strptime(start_date, "%Y-%m-%d").timestamp() * 1000)
    end_ms = int(datetime.strptime(end_date, "%Y-%m-%d").timestamp() * 1000)

    # 确保不超过当前时间（API限制）
    now_ms = int(datetime.now().timestamp() * 1000)
    if end_ms > now_ms:
        print(f"警告: 结束时间 {end_date} 是未来日期，已调整为当前时间")
        end_ms = now_ms

    all_klines = []
    batch = 0

    print(f"开始下载 {symbol} {interval} K线数据...")
    print(f"时间范围: {start_date} ~ {end_date}")
    print(f"预计数据量: 约 {(end_ms - start_ms) // 60000} 条 (1分钟粒度)")

    while start_ms < end_ms:
        params = {
            "symbol": symbol,
            "interval": interval,
            "startTime": start_ms,
            "limit": 1500  # API 单次最大限制
        }

        try:
            r = requests.get(base_url, params=params, timeout=30)
            r.raise_for_status()
            data = r.json()
        except Exception as e:
            print(f"请求失败: {e}")
            break

        if not data or len(data) == 0:
            break

        all_klines.extend(data)
        batch += 1

        # 下一条从最后一条的 close_time + 1ms 开始
        start_ms = data[-1][6] + 1

        # 每10批次打印一次进度
        if batch % 10 == 0:
            last_time = datetime.fromtimestamp(data[-1][0] / 1000)
            print(f"  批次 {batch}: 已下载 {len(all_klines)} 条, 最新时间: {last_time.strftime('%Y-%m-%d %H:%M')}")

    print(f"\n下载完成！共 {len(all_klines)} 条记录")

    # 保存为 CSV
    columns = [
        'open_time', 'open', 'high', 'low', 'close', 'volume',
        'close_time', 'quote_volume', 'trades', 'taker_buy_base',
        'taker_buy_quote', 'ignore'
    ]

    output_path = os.path.join(output_dir, f"{symbol}_{interval}.csv")
    with open(output_path, 'w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        writer.writerow(columns)
        writer.writerows(all_klines)

    print(f"保存路径: {output_path}")

    # 统计信息
    if all_klines:
        first_time = datetime.fromtimestamp(all_klines[0][0] / 1000)
        last_time = datetime.fromtimestamp(all_klines[-1][0] / 1000)
        print(f"数据范围: {first_time} ~ {last_time}")
        print(f"文件大小: {os.path.getsize(output_path) / 1024 / 1024:.2f} MB")

    return all_klines

if __name__ == "__main__":
    # 下载 BTCUSDT U本位合约 1分钟K线
    # 默认下载最近一年数据，或指定日期范围
    download_um_klines(
        symbol="BTCUSDT",
        interval="1m",
        # start_date="2025-06-01",  # 可选：指定开始日期
        # end_date="2026-06-02",    # 可选：指定结束日期（未来日期会自动调整为当前时间）
        output_dir="./data"
    )
