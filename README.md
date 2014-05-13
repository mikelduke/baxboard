baxboard
========

baxboard - a midi controller

******************************************************************************

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
  
******************************************************************************

  Files
  
  baxboard.ino - Arduino Sketch
  
  designs/baxboard.svg - An SVG image suitable for laser cutting the panel, 
    created in Inkscape, modification may be required depending on the equipment
    
  designs/TrellisPad.svg - The SVG image I created for use in the baxboard panel 
    design, posted separately for easy reuse
