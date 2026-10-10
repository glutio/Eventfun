#ifndef __EVENTFUN_H__
#define __EVENTFUN_H__

#include <string.h>
#include "BList.h"

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

  template<typename, typename>
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

  static void InvokeFunction(void* instance, const char* methodBytes, TSender* sender, TArgument argument) {
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

template<typename TSender, typename TArgument>
class EventSource
{
protected:
  Buratino::BList<EventDelegate<TSender, TArgument>> _list;

public:
  EventSource(int numEvents = 3) 
    : _list(numEvents) {}
  
  void operator += (EventDelegate<TSender, TArgument> delegate)
  {
    auto k = _list.Length();
    for(unsigned i = 0; i < _list.Length(); ++i) {
      if (_list[i] == delegate) {
        return;
      }
      if (!_list[i]) {
        k = i;
      }
    }

    if (k == _list.Length()) {
      _list.Add(delegate);
    }
    else {
      _list[k] = delegate;
    }
  }

  void operator += (void (*delegate)(TSender*, TArgument)) {
    *this += EventDelegate<TSender, TArgument>(delegate);
  }

  void operator -= (EventDelegate<TSender, TArgument> delegate) {
    for (unsigned i = 0; i < _list.Length(); ++i) {
      if (_list[i] == delegate) {
        _list[i] = EventDelegate<TSender, TArgument>();
      }
    }
  }

  void operator -= (void (*delegate)(TSender*, TArgument)) {
    *this -= EventDelegate<TSender, TArgument>(delegate);
  }

protected:
  void operator () (TSender* sender, TArgument argument) {
    for(unsigned i = 0; i < _list.Length(); ++i) {
      _list[i](sender, argument);
    }
  }

  friend TSender;
};

#endif
