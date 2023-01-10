/**
 * @file DSTripletTraversal.h
 * @date 02.01.2023
 * @author ralyqui
 */

#pragma once

#include "autopas/containers/TraversalInterface.h"
#include "autopas/options/DataLayoutOption.h"
#include "autopas/tripletFunctors/TripletCellFunctor.h"
#include "autopas/tripletFunctors/TripletFunctor.h"

namespace autopas {

//@todo c++20 change "class Functor" -> "std::derived_from<TripletFunctor> Functor"
template <class ParticleCell, class Functor, DataLayoutOption::Value dataLayout, bool useNewton3>
class DSTripletTraversal : public TraversalInterface {
 public:
  DSTripletTraversal(Functor *functor, double cutoff)
      : TraversalInterface(), _cutoff{cutoff}, _tripletCellFunctor(functor, cutoff){};

  ~DSTripletTraversal() final = default;

  [[nodiscard]] TraversalOption getTraversalType() const override { return autopas::TraversalOption::ds_triplet; }

  void traverseParticleTriplets() override;

  [[nodiscard]] bool isApplicable() const override { return true; }

  [[nodiscard]] bool getUseNewton3() const override { return useNewton3; };

  [[nodiscard]] DataLayoutOption getDataLayout() const override { return dataLayout; };

  void setCellsToTraverse(std::vector<ParticleCell> &cells) { _cells = &cells; }

  void initTraversal() override{};

  void endTraversal() override{};

 private:
  autopas::TripletCellFunctor<typename ParticleCell::ParticleType, typename ParticleCell::ParticleType, Functor,
                              useNewton3>
      _tripletCellFunctor;

  const double _cutoff;
  std::vector<ParticleCell> *_cells;
};

template <class ParticleCell, class Functor, DataLayoutOption::Value dataLayout, bool useNewton3>
void DSTripletTraversal<ParticleCell, Functor, dataLayout, useNewton3>::traverseParticleTriplets() {
  _tripletCellFunctor.processCell(this->_cells->at(0));
}
}  // namespace autopas