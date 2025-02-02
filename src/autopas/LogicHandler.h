/**
 * @file LogicHandler.h
 * @author seckler
 * @date 31.05.19
 */

#pragma once
#include <limits>

#include "autopas/iterators/ParticleIteratorWrapper.h"
#include "autopas/options/IteratorBehavior.h"
#include "autopas/selectors/AutoTuner.h"
#include "autopas/utils/logging/Logger.h"
#include "autopas/utils/markParticleAsDeleted.h"

namespace autopas {

/**
 * The LogicHandler takes care of the containers s.t. they are all in the same valid state.
 * This is mainly done by incorporating a global container rebuild frequency, which defines when containers and their
 * neighbor lists will be rebuild.
 */
template <typename Particle>
class LogicHandler {
 public:
  /**
   * Constructor of the LogicHandler.
   * @param autoTuner
   * @param rebuildFrequency
   */
  LogicHandler(autopas::AutoTuner<Particle> &autoTuner, unsigned int rebuildFrequency)
      : _neighborListRebuildFrequency{rebuildFrequency},
        _autoTuner(autoTuner),
        _particleBuffer(autopas_get_max_threads()),
        _haloParticleBuffer(autopas_get_max_threads()) {
    checkMinimalSize();
  }

  /**
   * Collects leaving particles from buffer and potentially inserts owned particles to the container.
   * @param insertOwnedParticlesToContainer Decides whether to insert owned particles to the container.
   * @return Leaving particles.
   */
  [[nodiscard]] std::vector<Particle> collectLeavingParticlesFromBuffer(bool insertOwnedParticlesToContainer) {
    const auto &boxMin = _autoTuner.getContainer()->getBoxMin();
    const auto &boxMax = _autoTuner.getContainer()->getBoxMax();
    std::vector<Particle> leavingBufferParticles{};
    for (auto &buffer : _particleBuffer) {
      if (insertOwnedParticlesToContainer) {
        for (const auto &p : buffer) {
          if (p.isDummy()) {
            continue;
          }
          if (utils::inBox(p.getR(), boxMin, boxMax)) {
            _autoTuner.getContainer()->template addParticle(p);
          } else {
            leavingBufferParticles.push_back(p);
          }
        }
        buffer.clear();
      } else {
        for (auto iter = buffer.begin(); iter != buffer.end();) {
          auto &p = *iter;

          auto fastRemoveP = [&]() {
            // Fast remove of particle, i.e., swap with last entry && pop.
            std::swap(p, buffer.back());
            buffer.pop_back();
            // Do not increment the iter afterwards!
          };
          if (p.isDummy()) {
            // We remove dummies!
            fastRemoveP();
          }
          if (utils::notInBox(p.getR(), boxMin, boxMax)) {
            leavingBufferParticles.push_back(p);
            fastRemoveP();
          } else {
            ++iter;
          }
        }
      }
    }
    return leavingBufferParticles;
  }

  /**
   * @copydoc AutoPas::updateContainer()
   */
  [[nodiscard]] std::vector<Particle> updateContainer() {
    bool doDataStructureUpdate = not neighborListsAreValid();

    if (doDataStructureUpdate) {
      _neighborListsAreValid = false;
    }
    // The next call also adds particles to the container if doDataStructureUpdate is true.
    auto leavingBufferParticles = collectLeavingParticlesFromBuffer(doDataStructureUpdate);

    AutoPasLog(debug, "Initiating container update.");
    auto leavingParticles = _autoTuner.getContainer()->updateContainer(not doDataStructureUpdate);
    leavingParticles.insert(leavingParticles.end(), leavingBufferParticles.begin(), leavingBufferParticles.end());

    // Substract the amount of leaving particles from the number of owned particles.
    _numParticlesOwned.fetch_sub(leavingParticles.size(), std::memory_order_relaxed);
    // updateContainer deletes all halo particles.
    std::for_each(_haloParticleBuffer.begin(), _haloParticleBuffer.end(), [](auto &buffer) { buffer.clear(); });
    _numParticlesHalo.exchange(0, std::memory_order_relaxed);
    return leavingParticles;
  }

