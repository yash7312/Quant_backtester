"""One-time data fetch: downloads daily OHLCV for a fixed ETF universe and writes Parquet files."""

import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import pyarrow as pa
import pyarrow.parquet as pq
import yfinance as yf

UNIVERSE = ["SPY", "QQQ", "IWM", "TLT", "GLD"]
START = "2019-01-01"
END = "2024-12-31"
DATA_DIR = Path(__file__).resolve().parent.parent / "data" / "raw"

SCHEMA = pa.schema([
    ("date", pa.date32()),
    ("open", pa.float64()),
    ("high", pa.float64()),
    ("low", pa.float64()),
    ("close", pa.float64()),
    ("volume", pa.int64()),
    ("adjusted_close", pa.float64()),
])


def fetch_and_write(symbol: str) -> dict:
    ticker = yf.Ticker(symbol)
    df = ticker.history(start=START, end=END, auto_adjust=False)

    if df.empty:
        raise RuntimeError(f"No data returned for {symbol}")

    table = pa.table({
        "date": pa.array(df.index.date, type=pa.date32()),
        "open": pa.array(df["Open"].values, type=pa.float64()),
        "high": pa.array(df["High"].values, type=pa.float64()),
        "low": pa.array(df["Low"].values, type=pa.float64()),
        "close": pa.array(df["Close"].values, type=pa.float64()),
        "volume": pa.array(df["Volume"].values, type=pa.int64()),
        "adjusted_close": pa.array(df["Adj Close"].values, type=pa.float64()),
    }, schema=SCHEMA)

    table = table.replace_schema_metadata({
        "symbol": symbol,
        "source": "yfinance",
        "start": START,
        "end": END,
    })

    out_path = DATA_DIR / f"{symbol}.parquet"
    pq.write_table(table, out_path)

    file_hash = hashlib.sha256(out_path.read_bytes()).hexdigest()
    print(f"  {symbol}: {len(df)} rows, {df.index.min().date()} to {df.index.max().date()}, sha256={file_hash[:16]}")

    return {
        "symbol": symbol,
        "rows": len(df),
        "date_min": str(df.index.min().date()),
        "date_max": str(df.index.max().date()),
        "file": str(out_path),
        "sha256": file_hash,
    }


def main():
    DATA_DIR.mkdir(parents=True, exist_ok=True)
    print(f"Fetching {len(UNIVERSE)} symbols, {START} to {END}")

    manifest = {
        "fetched_at": datetime.now(timezone.utc).isoformat(),
        "source": "yfinance",
        "start": START,
        "end": END,
        "symbols": {},
    }

    for symbol in UNIVERSE:
        try:
            manifest["symbols"][symbol] = fetch_and_write(symbol)
        except Exception as e:
            print(f"  {symbol}: FAILED — {e}")
            manifest["symbols"][symbol] = {"error": str(e)}

    manifest_path = DATA_DIR.parent / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2))
    print(f"Manifest written to {manifest_path}")


if __name__ == "__main__":
    main()
