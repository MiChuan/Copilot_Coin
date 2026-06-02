# merge_klines.py
import os
import zipfile
import pandas as pd
from glob import glob

def merge_klines(data_dir="./data", symbol="BTCUSDT", interval="1h", output="./data/BTCUSDT_1h_merged.csv"):
    """
    解压并合并所有月度K线数据为单个CSV文件
    """
    # 查找所有zip文件
    pattern = os.path.join(data_dir, "um/klines", symbol, f"{symbol}-{interval}-*.zip")
    zip_files = sorted(glob(pattern))

    if not zip_files:
        print(f"未找到数据文件: {pattern}")
        print("请先运行 download_um_data.py 下载数据")
        return

    print(f"找到 {len(zip_files)} 个数据文件")

    all_dfs = []
    columns = [
        'open_time', 'open', 'high', 'low', 'close', 'volume',
        'close_time', 'quote_volume', 'trades', 'taker_buy_base',
        'taker_buy_quote', 'ignore'
    ]

    for zip_path in zip_files:
        print(f"处理: {os.path.basename(zip_path)}")
        with zipfile.ZipFile(zip_path, 'r') as z:
            # zip内文件名格式: BTCUSDT-1h-2025-03.csv
            csv_name = z.namelist()[0]
            with z.open(csv_name) as f:
                df = pd.read_csv(f, header=None, names=columns)
                all_dfs.append(df)

    # 合并并排序
    merged = pd.concat(all_dfs, ignore_index=True)
    merged = merged.sort_values('open_time').reset_index(drop=True)

    # 转换时间戳为可读日期（可选）
    merged['open_time_readable'] = pd.to_datetime(merged['open_time'], unit='ms')
    merged['close_time_readable'] = pd.to_datetime(merged['close_time'], unit='ms')

    # 保存
    merged.to_csv(output, index=False)
    print(f"\n合并完成！")
    print(f"总记录数: {len(merged)}")
    print(f"时间范围: {merged['open_time_readable'].iloc[0]} ~ {merged['open_time_readable'].iloc[-1]}")
    print(f"保存路径: {output}")

    return merged

if __name__ == "__main__":
    merge_klines()
