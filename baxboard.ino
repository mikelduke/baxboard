/***************************************************

  baxboard
  
  http://www.mikelduke.com
  
  https://github.com/mikelduke/baxboard
  
***************************************************/

#include <Wire.h>
#include "Adafruit_Trellis.h"
#include <SoftwareSerial.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>

#define DEBUG_BUTTONS false
#define DEBUG_KNOBS false
#define DEBUG_MIDI true

//PINS
#define NUMKEYS 64 //total number of Trellis keys
#define MIDIOUT 2  //Pins to use for software serial for midi out
#define MIDIIN  3  //unused at this time
#define JOY_X A1   //Joystick pins
#define JOY_Y A0
#define BUTTON_1 4 //The four buttons above the knobs
#define BUTTON_2 5
#define BUTTON_3 6
#define BUTTON_4 7

#define TRELLIS_1 0x70
#define TRELLIS_2 0x71
#define TRELLIS_3 0x72
#define TRELLIS_4 0x73

#define MIDI_BAUD 31250
#define SERIAL_BAUD 9600

#define MAIN_LOOP_DELAY 30
#define TRELLIS_BUTTON_DELAY 10

//I2C LCD
#define I2C_ADDR      0x27  // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN 3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7
#define LCD_X 16
#define LCD_Y  2

//MIDI - Based on spec http://www.midi.org/techspecs/midimessages.php
//Status Messages
#define MIDI_NOTE_ON      0x90
#define MIDI_NOTE_OFF     0x80
#define MIDI_AFTERTOUCH   0xA0 //unused for now
#define MIDI_CONTROL      0xB0
#define MIDI_PITCH_CHANGE 0xE0

//MIDI Constants
#define MIDI_CHAN_MAX        15   //16 Channels
#define MIDI_CONTROLLER_MAX   7   //8 Controllers
#define MIDI_DATA_MAX       127   //128 Max Values/Notes/Pressures/Etc


Adafruit_Trellis matrix0 = Adafruit_Trellis();
Adafruit_Trellis matrix1 = Adafruit_Trellis();
Adafruit_Trellis matrix2 = Adafruit_Trellis();
Adafruit_Trellis matrix3 = Adafruit_Trellis();

Adafruit_TrellisSet trellis =  Adafruit_TrellisSet(&matrix0, &matrix1, &matrix2, &matrix3);

SoftwareSerial softMidi(MIDIIN, MIDIOUT);

LiquidCrystal_I2C lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

//Note names
char* midiNotes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#" };

const String boardName = "baxboard";

#define NUMPOTS 4
uint8_t knob[]           = { A7, A6, A3, A2 };
uint8_t midiController[] = {  7, 10, 12, 13 }; //default controller ids
uint8_t lastPotVal[NUMPOTS];

void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println(boardName);
  
  softMidi.begin(MIDI_BAUD);
  
  lcd.begin (LCD_X, LCD_Y);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  lcd.home();
  lcd.print("Welcome to");  
  lcd.setCursor ( 0, 1 );
  lcd.print(boardName);

  //initialize array of last pot read values
  for (uint8_t i = 0; i < NUMPOTS; i++)
    lastPotVal[i] = 0;

  trellis.begin(TRELLIS_1, TRELLIS_2, TRELLIS_3, TRELLIS_4);
  
  ledDemo();
  
  lcd.clear();
}


void loop() {
  delay(MAIN_LOOP_DELAY);
  
  readButtons();
  readKnobs();
}

/**
 * ledDemo
 * 
 * Simple loop to turn button leds on and off
 */
void ledDemo() {
  for (uint8_t i = 0; i < NUMKEYS; i++) {
    trellis.setLED(i);
    trellis.writeDisplay();    
    delay(TRELLIS_BUTTON_DELAY);
  }
  // then turn them off
  for (uint8_t i = 0; i < NUMKEYS; i++) {
    trellis.clrLED(i);
    trellis.writeDisplay();    
    delay(TRELLIS_BUTTON_DELAY);
  }
}

/**
 * readButtons
 *
 * Reads the buttons that are pressed and calls buttonPressed with the 
 * corrected button number, turns pressed button leds on until they are 
 * released.
 */
void readButtons() {
  if (trellis.readSwitches()) {
    for (uint8_t i = 0; i < NUMKEYS; i++) {
      if (trellis.justPressed(i)) {
        uint8_t m = mapButton(i);
        
        Serial.print("v"); Serial.println(m);
        trellis.setLED(i);
        buttonPressed(m);
      } 
      else if (trellis.justReleased(i)) {
        uint8_t m = mapButton(i);
        
        Serial.print("^"); Serial.println(m);
        trellis.clrLED(i); //TODO maybe add note off
      }
    }
    trellis.writeDisplay();
  }
}

