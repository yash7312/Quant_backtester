# Simulation Contract

This document defines the timing, ordering, execution, and accounting rules enforced by the qback simulation engine. Every invariant listed here must have a corresponding test. If the engine violates any rule, the run is invalid.

## 1. Bar Timestamp Convention

A daily bar's `date` field represents the **trading session date**. The bar is complete — all OHLCV fields are finalized — at market close (16:00 ET).

- `event_ts` = session close time (conceptually; represented as the session date in `Date.days_since_epoch`).
- `available_ts` = session close time. The strategy may observe a bar only after the close.

A bar with date `2023-03-15` represents trading activity on March 15, 2023. Its data becomes available to the strategy after the close on March 15.

## 2. Event Priority

When multiple events share the same timestamp, they are processed in this fixed order (lower number = fires first):

| Priority | EventType | Meaning |
|---|---|---|
| 0 | CorporateAction | Splits, dividends applied before any trading logic |
| 1 | MarketData | Bar arrives — prices become available for execution |
| 2 | FillEvent | Eligible orders fill against the current bar |
| 3 | MarkToMarket | Positions marked to close price, NAV computed |
| 4 | StrategySignal | Strategy observes completed bar, computes targets |
| 5 | OrderSubmission | New orders queued, eligible for the next bar |

Within the same `(timestamp, priority)`, a monotonically increasing `sequence` number breaks ties deterministically.

### Invariant

For any event `A` and event `B` on the same timestamp: if `A.priority < B.priority`, then `A` is processed before `B`. This is guaranteed by the `EventKey` comparator: `(timestamp, priority, sequence)` with `operator<=>`.

## 3. Order Eligibility (No Same-Bar Fills)

**Rule: a signal computed from bar *t* cannot produce a fill using bar *t*.**

The mechanism:

1. The strategy observes bar *t* at priority 4 (StrategySignal).
2. Resulting orders are submitted at priority 5 (OrderSubmission) on bar *t*.
3. Each submitted order has `eligible_date = submitted_date + 1 trading day`.
4. On bar *t+1*, at priority 2 (FillEvent), only orders where `eligible_date <= current_bar_date` are eligible.
5. Market orders fill at bar *t+1*'s open price. Limit orders check bar *t+1*'s price range.

### Invariant

`order.eligible_date > order.submitted_date` for every order. No order can fill on the bar that generated it.

## 4. Market Order Semantics

- Fill at the next eligible bar's **open** price.
- Cost adjustments applied on top: spread, commission, impact (see §8).
- If the eligible bar has zero volume, the order remains active for the next bar.

## 5. Limit Order Semantics

- A limit order specifies a limit price.
- On each eligible bar, check if the bar's price range `[low, high]` contains the limit price:
  - **Buy limit**: fills if `low <= limit_price`. Fill price = `min(open, limit_price)`.
  - **Sell limit**: fills if `high >= limit_price`. Fill price = `max(open, limit_price)`.
- If the bar's open already crosses the limit (buy: `open <= limit_price`, sell: `open >= limit_price`), fill at open.
- DAY orders that don't fill on their first eligible bar are cancelled. GTC orders remain active until filled, cancelled, or the simulation ends.

## 6. Mark-to-Market

All positions are marked to the **close price** of the most recent completed bar. This happens at priority 3 (MarkToMarket) on each bar's timestamp.

Mark price is used for:
- NAV computation: `NAV = cash + Σ(quantity × mark_price)`.
- Unrealized PnL: `unrealized_pnl = quantity × (mark_price - avg_cost)`.
- Risk metrics (exposure, concentration, leverage).

### Invariant

After every MarkToMarket event, `|NAV_computed - (cash + Σ(quantity × mark_price))| < $0.01`.

## 7. Missing / Stale Data

If a symbol has no bar for a trading day:
- **Marking**: carry forward the previous close as the mark price.
- **Execution**: skip that symbol for fill evaluation on the missing day. Orders remain active (if GTC) for the next available bar.
- **Logging**: emit a warning with the symbol and missing date.

No bar is fabricated. The absence is recorded, not interpolated.

## 8. Execution Cost Model

Costs are applied to the fill price and itemized separately in every `Fill` record.

### Reference model (testing baseline)
- Fill at eligible bar's open. Zero costs.

### Realistic model
- **Half-spread**: `spread_cost = 0.5 × spread_pct × price × quantity`. Default `spread_pct = 0.0001` (1 bps).
- **Commission**: `commission = commission_per_share × quantity`. Default `commission_per_share = $0.005`.
- **Market impact**: `impact = eta × p_ref × sigma_trailing × sqrt(q / ADV_trailing)`. Default `eta = 0.1`. `sigma_trailing` = 21-day realized volatility, `ADV_trailing` = 21-day average daily volume. Both computed from data available before the order.
- **Effective fill price**: `p_fill = p_open + side_sign × (spread/2 + impact/quantity)`.

### Invariant

For any fill, `total_cost() >= 0`. Costs cannot improve PnL: for identical trade sequences, adding positive costs must weakly decrease cumulative PnL.

## 9. Partial Fills and Volume Participation

If order quantity exceeds `participation_cap × bar_volume`:
- Fill `participation_cap × bar_volume` shares (rounded down to integer).
- Remaining quantity stays on the order as a partial fill.
- Default `participation_cap = 0.05` (5% of bar volume).

### Invariant

`fill.quantity <= participation_cap × bar.volume` for every fill (within integer rounding).

## 10. Corporate Actions

Adjusted close from yfinance encodes splits and dividends. Policy:

- **Signal computation**: use adjusted close for return calculations to avoid spurious signals at split/dividend dates.
- **Fill prices and accounting**: use raw (unadjusted) close and open for fills and cash accounting.
- **Detection**: any bar where `close != adjusted_close` indicates a corporate action in the historical data. Log the event.

The engine does not simulate dividend cash flows or split-adjusted share quantities in the current version. This is a known limitation.

## 11. Accounting

- **Precision**: IEEE 754 `double` throughout.
- **Reconciliation tolerance**: `|NAV - (cash + Σ(quantity × mark))| < $0.01` for known-answer tests using the reference execution model.
- **Average cost**: volume-weighted. When increasing a position, blend old and new cost. When reducing, realize PnL at `(fill_price - avg_cost) × closed_quantity` (long) or `(avg_cost - fill_price) × closed_quantity` (short).
- **Starting capital**: configurable, default $1,000,000.

## 12. Deterministic Replay

- All stochastic components use `std::mt19937_64` seeded from the run configuration's `seed` field.
- The RNG is drawn from a single sequence in a deterministic order (one draw per stochastic fill component).
- Given identical `(config, dataset, code revision, seed)`, the engine must produce byte-identical output files.

### Invariant

Two runs with the same config and data produce identical order, fill, and NAV outputs. Verified by hashing output files.

## 13. Summary of Testable Invariants

| # | Invariant | Test type |
|---|---|---|
| 1 | `order.eligible_date > order.submitted_date` | Unit |
| 2 | Strategy signal events fire after market data events on the same bar | Unit |
| 3 | Order submission events fire after strategy signal events on the same bar | Unit |
| 4 | `NAV = cash + Σ(quantity × mark_price)` within $0.01 | Integration |
| 5 | `fill.quantity <= participation_cap × bar.volume` | Unit |
| 6 | `fill.total_cost() >= 0` | Unit |
| 7 | Adding positive costs weakly decreases cumulative PnL | Integration |
| 8 | Perturbing future bars does not change earlier fills or PnL | Integration |
| 9 | Two identical runs produce identical outputs | E2E |
