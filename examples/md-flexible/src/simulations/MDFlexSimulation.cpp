/**
 * @file MDFlexSimulation.cpp
 * @author J. Körner
 * @date 07.04.2021
 */
#include "MDFlexSimulation.h"

#include "../Checkpoint.h"
#include "../parsing/MDFlexConfig.h"
#include "../Thermostat.h"
#include "autopas/molecularDynamics/LJFunctor.h"
#include "autopas/molecularDynamics/LJFunctorAVX.h"
#include "autopas/pairwiseFunctors/FlopCounterFunctor.h"

#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

MDFlexSimulation::~MDFlexSimulation() {
  if (_configuration->dontCreateEndConfig.value) {
    std::ofstream configFileEnd("MDFlex_end_" + autopas::utils::Timer::getDateStamp() + ".yaml");
    if (configFileEnd.is_open()) {
      configFileEnd << "# Generated by:" << std::endl;
      configFileEnd << "# ";
      for (int i = 0; i < _argc; ++i) {
        configFileEnd << _argv[i] << " ";
      }
      configFileEnd << std::endl;
      configFileEnd << *_configuration;
      configFileEnd.close();
    }
  }
}

void MDFlexSimulation::initialize(int dimensionCount, int argc, char **argv) {
 	_timers.total.start(); 
	_timers.initialization.start();

	_argc = argc;
	_argv = argv;

  _configuration = std::make_shared<MDFlexConfig>(argc, argv);

	initializeDomainDecomposition(dimensionCount);

	initializeParticlePropertiesLibrary();

	initializeAutoPasContainer();

	initializeObjects();

	_timers.initialization.stop();
}

std::tuple<size_t, bool> MDFlexSimulation::estimateNumberOfIterations() const {
  if (_configuration->tuningPhases.value > 0) {
    // @TODO: this can be improved by considering the tuning strategy
    // This is just a randomly guessed number but seems to fit roughly for default settings.
    size_t configsTestedPerTuningPhase = 90;
    if (_configuration->tuningStrategyOption.value == autopas::TuningStrategyOption::bayesianSearch or
        _configuration->tuningStrategyOption.value == autopas::TuningStrategyOption::bayesianClusterSearch) {
      configsTestedPerTuningPhase = _configuration->tuningMaxEvidence.value;
    }
    auto estimate = (_configuration->tuningPhases.value - 1) * _configuration->tuningInterval.value +
                    (_configuration->tuningPhases.value * _configuration->tuningSamples.value * configsTestedPerTuningPhase);
    return {estimate, false};
  } else {
    return {_configuration->iterations.value, true};
  }
}

bool MDFlexSimulation::needsMoreIterations() {
  return
		_iteration < _configuration->iterations.value
		or _numTuningPhasesCompleted < _configuration->tuningPhases.value;
}

void MDFlexSimulation::globalForces() {
  // skip application of zero force
  if (_configuration->globalForceIsZero()) {
    return;
  }

#ifdef AUTOPAS_OPENMP
#pragma omp parallel default(none) shared(_autoPasContainer)
#endif
  for (auto particle = _autoPasContainer->begin(autopas::IteratorBehavior::owned); particle.isValid(); ++particle) {
    particle->addF(_configuration->globalForce.value);
  }
}