/**
 * readKnobs
 * 
 * TODO Add Controller selection (MIDI_CONTROL + Controller#)
 */
void readKnobs() {
  int val = 0;
  uint8_t mapVal = 0;
  
  for (uint8_t i = 0; i < NUMPOTS; i++) {
    val = analogRead(knob[i]);
    mapVal = map(val, 0, 1023, 0, 127);
    if (mapVal != lastPotVal[i]) {
      
      lcd.setCursor(0, 0);
      lcd.print("Knob ");
      lcd.print(i + 1);
      lcd.print(':');
      
      for (uint8_t i = 7; i < LCD_X - 3; i++) lcd.print(' ');
      if (mapVal < 100) lcd.print(' ');
      if (mapVal <  10) lcd.print(' ');
      lcd.print(mapVal);
      
      #if DEBUG_KNOBS
        Serial.print("pot ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(val);
        Serial.print(" map: ");
        Serial.println(mapVal);
      #endif
      
      lastPotVal[i] = mapVal;
      midiCommand(MIDI_CONTROL, midiController[i],  mapVal); //mod
    }
  }
}

/**
 * mapButton
 *
 * @param uint8_t button raw button press number returned by readButtons
 * @return uint8_t corrected button number
 *
 * Remaps button numbers to go 0-63 from bottom left to upper right
 * TODO - create unmapButton(mButton) to convert from mapped button to trellis, possible use for led correction
 */
uint8_t mapButton(uint8_t button) {
  if (button >= 12 && button < 16)     button -= 12;  //bottom left trellis  0
  else if (button >= 8 && button < 12) ;              //button is right      8
  else if (button >= 4 && button < 8)  button += 12;  //                    16
  else if (button >= 0 && button < 4)  button += 24;  //                    24
  
  else if (button >= 28 && button < 32) button -= 24; //bottom right trellis 4
  else if (button >= 24 && button < 28) button -= 12; //                    12
  else if (button >= 20 && button < 24) ;             //button is right     20
  else if (button >= 16 && button < 20) button += 12; //                    28
  
  else if (button >= 44 && button < 48) button -= 12; //top right           32
  else if (button >= 40 && button < 44) ;             //                    40
  else if (button >= 36 && button < 40) button += 12; //                    48
  else if (button >= 32 && button < 36) button += 24; //                    56
  
  else if (button >= 60 && button < 64) button -= 24; //                    36
  else if (button >= 56 && button < 60) button -= 12; //                    44
  else if (button >= 52 && button < 56) ;             //                    52
  else /*  button >= 48 && button < 52*/button += 12; //                    60
  
  #if DEBUG_BUTTONS
    Serial.print("Mapped button: ");
    Serial.println(button);
  #endif

  return button;
}

/**
 * buttonPressed
 * 
 * @param uint8_t b - Corrected button number
 *
 * TODO: check settings to change what should happen when a button is pressed
 */
void buttonPressed(uint8_t b) {
  b += 0x1E;
  showNote(b);
  sendMidiNote(b);
}

/**
 * sendMidiNote
 * 
 * @param uint8_t note
 
 * Simple stub function to output midi note based with presets
 * TODO enable loading/changing values
 */
void sendMidiNote(uint8_t note) {
  midiCommand(MIDI_NOTE_ON, note, 0x45); //controller 1, pitch, medium velocity
}

/**
 * midiCommand
 *
 * @param uint8_t cmd      - Midi Command byte
 * @param uint8_t pitch    - Midi Pitch byte 
 * @param uint8_t velocity - Midi Velocity byte
 *
 * Sends midi command from the parameters using softMidi
 */
void midiCommand(uint8_t cmd, uint8_t pitch, uint8_t velocity) {
  softMidi.write(cmd);
  softMidi.write(pitch);
  softMidi.write(velocity);
  
  #if DEBUG_MIDI
    Serial.print(cmd); Serial.print(','); Serial.print(pitch); Serial.print(','); Serial.println(velocity);
  #endif
}

/**
 * showNote
 * 
 * @param note - Note number (0-127)
 *
 * Displays the note name on the lcd returned by noteToString and adds the octave
 */
void showNote(uint8_t note) {
  lcd.setCursor(0, LCD_Y);
  lcd.print("Note:");
  
  for (uint8_t i = 5; i < LCD_X - 3; i++) lcd.print(' ');
  
  char* n = noteToString(note);
  lcd.print(n);
  
  uint8_t octave = 0;
  if (note > 11) octave = note / 12;
  if (n[1] == NULL) lcd.print(' ');
  lcd.print(octave);
}

/**
 * noteToString
 *
 * @param note - Note Number (0-127)
 *
 * Converts the note number to the proper note name on the scale
 */
char* noteToString(uint8_t note) {
  return midiNotes[note % 12];
}