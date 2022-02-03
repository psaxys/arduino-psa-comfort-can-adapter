/*
Copyright 2019-2022, Ludwig V. <https://github.com/ludwig-v>
Copyright 2021, Nick V. (V3nn3tj3) <https://github.com/v3nn3tj3>
  
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License at <http://www.gnu.org/licenses/> for
more details.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.
*/

/////////////////////
//    Libraries    //
/////////////////////

#include <EEPROM.h>
#include <SPI.h>
#include <Time.h>
#include <TimeLib.h>
#include <Wire.h>
#include <DS1307RTC.h> // https://github.com/PaulStoffregen/DS1307RTC
#include <mcp2515.h> // https://github.com/autowp/arduino-mcp2515 + https://github.com/watterott/Arduino-Libs/tree/master/digitalWriteFast

/////////////////////
//  Configuration  //
/////////////////////

#define CS_PIN_CAN0 10
#define CS_PIN_CAN1 9
#define SERIAL_SPEED 115200
#define CAN_SPEED CAN_125KBPS // Entertainment CAN bus - Low speed
#define CAN_FREQ MCP_16MHZ // Switch to 8MHZ if you have a 8Mhz module

////////////////////
// Initialization //
////////////////////

MCP2515 CAN0(CS_PIN_CAN0); // CAN-BUS Shield N°1
MCP2515 CAN1(CS_PIN_CAN1); // CAN-BUS Shield N°2

////////////////////
//   Variables    //
////////////////////

// My variables
bool debugGeneral = false; // Get some debug informations on Serial
bool debugCAN0 = false; // Read data sent by ECUs from the car to Entertainment CAN bus using https://github.com/alexandreblin/python-can-monitor
bool debugCAN1 = false; // Read data sent by the NAC / SMEG to Entertainment CAN bus using https://github.com/alexandreblin/python-can-monitor
bool EconomyModeEnabled = true; // You can disable economy mode on the Telematic if you want to - Not recommended at all
bool Send_CAN2010_ForgedMessages = false; // Send forged CAN2010 messages to the CAR CAN-BUS Network (useful for testing CAN2010 device(s) from already existent connectors)
bool TemperatureInF = false; // Default Temperature in Celcius
bool mpgMi = false;
bool kmL = false; // km/L statistics instead of L/100
bool fixedBrightness = false; // Force Brightness value in case the calibration does not match your brightness value range
bool noFMUX = false; // If you don't have any useful button on the main panel, turn the SRC button on steering wheel commands into MENU - only works for CAN2010 SMEG / NAC -
byte languageID = 0; // Default is FR: 0 - EN: 1 / DE: 2 / ES: 3 / IT: 4 / PT: 5 / NL: 6 / BR: 9 / TR: 12 / RU: 14
bool listenCAN2004Language = false; // Switch language on CAN2010 devices if changed on supported CAN2004 devices, default: no
byte Time_day = 1; // Default day if the RTC module is not configured
byte Time_month = 1; // Default month if the RTC module is not configured
int Time_year = 2022; // Default year if the RTC module is not configured
byte Time_hour = 0; // Default hour if the RTC module is not configured
byte Time_minute = 0; // Default minute if the RTC module is not configured
bool resetEEPROM = false; // Switch to true to reset all EEPROM values
bool CVM_Emul = true; // Send suggested speed from Telematic to fake CVM (Multifunction camera inside the windshield) frame

bool emulateVIN = false; // Replace network VIN by another (donor car for example)
char vinNumber[18] = "VF3XXXXXXXXXXXXXX";

bool hasAnalogicButtons = false; // Analog buttons instead of FMUX
byte menuButton = 4;
byte volDownButton = 5;
byte volUpButton = 6;
byte scrollValue = 0;

// Default variables
bool Ignition = false;
bool SerialEnabled = false;
int Temperature = 0;
bool EconomyMode = false;
bool EngineRunning = false;
byte languageID_CAN2004 = 0;
bool AirConditioningON = false;
byte FanSpeed = 0;
bool FanOff = false;
bool AirRecycle = false;
bool DeMist = false;
bool DeFrost = false;
byte LeftTemp = 0;
byte RightTemp = 0;
bool Mono = false;
bool FootAerator = false;
bool WindShieldAerator = false;
bool CentralAerator = false;
bool AutoFan = false;
byte FanPosition = 0;
bool MaintenanceDisplayed = false;
byte carType = 0;
int buttonState = 0;
int lastButtonState = 0;
long lastDebounceTime = 0;
long buttonPushTime = 0;
long buttonSendTime = 0;
long debounceDelay = 100;
int daysSinceYearStart = 0;
unsigned long customTimeStamp = 0;
int vehicleSpeed = 0;
byte speedMargin = 3;
int engineRPM = 0;

// Language & Unit CAN2010 value
byte languageAndUnitNum = (languageID * 4) + 128;

// CAN-BUS Messages
struct can_frame canMsgSnd;
struct can_frame canMsgRcv;

