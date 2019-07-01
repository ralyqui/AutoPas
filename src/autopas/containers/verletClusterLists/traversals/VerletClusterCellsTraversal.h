/**
 * @file VerletClusterCellsTraversal.h
 * @author jspahl
 * @date 25.3.19
 */

#pragma once

#include <vector>
#include "autopas/containers/cellPairTraversals/CellPairTraversal.h"
#include "autopas/containers/cellPairTraversals/VerletClusterTraversalInterface.h"
#include "autopas/options/DataLayoutOption.h"
#include "autopas/pairwiseFunctors/CellFunctor.h"
#include "autopas/utils/CudaDeviceVector.h"
#if defined(AUTOPAS_CUDA)
#include "cuda_runtime.h"
#endif

namespace autopas {

/**
 * This Traversal is used to interact all clusters in VerletClusterCluster Container
 *
 * @tparam ParticleCell the type of cells
 * @tparam PairwiseFunctor The functor that defines the interaction of two particles.
 * @tparam DataLayout
 * @tparam useNewton3
 */
template <class ParticleCell, class PairwiseFunctor, DataLayoutOption DataLayout, bool useNewton3>
class VerletClusterCellsTraversal : public CellPairTraversal<ParticleCell>,
                                    public VerletClusterTraversalInterface<ParticleCell> {
  using Particle = typename ParticleCell::ParticleType;

 public:
  /**
   * Constructor for the VerletClusterClusterTraversal.
   * @param pairwiseFunctor The functor that defines the interaction of two particles.
   */
  VerletClusterCellsTraversal(PairwiseFunctor *pairwiseFunctor)
      : CellPairTraversal<ParticleCell>({2, 1, 1}),
        _functor(pairwiseFunctor),
        _cellFunctor(CellFunctor<typename ParticleCell::ParticleType, ParticleCell, PairwiseFunctor, DataLayout,
                                 useNewton3, true>(pairwiseFunctor)),
        _neighborMatrixDim(nullptr),
        _clusterSize(nullptr) {}

  TraversalOption getTraversalType() override { return TraversalOption::verletClusterCellsTraversal; }

  bool isApplicable() override {
    if (DataLayout == DataLayoutOption::cuda) {
      int nDevices = 0;
#if defined(AUTOPAS_CUDA)
      cudaGetDeviceCount(&nDevices);
      if (not _functor->getCudaWrapper()) return false;
#endif
      return nDevices > 0;
    } else
      return true;
  }

  std::tuple<TraversalOption, DataLayoutOption, bool> getSignature() override {
    return std::make_tuple(TraversalOption::verletClusterCellsTraversal, DataLayout, useNewton3);
  }

  void setVerletListPointer(unsigned int *clusterSize, std::vector<std::vector<std::vector<size_t>>> *neighborCellIds,
                            size_t *neighborMatrixDim, utils::CudaDeviceVector<unsigned int> *neighborMatrix) override {
    _clusterSize = clusterSize;
    _neighborCellIds = neighborCellIds;
    _neighborMatrixDim = neighborMatrixDim;
    _neighborMatrix = neighborMatrix;
  }

  void rebuildVerlet(const std::array<unsigned long, 3> &dims, std::vector<ParticleCell> &cells,
                     std::vector<std::array<double, 6>> &boundingBoxes, double distance) override {
    this->_cellsPerDimension = dims;
    int interactionRadius = 3;

    const size_t cellsSize = cells.size();
    _neighborCellIds->clear();
    _neighborCellIds->resize(cellsSize, {});

    if (DataLayout == DataLayoutOption::aos or DataLayout == DataLayoutOption::soa) {
      for (size_t i = 0; i < cellsSize; ++i) {
        auto pos = utils::ThreeDimensionalMapping::oneToThreeD(i, this->_cellsPerDimension);
        for (int x = -interactionRadius; x <= interactionRadius; ++x) {
          if (pos[0] + x < this->_cellsPerDimension[0]) {
            for (int y = -interactionRadius; y <= interactionRadius; ++y) {
              if (pos[1] + y < this->_cellsPerDimension[1]) {
                // add neighbors
                auto other = utils::ThreeDimensionalMapping::threeToOneD(pos[0] + x, pos[1] + y, pos[0],
                                                                         this->_cellsPerDimension);
                boxesOverlap(boundingBoxes[i][cid], boundingBoxes[other][cid], distance);
              }
            }
          }
        }
      }
      return;

    } else if (DataLayout == DataLayoutOption::cuda) {
      size_t neighborMatrixDim = 0;

      *_neighborMatrixDim = neighborMatrixDim;
#ifdef AUTOPAS_CUDA
      _neighborMatrix->copyHostToDevice(neighborMatrix.size(), neighborMatrix.data());
      utils::CudaExceptionHandler::checkErrorCode(cudaDeviceSynchronize());
#endif
      return;
    }
  }

  void initTraversal(std::vector<ParticleCell> &cells) override {
    switch (DataLayout) {
      case DataLayoutOption::aos: {
        return;
      }
      case DataLayoutOption::soa: {
        for (size_t i = 0; i < cells.size(); ++i) {
          _functor->SoALoader(cells[i], cells[i]._particleSoABuffer);
        }

        return;
      }
      case DataLayoutOption::cuda: {
        _storageCell._particleSoABuffer.resizeArrays(cells.size() * *_clusterSize);
        for (size_t i = 0; i < cells.size(); ++i) {
          _functor->SoALoader(cells[i], _storageCell._particleSoABuffer, i * *_clusterSize);
        }

        _functor->deviceSoALoader(_storageCell._particleSoABuffer, _storageCell._particleSoABufferDevice);
#ifdef AUTOPAS_CUDA
        utils::CudaExceptionHandler::checkErrorCode(cudaDeviceSynchronize());
#endif
        return;
      }
    }
  }

  void endTraversal(std::vector<ParticleCell> &cells) override {
    switch (DataLayout) {
      case DataLayoutOption::aos: {
        return;
      }
      case DataLayoutOption::soa: {
#ifdef AUTOPAS_OPENMP
#pragma omp parallel for
#endif
        for (size_t i = 0; i < cells.size(); ++i) {
          _functor->SoAExtractor(cells[i], cells[i]._particleSoABuffer);
        }

        return;
      }
      case DataLayoutOption::cuda: {
        _functor->deviceSoAExtractor(_storageCell._particleSoABuffer, _storageCell._particleSoABufferDevice);
#ifdef AUTOPAS_CUDA
        utils::CudaExceptionHandler::checkErrorCode(cudaDeviceSynchronize());
#endif
#ifdef AUTOPAS_OPENMP
#pragma omp parallel for
#endif
        for (size_t i = 0; i < cells.size(); ++i) {
          _functor->SoAExtractor(cells[i], _storageCell._particleSoABuffer, i * *_clusterSize);
        }
        return;
      }
    }
  }

  /**
   * This function interacts all cells with the other cells with their index in neighborCellIds
   * @param cells containing the particles
   */
  void traverseCellPairs(std::vector<ParticleCell> &cells) override {
    switch (DataLayout) {
      case DataLayoutOption::aos: {
        traverseCellPairsCPU(cells);
        return;
      }
      case DataLayoutOption::soa: {
        traverseCellPairsCPU(cells);
        return;
      }
      case DataLayoutOption::cuda: {
        traverseCellPairsGPU(cells);
        return;
      }
    }
  }

 private:
  void traverseCellPairsCPU(std::vector<ParticleCell> &cells) {
    for (size_t i = 0; i < cells.size(); ++i) {
      for (auto &j : (*_neighborCellIds)[i]) {
        _cellFunctor.processCellPair(cells[i], cells[j]);
      }
      _cellFunctor.processCell(cells[i]);
    }
  }

  void traverseCellPairsGPU(std::vector<ParticleCell> &cells) {
#ifdef AUTOPAS_CUDA
    if (!_functor->getCudaWrapper()) {
      _functor->CudaFunctor(_storageCell._particleSoABufferDevice, useNewton3);
      return;
    }

    auto cudaSoA = _functor->createFunctorCudaSoA(_storageCell._particleSoABufferDevice);

    if (useNewton3) {
      _functor->getCudaWrapper()->CellVerletTraversalN3Wrapper(cudaSoA.get(), cells.size(), *_clusterSize,
                                                               *_neighborMatrixDim, _neighborMatrix->get(), 0);
    } else {
      _functor->getCudaWrapper()->CellVerletTraversalNoN3Wrapper(cudaSoA.get(), cells.size(), *_clusterSize,
                                                                 *_neighborMatrixDim, _neighborMatrix->get(), 0);
    }
    utils::CudaExceptionHandler::checkErrorCode(cudaDeviceSynchronize());
#else
    utils::ExceptionHandler::exception("VerletClusterCellsTraversal was compiled without Cuda support");
#endif
  }

  /**
   * Returns true if the two boxes are within distance
   * @param box1
   * @param box2
   * @param distance between the boxes to return true
   * @return true if the boxes are overlapping
   */
  inline bool boxesOverlap(std::array<double, 6> &box1, std::array<double, 6> &box2, double distance) {
    for (int i = 0; i < 3; ++i) {
      if (box1[0 + i] - distance > box2[3 + i] || box1[3 + i] + distance < box2[0 + i]) return false;
    }
    return true;
  }

  /**
   * Pairwise functor used in this traversal
   */
  PairwiseFunctor *_functor;

  /**
   * CellFunctor to be used for the traversal defining the interaction between two cells.
   */
  CellFunctor<typename ParticleCell::ParticleType, ParticleCell, PairwiseFunctor, DataLayout, useNewton3, true>
      _cellFunctor;

  /**
   * SoA Storage cell containing SoAs and device Memory
   */
  ParticleCell _storageCell;

  // id of neighbor clusters of a clusters
  std::vector<std::vector<std::vector<size_t>>> *_neighborCellIds;

  size_t *_neighborMatrixDim;
  utils::CudaDeviceVector<unsigned int> *_neighborMatrix;

  unsigned int *_clusterSize;
};

}  // namespace autopas
