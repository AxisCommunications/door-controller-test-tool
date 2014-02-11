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

#include "PACSPeripheral.h"

/*
* Constructors. 
*/
PACSPeripheral::PACSPeripheral() {}
PACSPeripheral::PACSPeripheral(char* pId, PACSPeripheralType_t pType, uint8_t pPin, 
                               uint8_t pActiveLevel) {
    strcpy(id, pId);
    pin = pPin;
    type = pType;
    activeLevel = pActiveLevel;

}

/*
* Initializes the peripheral.
*/
void PACSPeripheral::initialize() {    
   /* Door and REX pins should be outputs.
    * The reader feedback pins should be inputs.
    * The lock pin is an input. 
    *
    * We set them to inactive to avoid any issues if the pin 
    * is not actually connected to anything.    
    */    
    switch (type) {
        case DOORMONITOR:
        case REX:
            pinMode(pin, OUTPUT);
            break;
        
        case GREENLED:
        case BEEPER:
            pinMode(pin, INPUT);
            break;

        case LOCK:
            pinMode(pin, INPUT);    
            break;
        
        default:
            break;
    }
    int initialLevel = ((activeLevel == HIGH) ? LOW : HIGH);
    digitalWrite(pin, initialLevel);    
    currentLevel = previousLevel = initialLevel;
    levelChanged = false;
}

/*
* Checks the current level of the peripheral against its previous value,
* and determines if a state change has occured.
*/
void PACSPeripheral::updateLevels() {
    float voltage;

    switch (type) {
        case DOORMONITOR:
        case REX:
        case GREENLED:
        case BEEPER:
            currentLevel = digitalRead(pin);
            break;
        case LOCK:                                
            voltage = (analogRead(pin) * (VOLTAGE_INPUT / 1023.0));            
            if (voltage >= VOLTAGE_THRESHOLD_HIGH) {
                currentLevel = HIGH;
            }
            else if (voltage < VOLTAGE_THRESHOLD_LOW) {
                currentLevel = LOW;
            }
            break;
        
        default:
            break;
    }

    if (currentLevel != previousLevel)
        levelChanged = true;
    else 
        levelChanged = false;

    previousLevel = currentLevel;
}

/* 
* Checks if the peripheral is currently active or not.
*/
bool PACSPeripheral::isActive() {
    return (currentLevel == activeLevel) ? true : false;
}