void setup() {
  int tmpVal;

  if (resetEEPROM) {
    EEPROM.update(0, 0);
    EEPROM.update(1, 0);
    EEPROM.update(2, 0);
    EEPROM.update(3, 0);
    EEPROM.update(4, 0);
    EEPROM.update(5, 0);
    EEPROM.update(6, 0);
    EEPROM.update(7, 0);
  }

  if (debugCAN0 || debugCAN1 || debugGeneral) {
    SerialEnabled = true;
  }

  // Read data from EEPROM
  tmpVal = EEPROM.read(0);
  if (tmpVal >= 128) {
    languageAndUnitNum = tmpVal;
  }

  if ((languageAndUnitNum % 2) == 0 && kmL) {
    languageAndUnitNum = languageAndUnitNum + 1;
  }

  tmpVal = EEPROM.read(1);
  if (tmpVal <= 32) {
    languageID_CAN2004 = tmpVal;
  }

  tmpVal = EEPROM.read(2);
  if (tmpVal <= 32) {
    languageID = tmpVal;
  }

  tmpVal = EEPROM.read(3);
  if (tmpVal == 1) {
    TemperatureInF = true;
  }

  tmpVal = EEPROM.read(4);
  if (tmpVal == 1) {
    mpgMi = true;
  }

  tmpVal = EEPROM.read(5);
  if (tmpVal <= 31) {
    Time_day = tmpVal;
  }

  tmpVal = EEPROM.read(6);
  if (tmpVal <= 12) {
    Time_month = tmpVal;
  }

  EEPROM.get(7, tmpVal); // int
  if (tmpVal >= 1872 && tmpVal <= 2127) {
    Time_year = tmpVal;
  }

  if (hasAnalogicButtons) {
    //Initialize buttons - MENU/VOL+/VOL-
    pinMode(menuButton, INPUT_PULLUP);
    pinMode(volDownButton, INPUT_PULLUP);
    pinMode(volUpButton, INPUT_PULLUP);
  }

  if (SerialEnabled) {
    // Initalize Serial for debug
    Serial.begin(SERIAL_SPEED);

    // CAN-BUS from car
    Serial.println("Initialization CAN0");
  }

  CAN0.reset();
  CAN0.setBitrate(CAN_SPEED, CAN_FREQ);
  while (CAN0.setNormalMode() != MCP2515::ERROR_OK) {
    delay(100);
  }

  if (SerialEnabled) {
    // CAN-BUS to CAN2010 device(s)
    Serial.println("Initialization CAN1");
  }

  CAN1.reset();
  CAN1.setBitrate(CAN_SPEED, CAN_FREQ);
  while (CAN1.setNormalMode() != MCP2515::ERROR_OK) {
    delay(100);
  }

  setSyncProvider(RTC.get); // Get time from the RTC module
  if (timeStatus() != timeSet) {
    if (SerialEnabled) {
      Serial.println("Unable to sync with the RTC");
    }

    // Set default time (01/01/2020 00:00)
    setTime(Time_hour, Time_minute, 0, Time_day, Time_month, Time_year);
    EEPROM.update(5, Time_day);
    EEPROM.update(6, Time_month);
    EEPROM.put(7, Time_year);
  } else if (SerialEnabled) {
    Serial.println("RTC has set the system time");
  }

  // Set hour on CAN-BUS Clock
  canMsgSnd.data[0] = hour();
  canMsgSnd.data[1] = minute();
  canMsgSnd.can_id = 0x228;
  canMsgSnd.can_dlc = 2;
  CAN0.sendMessage( & canMsgSnd);

  // Send fake EMF version
  canMsgSnd.data[0] = 0x25;
  canMsgSnd.data[1] = 0x1D;
  canMsgSnd.data[2] = 0x03;
  canMsgSnd.data[3] = 0x06;
  canMsgSnd.data[4] = 0x08;
  canMsgSnd.data[5] = 0x00;
  canMsgSnd.data[6] = 0x20;
  canMsgSnd.data[7] = 0x10;
  canMsgSnd.can_id = 0x5E5;
  canMsgSnd.can_dlc = 8;
  CAN0.sendMessage( & canMsgSnd);

  if (SerialEnabled) {
    Serial.print("Current Time: ");
    Serial.print(day());
    Serial.print("/");
    Serial.print(month());
    Serial.print("/");
    Serial.print(year());

    Serial.print(" ");

    Serial.print(hour());
    Serial.print(":");
    Serial.print(minute());

    Serial.println();
  }
}

