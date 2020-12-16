/**
 * @file VerletListsCellsNeighborList.h
 * @author tirgendetwas
 * @date 25.10.20
 */

#pragma once

#include "autopas/containers/verletListsCellBased/verletListsCells/VerletListsCellsNeighborListInterface.h"
#include "autopas/containers/verletListsCellBased/verletListsCells/traversals/VLCTraversalInterface.h"
#include "autopas/utils/ArrayMath.h"
#include "autopas/utils/StaticBoolSelector.h"

namespace autopas {
template <class ParticleCell>
class TraversalSelector;
/**
 * Neighbor list to be used with VerletListsCells container. Classic implementation of verlet lists based on linked
 * cells.
 * @tparam Particle Type of particle to be used for this neighbor list.
 * */
template <class Particle>
class VerletListsCellsNeighborList : public VerletListsCellsNeighborListInterface<Particle> {
 public:
  /**
   * Type of the data structure used to save the neighbor lists.
   * */
  using listType = typename VerletListsCellsHelpers<Particle>::NeighborListsType;

  //TODO
  using SoAPairOfParticleAndList = std::pair<size_t, std::vector<size_t, autopas::AlignedAllocator<size_t>>>;

  //TODO
  using soaListType = typename std::vector<std::vector<SoAPairOfParticleAndList>>;

  /**
   * Constructor for VerletListsCellsNeighborList. Initializes private attributes.
   * */
  VerletListsCellsNeighborList() : _aosNeighborList{}, _particleToCellMap{}, _soaNeighborList(), _soa{} {}

  /**
   * @copydoc VerletListsCellsNeighborListInterface::getContainerType()
   * */
  [[nodiscard]] ContainerOption getContainerType() const override { return ContainerOption::verletListsCells; }

  /**
   * @copydoc VerletListsCellsNeighborListInterface::buildAoSNeighborList()
   * */
  void buildAoSNeighborList(LinkedCells<Particle> &linkedCells, bool useNewton3, double cutoff, double skin,
                            double interactionLength, const TraversalOption buildTraversalOption) override {
    // Initialize a neighbor list for each cell.
    _aosNeighborList.clear();
	_internalLinkedCells = &linkedCells;
    auto &cells = linkedCells.getCells();
    size_t cellsSize = cells.size();
    _aosNeighborList.resize(cellsSize);
    for (size_t cellIndex = 0; cellIndex < cellsSize; ++cellIndex) {
      _aosNeighborList[cellIndex].reserve(cells[cellIndex].numParticles());
      size_t particleIndexWithinCell = 0;
      for (auto iter = cells[cellIndex].begin(); iter.isValid(); ++iter, ++particleIndexWithinCell) {
        Particle *particle = &*iter;
        _aosNeighborList[cellIndex].emplace_back(particle, std::vector<Particle *>());
        // In a cell with N particles, reserve space for 5N neighbors.
        // 5 is an empirically determined magic number that provides good speed.
        _aosNeighborList[cellIndex].back().second.reserve(cells[cellIndex].numParticles() * 5);
        _particleToCellMap[particle] = std::make_pair(cellIndex, particleIndexWithinCell);
      }
    }

    applyBuildFunctor(linkedCells, useNewton3, cutoff, skin, interactionLength, buildTraversalOption);
  }

  /**
   * @copydoc VerletListsCellsNeighborListInterface::getNumberOfPartners()
   * */
  const size_t getNumberOfPartners(const Particle *particle) const override {
    const auto [cellIndex, particleIndexInCell] = _particleToCellMap.at(const_cast<Particle *>(particle));
    size_t listSize = _aosNeighborList.at(cellIndex).at(particleIndexInCell).second.size();
    return listSize;
  }

  /**
   * Returns the neighbor list in AoS layout.
   * @return Neighbor list in AoS layout.
   * */
  typename VerletListsCellsHelpers<Particle>::NeighborListsType &getAoSNeighborList() { return _aosNeighborList; }

  auto &getSoANeighborList() {return _soaNeighborList; }

  template <class TFunctor>
  auto *loadSoA(TFunctor *f) {
    _soa.clear();
    size_t offset = 0;
    size_t index = 0;
    for (auto &cell : _internalLinkedCells->getCells()) {
      f->SoALoader(cell, _soa, offset);
      offset += cell.numParticles();
      index++;
    }
    return &_soa;
  }

