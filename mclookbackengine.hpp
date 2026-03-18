/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2020 Lew Wei Hao

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <https://www.quantlib.org/license.shtml>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

/*! \file mclookbackengine.hpp
    \brief Monte Carlo lookback fixed engines
*/

#ifndef mc_lookback_engines_hpp
#define mc_lookback_engines_hpp

#include <algorithm>
#include <ql/exercise.hpp>
#include <ql/instruments/lookbackoption.hpp>
#include <ql/instruments/payoffs.hpp>
#include <ql/pricingengines/lookback/mclookbackengine.hpp>
#include <ql/pricingengines/mcsimulation.hpp>
#include <ql/processes/blackscholesprocess.hpp>
#include "constantblackscholesprocess.hpp"
#include <utility>

namespace QuantLib {

    class LookbackFixedPathPricer_2 : public PathPricer<Path> {
      public:
        LookbackFixedPathPricer_2(Option::Type type, Real strike, DiscountFactor discount)
        : type_(type), strike_(strike), discount_(discount) {}

        Real operator()(const Path& path) const override {
            QL_REQUIRE(path.length() > 0, "empty path");

            Real runningMin = path.front();
            Real runningMax = path.front();
            for (Size i = 1; i < path.length(); ++i) {
                runningMin = std::min(runningMin, path[i]);
                runningMax = std::max(runningMax, path[i]);
            }

            Real payoff = 0.0;
            if (type_ == Option::Call) {
                payoff = std::max(runningMax - strike_, 0.0);
            } else {
                payoff = std::max(strike_ - runningMin, 0.0);
            }
            return discount_ * payoff;
        }

      private:
        Option::Type type_;
        Real strike_;
        DiscountFactor discount_;
    };

    template <class RNG = PseudoRandom, class S = Statistics>
    class MCFixedLookbackEngine_2 : public ContinuousFixedLookbackOption::engine,
                                    public McSimulation<SingleVariate,RNG,S> {
      public:
        typedef typename McSimulation<SingleVariate,RNG,S>::path_generator_type
            path_generator_type;
        typedef typename McSimulation<SingleVariate,RNG,S>::path_pricer_type
            path_pricer_type;
        // constructor
        MCFixedLookbackEngine_2(ext::shared_ptr<GeneralizedBlackScholesProcess> process,
                                Size timeSteps,
                                Size timeStepsPerYear,
                                bool brownianBridge,
                                bool antithetic,
                                Size requiredSamples,
                                Real requiredTolerance,
                                Size maxSamples,
                                BigNatural seed,
                                bool constantParameters);
      protected:
        // McSimulation implementation
        TimeGrid timeGrid() const override;
        ext::shared_ptr<path_generator_type> pathGenerator() const override {
            TimeGrid grid = timeGrid();

            ext::shared_ptr<StochasticProcess1D> processForMc = process_;
            if (constantParameters_) {
                ext::shared_ptr<PlainVanillaPayoff> payoff =
                    ext::dynamic_pointer_cast<PlainVanillaPayoff>(this->arguments_.payoff);
                QL_REQUIRE(payoff, "non-plain payoff given");

                processForMc =
                    makeMonteCarloProcess(process_, this->arguments_.exercise, payoff->strike(), true);
            }

            typename RNG::rsg_type gen = RNG::make_sequence_generator(grid.size() - 1, seed_);
            return ext::shared_ptr<path_generator_type>(
                new path_generator_type(processForMc, grid, gen, brownianBridge_));
        }
        ext::shared_ptr<path_pricer_type> pathPricer() const override;
        // data members
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        Size timeSteps_, timeStepsPerYear_;
        Size requiredSamples_, maxSamples_;
        Real requiredTolerance_;
        bool antithetic_;
        bool brownianBridge_;
        BigNatural seed_;
        bool constantParameters_;
    };

    template <class RNG, class S>
    inline MCFixedLookbackEngine_2<RNG, S>::MCFixedLookbackEngine_2(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process,
        Size timeSteps,
        Size timeStepsPerYear,
        bool brownianBridge,
        bool antitheticVariate,
        Size requiredSamples,
        Real requiredTolerance,
        Size maxSamples,
        BigNatural seed,
        bool constantParameters)
    : McSimulation<SingleVariate, RNG, S>(antitheticVariate, false), process_(std::move(process)),
      timeSteps_(timeSteps), timeStepsPerYear_(timeStepsPerYear), requiredSamples_(requiredSamples),
      maxSamples_(maxSamples), requiredTolerance_(requiredTolerance),
      brownianBridge_(brownianBridge), seed_(seed), constantParameters_(constantParameters) {
        QL_REQUIRE(timeSteps != Null<Size>() ||
                   timeStepsPerYear != Null<Size>(),
                   "no time steps provided");
        QL_REQUIRE(timeSteps == Null<Size>() ||
                   timeStepsPerYear == Null<Size>(),
                   "both time steps and time steps per year were provided");
        QL_REQUIRE(timeSteps != 0,
                   "timeSteps must be positive, " << timeSteps <<
                   " not allowed");
        QL_REQUIRE(timeStepsPerYear != 0,
                   "timeStepsPerYear must be positive, " << timeStepsPerYear <<
                   " not allowed");
        this->registerWith(process_);
    }


