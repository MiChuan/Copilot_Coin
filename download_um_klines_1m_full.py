# download_um_klines_full.py
import requests
import csv
import os
import time
from datetime import datetime, timedelta

def download_um_klines(symbol="BTCUSDT", interval="1m", start_date="2025-03-01", end_date="2026-03-02",
                        output_dir="./data", proxy=None, sleep_interval=0.1):
    """
    下载 Binance U本位合约 1分钟K线数据
    默认范围: 2025-03-01 ~ 2026-03-02 (约1年)
    sleep_interval: 每次请求间隔秒数，降低频率避免被限流
    """
    os.makedirs(output_dir, exist_ok=True)

    base_url = "https://fapi.binance.com/fapi/v1/klines"

    # 日期转毫秒时间戳
    start_ms = int(datetime.strptime(start_date, "%Y-%m-%d").timestamp() * 1000)
    end_ms = int(datetime.strptime(end_date, "%Y-%m-%d").timestamp() * 1000)

    # 确保不超过当前时间（API限制）
    now_ms = int(datetime.now().timestamp() * 1000)
    if end_ms > now_ms:
        print(f"警告: 结束时间 {end_date} 是未来日期，已调整为当前时间 {datetime.now().strftime('%Y-%m-%d')}")
        end_ms = now_ms

    # 配置代理
    proxies = None
    if proxy:
        proxies = {"http": proxy, "https": proxy}
        print(f"使用代理: {proxy}")
    else:
        for key in ['HTTP_PROXY', 'HTTPS_PROXY', 'http_proxy', 'https_proxy']:
            os.environ[key] = ''
        print("已禁用系统代理，直接连接...")

    all_klines = []
    batch = 0
    retry_count = 0
    max_retries = 5

    print(f"开始下载 {symbol} {interval} K线数据...")
    print(f"时间范围: {start_date} ~ {end_date}")
    print(f"请求间隔: {sleep_interval}秒")
    total_minutes = (end_ms - start_ms) // 60000
    print(f"预计数据量: 约 {total_minutes:,} 条 (1分钟粒度)")
    print(f"预计批次: 约 {total_minutes // 1500 + 1} 次请求")
    print("-" * 50)

    while start_ms < end_ms:
        params = {
            "symbol": symbol,
            "interval": interval,
            "startTime": start_ms,
            "limit": 1500
        }

        try:
            r = requests.get(base_url, params=params, timeout=30, proxies=proxies)
            r.raise_for_status()
            data = r.json()
            retry_count = 0
        except requests.exceptions.ConnectionError as e:
            retry_count += 1
            if retry_count > max_retries:
                print(f"\n连接失败超过 {max_retries} 次，终止下载")
                print(f"断点时间: {datetime.fromtimestamp(start_ms / 1000)}")
                break
            wait_time = sleep_interval * (2 ** retry_count)
            print(f"连接失败，{wait_time:.1f}秒后第 {retry_count} 次重试...")
            time.sleep(wait_time)
            continue
        except Exception as e:
            print(f"请求失败: {e}")
            print(f"断点时间: {datetime.fromtimestamp(start_ms / 1000)}")
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
            progress = (data[-1][0] - int(datetime.strptime(start_date, "%Y-%m-%d").timestamp() * 1000)) / (end_ms - int(datetime.strptime(start_date, "%Y-%m-%d").timestamp() * 1000)) * 100
            print(f"  批次 {batch:>4}: {len(all_klines):>8,} 条 | {last_time.strftime('%Y-%m-%d %H:%M')} | {progress:.1f}%")

        # 请求间隔，降低频率
        time.sleep(sleep_interval)

    print("-" * 50)
    print(f"下载完成！共 {len(all_klines):,} 条记录")

    if all_klines:
        # 保存为 CSV
        columns = [
            'open_time', 'open', 'high', 'low', 'close', 'volume',
            'close_time', 'quote_volume', 'trades', 'taker_buy_base',
            'taker_buy_quote', 'ignore'
        ]

        output_path = os.path.join(output_dir, f"{symbol}_{interval}_{start_date.replace('-','')}_{end_date.replace('-','')}.csv")
        with open(output_path, 'w', newline='', encoding='utf-8') as f:
            writer = csv.writer(f)
            writer.writerow(columns)
            writer.writerows(all_klines)

        print(f"保存路径: {output_path}")

        first_time = datetime.fromtimestamp(all_klines[0][0] / 1000)
        last_time = datetime.fromtimestamp(all_klines[-1][0] / 1000)
        print(f"数据范围: {first_time} ~ {last_time}")
        print(f"文件大小: {os.path.getsize(output_path) / 1024 / 1024:.2f} MB")

    return all_klines

if __name__ == "__main__":
    # 下载 BTCUSDT 1分钟K线，2025-03-01 到 2026-03-02
    download_um_klines(
        symbol="BTCUSDT",
        interval="1m",
        start_date="2025-03-01",
        end_date="2026-03-02",
        output_dir="./data",
        sleep_interval=0.1  # 每次请求间隔0.1秒
    )
