/**
 * @file DirectSumTriplet.h
 * @date 02.01.2023
 * @author ralyqui
 */

#pragma once

#include "autopas/cells/FullParticleCell.h"
#include "autopas/containers/CellBasedParticleContainer.h"

namespace autopas {

template<class Particle>
class DirectSumTriplet: public CellBasedParticleContainer<FullParticleCell<Particle>> {
 public:
  DirectSumTriplet(const std::array<double, 3> boxMin, const std::array<double, 3> boxMax, const double cutoff,
                   const double skin = 0.0)
      : CellBasedParticleContainer<FullParticleCell<Particle>>(boxMin, boxMax, cutoff, skin) {
    this->_cells.resize(2);
  }

  void iterate(TraversalInterface *traversal) override {
    traversal->initTraversal();
    traversal->traverseParticleTriplets();
    traversal->endTraversal();
  }

};
}  // namespace autopas
