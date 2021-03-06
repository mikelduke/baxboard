/******************************************************************************

  baxboard
  
  Created by Mikel Duke http://www.mikelduke.com
  
  Code https://github.com/mikelduke/baxboard
  
  Released under GPLv2
  
  *********************************************************

  This program is used in the Baxboard, a Midi Controller containing an 
  Arduino Nano, 4 Adafruit Trellis button boards, 4 pots, 4 momentary switches,
  a joystick, an I2C LCD, and a Midi plug.
  
  Compiles with 1.0.5 of the Arduino IDE
  
  *********************************************************
  
  Hardware
  
  The Midi out should go to a 5pin MIDI connector, pinout can be found in the 
  Arduino Midi Guide for Midi Out.
  
  http://arduino.cc/en/Tutorial/Midi?from=Tutorial.MIDI
  
  The buttons are 4 Adafruit Trellis boards with leds using the 70, 71, 72, 
  and 73 addresses. Rearrange the addresses in the Trellis.begin call as 
  needed.
  
  https://learn.adafruit.com/adafruit-trellis-diy-open-source-led-keypad/overview
  
  The LCD is an I2C LCD. I used a Sainsmart brand, but others could also be 
  used. The pin numbers for others may need to be changed.
  
  Example: http://www.sainsmart.com/new-sainsmart-iic-i2c-twi-1602-serial-lcd-module-display-for-arduino-uno-mega-r3.html
  
  *********************************************************
  
  Additional Libraries Used
  
  LiquidCrystal I2C - http://hmario.home.xs4all.nl/arduino/LiquidCrystal_I2C/
  Adafruit Trellis  - https://github.com/adafruit/Adafruit_Trellis_Library
  
  *********************************************************
  
  Pins
  
  A0 - Joystick
  A1 - Joystick
  A2 - Knob
  A3 - Knob
  A4 - I2C SDA
  A5 - I2C SCL
  A6 - Knob
  A7 - Knob
  D2 - Midi Out
  D3 - Midi In (Currently Unused)
  D4 - Button
  D5 - Button
  D6 - Button
  D7 - Button
  
  Buttons are set to use Internal Pull-ups.
  
******************************************************************************/

#include <Wire.h>
#include "Adafruit_Trellis.h"
#include <SoftwareSerial.h>
#include <LCD.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define DEBUG_TRELLIS        false
#define DEBUG_BUTTONS        false
#define DEBUG_KNOBS          false
#define DEBUG_MIDI           false
#define DEBUG_LOOP_TIME      false
#define DEBUG_BUTTON_PRESSED false
#define DEBUG_SCALES         false

//PINS
#define MIDIOUT 2  //Pins to use for software serial for midi out
#define MIDIIN  3  //unused at this time
#define BUTTON_PRESSED LOW

//Trellis info
#define NUMKEYS 64 //total number of Trellis keys
#define TRELLIS_1 0x70
#define TRELLIS_2 0x71
#define TRELLIS_3 0x72
#define TRELLIS_4 0x73

//Serial values
#define MIDI_BAUD 31250
#define SERIAL_BAUD 9600

#define MAIN_LOOP_DELAY 20      //delay needed for trellis, maybe could be reduced?
#define TRELLIS_BUTTON_DELAY  5 //delay for i2c operations

//I2C LCD - Pin numbers may need to be modified for different lcds
#define I2C_ADDR      0x27  // Define I2C Address where the PCF8574A is
#define BACKLIGHT_PIN 3
#define En_pin  2
#define Rw_pin  1
#define Rs_pin  0
#define D4_pin  4
#define D5_pin  5
#define D6_pin  6
#define D7_pin  7
#define LCD_X 16  //Characters wide
#define LCD_Y  2  //Line tall

//MIDI - Based on spec http://www.midi.org/techspecs/midimessages.php
//Status Messages
#define MIDI_NOTE_ON      0x90
#define MIDI_NOTE_OFF     0x80
#define MIDI_AFTERTOUCH   0xA0 //unused for now
#define MIDI_CONTROL      0xB0
#define MIDI_PITCH_CHANGE 0xE0

//MIDI Constants
#define MIDI_CHAN_MAX        16   //16 Channels -- 16 voices 0-15
#define MIDI_CONTROLLER_MAX 120   //120 Controllers
#define MIDI_DATA_MAX       127   //128 Max Values/Notes/Pressures/Etc
#define MIDI_VOICE_DEFAULT    0   //default voice to use 0-15. probably 0

#define MIDI_VELOCITY_ID 128      //Controller number to be mapped to Velocity byte


