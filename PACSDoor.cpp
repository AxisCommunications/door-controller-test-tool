/*
Copyright (C) 2014 Axis Communications

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "PACSDoor.h"
#include "PACSReader.h"
#include "PACSPeripheral.h"
/*
* Constructor. 
*/
PACSDoor::PACSDoor(char* doorId)
{
    strcpy(id, doorId);
    onStateChangeCallback = NULL;
}

/*
* Adds a new reader to the door.
*/
void PACSDoor::addReader(char* id, uint8_t pin0, uint8_t pin1) {
    
    readers.push_back(PACSReader(id, pin0, pin1));
}

/*
* Adds a new peripheral to the door.
*/
void PACSDoor::addPeripheral(char* id, PACSPeripheralType_t type, uint8_t pin, uint8_t activeLevel) {

    peripherals.push_back(PACSPeripheral(id, type, pin, activeLevel));
}

/*
* Finds and returns the peripheral with the specified id and type. 
* Returns NULL if none found.
*/
PACSPeripheral* PACSDoor::findPeripheral(char* someId, PACSPeripheralType_t someType) {
    
    for (unsigned i=0; i < peripherals.size(); i++) {
        if ((strcmp(peripherals[i].id, someId) == 0) && (peripherals[i].type == someType))
            return &peripherals[i];
    }
    return NULL;
}

/*
* Finds and returns the peripheral with the specified id.
* Returns NULL if none found.
*/
PACSPeripheral* PACSDoor::findPeripheralById(char* someId) {
    for (unsigned i=0; i < peripherals.size(); i++) {
        if ((strcmp(peripherals[i].id, someId) == 0))
            return &peripherals[i];
    }
    return NULL;
}

/*
* Finds and returns the reader with the specified id.
* Returns NULL if none found.
*/
PACSReader* PACSDoor::findReaderById(char* someId) {
    for (unsigned i=0; i < readers.size(); i++) {
        if ((strcmp(readers[i].id, someId) == 0))
            return &readers[i];
    }
    return NULL;
}

/*
* Initalizes all the peripherals and readers associated with the door.
*/
void PACSDoor::initialize() {
    // Initialize all peripherals and readers
    for (unsigned i=0; i < peripherals.size(); i++) {
      peripherals[i].initialize();
    }
    for (unsigned i=0; i < readers.size(); i++) {
      readers[i].initialize();
    } 
}

/*
* Swipe a 26bit Wiegand card at the specified reader. 
* Facility code can have a value of 0-255, and the card number 0-65535.
*/
bool PACSDoor::swipeCard(char* readerId, unsigned long facilityCode, unsigned long cardNumber) {  
    
  PACSReader* r = findReaderById(readerId);  
  
  if (r != NULL) {
    assembleWiegandData(facilityCode, cardNumber, r->pin0, r->pin1);        
    return true;
  } 
  return false;
}

/*
* Enter a pin number at the specified reader.
*/
bool PACSDoor::enterPIN(char* readerId, char* code) {

    PACSReader* r = findReaderById(readerId);

    if (r != NULL) {
        sendPIN(code, r->pin0, r->pin1);       
        return true;
    }
    return false;
}

/*
* Open the specified door monitor.
*/
bool PACSDoor::openDoor(char* doorMonitorId) {        

    PACSPeripheral* p = findPeripheralById(doorMonitorId);
    if ((p != NULL) && (p->type == DOORMONITOR)) {
        setPinActive(p->pin, p->activeLevel);       
        return true;        
    }
    return false;    
}

/*
* Close the specified door monitor.
*/
bool PACSDoor::closeDoor(char* doorMonitorId) {
    
    PACSPeripheral* p = findPeripheralById(doorMonitorId);
    if ((p != NULL) && (p->type == DOORMONITOR)) {
        setPinInactive(p->pin, p->activeLevel);       
        return true;        
    }
    return false;    
}

/*
* Pushes the specified REX button.
*/
bool PACSDoor::pushREX(char* rexId) {

    PACSPeripheral* p = findPeripheralById(rexId);
    if ((p != NULL) && (p->type == REX)) {
        setPinActive(p->pin, p->activeLevel);      
        delay(10);
        setPinInactive(p->pin, p->activeLevel);        
        return true;        
    }
    return false;    
}

/*
*
*/
bool PACSDoor::activateInput(char* inputId) {        

    PACSPeripheral* p = findPeripheralById(inputId);
    if ((p != NULL) && (p->type == DIGITAL_INPUT)) {
        setPinActive(p->pin, p->activeLevel);       
        return true;        
    }
    return false;    
}

