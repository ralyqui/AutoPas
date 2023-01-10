/**
 * @file TripletCellFunctor.h
 * @date 09.01.2023
 * @author ralyqui
 */

#pragma once

#include "autopas/tripletFunctors/TripletFunctor.h"

namespace autopas {

template <class Particle, class ParticleCell, class Functor, bool useNewton3 = true>
class TripletCellFunctor {
 public:
  explicit TripletCellFunctor(Functor *f, double cutoff) : _functor{f}, _cutoff{cutoff} {};

  void processCell(ParticleCell &cell);

 private:
  Functor *_functor;
  double _cutoff;

};

template <class Particle, class ParticleCell, class Functor, bool useNewton3>
void TripletCellFunctor<Particle, ParticleCell, Functor, useNewton3>::processCell(ParticleCell &cell) {
  std::cout << "Called process cell but not doing anything" << std::endl;
}

};  // namespace autopas