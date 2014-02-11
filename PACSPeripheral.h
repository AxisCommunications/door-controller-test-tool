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

#ifndef PACSPERIPHERAL_H_
#define PACSPERIPHERAL_H_

#include <Arduino.h>

#define VOLTAGE_INPUT 5.0f // What is the input voltage from lock. (Should be max 5V, otherwise we damage the hardware)
#define VOLTAGE_THRESHOLD_HIGH 4.5f // At what value we regard the lock as being locked/unlocked.
#define VOLTAGE_THRESHOLD_LOW 0.9f // At what value we regard the lock as being locked/unlocked.

#define PERIPHERAL_ID_MAX_LENGTH 16 // The max number of characters for the ID.

typedef enum {GREENLED, BEEPER, DOORMONITOR, REX, LOCK} PACSPeripheralType_t;

class PACSPeripheral {    
    public:
        PACSPeripheral();
        PACSPeripheral(char*, PACSPeripheralType_t, uint8_t, uint8_t);
        
        void initialize(); // Initialize the peripheral. Set pin to input/output and to default level.       
        void updateLevels(); // Update the current pin levels.                
        bool isActive();  // Check if peripheral is in active state.
        
        char id[PERIPHERAL_ID_MAX_LENGTH + 1]; // Id of the peripheral.        
        PACSPeripheralType_t type; // Peripheral type. LED, Beeper, REX, etc.        
        uint8_t pin; // Associated hardware-pin.                
        uint8_t activeLevel; // The level (HIGH/LOW) which is considered "active".
        uint8_t currentLevel; // Current pin level.
        uint8_t previousLevel; // The pin level of the last update.
        bool levelChanged; // Has the pin level changes since last update?
};

#endif