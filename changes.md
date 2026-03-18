# Changes introduced

## Goal
Implement optional **constant-parameter Monte Carlo simulation** for the three custom engines (European, Asian, Lookback), while keeping `main.cpp` unchanged.

## What was added

### 1) New constant process
- Added:
  - `constantblackscholesprocess.hpp`
  - `constantblackscholesprocess.cpp`
- Introduced class:
  - `ConstantBlackScholesProcess : StochasticProcess1D`
- Parameters stored once:
  - spot `x0`
  - risk-free rate `r`
  - dividend yield `q`
  - volatility `sigma`

### 2) Shared helper to avoid duplication
- Added helper in `constantblackscholesprocess.hpp`:
  - `makeMonteCarloProcess(process, exercise, strike, constantParameters)`
- Behavior:
  - If `constantParameters == false`: returns original process.
  - If `true`: extracts `(r, q, sigma)` at exercise maturity and strike, then returns a `ConstantBlackScholesProcess`.

This keeps extraction logic in one place for all engines.

### 3) Engine updates
- Updated custom engines:
  - `mceuropeanengine.hpp`
  - `mc_discr_arith_av_strike.hpp`
  - `mclookbackengine.hpp`
- Added:
  - `withConstantParameters(bool = true)` in each builder.
  - Internal storage of this flag.
  - `pathGenerator()` now uses `makeMonteCarloProcess(...)`.

## Why these changes

- `GeneralizedBlackScholesProcess` repeatedly queries term structures during simulation.
- Replacing it (optionally) with a constant process reduces repeated curve/vol lookups.
- This should improve runtime, with small pricing differences expected when curves/surface are non-flat.

## Additional fixes for consistency

- Replaced `boost::shared_ptr` usage with `ext::shared_ptr` in `mceuropeanengine.hpp`.
- Replaced undefined `LookbackFixedPathPricer_2` with QuantLib `LookbackFixedPathPricer` in `mclookbackengine.hpp`.

## Expected outcome

- Non-constant mode should match original QuantLib MC engine results.
- Constant mode may produce slightly different NPVs but typically faster execution.
- Impact can differ by product type (European, Asian, Lookback) due to payoff path dependence.