void MDFlexSimulation::printProgress(size_t iterationProgress, size_t maxIterations, bool maxIsPrecise) {
  // percentage of iterations complete
  double fractionDone = static_cast<double>(iterationProgress) / maxIterations;

  // length of the number of maxIterations
  size_t numCharsOfMaxIterations = std::to_string(maxIterations).size();

  // trailing information string
  std::stringstream info;
  info << std::setw(3) << std::round(fractionDone * 100) << "% " << std::setw(numCharsOfMaxIterations)
       << iterationProgress << "/";
  if (not maxIsPrecise) {
    info << "~";
  }
  info << maxIterations;

  // actual progress bar
  std::stringstream progressbar;
  progressbar << "[";
  // get current terminal width
  struct winsize w {};
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
  auto terminalWidth = w.ws_col;
  // the bar should fill the terminal window so subtract everything else (-2 for "] ")
  size_t maxBarWidth = terminalWidth - info.str().size() - progressbar.str().size() - 2;
  // sanity check for underflow
  if (maxBarWidth > terminalWidth) {
    std::cerr << "Warning! Terminal width appears to be too small or could not be read. Disabling progress bar."
              << std::endl;
    _configuration->dontShowProgressBar.value = true;
    return;
  }
  auto barWidth =
      std::max(std::min(static_cast<decltype(maxBarWidth)>(maxBarWidth * (fractionDone)), maxBarWidth), 1ul);
  // don't print arrow tip if >= 100%
  if (iterationProgress >= maxIterations) {
    progressbar << std::string(barWidth, '=');
  } else {
    progressbar << std::string(barWidth - 1, '=') << '>' << std::string(maxBarWidth - barWidth, ' ');
  }
  progressbar << "] ";
  // clear current line (=delete previous progress bar)
  std::cout << std::string(terminalWidth, '\r');
  // print everything
  std::cout << progressbar.str() << info.str() << std::flush;
}

void MDFlexSimulation::printStatistics() {
  using namespace std;
  size_t flopsPerKernelCall;

  switch (_configuration->functorOption.value) {
    case MDFlexConfig::FunctorOption ::lj12_6: {
      flopsPerKernelCall = autopas::LJFunctor<ParticleType, _shifting, _mixing>::getNumFlopsPerKernelCall();
      break;
    }
    case MDFlexConfig::FunctorOption ::lj12_6_Globals: {
      flopsPerKernelCall = autopas::LJFunctor<ParticleType, _shifting, _mixing, autopas::FunctorN3Modes::Both,
                                              /* globals */ true>::getNumFlopsPerKernelCall();
      break;
    }
    case MDFlexConfig::FunctorOption ::lj12_6_AVX: {
      flopsPerKernelCall = autopas::LJFunctorAVX<ParticleType, _shifting, _mixing>::getNumFlopsPerKernelCall();
      break;
    }
    default:
      throw std::runtime_error("Invalid Functor choice");
  }

  auto durationTotal = _timers.total.stop();
  auto durationSimulate = _timers.simulate.getTotalTime();
  auto durationSimulateSec = durationSimulate * 1e-9;

  // take total time as base for formatting since this should be the longest
  auto digitsTimeTotalNS = std::to_string(durationTotal).length();

  // Statistics
  cout << endl;
  cout << "Total number of particles at end of Simulation: "
       << _autoPasContainer->getNumberOfParticles(autopas::IteratorBehavior::ownedOrHalo) << endl;
  cout << "  Owned: " << _autoPasContainer->getNumberOfParticles(autopas::IteratorBehavior::owned) << endl;
  cout << "  Halo : " << _autoPasContainer->getNumberOfParticles(autopas::IteratorBehavior::halo) << endl;
  cout << "Standard Deviation of Homogeneity    : " << _homogeneity << endl;

  cout << fixed << setprecision(_floatStringPrecision);
  cout << "Measurements:" << endl;
  cout << timerToString("Time total      ", durationTotal, digitsTimeTotalNS, durationTotal);
  cout << timerToString("  Initialization", _timers.initialization.getTotalTime(), digitsTimeTotalNS, durationTotal);
  cout << timerToString("  Simulation    ", durationSimulate, digitsTimeTotalNS, durationTotal);
  cout << timerToString("    Boundaries  ", _timers.boundaries.getTotalTime(), digitsTimeTotalNS, durationSimulate);
  cout << timerToString("    Position    ", _timers.positionUpdate.getTotalTime(), digitsTimeTotalNS, durationSimulate);
  cout << timerToString("    Force       ", _timers.forceUpdateTotal.getTotalTime(), digitsTimeTotalNS,
                        durationSimulate);
  cout << timerToString("      Tuning    ", _timers.forceUpdateTuning.getTotalTime(), digitsTimeTotalNS,
                        _timers.forceUpdateTotal.getTotalTime());
  cout << timerToString("      NonTuning ", _timers.forceUpdateNonTuning.getTotalTime(), digitsTimeTotalNS,
                        _timers.forceUpdateTotal.getTotalTime());
  cout << timerToString("    Velocity    ", _timers.velocityUpdate.getTotalTime(), digitsTimeTotalNS, durationSimulate);
  cout << timerToString("    VTK         ", _timers.vtk.getTotalTime(), digitsTimeTotalNS, durationSimulate);
  cout << timerToString("    Thermostat  ", _timers.thermostat.getTotalTime(), digitsTimeTotalNS, durationSimulate);

  cout << timerToString("One iteration   ", _timers.simulate.getTotalTime() / _iteration, digitsTimeTotalNS,
                        durationTotal);
  auto mfups = _autoPasContainer->getNumberOfParticles(autopas::IteratorBehavior::owned) * _iteration * 1e-6 /
               (_timers.forceUpdateTotal.getTotalTime() * 1e-9);  // 1e-9 for ns to s, 1e-6 for M in MFUP
  cout << "Tuning iterations: " << _numTuningIterations << " / " << _iteration << " = "
       << ((double)_numTuningIterations / _iteration * 100) << "%" << endl;
  cout << "MFUPs/sec    : " << mfups << endl;

  if (_configuration->dontMeasureFlops.value) {
    autopas::FlopCounterFunctor<ParticleType> flopCounterFunctor(_autoPasContainer->getCutoff());
    _autoPasContainer->iteratePairwise(&flopCounterFunctor);

    auto flops = flopCounterFunctor.getFlops(flopsPerKernelCall) * _iteration;
    // approximation for flops of verlet list generation
    if (_autoPasContainer->getContainerType() == autopas::ContainerOption::verletLists)
      flops += flopCounterFunctor.getDistanceCalculations() *
               decltype(flopCounterFunctor)::numFlopsPerDistanceCalculation *
               floor(_iteration / _configuration->verletRebuildFrequency.value);

    cout << "GFLOPs       : " << flops * 1e-9 << endl;
    cout << "GFLOPs/sec   : " << flops * 1e-9 / durationSimulateSec << endl;
    cout << "Hit rate     : " << flopCounterFunctor.getHitRate() << endl;
  }
}

