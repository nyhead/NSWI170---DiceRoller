#include <Arduino.h>
#include "funshield.h"

constexpr byte LETTER_GLYPH[] {
  0b10001000,   // A
  0b10000011,   // b
  0b11000110,   // C
  0b10100001,   // d
  0b10000110,   // E
  0b10001110,   // F
  0b10000010,   // G
  0b10001001,   // H
  0b11111001,   // I
  0b11100001,   // J
  0b10000101,   // K
  0b11000111,   // L
  0b11001000,   // M
  0b10101011,   // n
  0b10100011,   // o
  0b10001100,   // P
  0b10011000,   // q
  0b10101111,   // r
  0b10010010,   // S
  0b10000111,   // t
  0b11000001,   // U
  0b11100011,   // v
  0b10000001,   // W
  0b10110110,   // ksi
  0b10010001,   // Y
  0b10100100,   // Z
};



constexpr byte EMPTY_GLYPH = 0b11111111;

constexpr int positionsCount = 4;
constexpr unsigned int scrollingInterval = 300;

/**
   Show chararcter on given position. If character is not letter, empty glyph is displayed instead.
   @param ch character to be displayed
   @param pos position (0 = leftmost)
*/
void displayChar(char ch, byte pos)
{
  byte glyph = EMPTY_GLYPH;
  if (isAlpha(ch)) {
    glyph = LETTER_GLYPH[ ch - (isUpperCase(ch) ? 'A' : 'a') ];
  }
  else if (isDigit(ch)) {
    glyph = digits[ch - '0'];
  }

  digitalWrite(latch_pin, LOW);
  shiftOut(data_pin, clock_pin, MSBFIRST, glyph);
  shiftOut(data_pin, clock_pin, MSBFIRST, 1 << pos);
  digitalWrite(latch_pin, HIGH);
}



class Display {
  private:
    int pos = 0;
    char disp_chars[4];
    char current() {
      pos = (pos + 1) % 4;
      return disp_chars[pos];
    }
  public:
    void set_val(int num) {
      for (int i = positionsCount - 1; i >= 0; i--) {
        disp_chars[i] = '0' + num % 10;
        num /= 10;
      }
    }
    void set_val(int num1, int num2) {
      int i = positionsCount - 1;
      while (i > positionsCount - 3) {
        disp_chars[i] = '0' + num2 % 10;
        num2 /= 10;
        i--;
      }
      disp_chars[i] = 'd';
      disp_chars[--i] = '0' + num1;
    }

    void show() {
      char ch = current();
      displayChar(ch, pos);
    }
};

constexpr int diceCount = 7;
struct Dice {
  enum States { Normal, Configuration, Generating };
  States state;
  int throws = 1;
  int throwLimit = 10;
  unsigned long time;
  int types[diceCount] = { 4, 6, 8, 10, 12, 20, 100 };
  int typeId;
  int genNum;

  void updateThrows() {
    throws++;
    if (throws == throwLimit) throws = 1;
  }

  void updateDiceType() {
    typeId = (typeId + 1) % diceCount;
  }

  int getType() {
    return types[typeId];
  }

  int getGenNum() {
    return genNum;
  }
  int getThrows() {
    return throws;
  }

  void setGenNum(long time) {
    int total = 0;
    srand(time);
    for (int i = 0;  i < throws; i++) {
      total += rand() % types[typeId] + 1;
    }
    genNum = total;
  }
};

struct Button {
  int pin;
  bool down;
  long when_pressed;
  enum State {Holding, Release, Nothing};
  State state = Nothing;
};

constexpr int buttonsCount = 3;
Button buttons[buttonsCount];
Display display;
Dice dice;

void init_button(Button &button, int pin) {
  button.pin = pin;
  button.down = false;
  pinMode(button.pin, INPUT);
}
int genMsgCounter = 0;
void updateDiceDisplay(Dice &dice, Display &display) {
  switch (dice.state) {
    case dice.Configuration:
      display.set_val(dice.getThrows(), dice.getType());
      break;
    case dice.Normal:
      display.set_val(dice.getGenNum());
      break;
    default:
      display.set_val(millis() % 10000);
      break;
  }
}

bool is_pressed(Button &button) {

  if (digitalRead(button.pin) == OFF) {
    button.down = false;
    button.state = button.Nothing;
    return false;
  }

  long now = millis();
  if (button.down) {
    if (now - button.when_pressed <= 0) {
      return false;
    }
    button.state = button.Holding;
    button.when_pressed += 300;
  }
  else {
    button.down = true;
    button.when_pressed = now + 1000;
  button.state = button.Release;
  }
  return true;
}

void actButton(Button &button, Dice &dice) {
  
  if (is_pressed(button)) {
    switch (button.pin) {
      case button1_pin:
        dice.state = dice.Normal;
        break;
      case button2_pin:
        dice.updateThrows();
        dice.state = dice.Configuration;
        break;
      case button3_pin:
        dice.updateDiceType();
        dice.state = dice.Configuration;
        break;
    }
  }
  

  if (button.state == button.Holding && button.pin == button1_pin) {
    dice.setGenNum(button.when_pressed);
    dice.state = dice.Generating;
  }
  if (button.state == button.Nothing && dice.state == dice.Generating && button.pin == button1_pin) {
    dice.state = dice.Normal;
  }
  if (button.state == button.Release && button.pin == button1_pin) {
    dice.setGenNum(button.when_pressed);
    dice.state = dice.Normal;
  }
}

void setup() {
  Serial.begin(9600);
  int b_pins[] = {button1_pin, button2_pin, button3_pin};
  for (int i = 0; i < buttonsCount; i++) {
    init_button(buttons[i], b_pins[i]);
  }

  pinMode(latch_pin, OUTPUT);
  pinMode(data_pin, OUTPUT);
  pinMode(clock_pin, OUTPUT);

  digitalWrite(latch_pin, OFF);
  digitalWrite(latch_pin, ON);

  dice.state = dice.Configuration;
  updateDiceDisplay(dice, display);
}
unsigned long int prev = 0;
int i = 1, j = 4;
void loop() {
  for (int i = 0; i < 3; i++) {
    actButton(buttons[i], dice);
    updateDiceDisplay(dice, display);
  }

  display.show();

  // unsigned long int now = millis();
  // if (now - prev >= 500) {
  //   display.set_val(i % 9, j % 101);
  //   i++;
  //   // j++;
  //   prev = now;
  // }
  // display.show();
}