/*
*
*/
bool PACSDoor::deactivateInput(char* inputId) {
    
    PACSPeripheral* p = findPeripheralById(inputId);
    if ((p != NULL) && (p->type == DIGITAL_INPUT)) {
        setPinInactive(p->pin, p->activeLevel);       
        return true;        
    }
    return false;    
}

/*
* Check if there has been any change in pin-states and if so, call the registered callback.
*/
void PACSDoor::updateLevels() {  

    // Update the levels for all registered peripherals and if there has 
    // been a change  in the different pin states, call the registered callback.
    for (unsigned i=0; i < peripherals.size(); i++) {        
        peripherals[i].updateLevels();        
        if (onStateChangeCallback) {
            if (peripherals[i].levelChanged) {
                onStateChangeCallback(*this, peripherals[i]);
            }                
        }    
    }
}

/*
* Registers a function to be called when a pin changes state.
*/
void PACSDoor::registerStateChangeCallback(StateChangeCallback *callback) {
    onStateChangeCallback = callback;
}
/*
* Set the pin to active state.
*/
void PACSDoor::setPinActive(uint8_t pin, uint8_t activeLevel) {    
    digitalWrite(pin, (activeLevel == HIGH) ? HIGH : LOW);
}
/*
* Set the pin to inactive state.
*/
void PACSDoor::setPinInactive(uint8_t pin, uint8_t activeLevel) {
    digitalWrite(pin, (activeLevel == HIGH) ? LOW : HIGH);
}

/*
* Calculate the wiegand binary to be sent.
*/
void PACSDoor::assembleWiegandData(unsigned long facilityCode, unsigned long cardNumber, uint8_t pin0, uint8_t pin1)
{
  // Wiegand 26bit format:
  //
  //        Fac.Code     CardNo
  //        |------||--------------|
  //       P000000000000000000000000P
  //       |                        |
  //       |                        |
  // Even parity bit         Odd parity bit
  //  (for the 12 bits         (for the 12 bits
  //  (to the right)            to the left)
  //
  
  // Mask out the 2 byte fac.code value and shift it to the left, 
  // making room for the cardnumber.
  facilityCode = (facilityCode & 0xFF) << 17; 
  // Mask out the 4 byte card value and shift it to the left,
  // making room for the odd parity bit.
  cardNumber = (cardNumber & 0xFFFF) << 1;
  // Combine the two.
  unsigned long wiegandData = facilityCode | cardNumber;
  //Mask the low part and count 1:s.
   // 0001 1111 1111 1110
  unsigned int low = wiegandData & 8190;

  boolean parity = false;
  while (low) {
    parity = !parity;
    low = low & (low - 1);
  }

  if (!parity) {
    bitSet(wiegandData, 0);
  }
  else {
    bitClear(wiegandData, 0);
  }

  //Mask the high part and count 1s:
  unsigned long high = wiegandData & 33546240; // 0000 0001 1111 1111 1110 0000 0000 0000
  parity = false;
  while (high) {
    parity = !parity;
    high = high & (high - 1);
  }

  if (parity) {
    bitSet(wiegandData, 25);
  } else {
    bitClear(wiegandData, 25);
  }
  
  transmitWiegandData(wiegandData, 26, pin0, pin1);
}

/*
* Do the actual encoding of the keypad presses, and send it for output.
*/
void PACSDoor::sendPIN(char* keySequence, uint8_t pin0, uint8_t pin1)
{
  // Keys are sent as 4bit values. To convert a character to it's
  // correct decimal value, we subtract 48, which is the ASCII value
  // of '0'. The asterisk is given the value 10, and the hash 11.
  int i = 0;  
  while(keySequence[i] != '\0') {    
    byte key = 255;
    if (isDigit(keySequence[i])) {
        key = (keySequence[i] - 0x30); 
    } else if (keySequence[i] == '*') {
        key = 0xA;
    } else if (keySequence[i] == '#') {
        key = 0xB;
    }    
    // Now output the key (if it's a valid key, i.e. a value between
    // 0 and 11. We are only sending 4 bits of information, so we mask 
    // these out first.
    if (key <= 11) {
      transmitWiegandData(key, 4, pin0, pin1);
      delay(50);
    }
    i++;        
  }
}

/*
* This is where the actual physical transmission  of the Wiegand data 
* takes place.
 */
void PACSDoor::transmitWiegandData(unsigned long data, int length, int pin0, int pin1)
{
  int i = length;
  int output_pin;

  noInterrupts();
  while (i > 0) {
    i--;           
    output_pin = (bitRead(data, i)) ? pin1 : pin0;
    digitalWrite(output_pin, LOW);
    delayMicroseconds(50);
    digitalWrite(output_pin, HIGH);    
    delayMicroseconds(950);
  }
  interrupts();
}