  template <class TFunctor>
  void extractSoA(TFunctor *f) {
    size_t offset = 0;
    for (auto &cell : _internalLinkedCells->getCells()) {
      f->SoAExtractor(cell, _soa, offset);
      offset += cell.numParticles();
    }
  }

  void generateSoAFromAoS(LinkedCells<Particle> &linkedCells)
  {
    _soaNeighborList.clear();

    std::unordered_map<Particle*, size_t> _particleToIndex;
    _particleToIndex.reserve(linkedCells.getNumParticles());
    size_t i = 0;
    for (auto iter = linkedCells.begin(IteratorBehavior::haloOwnedAndDummy); iter.isValid(); ++iter, ++i) {
      _particleToIndex[&(*iter)] = i;
    }

    auto &cells = linkedCells.getCells();
    size_t cellsSize = cells.size();
    _soaNeighborList.resize(cellsSize);

    for(size_t firstCellIndex = 0; firstCellIndex < cellsSize; ++firstCellIndex) {
      _soaNeighborList[firstCellIndex].reserve(cells[firstCellIndex].numParticles());
      size_t particleIndexCurrentCell = 0;

      for (auto particleIter = cells[firstCellIndex].begin(); particleIter.isValid(); ++particleIter) {
        Particle *currentParticle = &*particleIter;
        size_t currentIndex = _particleToIndex.at(currentParticle);

        _soaNeighborList[firstCellIndex].emplace_back(std::make_pair(currentIndex, std::vector<size_t, autopas::AlignedAllocator<size_t>>()));

        for(size_t neighbor = 0; neighbor < _aosNeighborList[firstCellIndex][particleIndexCurrentCell].second.size(); neighbor++)
        {
          _soaNeighborList[firstCellIndex][particleIndexCurrentCell].second.emplace_back
              (_particleToIndex.at(_aosNeighborList[firstCellIndex][particleIndexCurrentCell].second[neighbor]));
        }
        particleIndexCurrentCell++;
      }
    }
  }

 private:
  /**
   * Creates and applies generator functor for the building of the neighbor list.
   * @param linkedCells Linked Cells object used to build the neighbor list.
   * @param useNewton3 Whether Newton 3 should be used for the neighbor list.
   * @param cutoff Cutoff radius.
   * @param skin Skin of the verlet list.
   * @param interactionLength Interaction length of the underlying linked cells object.
   * @param buildTraversalOption Traversal option necessary for generator functor.
   * */
  void applyBuildFunctor(LinkedCells<Particle> &linkedCells, bool useNewton3, double cutoff, double skin,
                         double interactionLength, const TraversalOption buildTraversalOption) {
    typename VerletListsCellsHelpers<Particle>::VerletListGeneratorFunctor f(_aosNeighborList, _particleToCellMap,
                                                                             cutoff + skin);

    // Generate the build traversal with the traversal selector and apply the build functor with it.
    TraversalSelector<FullParticleCell<Particle>> traversalSelector;
    // Argument "cluster size" does not matter here.
    TraversalSelectorInfo traversalSelectorInfo(linkedCells.getCellBlock().getCellsPerDimensionWithHalo(),
                                                interactionLength, linkedCells.getCellBlock().getCellLength(), 0);
    autopas::utils::withStaticBool(useNewton3, [&](auto n3) {
      auto buildTraversal = traversalSelector.template generateTraversal<decltype(f), DataLayoutOption::aos, n3>(
          buildTraversalOption, f, traversalSelectorInfo);
      linkedCells.iteratePairwise(buildTraversal.get());
    });
  }

  /**
   * Internal neighbor list structure in AoS format - Verlet lists for each particle for each cell.
   * */
  typename VerletListsCellsHelpers<Particle>::NeighborListsType _aosNeighborList;

  /**
   * Mapping of each particle to its corresponding cell and id within this cell.
   */
  std::unordered_map<Particle *, std::pair<size_t, size_t>> _particleToCellMap;
  SoA<typename Particle::SoAArraysType> _soa;
  soaListType _soaNeighborList;
  LinkedCells<Particle>* _internalLinkedCells;
};
}  // namespace autopas