//EEPROM Addresses
#define ADR_VOICE     0
#define ADR_POT0      1
#define ADR_POT1      2
#define ADR_POT2      3
#define ADR_POT3      4
#define ADR_POT4      5
#define ADR_POT5      6
#define ADR_STARTNOTE 7
#define ADR_SCALE     8


//Trellis setup
Adafruit_Trellis matrix0 = Adafruit_Trellis();
Adafruit_Trellis matrix1 = Adafruit_Trellis();
Adafruit_Trellis matrix2 = Adafruit_Trellis();
Adafruit_Trellis matrix3 = Adafruit_Trellis();

Adafruit_TrellisSet trellis =  Adafruit_TrellisSet(&matrix0, &matrix1, &matrix2, &matrix3);

SoftwareSerial softMidi(MIDIIN, MIDIOUT);

LiquidCrystal_I2C lcd(I2C_ADDR,En_pin,Rw_pin,Rs_pin,D4_pin,D5_pin,D6_pin,D7_pin);

const String boardName = "baxboard";

//Knob setup
#define NUMPOTS 6
uint8_t knob[]           = { A7, A6, A3, A2, A1, A0 }; //knob pin numbers
uint8_t midiController[] = {  7, 10, 12, 13, MIDI_VELOCITY_ID, 14 }; //default controller ids
uint8_t lastPotVal[NUMPOTS];

//Button setup
#define NUMBUTTONS 4
#define BUTTON_UP    0
#define BUTTON_DOWN  1
#define BUTTON_LEFT  2
#define BUTTON_RIGHT 3
uint8_t button[] = { 4, 5, 6, 7 }; //button pin numbers
uint8_t lastButtonVal[NUMBUTTONS];

uint8_t selectedVoice = MIDI_VOICE_DEFAULT;

boolean sendNoteOff = true;
uint8_t startingNote = 0x1E;

long lastTime = 0; //debug variable used to hold loop start time

//Menu system setup and states
#define NUMMENU 10       //increase to match new menu options
enum menuItems { SETVOICE, SETPOT0, SETPOT1, SETPOT2, SETPOT3, SETPOT4, SETPOT5, SETSTART_NOTE, SETSCALE, SAVE };
uint8_t menuState = SETVOICE;
prog_char menu_0[] PROGMEM = "Set Voice";
prog_char menu_1[] PROGMEM = "Set Knob 1";
prog_char menu_2[] PROGMEM = "Set Knob 2";
prog_char menu_3[] PROGMEM = "Set Knob 3";
prog_char menu_4[] PROGMEM = "Set Knob 4";
prog_char menu_5[] PROGMEM = "Set Joy X";
prog_char menu_6[] PROGMEM = "Set Joy Y";
prog_char menu_7[] PROGMEM = "Start Note";
prog_char menu_8[] PROGMEM = "Scale Selection";
prog_char menu_9[] PROGMEM = "Save Settings";
PROGMEM const char *menuTable[] = { menu_0, menu_1, menu_2, menu_3, menu_4, menu_5, menu_6, menu_7, menu_8, menu_9 };
char menuBuffer[LCD_X + 1];       //max lcd size + null terminator

