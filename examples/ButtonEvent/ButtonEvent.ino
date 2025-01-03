// See demo at https://wokwi.com/projects/366570456936964097
#include "Eventfun.h"

class Button {
public:
  typedef EventDelegate<Button, bool> ChangeEvent;

protected:
  bool _isPressed;
  int _pin;

public:
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
      OnChange(this, isPressed);
    }
  }
};

class Program {
  Button& _button;

public:
  Program(Button& button) : _button(button) {
    pinMode(9, OUTPUT);
    pinMode(10, OUTPUT);
    pinMode(11, OUTPUT);
  
    _button.OnChange += Button::ChangeEvent(this, &Program::Led9);
    _button.OnChange += Button::ChangeEvent(this, &Program::Led10);
    _button.OnChange += Button::ChangeEvent(this, &Program::Led11);
  }

  ~Program() {
    _button.OnChange -= Button::ChangeEvent(this, &Program::Led9);
    _button.OnChange -= Button::ChangeEvent(this, &Program::Led10);
    _button.OnChange -= Button::ChangeEvent(this, &Program::Led11);
  }

  void Led9(Button*, bool isPressed) {
    digitalWrite(9, isPressed);
  }

  void Led10(Button*, bool isPressed) {
    digitalWrite(10, isPressed);
  }

  void Led11(Button*, bool isPressed) {
    digitalWrite(11, isPressed);
  }
};

Button _button(3 /* pin */);
Program _program(_button);

void setup() {
  // put your setup code here, to run once:
}

void loop() {
  // put your main code here, to run repeatedly:
  _button.Update();
}
