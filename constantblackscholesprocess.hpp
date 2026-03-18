#ifndef imt2026_constant_black_scholes_process_hpp
#define imt2026_constant_black_scholes_process_hpp

#include <ql/exercise.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include <ql/stochasticprocess.hpp>

namespace QuantLib {

    class ConstantBlackScholesProcess final : public StochasticProcess1D {
      public:
        ConstantBlackScholesProcess(Real x0, Rate riskFreeRate, Rate dividendYield, Volatility volatility);

        Real x0() const override;
        Real drift(Time t, Real x) const override;
        Real diffusion(Time t, Real x) const override;

        Real apply(Real x0, Real dx) const override;
        Real expectation(Time t0, Real x0, Time dt) const override;
        Real stdDeviation(Time t0, Real x0, Time dt) const override;
        Real variance(Time t0, Real x0, Time dt) const override;
        Real evolve(Time t0, Real x0, Time dt, Real dw) const override;

      private:
        Real x0_;
        Rate r_;
        Rate q_;
        Volatility sigma_;
    };

    inline ext::shared_ptr<StochasticProcess1D> makeMonteCarloProcess(
        const ext::shared_ptr<GeneralizedBlackScholesProcess>& process,
        const ext::shared_ptr<Exercise>& exercise,
        Real strike,
        bool constantParameters) {

        QL_REQUIRE(process, "null process");
        QL_REQUIRE(exercise, "null exercise");

        if (!constantParameters) {
            return process;
        }

        const Time maturity = process->time(exercise->lastDate());
        QL_REQUIRE(maturity >= 0.0, "negative maturity");

        const Rate r =
            process->riskFreeRate()->zeroRate(maturity, Continuous, NoFrequency, true).rate();
        const Rate q =
            process->dividendYield()->zeroRate(maturity, Continuous, NoFrequency, true).rate();
        const Volatility sigma = process->blackVolatility()->blackVol(maturity, strike, true);

        return ext::make_shared<ConstantBlackScholesProcess>(process->x0(), r, q, sigma);
    }

} // namespace QuantLib

#endif
