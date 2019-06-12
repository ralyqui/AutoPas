/**
 * @file GaussianProcess.h
 * @author Jan Nguyen
 * @date 17.05.19
 */

#pragma once

#include <Eigen/Dense>
#include "autopas/selectors/FeatureVector.h"

namespace autopas {

/**
 * Different acquisition functions
 */
enum AcquisitionFunction {
  /**
   * Upper confidence bound
   */
  ucb,
  /**
   * mean
   */
  mean
};

/**
 * Gaussian process is a stochastical model. It predicts the
 * output of a blackbox function for given input. To do so some sample
 * input-output pairs of the function should be provided as evidence.
 *
 * Currently the default mean is 0 and squared exponential kernel is used.
 * TODO: maybe offer some options.
 */
class GaussianProcess {
 private:
  std::vector<FeatureVector> _inputs;
  Eigen::VectorXd _outputs;

  /**
   * prior variance
   */
  double _theta;
  /**
   * Scale distance of each feature
   */
  std::vector<double> _dimScale;
  /**
   * fixed noise assumed
   */
  const double _sigma;

  Eigen::MatrixXd _covMat;
  Eigen::MatrixXd _covMatInv;
  Eigen::VectorXd _weights;

  /**
   * Kernel function to describe similarity between two features
   * @param f1
   * @param f2
   * @return
   */
  double kernel(FeatureVector f1, FeatureVector f2) const {
    std::vector<double> r = f1 - f2;
    double result = 0;
    for (unsigned i = 0; i < r.size(); ++i) {
      result += r[i] * r[i] * _dimScale[i];
    }
    return _theta * exp(-result);
  }

  /**
   * Calulates the kernel between input and all evidences
   * @param input
   * @return Vector of covariances
   */
  Eigen::VectorXd kernelVector(FeatureVector input) const {
    std::vector<double> k;
    for (FeatureVector d : _inputs) {
      k.push_back(kernel(input, d));
    }
    return Eigen::Map<Eigen::VectorXd>(k.data(), k.size());
  }

 public:
  /**
   * Construct a gaussian process
   * @param theta prior variance
   * @param dimScale scale each dimension before applying kernel
   * @param sigma fixed noise
   */
  GaussianProcess(double theta, std::vector<double> dimScale, double sigma)
      : _inputs(), _outputs(), _theta(theta), _dimScale(dimScale), _sigma(sigma), _covMat(), _covMatInv(), _weights() {}

  /**
   * Discard all evidences.
   */
  void clear() {
    // As long is _input is empty the matrices ar consided uninitialized.
    // So no further actions needed.
    _inputs.clear();
  }

  /**
   * Get the number of evidences provided.
   * @return
   */
  size_t numEvidences() const { return _inputs.size(); }

  /**
   * Provide a the output for a blackbox function
   * with given output.
   * @param input
   * @param output
   */
  void addEvidence(FeatureVector input, double output) {
    _inputs.push_back(input);
    long newSize = _inputs.size();

    // extend output vector
    _outputs.conservativeResize(newSize, Eigen::NoChange_t());
    _outputs(newSize - 1) = output;

    // extend covariance matrix
    _covMat.conservativeResize(newSize, newSize);
    for (unsigned i = 0; i < newSize - 1; ++i) {
      _covMat(newSize - 1, i) = _covMat(i, newSize - 1) = kernel(input, _inputs[i]);
    }
    _covMat(newSize - 1, newSize - 1) = kernel(input, input) + _sigma;  // add fixed noise to diagonal

    // calculate needed matrix and vector for predictions
    _covMatInv = _covMat.inverse();
    _weights = _covMatInv * _outputs;
  }

  /**
   * Predict the expected output of the blackbox function
   * at input given the evidence so far.
   * @param input
   * @return mean
   */
  double predictMean(FeatureVector input) const {
    if (_inputs.size() == 0) return 0.;

    return kernelVector(input).dot(_weights);
  }

  /**
   * The variance of the predicted output of predictMean().
   * @param input
   * @return variance
   */
  double predictVar(FeatureVector input) const {
    if (_inputs.size() == 0) return kernel(input, input);

    Eigen::VectorXd kVec = kernelVector(input);
    return kernel(input, input) - kVec.dot(_covMatInv * kVec);
  }

  /**
   * Find FeatureVector in samples which maximizes
   * given aquisition function.
   * TODO: maybe add parameters for hyperparameters of aquisition functions
   * @param af function to maximize
   * @param samples
   * @return
   */
  FeatureVector sampleAquisitionMax(AcquisitionFunction af, const std::vector<FeatureVector>& samples) const {
    int maxIdx = -1;
    double maxVal;

    // find maximum from samples
    for (unsigned i = 0; i < samples.size(); ++i) {
      double val;
      switch (af) {
        case ucb: {
          val = predictMean(samples[i]) + std::sqrt(predictVar(samples[i]));
          break;
        }
        case mean: {
          val = predictMean(samples[i]);
          break;
        }
      }

      if (maxIdx == -1 || val > maxVal) {
        maxIdx = i;
        maxVal = val;
      }
    }

    return samples[maxIdx];
  }
};
}  // namespace autopas
