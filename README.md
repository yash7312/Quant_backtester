# qback

A deterministic, auditable backtesting engine in C++ for US ETFs. Simulates the full path from market data through signals, orders, fills, and portfolio accounting under explicit execution assumptions — spread, fees, slippage, market impact, partial fills, and risk limits.

The goal is not a profitable strategy. It is a trustworthy simulation system that proves invariants, measures performance, and shows exactly how execution costs and risk constraints change the result.

## Current Status

**Layer 1 complete: domain model, simulation contract, and test framework.**

The system can fetch daily OHLCV data, store it as Parquet, read it into C++ domain structs, and has a full domain vocabulary (Order, Fill, Position, Event) with 28 passing unit tests validating simulation contract invariants. No simulation loop, strategy, or execution logic exists yet.

## Implemented Features

- [x] **Data fetch** — Python script downloads daily OHLCV from yfinance, writes typed Parquet files with schema metadata and SHA-256 hashes
- [x] **Parquet reader** — C++ reads Parquet via Arrow C++ API with column type-checking, schema metadata extraction, and chunk consolidation
- [x] **Domain types** — `Date`, `Bar`, `Instrument`, `Order`, `Fill`, `Position`, `Event`, `EventKey` structs and all supporting enums (`Side`, `OrderType`, `OrderStatus`, `TIF`, `EventType`)
- [x] **Simulation contract** — documented in `docs/simulation_contract.md`: timing rules, event priority, fill eligibility, cost model, accounting conventions, and 9 testable invariants
- [x] **Test framework** — Google Test via CMake FetchContent; 28 unit tests covering event ordering, position accounting, order lifecycle, fill cost invariants
- [x] **Build system** — CMake 3.20+, C++20, auto-detects Arrow/Parquet from pyarrow, static library shared between binary and tests
- [x] Event kernel (clock, event queue, priority ordering)
- [x] Portfolio & accounting ledger
- [x] Execution model & OMS
- [x] Feature pipeline
- [x] Strategy interface & implementations
- [x] Risk controls
- [x] Experiment runner & config
- [x] Analytics & reporting
- [] Research protocol (walk-forward, cost sensitivity)
- [] Performance benchmarks

## Architecture

### Current modules

```
src/qback/
├── main.cpp                  # CLI entry point — reads Parquet, prints summary
├── domain/
│   ├── bar.h / bar.cpp       # Date, Bar, Instrument
│   ├── types.h               # Enums: Side, OrderType, OrderStatus, TIF, EventType + to_string()
│   ├── event.h               # EventKey, Event, payload variants, factory functions
│   ├── order.h               # Order struct with eligibility and state tracking
│   ├── fill.h                # Fill struct with itemized costs and cash delta
│   └── position.h            # Position struct with avg cost tracking and PnL realization
└── data/
    ├── parquet_reader.h       # read_parquet(), read_all_parquet()
    └── parquet_reader.cpp     # Arrow C++ Parquet deserialization

tests/unit/
├── test_types.cpp             # Enum values, to_string(), event priority ordering
├── test_event.cpp             # EventKey comparison, sorting, same-bar ordering invariants
├── test_position.cpp          # Long/short accounting, avg cost blending, PnL realization
└── test_order.cpp             # Eligibility, remaining quantity, terminal states, fill costs

docs/
└── simulation_contract.md     # All timing, ordering, and accounting rules
```

### Planned modules (not yet implemented)

```
├── engine/                   # Event kernel, clock, priority queue
├── features/                 # Rolling computations (returns, volatility, momentum)
├── strategy/                 # IStrategy interface, buy-and-hold, momentum
├── portfolio/                # Cash, positions, marks, PnL, NAV ledger
├── risk/                     # Pre-trade limits, post-trade snapshots
├── execution/                # OMS, order state machine, fill models, cost models
├── analytics/                # Sharpe, drawdown, cost attribution
└── experiments/              # YAML config, run manifests, artifact writing
```

