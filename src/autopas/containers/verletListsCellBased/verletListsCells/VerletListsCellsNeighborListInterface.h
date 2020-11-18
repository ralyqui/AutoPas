/**
 * @file VerletListsCellsNeighborListInterface.h
 * @author tirgendetwas
 * @date 27.10.20
 */

#pragma once

#include "autopas/containers/linkedCells/LinkedCells.h"
#include "autopas/containers/verletListsCellBased/verletListsCells/VerletListsCellsHelpers.h"
#include "autopas/options/TraversalOption.h"

namespace autopas {
/**
 * Interface of neighbor lists to be used with VerletListsCells container.
 * @tparam Particle Type of particle to be used for the neighbor list.
 * */
template <class Particle>
class VerletListsCellsNeighborListInterface {
 public:
  /**
   * Default destructor.
   * */
  ~VerletListsCellsNeighborListInterface() = default;

  /**
   * Builds AoS neighbor list from underlying linked cells object.
   * @param linkedCells Linked Cells object used to build the neighbor list.
   * @param useNewton3 Whether Newton 3 should be used for the neighbor list.
   * @param cutoff Cutoff radius.
   * @param skin Skin of the verlet list.
   * @param interactionLength Interaction length of the underlying linked cells object.
   * @param buildTraversalOption Traversal option necessary for generator functor.
   * */
  virtual void buildAoSNeighborList(LinkedCells<Particle> &linkedCells, bool useNewton3, double cutoff, double skin,
                                    double interactionLength, const TraversalOption buildTraversalOption) = 0;

  /**
   * Get the neighbors list of a particle for this particular neighbor list and container combination.
   * @param particle
   * @return the neighbor list of the particle
   */
  virtual const std::vector<Particle *> &getVerletList(const Particle *particle) const = 0;

  /**
   * Returns the container type of this neighbor list and the container it belongs to.
   * @return ContainerOption for this neighbor list and the container it belongs to.
   */
  [[nodiscard]] virtual ContainerOption getContainerType() const = 0;
};

}  // namespace autopas