    template <class RNG, class S>
    inline TimeGrid MCFixedLookbackEngine_2<RNG,S>::timeGrid() const {

        Time residualTime = process_->time(this->arguments_.exercise->lastDate());
        if (timeSteps_ != Null<Size>()) {
            return TimeGrid(residualTime, timeSteps_);
        } else if (timeStepsPerYear_ != Null<Size>()) {
            Size steps = static_cast<Size>(timeStepsPerYear_*residualTime);
            return TimeGrid(residualTime, std::max<Size>(steps, 1));
        } else {
            QL_FAIL("time steps not specified");
        }
    }

    template <class RNG, class S>
    inline ext::shared_ptr<typename MCFixedLookbackEngine_2<RNG,S>::path_pricer_type>
    MCFixedLookbackEngine_2<RNG,S>::pathPricer() const {
        TimeGrid grid = this->timeGrid();
        DiscountFactor discount = process_->riskFreeRate()->discount(grid.back());

        ext::shared_ptr<PlainVanillaPayoff> payoff =
            ext::dynamic_pointer_cast<PlainVanillaPayoff>(this->arguments_.payoff);
        QL_REQUIRE(payoff, "non-plain payoff given");

        return ext::make_shared<LookbackFixedPathPricer_2>(payoff->optionType(),
                                                           payoff->strike(),
                                                           discount);
    }



    template <class RNG = PseudoRandom, class S = Statistics>
    class MakeMCFixedLookbackEngine_2 {
      public:
        explicit MakeMCFixedLookbackEngine_2(
            ext::shared_ptr<GeneralizedBlackScholesProcess> process);
        // named parameters
        MakeMCFixedLookbackEngine_2& withSteps(Size steps);
        MakeMCFixedLookbackEngine_2& withStepsPerYear(Size steps);
        MakeMCFixedLookbackEngine_2& withBrownianBridge(bool b = true);
        MakeMCFixedLookbackEngine_2& withAntitheticVariate(bool b = true);
        MakeMCFixedLookbackEngine_2& withSamples(Size samples);
        MakeMCFixedLookbackEngine_2& withAbsoluteTolerance(Real tolerance);
        MakeMCFixedLookbackEngine_2& withMaxSamples(Size samples);
        MakeMCFixedLookbackEngine_2& withSeed(BigNatural seed);
        MakeMCFixedLookbackEngine_2& withConstantParameters(bool b = true);
        // conversion to pricing engine
        operator ext::shared_ptr<PricingEngine>() const;
      private:
        ext::shared_ptr<GeneralizedBlackScholesProcess> process_;
        bool brownianBridge_ = false, antithetic_ = false;
        Size steps_, stepsPerYear_, samples_, maxSamples_;
        Real tolerance_;
        BigNatural seed_ = 0;
        bool constantParameters_ = false;
    };

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG, S>::MakeMCFixedLookbackEngine_2(
        ext::shared_ptr<GeneralizedBlackScholesProcess> process)
    : process_(std::move(process)), steps_(Null<Size>()), stepsPerYear_(Null<Size>()),
      samples_(Null<Size>()), maxSamples_(Null<Size>()), tolerance_(Null<Real>()) {}

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withSteps(Size steps) {
        steps_ = steps;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withStepsPerYear(Size steps) {
        stepsPerYear_ = steps;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withBrownianBridge(bool brownianBridge) {
        brownianBridge_ = brownianBridge;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withAntitheticVariate(bool b) {
        antithetic_ = b;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withSamples(Size samples) {
        QL_REQUIRE(tolerance_ == Null<Real>(),
                   "tolerance already set");
        samples_ = samples;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withAbsoluteTolerance(Real tolerance) {
        QL_REQUIRE(samples_ == Null<Size>(),
                   "number of samples already set");
        QL_REQUIRE(RNG::allowsErrorEstimate,
                   "chosen random generator policy "
                   "does not allow an error estimate");
        tolerance_ = tolerance;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withMaxSamples(Size samples) {
        maxSamples_ = samples;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withSeed(BigNatural seed) {
        seed_ = seed;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>&
    MakeMCFixedLookbackEngine_2<RNG,S>::withConstantParameters(bool b) {
        constantParameters_ = b;
        return *this;
    }

    template <class RNG, class S>
    inline MakeMCFixedLookbackEngine_2<RNG,S>::operator ext::shared_ptr<PricingEngine>() const {
        return ext::shared_ptr<PricingEngine>(new MCFixedLookbackEngine_2<RNG, S>(process_,
                                                                                   steps_,
                                                                                   stepsPerYear_,
                                                                                   brownianBridge_,
                                                                                   antithetic_,
                                                                                   samples_,
                                                                                   tolerance_,
                                                                                   maxSamples_,
                                                                                   seed_,
                                                                                   constantParameters_));
    }

} // namespace QuantLib

#endif // imt2026_mclookbackengine_hpp