### Planned data flow

```
[Python: yfinance] → Parquet files → [C++ Parquet reader] → Bar vectors
    → Feature pipeline → Strategy → Target weights
    → Risk checks → Orders → Execution model → Fills
    → Portfolio ledger → Analytics → CSV/JSON outputs
    → [Python: matplotlib] → HTML report
```

Only the first two stages (Python fetch → C++ Parquet reader → Bar vectors) produce runnable output. Domain types for all subsequent stages are defined but not yet wired into a simulation loop.

## Data Flow

### Implemented

1. **Fetch** (`scripts/fetch_data.py`): yfinance downloads daily OHLCV for SPY, QQQ, IWM, TLT, GLD (2019-01-01 to 2024-12-31). Writes one Parquet file per symbol into `data/raw/`. Each file carries schema metadata (symbol, source, date range). A `data/manifest.json` records row counts, date ranges, and SHA-256 hashes.

2. **Read** (`src/qback/data/parquet_reader.cpp`): Opens each `.parquet` file via `arrow::io::ReadableFile` → `parquet::arrow::OpenFile` → `ReadTable()`. Combines chunks, type-checks all 7 columns (`date32`, `float64` × 5, `int64`), extracts the symbol from Parquet schema metadata (falls back to filename stem). Deserializes into `qback::domain::Instrument` with a pre-reserved `vector<Bar>`.

3. **Display** (`src/qback/main.cpp`): Iterates loaded instruments, computes per-symbol min low / max high, prints a summary table. Accepts an optional data directory argument (defaults to `data/raw`).

### Not yet implemented

Event kernel, feature computation, strategy, execution, portfolio, risk, analytics, and experiment orchestration.

## Module Design

### `qback::domain` — Value types and enums

**`types.h`** — All enums used across the system:

| Enum | Values | Purpose |
|---|---|---|
| `Side` | `Buy`, `Sell` | Trade direction |
| `OrderType` | `Market`, `Limit` | How the order matches |
| `OrderStatus` | `New`, `Submitted`, `Active`, `Filled`, `PartialFill`, `Cancelled`, `Rejected` | Order lifecycle states |
| `TIF` | `Day`, `GTC` | Time-in-force |
| `EventType` | `CorporateAction(0)`, `MarketData(1)`, `FillEvent(2)`, `MarkToMarket(3)`, `StrategySignal(4)`, `OrderSubmission(5)` | Event priority — lower number fires first |

Each enum has a `to_string()` function. `side_sign(Side)` returns +1 for Buy, -1 for Sell.

**`event.h`** — Event system:

- `EventKey{timestamp, priority, sequence}` — ordered by `(timestamp, priority, sequence)` via `operator<=>`. This triple key guarantees fully deterministic event ordering.
- `Event{key, type, payload}` — carries a `std::variant` payload (`MarketDataPayload`, `StrategySignalPayload`, `OrderSubmissionPayload`, `FillPayload`, `MarkToMarketPayload`, `CorporateActionPayload`).
- Factory functions: `make_market_data_event()`, `make_strategy_signal_event()`, `make_order_submission_event()`.

**`order.h`** — Order struct:

```cpp
struct Order {
    uint64_t order_id;
    std::string symbol;
    Side side;
    OrderType type;
    int64_t quantity;                    // always positive
    std::optional<double> limit_price;   // set for Limit orders only
    TIF tif;
    OrderStatus status;
    Date submitted_date;
    Date eligible_date;                  // = submitted_date + 1 (contract §3)
    int64_t filled_quantity = 0;
    int64_t remaining() const;
    bool is_terminal() const;            // Filled, Cancelled, or Rejected
};
```

**`fill.h`** — Fill with itemized costs:

```cpp
struct Fill {
    uint64_t fill_id, order_id;
    std::string symbol;
    Side side;
    int64_t quantity;
    double price;                        // raw execution price
    double commission, spread_cost, impact_cost;  // itemized
    double total_cost() const;           // sum of all cost components
    double cash_delta() const;           // net cash impact (negative for buys)
    Date fill_date;
};
```