  /**
   * Pass values to the actual container.
   * @param boxMin
   * @param boxMax
   * @return Vector of particles that are outside the box after the resize.
   */
  std::vector<Particle> resizeBox(const std::array<double, 3> &boxMin, const std::array<double, 3> &boxMax) {
    const auto &oldMin = _autoTuner.getContainer()->getBoxMin();
    const auto &oldMax = _autoTuner.getContainer()->getBoxMax();

    // if nothing changed do nothing
    if (oldMin == boxMin and oldMax == boxMax) {
      return {};
    }

    // sanity check that new size is actually positive
    for (size_t i = 0; i < boxMin.size(); ++i) {
      if (boxMin[i] >= boxMax[i]) {
        utils::ExceptionHandler::exception(
            "New box size in dimension {} is not positive!\nboxMin[{}] = {}\nboxMax[{}] = {}", i, i, boxMin[i], i,
            boxMax[i]);
      }
    }

    // warn if domain changes too drastically
    const auto newLength = utils::ArrayMath::sub(boxMax, boxMin);
    const auto oldLength = utils::ArrayMath::sub(oldMax, oldMin);
    const auto relDiffLength = utils::ArrayMath::div(newLength, oldLength);
    for (size_t i = 0; i < newLength.size(); ++i) {
      // warning threshold is set arbitrary and up for change if needed
      if (relDiffLength[i] > 1.3 or relDiffLength[i] < 0.7) {
        AutoPasLog(warn,
                   "LogicHandler.resize(): Domain size changed drastically in dimension {}! Gathered AutoTuning "
                   "information might not be applicable anymore!\n"
                   "Size old box : {}\n"
                   "Size new box : {}\n"
                   "Relative diff: {}",
                   i, utils::ArrayUtils::to_string(oldLength), utils::ArrayUtils::to_string(newLength),
                   utils::ArrayUtils::to_string(relDiffLength));
      }
    }

    // check all particles
    std::vector<Particle> particlesNowOutside;
    for (auto pIter = _autoTuner.getContainer()->begin(); pIter.isValid(); ++pIter) {
      // make sure only owned ones are present
      if (not pIter->isOwned()) {
        utils::ExceptionHandler::exception(
            "LogicHandler::resizeBox() encountered non owned particle. "
            "When calling resizeBox() these should be already deleted. "
            "This could be solved by calling updateContainer() before resizeBox().");
      }
      // owned particles that are now outside are removed from the container and returned
      if (not utils::inBox(pIter->getR(), boxMin, boxMax)) {
        particlesNowOutside.push_back(*pIter);
        deleteParticle(pIter);
      }
    }

    // actually resize the container
    _autoTuner.resizeBox(boxMin, boxMax);
    // Set this flag, s.t., the container is rebuilt!
    _neighborListsAreValid = false;

    return particlesNowOutside;
  }