void MDFlexSimulation::writeVTKFile() {
  _timers.vtk.start();

  std::string fileBaseName = _configuration->vtkFileName.value;
  // only count number of owned particles here
  const auto numParticles = _autoPasContainer->getNumberOfParticles(autopas::IteratorBehavior::owned);
  std::ostringstream strstr;
  auto maxNumDigits = std::to_string(_configuration->iterations.value).length();
  strstr << fileBaseName << "_" << getMPISuffix() << std::setfill('0') << std::setw(maxNumDigits) << _iteration
         << ".vtk";
  std::ofstream vtkFile;
  vtkFile.open(strstr.str());

  if (not vtkFile.is_open()) {
    throw std::runtime_error("Simulation::writeVTKFile(): Failed to open file \"" + strstr.str() + "\"");
  }

  vtkFile << "# vtk DataFile Version 2.0\n"
          << "Timestep\n"
          << "ASCII\n";

  // print positions
  vtkFile << "DATASET STRUCTURED_GRID\n"
          << "DIMENSIONS 1 1 1\n"
          << "POINTS " << numParticles << " double\n";

  for (auto iter = _autoPasContainer->begin(autopas::IteratorBehavior::owned); iter.isValid(); ++iter) {
    auto pos = iter->getR();
    vtkFile << pos[0] << " " << pos[1] << " " << pos[2] << "\n";
  }
  vtkFile << "\n";

  vtkFile << "POINT_DATA " << numParticles << "\n";
  // print velocities
  vtkFile << "VECTORS velocities double\n";
  for (auto iter = _autoPasContainer->begin(autopas::IteratorBehavior::owned); iter.isValid(); ++iter) {
    auto v = iter->getV();
    vtkFile << v[0] << " " << v[1] << " " << v[2] << "\n";
  }
  vtkFile << "\n";

  // print Forces
  vtkFile << "VECTORS forces double\n";
  for (auto iter = _autoPasContainer->begin(autopas::IteratorBehavior::owned); iter.isValid(); ++iter) {
    auto f = iter->getF();
    vtkFile << f[0] << " " << f[1] << " " << f[2] << "\n";
  }
  vtkFile << "\n";

  // print TypeIDs
  vtkFile << "SCALARS typeIds int\n";
  vtkFile << "LOOKUP_TABLE default\n";
  for (auto iter = _autoPasContainer->begin(autopas::IteratorBehavior::owned); iter.isValid(); ++iter) {
    vtkFile << iter->getTypeId() << "\n";
  }
  vtkFile << "\n";

  // print TypeIDs
  vtkFile << "SCALARS particleIds int\n";
  vtkFile << "LOOKUP_TABLE default\n";
  for (auto iter = _autoPasContainer->begin(autopas::IteratorBehavior::owned); iter.isValid(); ++iter) {
    vtkFile << iter->getID() << "\n";
  }
  vtkFile << "\n";

  vtkFile.close();

  _timers.vtk.stop();
}

