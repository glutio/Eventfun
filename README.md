# Eventfun

Basic building blocks for event-driven Arduino sketches. Raise event signals for multiple listeners with a pair of classes `EventDelegate` and `EventSource`. A delegate is a wrapper around a function or method call, this is the event listener. The event source is a list of delegates that will all be called one by one when an event is raised. Most useful in combination with object oriented programming. Below is an example of a button class that can raise an event.

```
#include <Eventfun.h>

class Button {
public:
  // give the event listener delegate type a friendly name 
  typedef EventDelegate<Button, bool> ChangeEvent;

protected:
  bool _isPressed;
  int _pin;

public:
  // declare an event
  EventSource<Button, bool> OnChange;

public:
  Button(int pin): _pin(pin) {
    pinMode(_pin, INPUT_PULLUP);
    _isPressed = digitalRead(_pin) == LOW;
  }

  void Update() {
    auto isPressed = digitalRead(_pin) == LOW;
    if (isPressed != _isPressed) {
      _isPressed = isPressed;
      
      // raise event
      OnChange(this, isPressed);
    }
  }
};
```
Use `+=` to subscribe and `-=` to unsubscribe from events. Multiple subscriptions from the same listener are consolidated, ensuring each listener is registered only once. A listener delegate can be a function or a class method.
```
// listener signature
void onButtonChange(Button* sender, bool isPressed);

// regular function as a listener
button.OnChange += onButtonChange; // subscribe
button.OnChange -= onButtonChange; // unsubscribe

// nonstatic class method as a listener (`this` is of type Program)
button.OnChange += Button::ChangeEvent(this, &Program::onButtonChange); // put this in ctor
button.OnChange -= Button::ChangeEvent(this, &Program::onButtonChange); // put this in ~dtor
```

## EventDelegate


## EventSource
