// Native (non-Arduino) regression tests for Eventfun.
//
// Build & run (MSVC, from repo root):
//   cl /nologo /EHsc /std:c++14 /Fe:tests\run_tests.exe tests\EventfunTests.cpp
//   tests\run_tests.exe
//
// Build & run (g++/clang++, from repo root):
//   g++ -std=c++14 -o tests/run_tests tests/EventfunTests.cpp && ./tests/run_tests
//
// BList.h uses the bare `min()` function, which Arduino.h normally supplies
// globally. Provide the same shim here for a native build.
#include <cstdint>
#include <cstdio>
#include <cstdlib>

template<typename T>
T min(T a, T b) {
  return a < b ? a : b;
}

#include "../src/BList.h"
#include "../src/Eventfun.h"
#include "../src/BPtr.h"

static int g_failures = 0;

#define CHECK(cond) \
  do { \
    if (!(cond)) { \
      std::printf("  FAIL: %s (line %d)\n", #cond, __LINE__); \
      ++g_failures; \
    } \
  } while (0)

#define TEST(name) static void name()

// Helper: EventSource::operator() is protected (raising is restricted to the
// sender class). Derived classes get access to protected members regardless
// of the `friend TSender` declaration, so this lets tests raise events the
// same way a real sender class would.
template<typename TSender, typename TArgument>
struct Raiser : EventSource<TSender, TArgument> {
  using EventSource<TSender, TArgument>::operator();
};

template<typename TSender, typename TArgument>
static void Raise(EventSource<TSender, TArgument>& source, TSender* sender, TArgument arg) {
  static_cast<Raiser<TSender, TArgument>&>(source)(sender, arg);
}

struct Counter {
  int calls = 0;
  bool lastValue = false;
  void OnChange(Counter* /*sender*/, bool value) {
    ++calls;
    lastValue = value;
  }
};

static int g_funcCalls = 0;
static void FreeListener(void*, bool) {
  ++g_funcCalls;
}

static int g_funcCalls2 = 0;
static void FreeListener2(void*, bool) {
  ++g_funcCalls2;
}

struct CounterA {
  int calls = 0;
  void Method(void*, bool) { ++calls; }
};

struct CounterB {
  int calls = 0;
  void Method(void*, bool) { ++calls; }
};

TEST(FunctionListenerIsInvoked) {
  g_funcCalls = 0;
  EventSource<void, bool> source;
  source += FreeListener;
  Raise<void, bool>(source, nullptr, true);
  CHECK(g_funcCalls == 1);
}

TEST(MethodListenerIsInvoked) {
  Counter counter;
  EventSource<Counter, bool> source;
  source += EventDelegate<Counter, bool>(&counter, &Counter::OnChange);
  Raise<Counter, bool>(source, &counter, true);
  CHECK(counter.calls == 1);
  CHECK(counter.lastValue == true);
}

// --- Regression: duplicate subscriptions are consolidated ---
TEST(DuplicateSubscriptionIsConsolidated) {
  Counter counter;
  typedef EventDelegate<Counter, bool> Del;
  EventSource<Counter, bool> source;
  source += Del(&counter, &Counter::OnChange);
  source += Del(&counter, &Counter::OnChange); // duplicate, should not add a 2nd slot
  Raise<Counter, bool>(source, &counter, true);
  CHECK(counter.calls == 1);
}

// --- Regression: unsubscribe then subscribe a different listener into the
// freed slot must not crash (previously dereferenced a null Callable). ---
TEST(ResubscribeAfterUnsubscribeDoesNotCrash) {
  typedef EventDelegate<void, bool> Del;
  g_funcCalls = 0;
  g_funcCalls2 = 0;
  EventSource<void, bool> source;
  source += Del(FreeListener);
  source -= Del(FreeListener);      // frees the slot
  source += Del(FreeListener2);     // reuses the freed slot; must not crash
  Raise<void, bool>(source, nullptr, true);
  CHECK(g_funcCalls == 0);
  CHECK(g_funcCalls2 == 1);
}

// --- Regression: method delegates from two different classes must not be
// confused with each other (previously an unsafe same-type downcast). ---
TEST(DifferentClassMethodsAreNotConfused) {
  CounterA a;
  CounterB b;
  typedef EventDelegate<void, bool> Del;
  EventSource<void, bool> source;
  source += Del(&a, &CounterA::Method);
  source += Del(&b, &CounterB::Method);

  // Unsubscribing A's listener must not remove B's listener.
  source -= Del(&a, &CounterA::Method);

  Raise<void, bool>(source, nullptr, true);
  CHECK(a.calls == 0);
  CHECK(b.calls == 1);
}

// --- Regression: two different methods on the SAME instance/class must
// remain distinguishable (mirrors the ButtonEvent.ino example). ---
TEST(SameInstanceDifferentMethodsAreIndependent) {
  struct Multi {
    int led9 = 0, led10 = 0;
    void Led9(void*, bool) { ++led9; }
    void Led10(void*, bool) { ++led10; }
  } multi;
  typedef EventDelegate<void, bool> Del;
  EventSource<void, bool> source;
  source += Del(&multi, &Multi::Led9);
  source += Del(&multi, &Multi::Led10);
  source -= Del(&multi, &Multi::Led9);

  Raise<void, bool>(source, nullptr, true);
  CHECK(multi.led9 == 0);
  CHECK(multi.led10 == 1);
}

// --- Growth beyond the initial capacity still works. ---
TEST(ListGrowsBeyondInitialCapacity) {
  const int n = 10;
  Counter counters[n];
  typedef EventDelegate<Counter, bool> Del;
  EventSource<Counter, bool> source(3); // small initial capacity, forces growth
  for (int i = 0; i < n; ++i) {
    source += Del(&counters[i], &Counter::OnChange);
  }
  Raise<Counter, bool>(source, nullptr, true);
  for (int i = 0; i < n; ++i) {
    CHECK(counters[i].calls == 1);
  }
}

// --- BPtr: basic shared ownership + null safety (no allocation for null). ---
TEST(BPtrSharedOwnershipAndNullSafety) {
  static int destroyed = 0;
  struct Obj {
    ~Obj() { ++destroyed; }
  };
  destroyed = 0;
  {
    Buratino::BPtr<Obj> a(new Obj());
    {
      Buratino::BPtr<Obj> b = a; // shared ownership
      CHECK((bool)b);
    }
    CHECK(destroyed == 0); // b went out of scope, a still owns it
  }
  CHECK(destroyed == 1);

  Buratino::BPtr<Obj> empty; // default-constructed, no allocation, no crash
  CHECK(!(bool)empty);
  Buratino::BPtr<Obj> empty2(nullptr);
  CHECK(empty == empty2);
}

int main() {
  std::printf("Running Eventfun tests...\n");
  FunctionListenerIsInvoked();
  MethodListenerIsInvoked();
  DuplicateSubscriptionIsConsolidated();
  ResubscribeAfterUnsubscribeDoesNotCrash();
  DifferentClassMethodsAreNotConfused();
  SameInstanceDifferentMethodsAreIndependent();
  ListGrowsBeyondInitialCapacity();
  BPtrSharedOwnershipAndNullSafety();

  if (g_failures == 0) {
    std::printf("All tests passed.\n");
    return 0;
  }
  std::printf("%d check(s) failed.\n", g_failures);
  return 1;
}
