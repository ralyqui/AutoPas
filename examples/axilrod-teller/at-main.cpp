/**
 * @file at-main.cpp
 * @date 02.01.2023
 * @author ralyqui
 */

#include <iostream>

#include "autopas/AutoPas.h"
#include "autopasTools/generators/RandomGenerator.h"
#include "autopas/molecularDynamics/LJFunctor.h"

using Particle = autopas::MoleculeLJ<>;
using AutoPasContainer = autopas::AutoPas<Particle>;

void fillParticleContainer(AutoPasContainer &container) {
  int id = 0;
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      for (int k = 0; k < 10; ++k) {
        Particle p(
            autopasTools::generators::RandomGenerator::randomPosition(container.getBoxMin(), container.getBoxMax()),
            {0., 0., 0.}, id++);
        container.addParticle(p);
      }
    }
  }
}

int main() {
  AutoPasContainer autopas;
  autopas.setBoxMax({10, 10, 10});
  autopas.init();

  fillParticleContainer(autopas);

  int numIterations = 100;
  double cutoff = 1.;
  autopas::LJFunctor<Particle> functor(cutoff);
  for(int i = 0; i < numIterations; i++) {
        autopas.iteratePairwise(&functor);
  }


  return 0;
}