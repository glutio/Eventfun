#ifndef __BPTR_H__
#define __BPTR_H__

namespace Buratino {
/*
  BPtr - a smart pointer implementation
*/
template<typename T>
class BPtr {
private:
  T* ptr;
  int* refCount;

public:
  // Constructor
  explicit BPtr(T* p = nullptr)
    : ptr(p), refCount(p ? new int(1) : nullptr) {
  }

  // Copy constructor
  BPtr(const BPtr& other)
    : ptr(other.ptr), refCount(other.refCount) {
    if (refCount) (*refCount)++;
  }

  // Assignment operator
  BPtr& operator=(const BPtr& other) {
    if (this != &other) {
      Release();

      // Assign the new object and increase its reference count
      ptr = other.ptr;
      refCount = other.refCount;
      if (refCount) (*refCount)++;
    }
    return *this;
  }

  // Destructor
  ~BPtr() {
    Release();
  }

  // Dereference operator
  T& operator*() const {
    return *ptr;
  }

  // Arrow operator
  T* operator->() const {
    return ptr;
  }

  // Comparison operator
  bool operator ==(const BPtr& other) const {
    return ptr == other.ptr;
  }

  // Conversion operator to bool
  explicit operator bool() const {
    return ptr != nullptr;
  }

private:
  void Release() {
    if (refCount) {
      (*refCount)--;
      if (*refCount == 0) {
        delete ptr;
        delete refCount;
      }
    }
  }
};

}
#endif