//Note names
char* midiNotes[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#" };

//Scale patterns
#define WHOLE 2
#define HALF  1
#define WHOLEANDHALF 3
#define CHROMATIC   0
#define NUMOFSCALES 7    //number of scales + 1 for chromatic
uint8_t scales[][7] = {  { WHOLE, WHOLE, HALF, WHOLE, WHOLE, WHOLE, HALF },       //major
                         { WHOLE, HALF, WHOLE, WHOLE, HALF, WHOLE, WHOLE },       //minor
                         { WHOLE, HALF, WHOLE, WHOLE, HALF, WHOLEANDHALF, HALF }, //harmonic minor
                         { WHOLE, HALF, WHOLE, WHOLE, WHOLE, WHOLE, HALF },       //melodic minor
                         { WHOLE, HALF, WHOLE, WHOLE, WHOLE, HALF, WHOLE },       //dorian
                         { WHOLE, WHOLE, HALF, WHOLE, WHOLE, HALF, WHOLE }        //mixolydian
                      };
uint8_t selectedScale = CHROMATIC;

//Scale names
prog_char chromScaleName[] PROGMEM = "Chromatic";
prog_char majorScaleName[] PROGMEM = "Major";
prog_char minorScaleName[] PROGMEM = "Minor";
prog_char harmScaleName[]  PROGMEM = "Harmonic";
prog_char melScaleName[]   PROGMEM = "Melodic";
prog_char dorScaleName[]   PROGMEM = "Dorian";
prog_char mixScaleName[]   PROGMEM = "Mixolydian";
PROGMEM const char *scaleNameTable[] = { chromScaleName, majorScaleName, minorScaleName, 
    harmScaleName, melScaleName, dorScaleName, mixScaleName };
char scaleNameBuffer[LCD_X + 1];


void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println(boardName);
  
  //init lcd
  lcd.begin (LCD_X, LCD_Y);
  lcd.setBacklightPin(BACKLIGHT_PIN,POSITIVE);
  lcd.setBacklight(HIGH);
  
  softMidi.begin(MIDI_BAUD);
  
  //init array of last pot read values and send current knob positions to synth
  for (uint8_t i = 0; i < NUMPOTS; i++)
    lastPotVal[i] = 0;
  readKnobs();
  
  lcd.clear();
  lcd.home();
  lcd.print("Startup");
  
  //init buttons
   for (uint8_t i = 0; i < NUMBUTTONS; i++) {
    pinMode(button[i], INPUT_PULLUP);
    lastButtonVal[i] = HIGH;
  }
  
  loadEEPROMValues();
  
  trellis.begin(TRELLIS_1, TRELLIS_2, TRELLIS_3, TRELLIS_4);
  
  ledDemo();
  
  lcd.clear();
  lcd.home();
  lcd.print("Welcome to");  
  lcd.setCursor ( 0, 1 );
  lcd.print(boardName);
}


void loop() {
  #if DEBUG_LOOP_TIME
    lastTime = millis();
  #endif
  
  delay(MAIN_LOOP_DELAY);
  
  readTrellis();
  readKnobs();
  readButtons();
  
  #if DEBUG_LOOP_TIME
    Serial.print("Loop time: ");
    Serial.println(millis() - lastTime);
  #endif
}

/**
 * loadEEPROMValues
 *
 * Only ran from setup, load stored values from EEPROM.
 * 
 * Note: 255 is the default unwritten value, but with the midi spec most items
 * max out at 127 so we know these values were not written by this app
 */
void loadEEPROMValues() {
  uint8_t readVal = EEPROM.read(ADR_VOICE);
  if (readVal < 255) selectedVoice = readVal;
  
  readVal = EEPROM.read(ADR_POT0);
  if (readVal < 255) midiController[0] = readVal;
  
  readVal = EEPROM.read(ADR_POT1);
  if (readVal < 255) midiController[1] = readVal;
  
  readVal = EEPROM.read(ADR_POT2);
  if (readVal < 255) midiController[2] = readVal;
  
  readVal = EEPROM.read(ADR_POT3);
  if (readVal < 255) midiController[3] = readVal;
  
  readVal = EEPROM.read(ADR_POT4);
  if (readVal < 255) midiController[4] = readVal;
  
  readVal = EEPROM.read(ADR_POT5);
  if (readVal < 255) midiController[5] = readVal;
  
  readVal = EEPROM.read(ADR_STARTNOTE);
  if (readVal < 255) startingNote = readVal;
  
  readVal = EEPROM.read(ADR_SCALE);
  if (readVal < 255) selectedScale = readVal;
}

/**
 * saveEEPROMValues
 *
 * Saves current variables to eeprom using predefined addressed. 
 * 
 * Note: When adding additional pots/controls need to add lines to this 
 * function and the read function, in addition to the #defines above.
 */