void loop() {
  int tmpVal;

  if (hasAnalogicButtons) {
    // Receive buttons from the car
    if (((millis() - lastDebounceTime) > debounceDelay)) {
      tmpVal = 0;
      if (!digitalRead(menuButton)) tmpVal += 0b001;
      if (!digitalRead(volDownButton)) tmpVal += 0b010;
      if (!digitalRead(volUpButton)) tmpVal += 0b100;
      if (tmpVal != lastButtonState) {
        buttonPushTime = millis();
        buttonSendTime = 0;
        //buttonPushState = 0;
      }
      if ((millis() - buttonPushTime) > 100) {
        switch (tmpVal) {
        case 0b001:
          //canMsgSnd.data[0] = 0x02; // MENU button
          canMsgSnd.data[0] = 0x02;
          canMsgSnd.data[1] = 0x00;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.data[3] = 0x00;
          canMsgSnd.data[4] = 0x00;
          canMsgSnd.data[5] = 0xFF;
          canMsgSnd.data[6] = 0x00;
          canMsgSnd.data[7] = 0x00;
          canMsgSnd.can_id = 0x122;
          canMsgSnd.can_dlc = 8;
          // Menu button
          if (buttonSendTime == 0) {
            CAN1.sendMessage( & canMsgSnd);
            if (SerialEnabled) {
              Serial.println("Menu");
            }
            lastDebounceTime = millis();
            buttonSendTime = millis();
            //buttonPushState = 1;
          } else if (millis() - buttonPushTime > 800 && ((millis() - buttonPushTime < 2000 && millis() - buttonSendTime > 600) || (millis() - buttonPushTime > 2000 && millis() - buttonSendTime > 350))) {
            CAN1.sendMessage( & canMsgSnd);
            if (SerialEnabled) {
              Serial.println("Menu");
            }
            buttonSendTime = millis();
            lastDebounceTime = millis();
          }
          break;
        case 0b010:
          canMsgSnd.data[0] = 0x04; //Volume down
          canMsgSnd.data[1] = scrollValue;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.can_id = 0x21F;
          canMsgSnd.can_dlc = 3;
          // Menu button
          if (buttonSendTime == 0) {
            CAN1.sendMessage( & canMsgSnd);
            if (SerialEnabled) {
              Serial.println("Vol -");
            }
            lastDebounceTime = millis();
            buttonSendTime = millis();
            //buttonPushState = 1;
          } else if (millis() - buttonPushTime > 800 && ((millis() - buttonPushTime < 2000 && millis() - buttonSendTime > 600) || (millis() - buttonPushTime > 2000 && millis() - buttonSendTime > 350))) {
            CAN1.sendMessage( & canMsgSnd);
            if (SerialEnabled) {
              Serial.println("Vol -");
            }
            buttonSendTime = millis();
            lastDebounceTime = millis();
          }
          break;
        case 0b100:
          canMsgSnd.data[0] = 0x08; //Volume down
          canMsgSnd.data[1] = scrollValue;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.can_id = 0x21F;
          canMsgSnd.can_dlc = 3;
          // Menu button
          if (buttonSendTime == 0) {
            CAN1.sendMessage( & canMsgSnd);
            if (SerialEnabled) {
              Serial.println("Vol +");
            }
            lastDebounceTime = millis();
            buttonSendTime = millis();
            //buttonPushState = 1;
          } else if (millis() - buttonPushTime > 800 && ((millis() - buttonPushTime < 2000 && millis() - buttonSendTime > 600) || (millis() - buttonPushTime > 2000 && millis() - buttonSendTime > 350))) {
            CAN1.sendMessage( & canMsgSnd);
            if (SerialEnabled) {
              Serial.println("Vol +");
            }
            buttonSendTime = millis();
            lastDebounceTime = millis();
          }
          break;
        case 0b110:
          canMsgSnd.data[0] = 0x0C; //Mute
          canMsgSnd.data[1] = scrollValue;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.can_id = 0x21F;
          canMsgSnd.can_dlc = 3;
          // Menu button
          if (buttonSendTime == 0) {
            CAN1.sendMessage( & canMsgSnd);
            if (SerialEnabled) {
              Serial.println("Mute");
            }
            lastDebounceTime = millis();
            buttonSendTime = millis();
            //buttonPushState = 1;
          }
          break;
        default:
          //buttonPushState = 0;
          lastDebounceTime = millis();
        }
      }
      lastButtonState = tmpVal;
    }
  }

  // Receive CAN messages from the car
  if (CAN0.readMessage( & canMsgRcv) == MCP2515::ERROR_OK) {
    int id = canMsgRcv.can_id;
    int len = canMsgRcv.can_dlc;

    if (debugCAN0) {
      Serial.print("FRAME:ID=");
      Serial.print(id);
      Serial.print(":LEN=");
      Serial.print(len);

      char tmp[3];
      for (int i = 0; i < len; i++) {
        Serial.print(":");

        snprintf(tmp, 3, "%02X", canMsgRcv.data[i]);

        Serial.print(tmp);
      }

      Serial.println();

      CAN1.sendMessage( & canMsgRcv);
    } else if (!debugCAN1) {
      if (id == 54 && len == 8) { // Economy Mode detection
        if (bitRead(canMsgRcv.data[2], 7) == 1) {
          if (!EconomyMode && SerialEnabled) {
            Serial.println("Economy mode ON");
          }

          EconomyMode = true;
        } else {
          if (EconomyMode && SerialEnabled) {
            Serial.println("Economy mode OFF");
          }

          EconomyMode = false;
        }

        tmpVal = canMsgRcv.data[3];

        // Fix brightness when car lights are ON - Brightness Instrument Panel "20" > "2F" (32 > 47) - Depends on your car
        if (fixedBrightness && tmpVal >= 32) {
          canMsgRcv.data[3] = 0x28; // Set fixed value to avoid low brightness due to incorrect CAN2010 Telematic calibration
        }
        CAN1.sendMessage( & canMsgRcv);

        canMsgSnd.data[0] = 0x08;
        canMsgSnd.data[1] = 0x10;
        canMsgSnd.data[2] = 0xFF;
        canMsgSnd.data[3] = 0xFF;
        canMsgSnd.data[4] = 0x7F;
        canMsgSnd.data[5] = 0xFF;
        canMsgSnd.data[6] = 0x00;
        canMsgSnd.data[7] = 0x00;
        canMsgSnd.can_id = 0x167; // Fake EMF status frame
        canMsgSnd.can_dlc = 8;
        CAN0.sendMessage( & canMsgSnd);
      } else if (id == 359) { // EMF status frame
        // Do not forward, acting as FMUX on NAC
      } else if (id == 182 && len == 8) {
        engineRPM = (canMsgRcv.data[2] << 8) | (canMsgRcv.data[3] << 3);
        if (engineRPM > 0) {
          EngineRunning = true;
        } else {
          EngineRunning = false;
        }
        vehicleSpeed = ((canMsgRcv.data[2] << 8) | canMsgRcv.data[3]) / 100;
        CAN1.sendMessage( & canMsgRcv);
      } else if (id == 822 && len == 3 && emulateVIN) { // ASCII coded first 3 letters of VIN
        canMsgSnd.data[0] = vinNumber[0]; //V
        canMsgSnd.data[1] = vinNumber[1]; //F
        canMsgSnd.data[2] = vinNumber[2]; //3
        canMsgSnd.can_id = 0x336;
        canMsgSnd.can_dlc = 3;
        CAN1.sendMessage( & canMsgSnd);
      } else if (id == 950 && len == 6 && emulateVIN) { // ASCII coded 4-9 letters of VIN
        canMsgSnd.data[0] = vinNumber[3]; //X
        canMsgSnd.data[1] = vinNumber[4]; //X
        canMsgSnd.data[2] = vinNumber[5]; //X
        canMsgSnd.data[3] = vinNumber[6]; //X
        canMsgSnd.data[4] = vinNumber[7]; //X
        canMsgSnd.data[5] = vinNumber[8]; //X
        canMsgSnd.can_id = 0x3B6;
        canMsgSnd.can_dlc = 6;
        CAN1.sendMessage( & canMsgSnd);
      } else if (id == 694 && len == 8 && emulateVIN) { //ASCII coded 10-17 letters (last 8) of VIN
        canMsgSnd.data[0] = vinNumber[9]; //X
        canMsgSnd.data[1] = vinNumber[10]; //X
        canMsgSnd.data[2] = vinNumber[11]; //X
        canMsgSnd.data[3] = vinNumber[12]; //X
        canMsgSnd.data[4] = vinNumber[13]; //X
        canMsgSnd.data[5] = vinNumber[14]; //X
        canMsgSnd.data[6] = vinNumber[15]; //X
        canMsgSnd.data[7] = vinNumber[16]; //X
        canMsgSnd.can_id = 0x2B6;
        canMsgSnd.can_dlc = 8;
        CAN1.sendMessage( & canMsgSnd);
      } else if (id == 543 && len == 3) { // 0x21F Steering wheel commands - Generic
        tmpVal = canMsgRcv.data[0];
        scrollValue = canMsgRcv.data[1];

        if (tmpVal == 2 && noFMUX && carType == 0) { // Replace SRC by MENU (Valid for 208, C-Elysee calibrations for example)
          canMsgSnd.data[0] = 0x80; // MENU button
          canMsgSnd.data[1] = 0x00;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.data[3] = 0x00;
          canMsgSnd.data[4] = 0x00;
          canMsgSnd.data[5] = 0x02;
          canMsgSnd.data[6] = 0x00; // Volume potentiometer button
          canMsgSnd.data[7] = 0x00;
        } else {
          CAN1.sendMessage( & canMsgRcv);

          if (noFMUX || hasAnalogicButtons) { // Fake FMUX Buttons in the car
            canMsgSnd.data[0] = 0x00;
            canMsgSnd.data[1] = 0x00;
            canMsgSnd.data[2] = 0x00;
            canMsgSnd.data[3] = 0x00;
            canMsgSnd.data[4] = 0x00;
            canMsgSnd.data[5] = 0x02;
            canMsgSnd.data[6] = 0x00; // Volume potentiometer button
            canMsgSnd.data[7] = 0x00;
          }
        }
        canMsgSnd.can_id = 0x122;
        canMsgSnd.can_dlc = 8;
        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }
      } else if (id == 162 && noFMUX) { // 0xA2 - Steering wheel commands - C4 I
        // Replace RD45 commands (Valid for C4 II calibration for example)
        carType = 1;

        tmpVal = canMsgRcv.data[1];

        if (tmpVal == 8) { // MENU button pushed > MUSIC
          canMsgSnd.data[0] = 0x00;
          canMsgSnd.data[1] = 0x20;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.data[3] = 0x00;
          canMsgSnd.data[4] = 0x00;
          canMsgSnd.data[5] = 0x02;
          canMsgSnd.data[6] = 0x00; // Volume potentiometer button
          canMsgSnd.data[7] = 0x00;
        } else if (tmpVal == 4) { // MODE button pushed > NAV
          canMsgSnd.data[0] = 0x00;
          canMsgSnd.data[1] = 0x08;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.data[3] = 0x00;
          canMsgSnd.data[4] = 0x00;
          canMsgSnd.data[5] = 0x02;
          canMsgSnd.data[6] = 0x00; // Volume potentiometer button
          canMsgSnd.data[7] = 0x00;
        } else if (tmpVal == 16) { // ESC button pushed > APPS
          canMsgSnd.data[0] = 0x00;
          canMsgSnd.data[1] = 0x40;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.data[3] = 0x00;
          canMsgSnd.data[4] = 0x00;
          canMsgSnd.data[5] = 0x02;
          canMsgSnd.data[6] = 0x00; // Volume potentiometer button
          canMsgSnd.data[7] = 0x00;
        } else if (tmpVal == 32) { // OK button pushed > PHONE
          canMsgSnd.data[0] = 0x00;
          canMsgSnd.data[1] = 0x04;
          canMsgSnd.data[2] = 0x08;
          canMsgSnd.data[3] = 0x00;
          canMsgSnd.data[4] = 0x00;
          canMsgSnd.data[5] = 0x02;
          canMsgSnd.data[6] = 0x00; // Volume potentiometer button
          canMsgSnd.data[7] = 0x00;
        } else {
          CAN1.sendMessage( & canMsgRcv);

          // Fake FMUX Buttons in the car
          canMsgSnd.data[0] = 0x00;
          canMsgSnd.data[1] = 0x00;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.data[3] = 0x00;
          canMsgSnd.data[4] = 0x00;
          canMsgSnd.data[5] = 0x02;
          canMsgSnd.data[6] = 0x00; // Volume potentiometer button
          canMsgSnd.data[7] = 0x00;
        }
        canMsgSnd.can_id = 0x122;
        canMsgSnd.can_dlc = 8;
        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }
      } else if (id == 464 && len == 7 && EngineRunning) { // No fan activated if the engine is not ON on old models
        LeftTemp = canMsgRcv.data[5];
        RightTemp = canMsgRcv.data[6];
        if (LeftTemp == RightTemp) { // No other way to detect MONO mode
          Mono = true;
          LeftTemp = LeftTemp + 64;
        } else {
          Mono = false;
        }

        FanOff = false;
        // Fan Speed BSI_2010 = "41" (Off) > "49" (Full speed)
        tmpVal = canMsgRcv.data[2];
        if (tmpVal == 15) {
          FanOff = true;
          FanSpeed = 0x41;
        } else {
          FanSpeed = (tmpVal + 66);
        }

        // Position Fan
        tmpVal = canMsgRcv.data[3];

        if (tmpVal == 0x40) {
          FootAerator = false;
          WindShieldAerator = true;
          CentralAerator = false;
        } else if (tmpVal == 0x30) {
          FootAerator = false;
          WindShieldAerator = false;
          CentralAerator = true;
        } else if (tmpVal == 0x20) {
          FootAerator = true;
          WindShieldAerator = false;
          CentralAerator = false;
        } else if (tmpVal == 0x70) {
          FootAerator = false;
          WindShieldAerator = true;
          CentralAerator = true;
        } else if (tmpVal == 0x80) {
          FootAerator = true;
          WindShieldAerator = true;
          CentralAerator = true;
        } else if (tmpVal == 0x50) {
          FootAerator = true;
          WindShieldAerator = false;
          CentralAerator = true;
        } else if (tmpVal == 0x10) {
          FootAerator = false;
          WindShieldAerator = false;
          CentralAerator = false;
        } else if (tmpVal == 0x60) {
          FootAerator = true;
          WindShieldAerator = true;
          CentralAerator = false;
        } else {
          FootAerator = false;
          WindShieldAerator = false;
          CentralAerator = false;
        }

        tmpVal = canMsgRcv.data[4];
        if (tmpVal == 0x10) {
          DeMist = true;
          AirRecycle = false;
        } else if (tmpVal == 0x30) {
          AirRecycle = true;
        } else {
          AirRecycle = false;
        }

        AutoFan = false;
        DeMist = false;

        tmpVal = canMsgRcv.data[0];
        if (tmpVal == 0x11) {
          DeMist = true;
          AirConditioningON = true;
          FanOff = false;
        } else if (tmpVal == 0x12) {
          DeMist = true;
          AirConditioningON = false;
          FanOff = false;
        } else if (tmpVal == 0x21) {
          DeMist = true;
          AirConditioningON = true;
          FanOff = false;
        } else if (tmpVal == 0xA2) {
          FanOff = true;
          AirConditioningON = false;
        } else if (tmpVal == 0x22) {
          AirConditioningON = false;
        } else if (tmpVal == 0x20) {
          AirConditioningON = true;
        } else if (tmpVal == 0x02) {
          AirConditioningON = false;
          AutoFan = false;
        } else if (tmpVal == 0x00) {
          AirConditioningON = true;
          AutoFan = true;
        }

        if (!FootAerator && !WindShieldAerator && CentralAerator) {
          FanPosition = 0x34;
        } else if (FootAerator && WindShieldAerator && CentralAerator) {
          FanPosition = 0x84;
        } else if (!FootAerator && WindShieldAerator && CentralAerator) {
          FanPosition = 0x74;
        } else if (FootAerator && !WindShieldAerator && CentralAerator) {
          FanPosition = 0x54;
        } else if (FootAerator && !WindShieldAerator && !CentralAerator) {
          FanPosition = 0x24;
        } else if (!FootAerator && WindShieldAerator && !CentralAerator) {
          FanPosition = 0x44;
        } else if (FootAerator && WindShieldAerator && !CentralAerator) {
          FanPosition = 0x64;
        } else {
          FanPosition = 0x04; // Nothing
        }

        if (DeMist) {
          FanSpeed = 0x10;
          FanPosition = FanPosition + 16;
        } else if (AutoFan) {
          FanSpeed = 0x10;
        }

        if (FanOff) {
          AirConditioningON = false;
          FanSpeed = 0x41;
          LeftTemp = 0x00;
          RightTemp = 0x00;
          FanPosition = 0x04;
        }

        if (AirConditioningON) {
          canMsgSnd.data[0] = 0x01; // A/C ON - Auto Soft : "00" / Auto Normal "01" / Auto Fast "02"
        } else {
          canMsgSnd.data[0] = 0x09; // A/C OFF - Auto Soft : "08" / Auto Normal "09" / Auto Fast "0A"
        }

        canMsgSnd.data[1] = 0x00;
        canMsgSnd.data[2] = 0x00;
        canMsgSnd.data[3] = LeftTemp;
        canMsgSnd.data[4] = RightTemp;
        canMsgSnd.data[5] = FanSpeed;
        canMsgSnd.data[6] = FanPosition;
        canMsgSnd.data[7] = 0x00;
        canMsgSnd.can_id = 0x350;
        canMsgSnd.can_dlc = 8;
        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }
      } else if (id == 246 && len == 8) {
        tmpVal = canMsgRcv.data[0];
        if (tmpVal > 128) {
          if (!Ignition && SerialEnabled) {
            Serial.println("Ignition ON");
          }

          Ignition = true;
        } else {
          if (Ignition && SerialEnabled) {
            Serial.println("Ignition OFF");
          }

          Ignition = false;
        }

        tmpVal = ceil(canMsgRcv.data[5] / 2.0) - 40; // Temperatures can be negative but we only have 0 > 255, the new range is starting from -40°C
        if (Temperature != tmpVal) {
          Temperature = tmpVal;

          if (SerialEnabled) {
            Serial.print("Ext. Temperature: ");
            Serial.print(tmpVal);
            Serial.println("°C");
          }
        }

        CAN1.sendMessage( & canMsgRcv);
      } else if (id == 360 && len == 8) { // Instrument Panel
        canMsgSnd.data[0] = canMsgRcv.data[0];
        canMsgSnd.data[1] = canMsgRcv.data[1];
        canMsgSnd.data[2] = canMsgRcv.data[5]; // Investigation to do
        canMsgSnd.data[3] = canMsgRcv.data[3];
        canMsgSnd.data[4] = canMsgRcv.data[5]; // Investigation to do
        canMsgSnd.data[5] = canMsgRcv.data[5]; // Investigation to do
        canMsgSnd.data[6] = canMsgRcv.data[6];
        canMsgSnd.data[7] = canMsgRcv.data[7];
        canMsgSnd.can_id = 0x168;
        canMsgSnd.can_dlc = 8;

        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) { // Will generate some light issues on the instrument panel
          CAN0.sendMessage( & canMsgSnd);
        }
      } else if (id == 545) { // Trip info
        CAN1.sendMessage( & canMsgRcv); // Forward original frame

        customTimeStamp = (long) hour() * (long) 3600 + minute() * 60 + second();
        daysSinceYearStart = daysSinceYearStartFct();

        canMsgSnd.data[0] = (((1 << 8) - 1) & (customTimeStamp >> (12)));
        canMsgSnd.data[1] = (((1 << 8) - 1) & (customTimeStamp >> (4)));
        canMsgSnd.data[2] = (((((1 << 4) - 1) & (customTimeStamp)) << 4)) + (((1 << 4) - 1) & (daysSinceYearStart >> (8)));
        canMsgSnd.data[3] = (((1 << 8) - 1) & (daysSinceYearStart));
        canMsgSnd.data[4] = 0x00;
        canMsgSnd.data[5] = 0xC0;
        canMsgSnd.data[6] = languageID;
        canMsgSnd.can_id = 0x3F6; // Fake EMF Time frame
        canMsgSnd.can_dlc = 7;

        CAN0.sendMessage( & canMsgSnd);
      } else if (id == 296 && len == 8) { // Instrument Panel
        canMsgSnd.data[0] = canMsgRcv.data[4]; // Main driving lights
        canMsgSnd.data[1] = canMsgRcv.data[6];
        canMsgSnd.data[2] = canMsgRcv.data[7];

        tmpVal = canMsgRcv.data[0];
        if (tmpVal == 96) { // Handbrake
          canMsgSnd.data[3] = 0x02;
        } else {
          canMsgSnd.data[3] = 0x00;
        }

        canMsgSnd.data[4] = 0x00; // ESP - Investigation to do
        canMsgSnd.data[5] = 0x00; // Low Fuel - Investigation to do
        canMsgSnd.data[6] = 0x04;
        canMsgSnd.data[7] = 0x00;
        canMsgSnd.can_id = 0x128;
        canMsgSnd.can_dlc = 8;

        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) { // Will generate some light issues on the instrument panel
          CAN0.sendMessage( & canMsgSnd);
        }
      } else if (id == 935 && len == 8) { // Maintenance
        canMsgSnd.data[0] = 0x40;
        canMsgSnd.data[1] = canMsgRcv.data[5]; // Value x255 +
        canMsgSnd.data[2] = canMsgRcv.data[6]; // Value x1 = Number of days till maintenance (FF FF if disabled)
        canMsgSnd.data[3] = canMsgRcv.data[3]; // Value x5120 +
        canMsgSnd.data[4] = canMsgRcv.data[4]; // Value x20 = km left till maintenance
        canMsgSnd.can_id = 0x3E7; // New maintenance frame ID
        canMsgSnd.can_dlc = 5;

        if (SerialEnabled && !MaintenanceDisplayed) {
          Serial.print("Next maintenance in: ");
          if (canMsgRcv.data[3] != 0xFF && canMsgRcv.data[4] != 0xFF) {
            tmpVal = (canMsgRcv.data[3] * 5120) + (canMsgRcv.data[4] * 20);
            Serial.print(tmpVal);
            Serial.println(" km");
          }
          if (canMsgRcv.data[5] != 0xFF && canMsgRcv.data[6] != 0xFF) {
            tmpVal = (canMsgRcv.data[5] * 255) + canMsgRcv.data[6];
            Serial.print(tmpVal);
            Serial.println(" days");
          }
          MaintenanceDisplayed = true;
        }

        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }
      } else if (id == 424 && len == 8) { // Cruise control
        CAN1.sendMessage( & canMsgRcv);

        canMsgSnd.data[0] = canMsgRcv.data[1];
        canMsgSnd.data[1] = canMsgRcv.data[2];
        canMsgSnd.data[2] = canMsgRcv.data[0];
        canMsgSnd.data[3] = 0x80;
        canMsgSnd.data[4] = 0x14;
        canMsgSnd.data[5] = 0x7F;
        canMsgSnd.data[6] = 0xFF;
        canMsgSnd.data[7] = 0x98;
        canMsgSnd.can_id = 0x228; // New cruise control frame ID
        canMsgSnd.can_dlc = 8;
        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }
      } else if (id == 727 && len == 5 && listenCAN2004Language) { // CAN2004 Matrix
        tmpVal = canMsgRcv.data[0];
        if (tmpVal > 32) {
          kmL = true;
          tmpVal = tmpVal - 32;
        }

        if (tmpVal <= 32 && languageID_CAN2004 != tmpVal) {
          languageID_CAN2004 = tmpVal;
          EEPROM.update(1, languageID_CAN2004);

          // Change language and unit on ID 608 for CAN2010 Telematic language change
          languageAndUnitNum = (languageID_CAN2004 * 4) + 128;
          if (kmL) {
            languageAndUnitNum = languageAndUnitNum + 1;
          }
          EEPROM.update(0, languageAndUnitNum);

          if (SerialEnabled) {
            Serial.print("CAN2004 Matrix - Change Language: ");
            Serial.print(tmpVal);
            Serial.println();
          }
        } else {
          Serial.print("CAN2004 Matrix - Unsupported language ID: ");
          Serial.print(tmpVal);
          Serial.println();
        }
      } else if (id == 865) { // 0x361 - Personalization menu
        // Work in progress, do not forward possible old personalization settings from CAN2004
      } else if (id == 608 && len == 8) { // 0x260
        // Do not forward original message, it has been completely redesigned on CAN2010
        // Also forge missing messages from CAN2004

        // Language / Units / Settings
        canMsgSnd.data[0] = languageAndUnitNum;

        if (TemperatureInF) {
          canMsgSnd.data[1] = 0x5C;
        } else {
          canMsgSnd.data[1] = 0x1C;
        }
        if (mpgMi) {
          canMsgSnd.data[1] = canMsgSnd.data[1] + 128;
        }

        // Vehicle Menu settings on NAC/RCC: Investigation to do
        // **************************
        canMsgSnd.data[2] = 0x00;
        canMsgSnd.data[3] = 0x00;
        canMsgSnd.data[4] = 0x00;
        canMsgSnd.data[5] = 0x00;
        canMsgSnd.data[6] = 0x00;
        // **************************

        canMsgSnd.can_id = 0x260;
        canMsgSnd.can_dlc = 7;
        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }

        // Economy mode simulation
        if (EconomyMode && EconomyModeEnabled) {
          canMsgSnd.data[0] = 0x14;
          if (Ignition) {
            canMsgSnd.data[5] = 0x0E;
          } else {
            canMsgSnd.data[5] = 0x0C;
          }
        } else {
          if (EngineRunning) {
            canMsgSnd.data[0] = 0x54;
          } else {
            canMsgSnd.data[0] = 0x04;
          }
          canMsgSnd.data[5] = 0x0F;
        }
        canMsgSnd.data[1] = 0x03;
        canMsgSnd.data[2] = 0xDE;

        canMsgSnd.data[3] = 0x00; // Increasing value,
        canMsgSnd.data[4] = 0x00; // counter ?

        canMsgSnd.data[6] = 0xFE;
        canMsgSnd.data[7] = 0x00;
        canMsgSnd.can_id = 0x236;
        canMsgSnd.can_dlc = 8;
        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }

        // Current Time
        // If time is synced
        if (timeStatus() != timeNotSet) {
          canMsgSnd.data[0] = (year() - 1872); // Year would not fit inside one byte (0 > 255), substract 1872 and you get this new range (1872 > 2127)
          canMsgSnd.data[1] = month();
          canMsgSnd.data[2] = day();
          canMsgSnd.data[3] = hour();
          canMsgSnd.data[4] = minute();
          canMsgSnd.data[5] = 0x3F;
          canMsgSnd.data[6] = 0xFE;
        } else {
          canMsgSnd.data[0] = (Time_year - 1872); // Year would not fit inside one byte (0 > 255), substract 1872 and you get this new range (1872 > 2127)
          canMsgSnd.data[1] = Time_month;
          canMsgSnd.data[2] = Time_day;
          canMsgSnd.data[3] = Time_hour;
          canMsgSnd.data[4] = Time_minute;
          canMsgSnd.data[5] = 0x3F;
          canMsgSnd.data[6] = 0xFE;
        }
        canMsgSnd.can_id = 0x276;
        canMsgSnd.can_dlc = 7;
        CAN1.sendMessage( & canMsgSnd);
        if (Send_CAN2010_ForgedMessages) {
          CAN0.sendMessage( & canMsgSnd);
        }

        if (!EngineRunning) {
          AirConditioningON = false;
          FanSpeed = 0x41;
          LeftTemp = 0x00;
          RightTemp = 0x00;
          FanPosition = 0x04;

          canMsgSnd.data[0] = 0x09;
          canMsgSnd.data[1] = 0x00;
          canMsgSnd.data[2] = 0x00;
          canMsgSnd.data[3] = LeftTemp;
          canMsgSnd.data[4] = RightTemp;
          canMsgSnd.data[5] = FanSpeed;
          canMsgSnd.data[6] = FanPosition;
          canMsgSnd.data[7] = 0x00;
          canMsgSnd.can_id = 0x350;
          canMsgSnd.can_dlc = 8;
          CAN1.sendMessage( & canMsgSnd);
          if (Send_CAN2010_ForgedMessages) {
            CAN0.sendMessage( & canMsgSnd);
          }
        }
      } else {
        CAN1.sendMessage( & canMsgRcv);
      }
    } else {
      CAN1.sendMessage( & canMsgRcv);
    }
  }

  // Forward messages from the CAN2010 device(s) to the car
  if (CAN1.readMessage( & canMsgRcv) == MCP2515::ERROR_OK) {
    int id = canMsgRcv.can_id;
    int len = canMsgRcv.can_dlc;

    if (debugCAN1) {
      Serial.print("FRAME:ID=");
      Serial.print(id);
      Serial.print(":LEN=");
      Serial.print(len);

      char tmp[3];
      for (int i = 0; i < len; i++) {
        Serial.print(":");

        snprintf(tmp, 3, "%02X", canMsgRcv.data[i]);

        Serial.print(tmp);
      }

      Serial.println();

      CAN0.sendMessage( & canMsgRcv);
    } else if (!debugCAN0) {
      if (id == 923 && len == 5) {
        Time_year = canMsgRcv.data[0] + 1872; // Year would not fit inside one byte (0 > 255), add 1872 and you get this new range (1872 > 2127)
        Time_month = canMsgRcv.data[1];
        Time_day = canMsgRcv.data[2];
        Time_hour = canMsgRcv.data[3];
        Time_minute = canMsgRcv.data[4];

        setTime(Time_hour, Time_minute, 0, Time_day, Time_month, Time_year);
        RTC.set(now()); // Set the time on the RTC module too
        EEPROM.update(5, Time_day);
        EEPROM.update(6, Time_month);
        EEPROM.put(7, Time_year);

        // Set hour on CAN-BUS Clock
        canMsgSnd.data[0] = hour();
        canMsgSnd.data[1] = minute();
        canMsgSnd.can_id = 0x228;
        canMsgSnd.can_dlc = 1;
        CAN0.sendMessage( & canMsgSnd);

        if (SerialEnabled) {
          Serial.print("Change Hour/Date: ");
          Serial.print(day());
          Serial.print("/");
          Serial.print(month());
          Serial.print("/");
          Serial.print(year());

          Serial.print(" ");

          Serial.print(hour());
          Serial.print(":");
          Serial.print(minute());

          Serial.println();
        }
      } else if (id == 347 && len == 8) {
        tmpVal = canMsgRcv.data[0];
        if (tmpVal >= 128) {
          languageAndUnitNum = tmpVal;
          EEPROM.update(0, languageAndUnitNum);

          if (SerialEnabled) {
            Serial.print("Telematic - Change Language and Unit (Number): ");
            Serial.print(tmpVal);
            Serial.println();
          }

          tmpVal = canMsgRcv.data[1];
          if (tmpVal >= 128) {
            mpgMi = true;
            EEPROM.update(4, 1);

            tmpVal = tmpVal - 128;
          } else {
            mpgMi = false;
            EEPROM.update(4, 0);
          }

          if (tmpVal >= 64) {
            TemperatureInF = true;
            EEPROM.update(3, 1);

            if (SerialEnabled) {
              Serial.print("Telematic - Change Temperature Type: Fahrenheit");
              Serial.println();
            }
          } else if (tmpVal >= 0) {
            TemperatureInF = false;
            EEPROM.update(3, 0);

            if (SerialEnabled) {
              Serial.print("Telematic - Change Temperature Type: Celcius");
              Serial.println();
            }
          }
        } else {
          tmpVal = ceil(tmpVal / 4.0);
          if (canMsgRcv.data[1] >= 128) {
            tmpVal--;
          }
          languageID = tmpVal;

          // CAN2004 Head-up panel is only one-way talking, we can't change the language on it from the CAN2010 Telematic :-(

          if (SerialEnabled) {
            Serial.print("Telematic - Change Language (ID): ");
            Serial.print(tmpVal);
            Serial.println();
          }
        }
      } else if (id == 489 && len >= 2 && CVM_Emul) { // Telematic suggested speed to fake CVM frame
        CAN0.sendMessage( & canMsgRcv);

        canMsgSnd.data[0] = canMsgRcv.data[1];
        canMsgSnd.data[1] = ((bitRead(canMsgRcv.data[3], 4) == 1 && bitRead(canMsgRcv.data[3], 3) == 1 && vehicleSpeed > (canMsgRcv.data[0] + speedMargin)) ? 0x30 : 0x10); // POI Over-speed, make speed limit blink
        canMsgSnd.data[2] = 0x00;
        canMsgSnd.data[3] = 0x00;
        canMsgSnd.data[4] = 0x7C;
        canMsgSnd.data[5] = 0xF8;
        canMsgSnd.data[6] = 0x00;
        canMsgSnd.data[7] = 0x00;
        canMsgSnd.can_id = 0x268; // CVM Frame ID
        canMsgSnd.can_dlc = 8;
        CAN1.sendMessage( & canMsgSnd);
      } else if (id == 485 && len == 7) {
        // Ambience mapping
        tmpVal = canMsgRcv.data[5];
        if (tmpVal == 0x00) { // User
          canMsgRcv.data[6] = 0x40;
        } else if (tmpVal == 0x08) { // Classical
          canMsgRcv.data[6] = 0x44;
        } else if (tmpVal == 0x10) { // Jazz
          canMsgRcv.data[6] = 0x48;
        } else if (tmpVal == 0x18) { // Pop-Rock
          canMsgRcv.data[6] = 0x4C;
        } else if (tmpVal == 0x28) { // Techno
          canMsgRcv.data[6] = 0x54;
        } else if (tmpVal == 0x20) { // Vocal
          canMsgRcv.data[6] = 0x50;
        } else { // Default : User
          canMsgRcv.data[6] = 0x40;
        }

        // Loudness / Volume linked to speed
        tmpVal = canMsgRcv.data[4];
        if (tmpVal == 0x10) { // Loudness / not linked to speed
          canMsgRcv.data[5] = 0x40;
        } else if (tmpVal == 0x14) { // Loudness / Volume linked to speed
          canMsgRcv.data[5] = 0x47;
        } else if (tmpVal == 0x04) { // No Loudness / Volume linked to speed
          canMsgRcv.data[5] = 0x07;
        } else if (tmpVal == 0x00) { // No Loudness / not linked to speed
          canMsgRcv.data[5] = 0x00;
        } else { // Default : No Loudness / not linked to speed
          canMsgRcv.data[5] = 0x00;
        }

        // Bass
        // CAN2004 Telematic Range: (-9) "54" > (-7) "57" > ... > "72" (+9) ("63" = 0)
        // CAN2010 Telematic Range: "32" > "88" ("60" = 0)
        tmpVal = canMsgRcv.data[2];
        canMsgRcv.data[2] = ((tmpVal - 32) / 4) + 57; // Converted value

        // Treble
        // CAN2004 Telematic Range: (-9) "54" > (-7) "57" > ... > "72" (+9) ("63" = 0)
        // CAN2010 Telematic Range: "32" > "88" ("60" = 0)
        tmpVal = canMsgRcv.data[3];
        canMsgRcv.data[4] = ((tmpVal - 32) / 4) + 57; // Converted value on position 4 (while it's on 3 on a old amplifier)

        // Balance - Left / Right
        // CAN2004 Telematic Range: (-9) "54" > (-7) "57" > ... > "72" (+9) ("63" = 0)
        // CAN2010 Telematic Range: "32" > "88" ("60" = 0)
        tmpVal = canMsgRcv.data[1];
        canMsgRcv.data[1] = ((tmpVal - 32) / 4) + 57; // Converted value

        // Balance - Front / Back
        // CAN2004 Telematic Range: (-9) "54" > (-7) "57" > ... > "72" (+9) ("63" = 0)
        // CAN2010 Telematic Range: "32" > "88" ("60" = 0)
        tmpVal = canMsgRcv.data[0];
        canMsgRcv.data[0] = ((tmpVal - 32) / 4) + 57; // Converted value

        // Mediums ?
        canMsgRcv.data[3] = 63; // 0x3F = 63

        CAN0.sendMessage( & canMsgRcv);
      } else {
        CAN0.sendMessage( & canMsgRcv);
      }
    } else {
      CAN0.sendMessage( & canMsgRcv);
    }

  }
}

int daysSinceYearStartFct() {
  // Given a day, month, and year (4 digit), returns 
  // the day of year. Errors return 999.
  int daysInMonth[] = {31,28,31,30,31,30,31,31,30,31,30,31};
  // Check if it is a leap year, this is confusing business
  // See: https://support.microsoft.com/en-us/kb/214019
  if (year()%4  == 0) {
    if (year()%100 != 0) {
      daysInMonth[1] = 29;
    }
    else {
      if (year()%400 == 0) {
        daysInMonth[1] = 29;
      }
    }
   }
  
  int doy = 0;
  for (int i = 0; i < month() - 1; i++) {
    doy += daysInMonth[i];
  }
  
  doy += day();
  return doy;
}