**`position.h`** — Position with accounting:

```cpp
struct Position {
    std::string symbol;
    int64_t quantity = 0;         // positive=long, negative=short
    double avg_cost = 0.0;        // volume-weighted average entry price
    double mark_price = 0.0;
    double realized_pnl = 0.0;    // cumulative
    double market_value() const;  // quantity × mark_price
    double unrealized_pnl() const; // quantity × (mark - avg_cost)
    void apply(int64_t signed_qty, double fill_price);
};
```

`Position::apply()` handles both increasing and reducing/flipping positions:
- **Increasing**: blends average cost via `(old_value + new_value) / new_quantity`.
- **Reducing**: realizes PnL at `(fill_price - avg_cost) × closed_quantity` (long) or `(avg_cost - fill_price) × closed_quantity` (short). If the position flips, the remaining shares carry the new fill price as their cost basis.

### `qback::data` — Parquet I/O

- `read_parquet(path) → Instrument` — type-checked single-file reader.
- `read_all_parquet(directory) → vector<Instrument>` — directory scan, sorted by symbol.

## Parquet Schema

Written by `scripts/fetch_data.py`, read by `src/qback/data/parquet_reader.cpp`:

| Column | Arrow Type | Description |
|---|---|---|
| `date` | `date32` | Trading session date |
| `open` | `float64` | Session open price |
| `high` | `float64` | Session high price |
| `low` | `float64` | Session low price |
| `close` | `float64` | Session close price (unadjusted) |
| `volume` | `int64` | Trading volume in shares |
| `adjusted_close` | `float64` | Split/dividend-adjusted close |

Schema metadata keys: `symbol`, `source`, `start`, `end`.

## Simulation Contract

Fully documented in [`docs/simulation_contract.md`](docs/simulation_contract.md). Key rules:

| Rule | Summary |
|---|---|
| Bar timestamp | Trading session date; bar finalized at market close |
| No same-bar fills | Signal from bar *t* cannot fill using bar *t*; `order.eligible_date > order.submitted_date` |
| Event priority | CorporateAction(0) → MarketData(1) → Fill(2) → MarkToMarket(3) → StrategySignal(4) → OrderSubmission(5) |
| Market orders | Fill at next eligible bar's open price + costs |
| Limit orders | Fill if bar [low,high] crosses limit; fill price = min/max(open, limit) |
| Mark-to-market | Close price of most recent completed bar |
| NAV invariant | `NAV = cash + Σ(quantity × mark_price)` within $0.01 |
| Costs | Non-negative; adding costs weakly decreases PnL |
| Partial fills | Capped at `participation_cap × bar_volume` (default 5%) |
| Missing data | Carry forward previous close for marks; skip execution |
| Corporate actions | Adjusted close for signals; raw close for fills and accounting |
| Determinism | `std::mt19937_64` with configured seed; identical inputs → identical outputs |

9 testable invariants are enumerated in the contract; 5 are already tested in the unit suite (event ordering, eligibility, cost non-negativity). The remaining 4 require the simulation loop (NAV reconciliation, participation cap enforcement, cost-PnL monotonicity, deterministic replay).

## Strategies

No strategies are implemented.

Planned: buy-and-hold baseline, volatility-scaled cross-sectional momentum (12-month return, skip 1 month, vol-normalized, long top 2 / short bottom 2 of the 5-ETF universe, weekly rebalance).

## Build & Run

### Prerequisites

- g++ 13+ (C++20)
- Python 3.12 with pyarrow and yfinance (provides both the data-fetch runtime and the Arrow/Parquet C++ headers + shared libraries)
- CMake 3.20+ (installed via `pip install cmake` in the venv)

### Setup

