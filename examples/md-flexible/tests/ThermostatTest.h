/**
 * @file ThermostatTest.h
 * @author N. Fottner
 * @date 28/08/19.
 */
#pragma once
#include "AutoPasTestBase.h"
#include "Generator.h"
#include "Objects/Objects.h"
#include "PrintableMolecule.h"
#include "Thermostat.h"
#include "TimeDiscretization.h"
#include "autopas/AutoPas.h"
#include "autopas/molecularDynamics/ParticlePropertiesLibrary.h"
#include "autopas/utils/ArrayUtils.h"
#include "testingHelpers/commonTypedefs.h"

class ThermostatTest : public AutoPasTestBase {
 public:
  using AutoPasType = autopas::AutoPas<Molecule, autopas::FullParticleCell<Molecule>>;
  using ThermostatType = Thermostat<AutoPasType, ParticlePropertiesLibrary<double, size_t>>;

  ThermostatTest() : AutoPasTestBase(), _particlePropertiesLibrary(ParticlePropertiesLibrary<double, size_t>()) {
    _particlePropertiesLibrary.addType(0, 1., 1., 1.); /*initializing the default particlePropertiesLibrary*/
  }

 protected:
  /**
   * Fills an autopas container with a given grid of copies of the dummy particle and initializes the container.
   * @param autopas
   * @param dummy
   * @param particlesPerDim
   */
  void initContainer(AutoPasType &autopas, const Molecule &dummy, std::array<size_t, 3> particlesPerDim);

  /**
   * Applies brownian motion to a system and checks that all velocities have changed.
   * @param dummyMolecule
   * @param useCurrentTemp
   */
  void testBrownianMotion(const Molecule &dummyMolecule, bool useCurrentTemp);

  ParticlePropertiesLibrary<double, size_t> _particlePropertiesLibrary;
  AutoPasType _autopas;
};