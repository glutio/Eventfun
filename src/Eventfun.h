#ifndef __EVENTFUN_H__
#define __EVENTFUN_H__

#include <string.h>
#include <Buratino.h>

// EventDelegate type-erases either a free function or an instance method
// with signature void(TSender*, TArgument) without any heap allocation,
// virtual dispatch, or RTTI. The member-function pointer is copied into a
// small fixed-size buffer and invoked through a per-TClass trampoline
// function. Because Trampoline<TClass> is a distinct function template
// instantiation, its address is unique per TClass, which gives us a
// type-safe comparison key (no typeid needed) at essentially zero cost -
// important on Arduino targets where RTTI is typically unavailable and
// memory is scarce.
template<typename TSender, typename TArgument>
class EventDelegate {
public:
  typedef TSender SenderType;
  typedef TArgument ArgumentType;

  EventDelegate()
    : _instance(0), _invoke(0) {
    memset(_method, 0, sizeof(_method));
  }

  EventDelegate(void (*func)(TSender* sender, TArgument argument))
    : _instance(0), _invoke(&EventDelegate::InvokeFunction) {
    memset(_method, 0, sizeof(_method));
    memcpy(_method, &func, sizeof(func));
  }

  template<typename TClass>
  EventDelegate(TClass* instance, void (TClass::*method)(TSender* sender, TArgument argument))
    : _instance(instance), _invoke(&EventDelegate::template InvokeMethod<TClass>) {
    static_assert(sizeof(method) <= sizeof(_method), "Member function pointer too large for EventDelegate storage");
    memset(_method, 0, sizeof(_method));
    memcpy(_method, &method, sizeof(method));
  }

  void operator()(TSender* sender, TArgument argument) const {
    if (_invoke) {
      _invoke(_instance, _method, sender, argument);
    }
  }

protected:
  explicit operator bool() const {
    return _invoke != 0;
  }

  bool operator == (const EventDelegate<TSender, TArgument>& other) const {
    if (_invoke != other._invoke) return false;
    if (!_invoke) return true; // both are empty/unset delegates
    if (_instance != other._instance) return false;
    return memcmp(_method, other._method, sizeof(_method)) == 0;
  }

  template<typename, typename, unsigned>
  friend class EventSource;

private:
  typedef void (*InvokeFn)(void* instance, const char* method, TSender* sender, TArgument argument);
  typedef void (*FreeFunc)(TSender*, TArgument);

  template<typename TClass>
  static void InvokeMethod(void* instance, const char* methodBytes, TSender* sender, TArgument argument) {
    void (TClass::*method)(TSender*, TArgument);
    memcpy(&method, methodBytes, sizeof(method));
    (static_cast<TClass*>(instance)->*method)(sender, argument);
  }

  static void InvokeFunction(void*, const char* methodBytes, TSender* sender, TArgument argument) {
    FreeFunc func;
    memcpy(&func, methodBytes, sizeof(func));
    func(sender, argument);
  }

  // Member-function pointers need exactly 2*sizeof(void*) bytes on every
  // GCC/Clang-based Arduino toolchain (AVR, ARM, Xtensa, RISC-V) - verified
  // directly by cross-compiling probes for avr-g++ (ATmega328P) and
  // arm-none-eabi-g++ (SAMD21 Cortex-M0+): simple, polymorphic-base,
  // multiple-inheritance, and virtual-multiple-inheritance member pointers
  // all measured 2*sizeof(void*) on both. static_assert above still turns
  // an unexpectedly larger pointer into a compile error instead of silent
  // buffer corruption.
  static const unsigned MethodStorageSize = sizeof(void*) * 2;

  void* _instance;
  char _method[MethodStorageSize];
  InvokeFn _invoke;
};

// Where an EventSource keeps its delegates: Capacity slots inside the object
// (no heap), or with Capacity 0 a heap list that grows as listeners subscribe.
template<typename TDelegate, unsigned Capacity>
class EventSlots {
protected:
  TDelegate _slots[Capacity];
  explicit EventSlots(unsigned) {}
  unsigned slotCount() { return Capacity; }
  TDelegate& slot(unsigned i) { return _slots[i]; }
  bool addSlot(const TDelegate&) { return false; }
};

template<typename TDelegate>
class EventSlots<TDelegate, 0> {
protected:
  Buratino::BList<TDelegate> _slots;
  explicit EventSlots(unsigned capacity) : _slots(capacity) {}
  unsigned slotCount() { return _slots.Length(); }
  TDelegate& slot(unsigned i) { return _slots[i]; }
  bool addSlot(const TDelegate& delegate) { return _slots.Add(delegate); }
};

// An event. Capacity > 0 stores that many listeners in place (no heap);
// Capacity 0 (the default) grows on the heap, starting with `capacity` slots
// (none until the first listener subscribes, if 0).
template<typename TSender, typename TArgument, unsigned Capacity = 0>
class EventSource : protected EventSlots<EventDelegate<TSender, TArgument>, Capacity>
{
  typedef EventDelegate<TSender, TArgument> Delegate;

public:
  explicit EventSource(unsigned capacity = 0)
    : EventSlots<Delegate, Capacity>(capacity) {}

  // Returns false if the delegate is empty or there is no room (all in-place
  // slots taken, or out of memory). Subscribing twice is a no-op that
  // returns true.
  bool operator += (Delegate delegate) {
    if (!delegate) {
      return false;
    }
    Delegate* empty = 0;
    for (unsigned i = 0; i < this->slotCount(); ++i) {
      Delegate& d = this->slot(i);
      if (d == delegate) {
        return true;
      }
      if (!empty && !d) {
        empty = &d;
      }
    }
    if (empty) {
      *empty = delegate;
      return true;
    }
    return this->addSlot(delegate);
  }

  bool operator += (void (*delegate)(TSender*, TArgument)) {
    return *this += Delegate(delegate);
  }

  void operator -= (Delegate delegate) {
    for (unsigned i = 0; i < this->slotCount(); ++i) {
      if (this->slot(i) == delegate) {
        this->slot(i) = Delegate();
      }
    }
  }

  void operator -= (void (*delegate)(TSender*, TArgument)) {
    *this -= Delegate(delegate);
  }

protected:
  void operator () (TSender* sender, TArgument argument) {
    for (unsigned i = 0; i < this->slotCount(); ++i) {
      Delegate d = this->slot(i);  // a listener may (un)subscribe while running
      d(sender, argument);
    }
  }

  friend TSender;
};

#endif