```bash
# Activate the Python venv (required — CMake locates Arrow libs through pyarrow)
source /home/yash7312/Desktop/2026_autumn/.venv/bin/activate

# Fetch market data (one-time, requires internet)
python scripts/fetch_data.py

# Build
cmake -B build
cmake --build build

# Run data verification
./build/qback

# Run tests
./build/qback_tests
```

### Optional: custom data directory

```bash
./build/qback /path/to/parquet/directory
```

## Testing

28 unit tests via Google Test (fetched automatically by CMake via FetchContent). Run with:

```bash
./build/qback_tests
```

### Test coverage by domain area

| Test suite | Tests | What it validates |
|---|---|---|
| `Types` | 5 | Enum `to_string()`, `side_sign()`, `EventType` priority ordering (contract §2) |
| `EventKey` | 4 | Timestamp precedence, priority ordering, sequence tie-breaking, sort correctness |
| `Event` | 2 | Signal fires before order on same bar, market data fires before signal (contract §2, §3) |
| `Position` | 11 | Long/short entry, avg cost blending, profit/loss realization, close/reopen, unrealized PnL |
| `Order` | 3 | `eligible_date > submitted_date` (contract §3), `remaining()`, terminal state detection |
| `Fill` | 3 | Cost non-negativity (contract §8), buy/sell cash delta correctness |

### Invariants tested

- [x] `EventType` numeric values enforce correct priority order (§2)
- [x] `EventKey` comparison produces correct chronological + priority ordering (§2)
- [x] Signal events fire before order events on the same bar (§2, §3)
- [x] `order.eligible_date > order.submitted_date` (§3)
- [x] `fill.total_cost() >= 0` (§8)
- [ ] `NAV = cash + Σ(quantity × mark)` (needs portfolio module)
- [ ] `fill.quantity <= participation_cap × bar.volume` (needs execution module)
- [ ] Adding costs weakly decreases PnL (needs end-to-end run)
- [ ] Identical inputs → identical outputs (needs experiment runner)

## Example Output

Actual output from `./build/qback` on the fetched dataset:

```
Reading parquet files from: "/home/yash7312/Desktop/2026_autumn/Quant_projs/Q1/data/raw"

Symbol     Rows    First date     Last date         Low        High
-----------------------------------------------------------------
GLD        1509    2019-01-02    2024-12-30      119.54      257.71
IWM        1509    2019-01-02    2024-12-30       95.69      244.98
QQQ        1509    2019-01-02    2024-12-30      149.49      539.15
SPY        1509    2019-01-02    2024-12-30      218.26      609.07
TLT        1509    2019-01-02    2024-12-30       82.42      179.70

Data access verified: 5 instruments loaded.
```

Actual output from `./build/qback_tests`:

```
[==========] Running 28 tests from 6 test suites.
[  PASSED  ] 28 tests.
```

## Performance

No benchmarks measured yet. Benchmarking is planned for Layer 10 using Google Benchmark, `perf`, and `valgrind --tool=callgrind`.

## Research / Validation

Not yet implemented. Planned: chronological train (2019–2021) / validation (2022–mid 2023) / test (mid 2023–2024) split, walk-forward evaluation, cost-model ablation across 4 execution-model rungs, benchmark comparison vs SPY and equal-weight buy-and-hold.

## Reproducibility

- **Data fetch**: `data/manifest.json` records fetch timestamp, source, date ranges, and SHA-256 hash per Parquet file. The manifest is regenerated on each fetch run.
- **Build**: CMake with `CMAKE_EXPORT_COMPILE_COMMANDS ON`. Compiler: g++ 13.3.0, standard: C++20. Google Test v1.15.2 fetched at configure time.
- **Determinism**: not yet enforced (no simulation loop exists). Planned: seeded `std::mt19937_64`, run manifests with config hash + data hash + git SHA + seed.

Data files (`data/raw/*.parquet`) are gitignored. Regenerate with `python scripts/fetch_data.py`.

## Repository Layout