std::string MDFlexSimulation::getMPISuffix() {
  std::string suffix;
#ifdef AUTOPAS_INTERNODE_TUNING
  int rank;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  std::ostringstream output;
  output << "mpi_rank_" << rank << "_";
  suffix = output.str();
#endif
  return suffix;
}

std::string MDFlexSimulation::timerToString(const std::string &name, long timeNS, size_t numberWidth, long maxTime) {
  // only print timers that were actually used
  if (timeNS == 0) {
    return "";
  }

  std::ostringstream ss;
  ss << std::fixed << std::setprecision(_floatStringPrecision) << name << " : " << std::setw(numberWidth) << std::right
     << timeNS
     << " ns ("
     // min width of the representation of seconds is numberWidth - 9 (from conversion) + 4 (for dot and digits after)
     << std::setw(numberWidth - 5ul) << ((double)timeNS * 1e-9) << "s)";
  if (maxTime != 0) {
    ss << " =" << std::setw(7) << std::right << ((double)timeNS / (double)maxTime * 100) << "%";
  }
  ss << std::endl;
  return ss.str();
}

void MDFlexSimulation::calculatePositions() {
  using autopas::utils::ArrayMath::add;
  using autopas::utils::ArrayMath::mulScalar;

	const double deltaT = _configuration->deltaT.value;

#ifdef AUTOPAS_OPENMP
#pragma omp parallel
#endif
  for (auto iter = _autoPasContainer->begin(autopas::IteratorBehavior::owned); iter.isValid(); ++iter) {
    auto v = iter->getV();
    auto m = _particlePropertiesLibrary->getMass(iter->getTypeId());
    auto f = iter->getF();
    iter->setOldF(f);
    iter->setF({0., 0., 0.});
    v = mulScalar(v, deltaT);
    f = mulScalar(f, (deltaT * deltaT / (2 * m)));
    auto newR = add(v, f);
    iter->addR(newR);
  }
}

void MDFlexSimulation::initializeParticlePropertiesLibrary(){
  if (_configuration->epsilonMap.value.empty()) {
    throw std::runtime_error("No properties found in particle properties library!");
  }

  if (_configuration->epsilonMap.value.size() != _configuration->sigmaMap.value.size() or
      _configuration->epsilonMap.value.size() != _configuration->massMap.value.size()) {
    throw std::runtime_error("Number of particle properties differ!");
  }

  _particlePropertiesLibrary = std::make_shared<ParticlePropertiesLibraryType>(_configuration->cutoff.value);

  for (auto [type, epsilon] : _configuration->epsilonMap.value) {
    _particlePropertiesLibrary->addType(type, epsilon, _configuration->sigmaMap.value.at(type),
                                        _configuration->massMap.value.at(type));
  }
  _particlePropertiesLibrary->calculateMixingCoefficients();
}

