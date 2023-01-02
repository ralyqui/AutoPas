/**
 * @file DSTripletTraversal.h
 * @date 02.01.2023
 * @author ralyqui
 */

#pragma once

#include "autopas/containers/TraversalInterface.h"
#include "autopas/options/DataLayoutOption.h"
#include "autopas/tripletFunctors/TripletFunctor.h"

namespace autopas {

//@todo c++20 change "class Functor" -> "std::derived_from<TripletFunctor> Functor"
template <class ParticleCell, class Functor, DataLayoutOption::Value dataLayout, bool useNewton3>
class DSTripletTraversal : public TraversalInterface {
 public:
  DSTripletTraversal(Functor functor, double cutoff)
      : TraversalInterface(), _cutoff{cutoff}, _tripletFunctor(functor, cutoff){};

  ~DSTripletTraversal() = default;

  [[nodiscard]] virtual TraversalOption getTraversalType() { return autopas::TraversalOption::ds_triplet; }

 private:
  autopas::TripletFunctor<typename ParticleCell::ParticleType> _tripletFunctor;
  const double _cutoff;
};
}  // namespace autopas