void saveEEPROMValues() {
  EEPROM.write(ADR_VOICE, selectedVoice);
  EEPROM.write(ADR_POT0, midiController[0]);
  EEPROM.write(ADR_POT1, midiController[1]);
  EEPROM.write(ADR_POT2, midiController[2]);
  EEPROM.write(ADR_POT3, midiController[3]);
  EEPROM.write(ADR_POT4, midiController[4]);
  EEPROM.write(ADR_POT5, midiController[5]);
  EEPROM.write(ADR_STARTNOTE, startingNote);
  EEPROM.write(ADR_SCALE, selectedScale);
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
 * readTrellis
 *
 * Reads the buttons that are pressed and calls trellisPressed with the 
 * corrected button number, turns pressed button leds on until they are 
 * released.
 *
 * Note: This only detects buttons when they are pressed. Holding buttons 
 * currently has no different effect other than the lcd stays lit.
 */
void readTrellis() {
  if (trellis.readSwitches()) {
    for (uint8_t i = 0; i < NUMKEYS; i++) {
      if (trellis.justPressed(i)) {
        uint8_t m = mapButton(i);
        
        #if DEBUG_TRELLIS
          Serial.print("v"); Serial.println(m);
        #endif
        
        trellis.setLED(i);
        trellisPressed(m);
      } 
      else if (trellis.justReleased(i)) {
        uint8_t m = mapButton(i);
        
        #if DEBUG_TRELLIS
          Serial.print("^"); Serial.println(m);
        #endif
        
        trellis.clrLED(i);
        trellisReleased(m);
      }
    }
    trellis.writeDisplay();
  }
}

/**
 * readKnobs
 * 
 * Reads the knobs, if the value of one is different from the previous read, 
 * then is sends its value to the midicontroller number selected for the 
 * selected voice. 
 *
 * Note: Currently, when the voice is changed, the knobs do not resend their 
 * values. This helps to preserve the previous voice's settings.
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
	  
      //only send control commands for valid controller numbers; >= 128 is reserved for other internal mappings
      if (midiController[i] < 128)
        midiCommand(MIDI_CONTROL + selectedVoice, midiController[i],  mapVal); //mod
    }
  }
}

/**
 * readButtons
 *
 * TODO change to debouncer with timer so that presses can be repeated if the button is held down
 */
void readButtons() {
  #if DEBUG_BUTTONS
    Serial.print("button ");
  #endif
  
  for (uint8_t i = 0; i < NUMBUTTONS; i++) {
    uint8_t val = digitalRead(button[i]);
    #if DEBUG_BUTTONS
      Serial.print(val);
      Serial.print(", ");
    #endif
    
    if (val != lastButtonVal[i]) {
      if (val == BUTTON_PRESSED)
        buttonPressed(i);
      lastButtonVal[i] = val;
    }
  }
  
  #if DEBUG_BUTTONS
    Serial.println();
  #endif
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
  
  #if DEBUG_TRELLIS
    Serial.print("Mapped button: ");
    Serial.println(button);
  #endif

  return button;
}

/**
 * trellisPressed
 * 
 * @param uint8_t b - Corrected button number (0-63) beginning at bottom left
 *
 * Adds starting note to the corrected button number (0-63) to the scale 
 * position if one is selected (I-VII), displays the note name, and sends 
 * the midi NOTE ON command.
 */
void trellisPressed(uint8_t b) {
  b += startingNote;
  b = buttonToScaleMap(b);
  showNote(b);
  sendMidiNote(b);
}

/**
 * trellisReleased
 *
 * @param uint8_t b - Corrected button number
 *
 * Send note off, command is note with Velocity = 0
 */
void trellisReleased(uint8_t b) {
  if (sendNoteOff)
    midiCommand(MIDI_NOTE_OFF + selectedVoice, b, 0);
}

/**
 * buttonPressed
 *
 * @param uint8_t b - Button number that was pressed
 *
 * Handler for button presses, expects 4 buttons (0-3)
 * Controls menu changes and events
 */
void buttonPressed(uint8_t b) {
  #if DEBUG_BUTTON_PRESSED
    Serial.print("Button: ");
    Serial.print(b);
    Serial.print(" old menuState: ");
    Serial.print(menuState);
  #endif
  
  //handle menu changes first
  if (b == BUTTON_UP && menuState > 0) menuState--;
  else if (b == BUTTON_DOWN && menuState + 1 < NUMMENU) menuState++;
  showMenuItem();
  
  #if DEBUG_BUTTON_PRESSED
    Serial.print(" new menuState: ");
    Serial.println(menuState);
  #endif
  
  //handle voice changes
  if (menuState == SETVOICE) {
    if (b == BUTTON_LEFT && selectedVoice > 0) selectedVoice--;
    else if (b == BUTTON_RIGHT && selectedVoice + 1 < MIDI_CHAN_MAX) selectedVoice++;

    lcd.setCursor(LCD_X - 2, 0);
    if (selectedVoice < 10) lcd.print(' ');
    lcd.print(selectedVoice + 1);
  }
  else if (menuState == SETPOT0) {
    changeMidiController(b, 0);
  }
  else if (menuState == SETPOT1) {
    changeMidiController(b, 1);
  }
  else if (menuState == SETPOT2) {
    changeMidiController(b, 2);
  }
  else if (menuState == SETPOT3) {
    changeMidiController(b, 3);
  }
  else if (menuState == SETPOT4) {
    changeMidiController(b, 4);
  }
  else if (menuState == SETPOT5) {
    changeMidiController(b, 5);
  }
  else if (menuState == SETSTART_NOTE) {
    if (b == BUTTON_LEFT && startingNote > 0) startingNote--;
    else if (b == BUTTON_RIGHT && startingNote + 1 < 127) startingNote++;
    
    lcd.setCursor(LCD_X - 3, 0);
    showNote(startingNote);
  }
  else if (menuState == SETSCALE) {
    if (b == BUTTON_LEFT && selectedScale > 0) selectedScale--;
    else if (b == BUTTON_RIGHT && selectedScale + 1 < NUMOFSCALES) selectedScale++;
    
    strcpy_P(scaleNameBuffer, (char*)pgm_read_word(&(scaleNameTable[selectedScale]))); //load text from progmem
    lcd.setCursor(0, 1);
    lcd.print(scaleNameBuffer);
  }
  else if (menuState == SAVE) {
    if (b == BUTTON_LEFT || b == BUTTON_RIGHT) {
      saveEEPROMValues();
      lcd.setCursor(LCD_X - 4, 0);
      lcd.print("Done");
    }
  }
}

/**
 * changeMidiController
 *
 * @param b - Button Number
 * @param c - Knob Number
 *
 * Handles menu state changes for midi controller ids
 */
void changeMidiController(uint8_t b, uint8_t c) {
  if (b == BUTTON_LEFT && midiController[c] > 0) midiController[c]--;
  else if (b == BUTTON_RIGHT && midiController[c] + 1 < 200) midiController[c]++;

  lcd.setCursor(LCD_X - 3, 0);
  if (midiController[c] < 100) lcd.print(' ');
  if (midiController[c] <  10) lcd.print(' ');
  lcd.print(midiController[c]);
}

/**
 * showMenuItem
 * 
 * Loads and displays the string for the selected menuState from PROGMEM
 */
void showMenuItem() {
  strcpy_P(menuBuffer, (char*)pgm_read_word(&(menuTable[menuState]))); //load menu text from progmem
    
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(menuBuffer);
}

/**
 * sendMidiNote
 * 
 * @param uint8_t note
 
 * Simple function to output midi note
 * 
 */
void sendMidiNote(uint8_t note) {
  if (note > 127) note = 127; //max possible note value
  uint8_t v = 0x45;
  
  //loop through knob controller ids to find if one is mapped to velocity, use last value
  for (uint8_t i = 0; i < NUMPOTS; i++) {
    if (midiController[i] == MIDI_VELOCITY_ID) {
      v = lastPotVal[i];
      break;
    }
  }
  
  midiCommand(MIDI_NOTE_ON + selectedVoice, note, v);
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
 *
 * Note: Some software counts octaves from 1, this counts from 0
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


/**
 * buttonToScaleMap
 *
 * @param b - Corrected Button Number + Starting note
 *
 * @return Midi note value (0-127)
 *
 * Converts the corrected button number + starting note, to the note on the 
 * selected scale (I, II, III, IV, V, VI, VII, I) by column for the scale 
 * starting with the note of the leftmost button. 
 *
 * The Rows increment based on the 8th increment from the starting note, with 
 * the 4th row being two octaves higher.
 * Ex: Starting note C3, Major scale, Rows are from bottom up: 
 *    C3, G#3, E4, C5, G#5, E6, C7, G#7
 * 
 * If selected scale is Chromatic, returns b.
 * 
 * TODO: Find a better pattern for the Y axis, or make it configurable
 */
uint8_t buttonToScaleMap(uint8_t b) {
  #if DEBUG_SCALES
    Serial.print("Button: ");
    Serial.print(b);
    Serial.print(", ");
    Serial.print(noteToString(b));
  #endif
  if (selectedScale == CHROMATIC) {
    #if DEBUG_SCALES
      Serial.println(" Chromatic ");
    #endif
    
    return b;
  }
  
  uint8_t x = (b - startingNote) % 8;  //button x
  uint8_t note = (b - x) * (12 / 8);
  //note += ( 12 / 8 * b );
  if (note > 127) return note;
  
  #if DEBUG_SCALES
    Serial.print(" x: ");
    Serial.print(x);
  #endif
  
  for (uint8_t i = 0; i < x; i++) {
    uint8_t scaleNote = scales[selectedScale-1][i];
    
  #if DEBUG_SCALES
    Serial.print(" scale note: ");
    Serial.print(scaleNote);
  #endif
  
    note += scaleNote; //add scale notes
  }
  
  #if DEBUG_SCALES
    Serial.print(" Note: ");
    Serial.print(note);
    Serial.print(", ");
    Serial.println(noteToString(note));
  #endif
  return note;
}