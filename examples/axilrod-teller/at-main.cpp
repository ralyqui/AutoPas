/**
 * @file at-main.cpp
 * @date 02.01.2023
 * @author ralyqui
 */

#include <iostream>

#include "autopas/AutoPas.h"
#include "autopas/molecularDynamics/ATFunctor.h"
#include "autopas/molecularDynamics/MoleculeAT.h"
#include "autopasTools/generators/RandomGenerator.h"

using Particle = autopas::MoleculeAT;
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
  autopas::ATFunctor<Particle> functor(cutoff, 0);
  for(int i = 0; i < numIterations; i++) {
        autopas.iterateTriplets(&functor);
  }

  return 0;
}