/**
 * @file RegularGrid.h
 * @author J. Körner
 * @date 19.04.2021
 */
#pragma once

#include <list>
#include <memory>

#include "DomainDecomposition.h"
#include "autopas/AutoPas.h"
#include "mpi.h"
#include "src/TypeDefinitions.h"

/**
 * This class can be used as a domain decomposition which divides the domain in equal sized rectangular subdomains.
 * The number of subdomains is equal to the number of MPI processes available.
 */
class RegularGrid final : public DomainDecomposition {
 public:
  /**
   * Constructor.
   * @param argc The argument count passed to the main function.
   * @param argv The argument vector passed to the main function.
   * @param dimensionCount The number of dimensions for this domain decomposition.
   * @param globalBoxMin The minimum coordinates of the global domain.
   * @param globalBoxMax The maximum coordinates of the global domain.
   */
  RegularGrid(const int &dimensionCount, const std::vector<double> &globalBoxMin,
              const std::vector<double> &globalBoxMax);

  /**
   * Destructor.
   */
  ~RegularGrid();

  /**
   * Type for the AutoPas container
   */
  using SharedAutoPasContainer = std::shared_ptr<autopas::AutoPas<ParticleType>>;

  /**
   * Used to update the domain to the current topology.
   * Currently does nothing
   */
  void update() override;

  /**
   * Returns the number of dimesnions in the domain decomposition.
   */
  const int getDimensionCount() override { return _dimensionCount; }

  /**
   * Returns the minimum coordinates of global domain.
   */
  std::vector<double> getGlobalBoxMin() override { return _globalBoxMin; }

  /**
   * Returns the maximum coordinates of global domain.
   */
  std::vector<double> getGlobalBoxMax() override { return _globalBoxMax; }

  /**
   * Returns the minimum coordinates of local domain.
   */
  std::vector<double> getLocalBoxMin() override { return _localBoxMin; }

  /**
   * Returns the maximum coordinates of local domain.
   */
  std::vector<double> getLocalBoxMax() override { return _localBoxMax; }

  /**
   * Sets the halo width.
   * If it is not set manually the halo width depends on the size of the local box.
   */
  void setHaloWidth(double width);

  /**
   * Checks if the provided coordinates are located in the local domain.
   */
  bool isInsideLocalDomain(const std::vector<double> &coordinates) override;

  /**
   * Converts a domain id to the domain index, i.e. rank of the local processor.
   */
  int convertIdToIndex(const std::vector<int> &domainIndex);

  /**
   * Exchanges halo particles with all neighbours of the provided AutoPasContainer.
   * @param autoPasContainer The container, where the halo particles originate from.
   */
  void exchangeHaloParticles(SharedAutoPasContainer &autoPasContainer);

  /**
   * Exchanges migrating particles with all neighbours of the provided AutoPasContainer.
   * @param autoPasContainer The container, where the migrating particles originate from.
   */
  void exchangeMigratingParticles(SharedAutoPasContainer &autoPasContainer);

  /**
   * Received data which has been sent by a specifig neighbour of this domain.
   * @param neighbour The neighbour where the data originates from.
   * @param dataBuffer The buffer where the received data will be stored.
   */
  void receiveDataFromNeighbour(const int &neighbour, std::vector<char> &dataBuffer);

  /**
   * Sends data to a specific neighbour of this domain.
   * @param sendBuffer The buffer which will be sent to the neighbour.
   * @param neighbour The neighbour to which the data will be sent.
   */
  void sendDataToNeighbour(std::vector<char> sendBuffer, const int &neighbour);

  /**
   * Waits for all send requests to be finished.
   */
  void waitForSendRequests();

  /**
   * Returns this domain's index / the processor's rank.
   */
  int getDomainIndex() { return _domainIndex; }

  /**
   * Returns the number of domains in each dimension
   */
  std::vector<int> getDecomposition() { return _decomposition; }

 private:
  /**
   * The number of dimensions in this decomposition.
   */
  int _dimensionCount;

  /**
   * The number of subdomains in this decomposition.
   */
  int _subdomainCount;

  /**
   * The minimum coordinates of the global domain.
   */
  std::vector<double> _globalBoxMin;

  /**
   * The maximum coordinates of the global domain.
   */
  std::vector<double> _globalBoxMax;

  /**
   * The decomposition computed depending on the number of subdomains.
   */
  std::vector<int> _decomposition;

  /**
   * The MPI communicator containing all processes which own a subdomain in this decomposition.
   */
  MPI_Comm _communicator;

  /**
   * Stores the halo width.
   */
  double _haloWidth;

  /**
   * The index of the current processor's domain.
   * This also is the rank of the current processor.
   */
  int _domainIndex;

  /**
   * The ID of the current processor's domain.
   */
  std::vector<int> _domainId;

  /**
   * The indices of the local domain's neighbours.
   * These correspond to the ranks of the processors which own the neigbour domain.
   */
  std::vector<int> _neighbourDomainIndices;

  /**
   * The minimum cooridnates of the local domain.
   */
  std::vector<double> _localBoxMin;

  /**
   * The maximum cooridnates of the local domain.
   */
  std::vector<double> _localBoxMax;

  /**
   * A temporary buffer used for MPI send requests.
   */
  std::vector<MPI_Request> _sendRequests;

  /**
   * A temporary buffer for data which is sent by MPI_Send.
   */
  std::vector<std::vector<char>> _sendBuffers;

  /**
   * Initializes the domain decomposition.
   * This needs to be called before initialzieMPICommunicator.
   */
  void initializeDecomposition();

  /**
   * Initializes the MPI communicator.
   * This needs to be called before initializeLocalDomain.
   */
  void initializeMPICommunicator();

  /**
   * Initializes the local domain.
   * This needs to be called before initializeLocalBox.
   */
  void initializeLocalDomain();

  /**
   * Initializes the global domain coordinates.
   */
  void initializeGlobalBox(const std::vector<double> &globalBoxMin, const std::vector<double> &globalBoxMax);

  /**
   * Initializes the local domain coordinates.
   * This needs to be called after initializeLocalDomain and initialzieGlobalDomain.
   */
  void initializeLocalBox();

  /**
   * Initializes the neighbour ids.
   * This needs to be called after initializeLocalDomain.
   */
  void initializeNeighbourIds();

  /**
   * Updates the local box.
   */
  void updateLocalBox();

  /**
   * Sends particles of type ParticleType to a receiver.
   * @param particles The particles to be sent to the receiver.
   * @param receiver The recipient of the particels.
   */
  void sendParticles(std::vector<ParticleType> &particles, int &receiver);

  /**
   * Received particles sent by a sender.
   * @param receivedParticles The container where the received particles will be stored.
   * @param source The sender id/rank.
   */
  void receiveParticles(std::vector<ParticleType> &receivedParticles, int &source);
};
