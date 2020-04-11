/**
 * @file PredictiveTuning.h
 * @author Julian Pelloth
 * @date 01.04.2020
 */

#pragma once

#include <set>
#include <sstream>

#include "TuningStrategyInterface.h"
#include "autopas/containers/CompatibleTraversals.h"
#include "autopas/selectors/OptimumSelector.h"
#include "autopas/utils/ExceptionHandler.h"

namespace autopas {

/**
 * Exhaustive full search of the search space by testing every applicable configuration and then selecting the optimum.
 */
class PredictiveTuning : public TuningStrategyInterface {
public:
    /**
    * Constructor for the PredictiveTuning that generates the search space from the allowed options.
    * @param allowedContainerOptions
    * @param allowedTraversalOptions
    * @param allowedDataLayoutOptions
    * @param allowedNewton3Options
    * @param allowedCellSizeFactors
    */
    PredictiveTuning(const std::set<ContainerOption> &allowedContainerOptions, const std::set<double> &allowedCellSizeFactors,
                    const std::set<TraversalOption> &allowedTraversalOptions,
                    const std::set<DataLayoutOption> &allowedDataLayoutOptions,
                    const std::set<Newton3Option> &allowedNewton3Options)
                : _containerOptions(allowedContainerOptions) {
        // sets search space and current config
        populateSearchSpace(allowedContainerOptions, allowedCellSizeFactors, allowedTraversalOptions,
                            allowedDataLayoutOptions, allowedNewton3Options);
    }

    /**
    * Constructor for the PredictiveTuning that only contains the given configurations.
    * This constructor assumes only valid configurations are passed! Mainly for easier unit testing.
    * @param allowedConfigurations Set of configurations AutoPas can choose from.
    */
    explicit PredictiveTuning(std::set<Configuration> allowedConfigurations)
                : _containerOptions{}, _searchSpace(std::move(allowedConfigurations)), _currentConfig(_searchSpace.begin()) {
        for (auto config : _searchSpace) {
            _containerOptions.insert(config.container);
        }
    }

    inline const Configuration &getCurrentConfiguration() const override { return *_currentConfig; }

    inline void removeN3Option(Newton3Option badNewton3Option) override;

    inline void addEvidence(long time) override {
        _traversalTimes[*_currentConfig] = time;
        _traversalTimesStorage[*_currentConfig].emplace_back(std::make_pair(_timer, time));
    }

    inline void reset() override {
        _traversalTimes.clear();
        _traversalPredictions.clear();
        _optimalSearchSpace.clear();
        selectPossibleConfigurations();

        // sanity check
        if(_optimalSearchSpace.empty()){
            autopas::utils::ExceptionHandler::exception(
                    "PredicitveTuning: No possible configuration prediction found!");
        }

        _currentConfig = _searchSpace.begin();
        while(_optimalSearchSpace.count(*_currentConfig) == 0 && _currentConfig != _searchSpace.end()) {
            ++_currentConfig;
        }
    }

    inline bool tune(bool = false) override;

    inline std::set<ContainerOption> getAllowedContainerOptions() const override { return _containerOptions; }

    inline bool searchSpaceIsTrivial() const override { return _searchSpace.size() == 1; }

    inline bool searchSpaceIsEmpty() const override { return _searchSpace.empty(); }

    //Functions are there for unit testing right now - should be deleted afterwards.
    inline std::set<Configuration> getOptimalSearchSpace() { return _optimalSearchSpace; }
    inline std::unordered_map<Configuration, size_t, ConfigHash>  getTraversalPredictions () { return _traversalPredictions; }

    private:
    /**
    * Fills the search space with the cartesian product of the given options (minus invalid combinations).
    * @param allowedContainerOptions
    * @param allowedTraversalOptions
    * @param allowedDataLayoutOptions
    * @param allowedNewton3Options
    */
    inline void populateSearchSpace(const std::set<ContainerOption> &allowedContainerOptions,
                                    const std::set<double> &allowedCellSizeFactors,
                                    const std::set<TraversalOption> &allowedTraversalOptions,
                                    const std::set<DataLayoutOption> &allowedDataLayoutOptions,
                                    const std::set<Newton3Option> &allowedNewton3Options);

    inline void selectOptimalConfiguration();

    inline void selectPossibleConfigurations();

    inline void predictConfigurations();

    inline void linePrediction();

