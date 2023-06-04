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

Event listener is defined as a `void` function taking `sender` and `arg` as arguments. Where `sender` is the class raising the event (or `void` if used outside of a class) and `arg` is an argument of any type passed to the event listener. If you need to pass more than one argument you will need to put all your arguments into a `struct`.

```
// declare a structure to hold all arguments
struct JoystickMoveArgs {
  int x;
  int y;
}

// declare a listener
void OnJoystickMove(Joystick* sender, JoystickMoveArgs args);

// subscribe to the event
joystick.OnMove += OnJoystickMove;
```

## EventSource

Declare an event in your class with `EventSource` and raise the event by calling the `operator()` on the event.
```
class Joystick
public:
  // optionally declare a friendly name for delegate type
  typedef EventDelegate<Joystick, JoystickEventArgs> JoystickMoveEvent;
  // declare the event
  EventSource<Joystick, JoystickEventArgs> OnMove;
  
  void Update() {
    // create the argument structure
    JoystickEventArgs args;
    args.x = readAnalog(xPin);
    args.y = readAnalog(yPin);
    // raise the event
    OnMove(args);
  }
};
```
Because `EventSource` supports multiple listeners it maintains a list of a certain size. If more listeners subscribe the list will grow but that involves allocating memory for the new list and copying the old list to the new list. Try to avoid this by specifying the expected number of listeners, default is 3.
```
// provide the number of listeners via the constructor initializer list
Joystick() : OnMove(10) // expect up to 10 listeners
{}
```
