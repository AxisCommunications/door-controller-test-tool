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
    
    int initialLevel = ((activeLevel == HIGH) ? LOW : HIGH);
    currentLevel = previousLevel = initialLevel;
    levelChanged = false;

    switch (type) {
        case DOORMONITOR:
        case REX:
        case DIGITAL_INPUT:
        case DIGITAL_OUTPUT:
            pinMode(pin, OUTPUT);
            digitalWrite(pin, initialLevel);    
            break;                        

        case GREENLED:
        case BEEPER:
        case LOCK:
            pinMode(pin, INPUT);
            break;
        
        default:
            break;
    }
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
        case LOCK:   
        case DIGITAL_INPUT:
        case DIGITAL_OUTPUT:
            currentLevel = digitalRead(pin);
            break;

        default:
            break;
    }

    if (currentLevel != previousLevel) {
        levelChanged = true;
    }
    else {
        levelChanged = false;
    }

    previousLevel = currentLevel;
}

/* 
* Checks if the peripheral is currently active or not.
*/
bool PACSPeripheral::isActive() {
    return (currentLevel == activeLevel) ? true : false;
}