    std::set<ContainerOption> _containerOptions;
    std::set<Configuration> _searchSpace;
    std::set<Configuration>::iterator _currentConfig;
    std::unordered_map<Configuration, size_t, ConfigHash> _traversalTimes;
    std::unordered_map<Configuration, std::vector<std::pair<int, size_t>>, ConfigHash> _traversalTimesStorage;
    std::unordered_map<Configuration, size_t, ConfigHash> _traversalPredictions;
    std::set<Configuration> _optimalSearchSpace;
    int _timer = 0;
};

void PredictiveTuning::populateSearchSpace(const std::set<ContainerOption> &allowedContainerOptions,
                                         const std::set<double> &allowedCellSizeFactors,
                                         const std::set<TraversalOption> &allowedTraversalOptions,
                                         const std::set<DataLayoutOption> &allowedDataLayoutOptions,
                                         const std::set<Newton3Option> &allowedNewton3Options) {
    // generate all potential configs
    for (auto &containerOption : allowedContainerOptions) {
        // get all traversals of the container and restrict them to the allowed ones
        const std::set<TraversalOption> &allContainerTraversals = compatibleTraversals::allCompatibleTraversals(containerOption);
        std::set<TraversalOption> allowedAndApplicable;
        std::set_intersection(allowedTraversalOptions.begin(), allowedTraversalOptions.end(),
                              allContainerTraversals.begin(), allContainerTraversals.end(),
                              std::inserter(allowedAndApplicable, allowedAndApplicable.begin()));

        for (auto &cellSizeFactor : allowedCellSizeFactors)
            for (auto &traversalOption : allowedAndApplicable) {
                for (auto &dataLayoutOption : allowedDataLayoutOptions) {
                    for (auto &newton3Option : allowedNewton3Options) {
                        _searchSpace.emplace(containerOption, cellSizeFactor, traversalOption, dataLayoutOption, newton3Option);
                    }
                }
            }
    }

    AutoPasLog(debug, "Points in search space: {}", _searchSpace.size());

    if (_searchSpace.empty()) {
        autopas::utils::ExceptionHandler::exception("PredictiveTuning: No valid configurations could be created.");
    }

    for(auto &configuration : _searchSpace){
        std::vector<std::pair<int, size_t>> vector;
        _traversalTimesStorage.emplace(configuration, vector);
    }

    _currentConfig = _searchSpace.begin();
}

bool PredictiveTuning::tune(bool) {
    // repeat as long as traversals are not applicable or we run out of configs
    do {
        ++_currentConfig;
    } while(_optimalSearchSpace.count(*_currentConfig) == 0 && _currentConfig != _searchSpace.end());

    if (_currentConfig == _searchSpace.end()) {
        selectOptimalConfiguration();
        _timer++;
        return false;
    }

    return true;
}

void PredictiveTuning::selectPossibleConfigurations() {
    if(_searchSpace.size() == 1 || _timer == 0 || _timer == 1){
        _optimalSearchSpace = _searchSpace;
        return;
    }

    predictConfigurations();

    auto optimum = std::min_element(_traversalPredictions.begin(), _traversalPredictions.end(),
                                    [](std::pair<Configuration, size_t> a, std::pair<Configuration, size_t> b) -> bool {
                                     return a.second < b.second;
                                    });

    //_optimalSearchSpace.emplace(optimum->first);

    for(auto &configuration : _searchSpace){
        auto vector = _traversalTimesStorage[configuration];
        if( (_timer - vector[vector.size()].first) >= 5 || ((float) _traversalPredictions[configuration] / optimum->second <= 1.2) ){
            _optimalSearchSpace.emplace(configuration);
        }
    }
}

void PredictiveTuning::predictConfigurations() {
    linePrediction();
    /*switch (method) {
    * case line: linePrediction(); break;
    * default:

    }*/
}

void PredictiveTuning::linePrediction() {
    for(auto &configuration : _searchSpace) {
        auto vector = _traversalTimesStorage[configuration];
        auto tmp1 = vector[vector.size()-1];
        auto tmp2 = vector[vector.size()-2];

        auto prediction = tmp1.second + (tmp1.second - tmp2.second) / (tmp1.first - tmp2.first) * (_timer - tmp1.first);
        _traversalPredictions[configuration] = prediction;
    }
}

void PredictiveTuning::selectOptimalConfiguration() {
    if (_optimalSearchSpace.size() == 1) {
        _currentConfig = _optimalSearchSpace.begin();
        return;
    }

    // Time measure strategy
    if (_traversalTimes.empty()) {
        utils::ExceptionHandler::exception(
                "PredictiveTuning: Trying to determine fastest configuration without any measurements! "
                    "Either selectOptimalConfiguration was called too early or no applicable configurations were found");
    }

    auto optimum = std::min_element(_traversalTimes.begin(), _traversalTimes.end(),
                                    [](std::pair<Configuration, size_t> a, std::pair<Configuration, size_t> b) -> bool {
                                     return a.second < b.second;
                                    });

    _currentConfig = _searchSpace.find(optimum->first);
    // sanity check
    if (_currentConfig == _searchSpace.end()) {
        autopas::utils::ExceptionHandler::exception(
                "PredicitveTuning: Optimal configuration not found in list of configurations!");
    }


    // measurements are not needed anymore
    _traversalTimes.clear();
    _traversalPredictions.clear();
    _optimalSearchSpace.clear();

    AutoPasLog(debug, "Selected Configuration {}", _currentConfig->toString());
}

void PredictiveTuning::removeN3Option(Newton3Option badNewton3Option) {
    for (auto ssIter = _searchSpace.begin(); ssIter != _searchSpace.end();) {
        if (ssIter->newton3 == badNewton3Option) {
            // change current config to the next non-deleted
            if (ssIter == _currentConfig) {
                ssIter = _searchSpace.erase(ssIter);
                _currentConfig = ssIter;
            } else {
                ssIter = _searchSpace.erase(ssIter);
            }
        } else {
            ++ssIter;
        }
    }

    if (this->searchSpaceIsEmpty()) {
        utils::ExceptionHandler::exception(
                "Removing all configurations with Newton 3 {} caused the search space to be empty!", badNewton3Option);
    }
}

}  // namespace autopas

