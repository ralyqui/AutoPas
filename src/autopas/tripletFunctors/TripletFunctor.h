/**
 * @file TripletFunctor.h
 * @date 02.01.2023
 * @author ralyqui
 */

#pragma once

#include "autopas/utils/ExceptionHandler.h"

namespace autopas {

template <class Particle>
class TripletFunctor {
 public:
  explicit TripletFunctor(double cutoff) : _cutoff{cutoff} {};

  virtual ~TripletFunctor() = default;

  /**
   * This function is called at the start of each traversal.
   * Use it for resetting global values or initializing them.
   */
  virtual void initTraversal(){};

  /**
   * This function is called at the end of each traversal.
   * You may accumulate values in this step.
   * @param newton3
   */
  virtual void endTraversal(bool newton3){};

  /**
   * Functor for arrays of structures in 3-body interactions (AoS).
   *
   * This functor should calculate the forces or any other interaction
   * between three particles.
   * This should include a cutoff check if needed!
   * @param i Particle i
   * @param j Particle j
   * @param k Particle k
   * @param newton3 defines whether or whether not to use newton 3
   */
  virtual void AoSFunctor(Particle &i, Particle &j, Particle &k, bool newton3) {
    utils::ExceptionHandler::exception("Functor::AoSTripletFunctor: not yet implemented");
  }

  template <class ParticleCell, class SoAArraysType>
  void SoALoader(ParticleCell &cell, SoA<SoAArraysType> &soa, size_t offset) {}

  template <typename ParticleCell, class SoAArraysType>
  void SoAExtractor(ParticleCell &cell, SoA<SoAArraysType> &soa, size_t offset) {}

 private:
  double _cutoff;
};
}  // namespace autopas