  /**
   * @copydoc AutoPas::addParticle()
   */
  void addParticle(const Particle &p) {
    if (not neighborListsAreValid()) {
      // Container has to be invalid to be able to add Particles!
      _autoTuner.getContainer()->addParticle(p);
    } else {
      const auto &boxMin = _autoTuner.getContainer()->getBoxMin();
      const auto &boxMax = _autoTuner.getContainer()->getBoxMax();
      if (utils::notInBox(p.getR(), boxMin, boxMax)) {
        autopas::utils::ExceptionHandler::exception(
            "LogicHandler: Trying to add a particle that is not in the bounding box.\n"
            "Box Min {}\n"
            "Box Max {}\n"
            "{}",
            utils::ArrayUtils::to_string(boxMin), utils::ArrayUtils::to_string(boxMax), p.toString());
      }
      // If the container is valid, we add it to the particle buffer.
      _particleBuffer[autopas_get_thread_num()].push_back(p);
    }
    _numParticlesOwned.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * @copydoc AutoPas::addHaloParticle()
   */
  void addHaloParticle(const Particle &haloParticle) {
    auto container = _autoTuner.getContainer();
    const auto &boxMin = container->getBoxMin();
    const auto &boxMax = container->getBoxMax();
    if (utils::inBox(haloParticle.getR(), boxMin, boxMax)) {
      autopas::utils::ExceptionHandler::exception(
          "LogicHandler: Trying to add a halo particle that is not outside the box of the container.\n"
          "Box Min {}\n"
          "Box Max {}\n"
          "{}",
          utils::ArrayUtils::to_string(boxMin), utils::ArrayUtils::to_string(boxMax), haloParticle.toString());
    }
    if (not neighborListsAreValid()) {
      // If the neighbor lists are not valid, we can add the particle.
      container->template addHaloParticle</* checkInBox */ false>(haloParticle);
    } else {
      // Check if we can update an existing halo(dummy) particle.
      bool updated = container->updateHaloParticle(haloParticle);
      if (not updated) {
        // If we couldn't find an existing particle, add it to the halo particle buffer.
        _haloParticleBuffer[autopas_get_thread_num()].push_back(haloParticle);
        _haloParticleBuffer[autopas_get_thread_num()].back().setOwnershipState(OwnershipState::halo);
      }
    }
    _numParticlesHalo.fetch_add(1, std::memory_order_relaxed);
  }

  /**
   * @copydoc AutoPas::deleteAllParticles()
   */
  void deleteAllParticles() {
    _neighborListsAreValid = false;
    _autoTuner.getContainer()->deleteAllParticles();
    std::for_each(_particleBuffer.begin(), _particleBuffer.end(), [](auto &buffer) { buffer.clear(); });
    std::for_each(_haloParticleBuffer.begin(), _haloParticleBuffer.end(), [](auto &buffer) { buffer.clear(); });
    // all particles are gone -> reset counters.
    _numParticlesOwned.exchange(0, std::memory_order_relaxed);
    _numParticlesHalo.exchange(0, std::memory_order_relaxed);
  }

  /**
   * Deletes a single particle and updates internal particle counters.
   * @param iter
   */
  void deleteParticle(ParticleIteratorWrapper<Particle, true> &iter) { deleteParticle(*iter); }

  /**
   * Deletes a single particle and updates internal particle counters.
   * @param particle reference to particles that should be deleted
   */
  void deleteParticle(Particle &particle) {
    if (particle.isOwned()) {
      _numParticlesOwned.fetch_sub(1, std::memory_order_relaxed);
    } else {
      _numParticlesHalo.fetch_sub(1, std::memory_order_relaxed);
    }
    internal::markParticleAsDeleted(particle);
  }

  /**
   * @copydoc AutoPas::iterate()
   */
  template <class Functor>
  bool iteratePairwise(Functor *f) {
    const bool doRebuild = not neighborListsAreValid();

    bool result = _autoTuner.iteratePairwise(f, doRebuild, _particleBuffer, _haloParticleBuffer);

    if (doRebuild /*we have done a rebuild now*/) {
      // list is now valid
      _neighborListsAreValid = true;
      _stepsSinceLastListRebuild = 0;
    }
    ++_stepsSinceLastListRebuild;

    return result;
  }

  /**
   * @copydoc AutoPas::iterateTriplets()
   */
  template <class Functor>
  bool iterateTriplets(Functor *f) {
    const bool doRebuild = not neighborListsAreValid();

    bool result = _autoTuner.iterateTriplets(f, doRebuild, _particleBuffer, _haloParticleBuffer);

    return result;
  }

  /**
   * @copydoc AutoPas::begin()
   */
  autopas::ParticleIteratorWrapper<Particle, true> begin(IteratorBehavior behavior) {
    /// @todo: we might have to add a rebuild here, if the verlet cluster lists are used.
    auto iter = _autoTuner.getContainer()->begin(behavior);
    for (int i = 0; i < autopas_get_max_threads(); ++i) {
      if (behavior & IteratorBehavior::owned) {
        iter.addAdditionalVector(_particleBuffer[i]);
      }
      if (behavior & IteratorBehavior::halo) {
        iter.addAdditionalVector(_haloParticleBuffer[i]);
      }
    }
    return iter;
  }

  /**
   * @copydoc AutoPas::begin()
   */
  autopas::ParticleIteratorWrapper<Particle, false> begin(IteratorBehavior behavior) const {
    /// @todo: we might have to add a rebuild here, if the verlet cluster lists are used.
    auto iter = std::as_const(_autoTuner).getContainer()->begin(behavior);
    for (int i = 0; i < autopas_get_max_threads(); ++i) {
      if (behavior & IteratorBehavior::owned) {
        iter.addAdditionalVector(_particleBuffer[i]);
      }
      if (behavior & IteratorBehavior::halo) {
        iter.addAdditionalVector(_haloParticleBuffer[i]);
      }
    }
    return iter;
  }

  /**
   * @copydoc AutoPas::getRegionIterator()
   */
  autopas::ParticleIteratorWrapper<Particle, true> getRegionIterator(std::array<double, 3> lowerCorner,
                                                                     std::array<double, 3> higherCorner,
                                                                     IteratorBehavior behavior) {
    // sanity check: Most of our stuff depends on `inBox` which does not handle lowerCorner > higherCorner well.
    for (size_t d = 0; d < 3; ++d) {
      if (lowerCorner[d] > higherCorner[d]) {
        autopas::utils::ExceptionHandler::exception(
            "Requesting region Iterator where the upper corner is lower than the lower corner!\n"
            "Lower corner: {}\n"
            "Upper corner: {}",
            lowerCorner, higherCorner);
      }
    }

    /// @todo: we might have to add a rebuild here, if the verlet cluster lists are used.
    auto iter = _autoTuner.getContainer()->getRegionIterator(lowerCorner, higherCorner, behavior);
    for (int i = 0; i < autopas_get_max_threads(); ++i) {
      if (behavior & IteratorBehavior::owned) {
        iter.addAdditionalVector(_particleBuffer[i]);
      }
      if (behavior & IteratorBehavior::halo) {
        iter.addAdditionalVector(_haloParticleBuffer[i]);
      }
    }
    return iter;
  }

  /**
   * @copydoc AutoPas::getRegionIterator()
   */
  autopas::ParticleIteratorWrapper<Particle, false> getRegionIterator(std::array<double, 3> lowerCorner,
                                                                      std::array<double, 3> higherCorner,
                                                                      IteratorBehavior behavior) const {
    // sanity check: Most of our stuff depends on `inBox` which does not handle lowerCorner > higherCorner well.
    for (size_t d = 0; d < 3; ++d) {
      if (lowerCorner[d] > higherCorner[d]) {
        autopas::utils::ExceptionHandler::exception(
            "Requesting region Iterator where the upper corner is lower than the lower corner!\n"
            "Lower corner: {}\n"
            "Upper corner: {}",
            lowerCorner, higherCorner);
      }
    }

    /// @todo: we might have to add a rebuild here, if the verlet cluster lists are used.
    auto iter = std::as_const(_autoTuner).getContainer()->getRegionIterator(lowerCorner, higherCorner, behavior);
    for (int i = 0; i < autopas_get_max_threads(); ++i) {
      if (behavior & IteratorBehavior::owned) {
        iter.addAdditionalVector(_particleBuffer[i]);
      }
      if (behavior & IteratorBehavior::halo) {
        iter.addAdditionalVector(_haloParticleBuffer[i]);
      }
    }
    return iter;
  }

  /**
   * Get the number of owned particles.
   * @return
   */
  [[nodiscard]] unsigned long getNumParticlesOwned() const { return _numParticlesOwned; }

  /**
   * Get the number of halo particles.
   * @return
   */
  [[nodiscard]] unsigned long getNumParticlesHalo() const { return _numParticlesHalo; }

 private:
  void checkMinimalSize() {
    auto container = _autoTuner.getContainer();
    // check boxSize at least cutoff + skin
    for (unsigned int dim = 0; dim < 3; ++dim) {
      if (container->getBoxMax()[dim] - container->getBoxMin()[dim] <
          container->getCutoff() + container->getVerletSkin()) {
        autopas::utils::ExceptionHandler::exception(
            "Box (boxMin[{}]={} and boxMax[{}]={}) is too small.\nHas to be at least cutoff({}) + skin({}) = {}.", dim,
            container->getBoxMin()[dim], dim, container->getBoxMax()[dim], container->getCutoff(),
            container->getVerletSkin(), container->getCutoff() + container->getVerletSkin());
      }
    }
  }

  bool neighborListsAreValid() {
    if (_stepsSinceLastListRebuild >= _neighborListRebuildFrequency or _autoTuner.willRebuild()) {
      _neighborListsAreValid = false;
    }
    return _neighborListsAreValid;
  }

  /**
   * Specifies after how many pair-wise traversals the neighbor lists (if they exist) are to be rebuild.
   */
  const unsigned int _neighborListRebuildFrequency;

  /**
   * Reference to the AutoTuner that owns the container, ...
   */
  autopas::AutoTuner<Particle> &_autoTuner;

  /**
   * Specifies if the neighbor list is valid.
   */
  bool _neighborListsAreValid{false};

  /**
   * Steps since last rebuild
   */
  unsigned int _stepsSinceLastListRebuild{std::numeric_limits<unsigned int>::max()};

  /**
   * Atomic tracker of the number of owned particles.
   */
  std::atomic<size_t> _numParticlesOwned{0ul};

  /**
   * Atomic tracker of the number of halo particles.
   */
  std::atomic<size_t> _numParticlesHalo{0ul};

  /**
   * Buffer to store particles that should not yet be added to the container. There is one buffer per thread.
   * @note This buffer could potentially be replaced by a ParticleCell.
   */
  std::vector<std::vector<Particle>> _particleBuffer;

  /**
   * Buffer to store halo particles that should not yet be added to the container. There is one buffer per thread.
   * @note This buffer could potentially be replaced by a ParticleCell.
   */
  std::vector<std::vector<Particle>> _haloParticleBuffer;
};
}  // namespace autopas
