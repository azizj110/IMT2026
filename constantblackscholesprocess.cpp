#include "constantblackscholesprocess.hpp"
#include <cmath>

namespace QuantLib {

    ConstantBlackScholesProcess::ConstantBlackScholesProcess(
        Real x0, Rate riskFreeRate, Rate dividendYield, Volatility volatility)
    : x0_(x0), r_(riskFreeRate), q_(dividendYield), sigma_(volatility) {
        QL_REQUIRE(x0_ > 0.0, "x0 must be positive");
        QL_REQUIRE(sigma_ >= 0.0, "volatility must be non-negative");
    }

    Real ConstantBlackScholesProcess::x0() const {
        return x0_;
    }

    Real ConstantBlackScholesProcess::drift(Time, Real x) const {
        return (r_ - q_) * x;
    }

    Real ConstantBlackScholesProcess::diffusion(Time, Real x) const {
        return sigma_ * x;
    }

    Real ConstantBlackScholesProcess::apply(Real x0, Real dx) const {
        return x0 * std::exp(dx);
    }

    Real ConstantBlackScholesProcess::expectation(Time, Real x0, Time dt) const {
        return x0 * std::exp((r_ - q_) * dt);
    }

    Real ConstantBlackScholesProcess::variance(Time, Real x0, Time dt) const {
        const Real varLog = sigma_ * sigma_ * dt;
        const Real mean = expectation(0.0, x0, dt);
        return mean * mean * (std::exp(varLog) - 1.0);
    }

    Real ConstantBlackScholesProcess::stdDeviation(Time t0, Real x0, Time dt) const {
        return std::sqrt(variance(t0, x0, dt));
    }

    Real ConstantBlackScholesProcess::evolve(Time, Real x0, Time dt, Real dw) const {
        const Real mu = (r_ - q_ - 0.5 * sigma_ * sigma_) * dt;
        const Real vol = sigma_ * std::sqrt(dt);
        return x0 * std::exp(mu + vol * dw);
    }

} // namespace QuantLib
