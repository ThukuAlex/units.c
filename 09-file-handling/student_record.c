"""
XAUUSD Gold Signal Bot - Exness MT5 Backend Server
===================================================
Connects directly to your Exness MT5 terminal to fetch
live gold prices and calculate RSI, EMA, Fibonacci signals.

Requirements:
    pip install flask flask-cors MetaTrader5 pandas numpy

IMPORTANT:
    MT5 must be open and logged into your Exness account
    while this server is running!

Usage:
    python server_exness.py
"""

from flask import Flask, jsonify
from flask_cors import CORS
import MetaTrader5 as mt5
import pandas as pd
import numpy as np
from datetime import datetime
import traceback

app = Flask(__files__)
CORS(app)

# ── CONFIGURATION ─────────────────────────────────────────────────────────
MT5_LOGIN    = 134119673           # Your Exness MT5 account number (e.g. 12345678)
MT5_PASSWORD = _Freezysea4          # Your Exness MT5 password
MT5_SERVER  Exness-MT5Real9          # Your Exness server (e.g. "Exness-MT5Real8")

SYMBOL       = "XAUUSDm"   # Gold on Exness (try "XAUUSD" if this doesn't work)
TIMEFRAME    = mt5.TIMEFRAME_H1   # H1 candles — change to TIMEFRAME_M15 for 15min
CANDLE_COUNT = 250


# ── MT5 CONNECTION ────────────────────────────────────────────────────────
def connect_mt5():
    if not mt5.initialize():
        raise Exception(f"MT5 initialize failed: {mt5.last_error()}")

    if MT5_LOGIN and MT5_PASSWORD and MT5_SERVER:
        authorized = mt5.login(MT5_LOGIN, password=MT5_PASSWORD, server=MT5_SERVER)
        if not authorized:
            raise Exception(f"MT5 login failed: {mt5.last_error()}")

    info = mt5.account_info()
    if info is None:
        raise Exception("MT5 not logged in. Please open MT5 and log into Exness first.")
    return info


def fetch_candles():
    connect_mt5()
    rates = mt5.copy_rates_from_pos(SYMBOL, TIMEFRAME, 0, CANDLE_COUNT)
    mt5.shutdown()

    if rates is None or len(rates) == 0:
        raise Exception(f"No data for {SYMBOL}. Make sure MT5 is open and logged in.")

    df = pd.DataFrame(rates)
    df["time"] = pd.to_datetime(df["time"], unit="s")
    df.rename(columns={"open":"open","high":"high","low":"low","close":"close","tick_volume":"volume"}, inplace=True)
    return df


# ── INDICATORS ────────────────────────────────────────────────────────────
def calc_rsi(series, period=14):
    delta    = series.diff()
    gain     = delta.clip(lower=0)
    loss     = -delta.clip(upper=0)
    avg_gain = gain.ewm(com=period - 1, min_periods=period).mean()
    avg_loss = loss.ewm(com=period - 1, min_periods=period).mean()
    rs  = avg_gain / avg_loss
    return 100 - (100 / (1 + rs))


def calc_ema(series, period):
    return series.ewm(span=period, adjust=False).mean()


def detect_candle(row):
    body  = abs(row["close"] - row["open"])
    total = row["high"] - row["low"]
    if total == 0:
        return {"name": "None", "bullish": None}

    upper_wick = row["high"] - max(row["close"], row["open"])
    lower_wick = min(row["close"], row["open"]) - row["low"]
    body_pct   = body / total

    if body_pct < 0.35:
        if lower_wick > body * 2:
            return {"name": "Pin Bar", "bullish": True}
        if upper_wick > body * 2:
            return {"name": "Pin Bar", "bullish": False}

    if body_pct > 0.6:
        return {"name": "Engulfing", "bullish": row["close"] > row["open"]}

    if body_pct < 0.1:
        return {"name": "Doji", "bullish": None}

    return {"name": "None", "bullish": None}


def find_fib_level(price, recent_high, recent_low):
    if recent_high == recent_low:
        return {"level": "None", "zone": "none"}

    swing  = recent_high - recent_low
    levels = {
        "23.6%": recent_high - 0.236 * swing,
        "38.2%": recent_high - 0.382 * swing,
        "50.0%": recent_high - 0.500 * swing,
        "61.8%": recent_high - 0.618 * swing,
        "78.6%": recent_high - 0.786 * swing,
    }
    tolerance = swing * 0.02

    for label, level in levels.items():
        if abs(price - level) <= tolerance:
            zone = "golden" if label == "61.8%" else "strong" if label == "50.0%" else "moderate"
            return {"level": label, "zone": zone, "price": round(level, 2)}

    return {"level": "None", "zone": "none"}


