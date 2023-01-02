/**
 * @file ATFunctor.h
 * @date 31.12.2022
 * @author ralyqui
 */

#pragma once

#include "autopas/pairwiseFunctors/Functor.h"
#include "autopas/utils/ArrayMath.h"

namespace autopas {

/**
 * Functor handling three-body interactions for the Axilrod-Teller potential.
 * @tparam Particle The type of the particle.
 * @tparam useNewton3 Switch for the functor to support newton3 on, off or both. See FunctorN3Modes for possible values.
 */
template <class Particle, FunctorN3Modes useNewton3 = FunctorN3Modes::Both>
class ATFunctor : public Functor<Particle, ATFunctor<Particle, useNewton3>> {
  /**
   * Structure of the SoAs defined by the particle.
   */
  using SoAArraysType = typename Particle::SoAArraysType;

 public:

  /**
   * Constructor of the ATFunctor.
   * @param cutoff
   * @param ATParameter
   */
  explicit ATFunctor(double cutoff, double ATParameter)
      : Functor<Particle, ATFunctor<Particle, useNewton3>>(cutoff),
        _cutoffSquared{cutoff * cutoff},
        _ATParameter{ATParameter} {}

  /**
   * @copydoc Functor::allowsNewton3()
   */
  bool allowsNewton3() final override { return false; };

  /**
   * @copydoc Functor::allowsNonNewton3()
   */
  bool allowsNonNewton3() final override { return true; };

  /**
   * @copydoc Functor::isRelevantForTuning()
   */
  bool isRelevantForTuning() final override { return false; };

  void AoSTripletFunctor(Particle &i, Particle &j, Particle &k, bool newton3) final override {
    if (newton3) {
      utils::ExceptionHandler::exception("Newton3 not currently supported for three-body interactions");
    }

    // Possibly unnecessary overhead since the check should also be done in the container
    if (!_checkCutoff(i, j, k)) {
      return;
    }

    double force = 0;
    // @todo: implement force calculation

    i.addF(force);
  }

 private:
  const double _ATParameter;
  const double _cutoffSquared;

  bool _checkCutoff(Particle &i, Particle &j, Particle &k) {
    auto dr = utils::ArrayMath::sub(i.getR(), j.getR());
    double dr2 = utils::ArrayMath::dot(dr, dr);

    if (dr2 > _cutoffSquared) {
      return false;
    }

    dr = utils::ArrayMath::sub(i.getR(), k.getR());
    dr2 = utils::ArrayMath::dot(dr, dr);

    if (dr2 > _cutoffSquared) {
      return false;
    }

    dr = utils::ArrayMath::sub(j.getR(), k.getR());
    dr2 = utils::ArrayMath::dot(dr, dr);

    if (dr2 > _cutoffSquared) {
      return false;
    }

    return true;
  }

};

}  // namespace autopas
