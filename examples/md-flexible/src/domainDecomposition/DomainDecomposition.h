/**
 * @file DomainDecomposition.h
 * @author J. Körner
 * @date 19.04.2021
 */
#pragma once

#include <array>

#include "autopas/AutoPasDecl.h"
#include "src/TypeDefinitions.h"

/**
 * An interface for domain decompositions which can be used in the simulation
 */
class DomainDecomposition {
 public:
  /**
   * Destructor.
   */
  virtual ~DomainDecomposition() = default;

  /**
   * Type for the AutoPas container
   */
  using AutoPasType = autopas::AutoPas<ParticleType>;

  /**
   * Used to update the domain to the current topology.
   * Handles the diffuse load balancing by resizing the domains according to their work done.
   * @param work: The work performed in the AutoPas container.
   */
  virtual void update(const double &work) = 0;

  /**
   * Returns the index of the local domain in the global domain context.
   * @return domain index.
   */
  [[nodiscard]] virtual int getDomainIndex() const = 0;

  /**
   * Returns the minimum coordinates of the global domain.
   * @return bottom left front corner of the global domain.
   */
  [[nodiscard]] virtual std::array<double, 3> getGlobalBoxMin() const = 0;

  /**
   * Returns the maximum coordinates of the global domain.
   * @return top right back corner of the global domain.
   */
  [[nodiscard]] virtual std::array<double, 3> getGlobalBoxMax() const = 0;

  /**
   * Returns the minimum coordinates of the local domain.
   * @return bottom left front corner of the local domain.
   */
  [[nodiscard]] virtual std::array<double, 3> getLocalBoxMin() const = 0;

  /**
   * Returns the maximum coordinates of the local domain.
   * @return top right back corner of the local domain.
   */
  [[nodiscard]] virtual std::array<double, 3> getLocalBoxMax() const = 0;

  /**
   * Checks if the provided coordinates are located in the local domain.
   * @param coordinates: The coordinates in question.
   * @return true if the coordinates lie inside the local domain, false otherwise.
   */
  [[nodiscard]] virtual bool isInsideLocalDomain(const std::array<double, 3> &coordinates) const = 0;
};