void MDFlexSimulation::initializeAutoPasContainer() {
  if (_configuration->logFileName.value.empty()) {
  	_outputStream = &std::cout;
  } else {
  	_logFile = std::make_shared<std::ofstream>();
		_logFile->open(_configuration->logFileName.value);
  	_outputStream = &(*_logFile);
  }

  _autoPasContainer = std::make_shared<autopas::AutoPas<ParticleType>>(*_outputStream);
  _autoPasContainer->setAllowedCellSizeFactors(*_configuration->cellSizeFactors.value);
  _autoPasContainer->setAllowedContainers(_configuration->containerOptions.value);
  _autoPasContainer->setAllowedDataLayouts(_configuration->dataLayoutOptions.value);
  _autoPasContainer->setAllowedNewton3Options(_configuration->newton3Options.value);
  _autoPasContainer->setAllowedTraversals(_configuration->traversalOptions.value);
  _autoPasContainer->setAllowedLoadEstimators(_configuration->loadEstimatorOptions.value);
  _autoPasContainer->setBoxMin(_configuration->boxMin.value);
  _autoPasContainer->setBoxMax(_configuration->boxMax.value);
  _autoPasContainer->setCutoff(_configuration->cutoff.value);
  _autoPasContainer->setRelativeOptimumRange(_configuration->relativeOptimumRange.value);
  _autoPasContainer->setMaxTuningPhasesWithoutTest(_configuration->maxTuningPhasesWithoutTest.value);
  _autoPasContainer->setRelativeBlacklistRange(_configuration->relativeBlacklistRange.value);
  _autoPasContainer->setEvidenceFirstPrediction(_configuration->evidenceFirstPrediction.value);
  _autoPasContainer->setExtrapolationMethodOption(_configuration->extrapolationMethodOption.value);
  _autoPasContainer->setNumSamples(_configuration->tuningSamples.value);
  _autoPasContainer->setMaxEvidence(_configuration->tuningMaxEvidence.value);
  _autoPasContainer->setSelectorStrategy(_configuration->selectorStrategy.value);
  _autoPasContainer->setTuningInterval(_configuration->tuningInterval.value);
  _autoPasContainer->setTuningStrategyOption(_configuration->tuningStrategyOption.value);
  _autoPasContainer->setMPIStrategy(_configuration->mpiStrategyOption.value);
  _autoPasContainer->setVerletClusterSize(_configuration->verletClusterSize.value);
  _autoPasContainer->setVerletRebuildFrequency(_configuration->verletRebuildFrequency.value);
  _autoPasContainer->setVerletSkin(_configuration->verletSkinRadius.value);
  _autoPasContainer->setAcquisitionFunction(_configuration->acquisitionFunctionOption.value);
  _autoPasContainer->init();
}

void MDFlexSimulation::initializeObjects(){
  if (not _configuration->checkpointfile.value.empty()) {
    Checkpoint::loadParticles(*_autoPasContainer, _configuration->checkpointfile.value);
  }

  for (const auto &object : _configuration->cubeGridObjects) {
    object.generate(*_autoPasContainer);
  }
  for (const auto &object : _configuration->cubeGaussObjects) {
    object.generate(*_autoPasContainer);
  }
  for (const auto &object : _configuration->cubeUniformObjects) {
    object.generate(*_autoPasContainer);
  }
  for (const auto &object : _configuration->sphereObjects) {
    object.generate(*_autoPasContainer);
  }
  for (const auto &object : _configuration->cubeClosestPackedObjects) {
    object.generate(*_autoPasContainer);
  }

  if (_configuration->useThermostat.value and _configuration->deltaT.value != 0) {
    if (_configuration->addBrownianMotion.value) {
      Thermostat::addBrownianMotion(*_autoPasContainer, *_particlePropertiesLibrary,
				_configuration->initTemperature.value);
    }
    Thermostat::apply(*_autoPasContainer, *_particlePropertiesLibrary, _configuration->initTemperature.value,
                      std::numeric_limits<double>::max());
  }
}