```
Q1/
├── CMakeLists.txt              # Build system — Arrow from pyarrow, Google Test via FetchContent
├── README.md                   # This file
├── project_plan.md             # Full project plan with resolved design decisions
├── .gitignore                  # Excludes build/, data/raw/, manifest, pycache
├── docs/
│   └── simulation_contract.md  # Timing, ordering, and accounting rules
├── scripts/
│   └── fetch_data.py           # One-time data download (Python)
├── data/
│   ├── raw/                    # Parquet files (gitignored)
│   └── manifest.json           # Fetch metadata (gitignored)
├── src/
│   └── qback/
│       ├── main.cpp            # Entry point
│       ├── domain/
│       │   ├── bar.h / bar.cpp # Date, Bar, Instrument
│       │   ├── types.h         # Side, OrderType, OrderStatus, TIF, EventType
│       │   ├── event.h         # EventKey, Event, payloads, factory functions
│       │   ├── order.h         # Order with eligibility tracking
│       │   ├── fill.h          # Fill with itemized costs
│       │   └── position.h      # Position with avg cost and PnL
│       └── data/
│           ├── parquet_reader.h
│           └── parquet_reader.cpp
└── tests/
    └── unit/
        ├── test_types.cpp
        ├── test_event.cpp
        ├── test_position.cpp
        └── test_order.cpp
```

## Design Decisions

| Decision | Choice | Rationale |
|---|---|---|
| Language | C++20 for engine, Python only for data fetch + report rendering | Systems-level performance demonstration; Arrow C++ gives zero-copy Parquet reads |
| Data source | yfinance (daily OHLCV) | Free, no API key, sufficient for daily bar backtesting |
| Storage format | Parquet (one file per symbol) | Columnar, typed, self-describing schema with metadata; Arrow C++ reads directly |
| Arrow/Parquet C++ linkage | pyarrow's bundled headers and `.so` files | Avoids system-level package install; pyarrow 25 ships complete C++ Arrow 25 |
| Universe | SPY, QQQ, IWM, TLT, GLD | 5 liquid ETFs across equities, bonds, commodities; all existed before 2019 |
| Signal granularity | Daily (not intraday) | Reduces data cost and complexity; minute-bar execution is a future extension |
| Test framework | Google Test v1.15.2 via FetchContent | No system install needed; widely understood; integrates with CTest |
| Event ordering | `EventKey{timestamp, priority, sequence}` with `operator<=>` | Three-field key makes ordering fully deterministic without ambiguity |
| Position accounting | FIFO-like avg cost blending | Simple, deterministic, matches common institutional accounting |

## Unresolved Decisions

- **yaml-cpp integration**: planned for Layer 7 (experiment runner) but not yet added to CMake
- **spdlog vs custom logger**: deferred until structured logging is needed
- **Eigen dependency**: only needed if portfolio optimization requires matrix operations; deferred

## Future Work

Ordered by implementation layer:

1. ~~**Layer 1**: Domain model, simulation contract, test framework~~ **(done)**
2. **Layer 2**: Deterministic event kernel (priority queue, clock, bar feed)
3. **Layer 3**: Portfolio & accounting ledger (cash, positions, marks, PnL, NAV invariant)
4. **Layer 4**: Execution model & OMS (order state machine, reference + realistic fill models, cost itemization)
5. **Layer 5**: Feature pipeline & strategies (rolling computations, buy-and-hold, momentum)
6. **Layer 6**: Risk controls (position limits, exposure limits, reason-coded rejections)
7. **Layer 7**: Experiment runner (YAML config, full pipeline wiring, known-answer test, determinism proof)
8. **Layer 8**: Analytics & reporting (Sharpe, drawdown, cost attribution, HTML report via Python)
9. **Layer 9**: Research protocol (train/val/test split, walk-forward, cost sensitivity, benchmark comparison)
10. **Layer 10**: Adversarial test suite & performance benchmarks (edge-case scenarios, measured throughput, profiling)
