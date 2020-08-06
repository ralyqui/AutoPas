//
// Created by lunaticcoding on 04.05.20.
//

#pragma AUTOPAS_PARTICLELIST_H

#include <vector>

template <class Type>
class ParticleVector {
  using particleListImpType = std::vector<Type>;

  using iterator = typename particleListImpType::iterator;
  using const_iterator = typename particleListImpType::const_iterator;

 public:
  ParticleVector<Type>() {
    _dirty = false;
    _dirtyIndex = 0;
    particleListImp = std::vector<Type>();
  }

  bool isDirty() { return _dirty; }

  void markAsClean() {
    _dirty = false;
    _dirtyIndex = particleListImp.size();
  }

  void push_back(Type &value) {
    particleListLock.lock();
    _dirty |= particleListImp.capacity() == particleListImp.size();
    if (_dirty) {
      _dirtyIndex = 0;
    }
    particleListImp.push_back(value);
    particleListLock.unlock();
  }

  int totalSize() { return particleListImp.size(); }

  int dirtySize() { return totalSize() - _dirtyIndex; }

  iterator beginDirty() { return particleListImp.begin() + _dirtyIndex; }
  iterator endDirty() { return particleListImp.end(); }

 private:
  bool _dirty;
  int _dirtyIndex;
  autopas::AutoPasLock particleListLock;
  std::vector<Type> particleListImp;
};