# ── SIGNAL ANALYSIS ───────────────────────────────────────────────────────
def analyse_signal(df):
    close = df["close"]
    price  = close.iloc[-1]
    ema50  = calc_ema(close, 50).iloc[-1]
    ema200 = calc_ema(close, 200).iloc[-1]
    rsi    = calc_rsi(close).iloc[-1]
    candle = detect_candle(df.iloc[-1])

    recent_high = df["high"].tail(50).max()
    recent_low  = df["low"].tail(50).min()
    fib         = find_fib_level(price, recent_high, recent_low)

    buy_score = sell_score = 0
    conditions = []

    # 1. EMA 200 Trend
    above200 = price > ema200
    if above200:
        buy_score += 1
        conditions.append({"text": "Price above 200 EMA (Uptrend)", "status": "pass", "val": "BULL ↑"})
    else:
        sell_score += 1
        conditions.append({"text": "Price below 200 EMA (Downtrend)", "status": "fail", "val": "BEAR ↓"})

    # 2. EMA Cross
    if ema50 > ema200:
        buy_score += 1
        conditions.append({"text": "50 EMA above 200 EMA (Golden Cross)", "status": "pass", "val": "GOLDEN ✦"})
    else:
        sell_score += 1
        conditions.append({"text": "50 EMA below 200 EMA (Death Cross)", "status": "fail", "val": "DEATH ✦"})

    # 3. RSI
    if rsi < 30:
        buy_score += 1.5
        conditions.append({"text": f"RSI oversold at {rsi:.1f} — Buy zone", "status": "pass", "val": f"RSI {rsi:.1f}"})
    elif rsi > 70:
        sell_score += 1.5
        conditions.append({"text": f"RSI overbought at {rsi:.1f} — Sell zone", "status": "fail", "val": f"RSI {rsi:.1f}"})
    else:
        conditions.append({"text": f"RSI neutral at {rsi:.1f}", "status": "warn", "val": f"RSI {rsi:.1f}"})

    # 4. Fibonacci
    if fib["zone"] != "none":
        score = 2 if fib["level"] == "61.8%" else 1.5 if fib["level"] == "50.0%" else 1
        if above200: buy_score += score
        else: sell_score += score
        fib_status = "pass" if fib["level"] in ["61.8%", "50.0%"] else "warn"
        conditions.append({"text": f"Price at {fib['level']} Fibonacci level", "status": fib_status, "val": f"FIB {fib['level']}"})
    else:
        conditions.append({"text": "No key Fibonacci level near price", "status": "warn", "val": "NO FIB"})

    # 5. Candle
    if candle["name"] != "None":
        if candle["bullish"] is True:
            buy_score += 1.5
            conditions.append({"text": f"Bullish {candle['name']} confirmed", "status": "pass", "val": f"BULL {candle['name'].upper()}"})
        elif candle["bullish"] is False:
            sell_score += 1.5
            conditions.append({"text": f"Bearish {candle['name']} confirmed", "status": "fail", "val": f"BEAR {candle['name'].upper()}"})
        else:
            conditions.append({"text": "Doji — waiting for confirmation", "status": "warn", "val": "DOJI"})
    else:
        conditions.append({"text": "No clear candle signal", "status": "warn", "val": "NONE"})

    # Final signal
    if buy_score >= 4:
        signal, strength = "BUY", min(5, round(buy_score))
    elif sell_score >= 4:
        signal, strength = "SELL", min(5, round(sell_score))
    else:
        signal, strength = "WAIT", 2

    sl_dist = price * 0.005
    tp_dist = sl_dist * 2
    direction = 1 if signal == "BUY" else -1

    return {
        "signal":      signal,
        "strength":    strength,
        "price":       round(price, 2),
        "ema50":       round(ema50, 2),
        "ema200":      round(ema200, 2),
        "rsi":         round(rsi, 2),
        "fib":         fib,
        "candle":      candle,
        "conditions":  conditions,
        "entry":       round(price, 2),
        "stop_loss":   round(price - direction * sl_dist, 2),
        "take_profit": round(price + direction * tp_dist, 2),
        "buy_score":   round(buy_score, 1),
        "sell_score":  round(sell_score, 1),
        "symbol":      SYMBOL,
        "timeframe":   "H1",
        "timestamp":   datetime.utcnow().strftime("%Y-%m-%d %H:%M UTC"),
    }


# ── ROUTES ────────────────────────────────────────────────────────────────
@app.route("/signal")
def signal():
    try:
        df     = fetch_candles()
        result = analyse_signal(df)
        return jsonify({"success": True, "data": result})
    except Exception as e:
        return jsonify({"success": False, "error": str(e), "trace": traceback.format_exc()}), 500


@app.route("/health")
def health():
    try:
        connect_mt5()
        info = mt5.account_info()
        mt5.shutdown()
        return jsonify({
            "status":   "connected",
            "broker":   info.company,
            "account":  info.login,
            "balance":  info.balance,
            "currency": info.currency,
            "symbol":   SYMBOL,
        })
    except Exception as e:
        return jsonify({"status": "error", "error": str(e)}), 500


# ── START ─────────────────────────────────────────────────────────────────
if __name__ == "__main__":
    print("=" * 55)
    print("  XAUUSD Signal Bot — Exness MT5 Backend")
    print("=" * 55)
    print(f"  Symbol    : {SYMBOL}")
    print(f"  Timeframe : H1")
    print()
    print("  ⚠️  Make sure MT5 is OPEN and logged into Exness!")
    print()
    print("  Server running at: http://localhost:5000")
    print("  Then open: Gold_Signal_Bot_Live.html")
    print("=" * 55)
    app.run(port=5000, debug=False)
