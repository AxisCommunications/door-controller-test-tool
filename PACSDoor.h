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

#ifndef PACSDOOR_H_
#define PACSDOOR_H_

#include <Arduino.h>
#include <StandardCplusplus.h>
#include <vector>
#include <serstream>

#include "PACSReader.h"
#include "PACSPeripheral.h"

#define DOOR_ID_MAX_LENGTH 16 // The max number of characters for the ID.

using namespace std;

class PACSDoor {

    public:
        PACSDoor(char*);

        void addReader(char*, uint8_t, uint8_t);
        void addPeripheral(char*, PACSPeripheralType_t, uint8_t, uint8_t);                
        PACSPeripheral* findPeripheral(char*, PACSPeripheralType_t);        
        PACSPeripheral* findPeripheralById(char*);        
        PACSReader* findReaderById(char*);                
        void initialize(); // Initialize all readers/peripherals.
        void updateLevels(); // Update the current pin levels of all peripherals.                
        
        // Commands
        bool swipeCard(char*, unsigned long, unsigned long);
        bool enterPIN(char*, char*);
        bool openDoor(char*);
        bool closeDoor(char*);
        bool pushREX(char*);    
        bool activateInput(char*);
        bool deactivateInput(char*);
        
        // Callback called when pin state changes.
        typedef void StateChangeCallback(PACSDoor&, PACSPeripheral&);
        void registerStateChangeCallback(StateChangeCallback*);        

        char id[DOOR_ID_MAX_LENGTH + 1]; // Door id, to match commands against.
    
        // Our vectors of readers and peripherals.
        std::vector<PACSReader> readers;
        std::vector<PACSPeripheral> peripherals;

    private:                
        void setPinActive(uint8_t, uint8_t);
        void setPinInactive(uint8_t, uint8_t);
        void initPins();        
        void sendPIN(char*, uint8_t, uint8_t);
        void assembleWiegandData(unsigned long, unsigned long, uint8_t, uint8_t);
        void transmitWiegandData(unsigned long, int, int, int);

        // Pointer to the callback functions provided.
        StateChangeCallback *onStateChangeCallback;

    };

#endif
