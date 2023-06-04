#ifndef __EVENTFUN_H__
#define __EVENTFUN_H__

#include "BPtr.h"
#include "BList.h"

template<typename TSender, typename TArgument>
class EventDelegate {
protected:
  struct Callable {
    virtual ~Callable() {}
    virtual void Call(TSender* sender, TArgument argument) = 0;
    virtual char Type() = 0;
    virtual operator == (const Callable& other) const = 0;
  };

  template<typename TClass>
  struct CallableMethodImpl : public Callable {
    TClass* _instance;
    void (TClass::*_method)(TSender*, TArgument);

    CallableMethodImpl(TClass* instance, void (TClass::*method)(TSender*, TArgument))
      : _instance(instance), _method(method) {}

    virtual void Call(TSender* sender, TArgument argument) {
      (_instance->*_method)(sender, argument);
    }

    virtual char Type() {
      return 'M';
    }

    virtual operator == (const Callable& other) const {
      if (Type() != other.Type()) return false;
      auto callable = (const CallableMethodImpl<TClass>&)other;
      return _instance == callable._instance && _method == callable._method;
    }
  };

  struct CallableFunctionImpl : public Callable {
    void (*_func)(TSender*, TArgument);

    CallableFunctionImpl(void (*func)(TSender*, TArgument))
      : _func(func) {}

    virtual void Call(TSender* sender, TArgument argument) {
      (_func)(sender, argument);
    }

    virtual char Type() {
      return 'F';
    }

    virtual operator == (const Callable& other) const {
      if (Type() != other.Type()) return false;
      auto callable = (CallableFunctionImpl&)other;
      return _func == callable._func;
    }
  };

protected:
  Buratino::BPtr<Callable> _callable;  // smart pointer

public:
  typedef TSender SenderType;
  typedef TArgument ArgumentType;

  template<typename TClass>
  EventDelegate(const TClass* instance, void (TClass::*method)(TSender* sender, TArgument argument))
    : _callable(new CallableMethodImpl<TClass>(instance, method)) {}

  EventDelegate(void (*func)(TSender* sender, TArgument argument))
    : _callable(new CallableFunctionImpl(func)) {}

  EventDelegate()
    : _callable(0) {}

  ~EventDelegate() {
  }

  void operator()(TSender* sender, TArgument argument) {
    if (_callable) {
      _callable->Call(sender, argument);
    }
  }

protected:
  explicit operator bool() const {
    return (bool)_callable;
  }

  operator == (const EventDelegate<TSender, TArgument>& other) const {
    return *_callable == *other._callable;
  }

  template<typename, typename>
  friend class EventSource;
};

template<typename TSender, typename TArgument>
class EventSource
{
protected:
  Buratino::BList<EventDelegate<TSender, TArgument>> _list;

public:
  EventSource(int numEvents = 3) 
    : _list(numEvents) {}
  
  operator += (EventDelegate<TSender, TArgument> delegate)
  {
    auto k = _list.Length();
    for(auto i = 0; i < _list.Length(); ++i) {
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

  operator += (void (*delegate)(TSender*, TArgument)) {
    this += EventDelegate<TSender, TArgument>(delegate);
  }

  operator -= (EventDelegate<TSender, TArgument> delegate) {
    for (auto i = 0; i < _list.Length(); ++i) {
      if (_list[i] == delegate) {
        _list[i] = EventDelegate<TSender, TArgument>();
      }
    }
  }

  operator -= (void (*delegate)(TSender*, TArgument)) {
    this -= EventDelegate<TSender, TArgument>(delegate);
  }

protected:
  operator () (TSender* sender, TArgument argument) {
    for(auto i = 0; i < _list.Length(); ++i) {
      _list[i](sender, argument);
    }
  }

  friend TSender;
};

#endif