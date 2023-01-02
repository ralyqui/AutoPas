/**
 * @file MoleculeAT.h
 * @date 31.12.2022
 * @author ralyqui
 */

#pragma once

#include "autopas/particles/Particle.h"

namespace autopas {

class MoleculeAT final : public Particle {
 public:
  MoleculeAT() = default;

  ~MoleculeAT() final = default;

  /**
   * Constructor of the Axilrod-Teller molecule
   * @param pos Position of the molecule.
   * @param v Velocitiy of the molecule.
   * @param moleculeId Id of the molecule.
   */
  explicit MoleculeAT(std::array<double, 3> pos, std::array<double, 3> v, unsigned long moleculeId)
      : Particle(pos, v, moleculeId){};
};

}  // namespace autopas
