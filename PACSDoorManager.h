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

#ifndef PACSDOORMANAGER_H_
#define PACSDOORMANAGER_H_

#include "PACSDoor.h"
#include <StandardCplusplus.h>
#include <vector>
#include <serstream>

class PACSDoorManager {
    public:
        PACSDoorManager();

        PACSDoor* createDoor(char*);
        void setDoorId(char* oldId, char* newId);
        void initializeDoors();

        // Door actions
        bool swipeCard(char*, char*, unsigned long, unsigned long);
        bool enterPIN(char*, char*, char*);
        bool openDoor(char*, char*);
        bool closeDoor(char*, char*);
        bool pushREX(char*, char*);   
        bool activateInput(char*, char*);
        bool deactivateInput(char*, char*);
        
        void updateLevels();        
        int isPeripheralActive(char*, char*);

        // Callback for peripheral state changes.
        typedef void StateChangeCallback(PACSDoor&, PACSPeripheral&);
        void registerStateChangeCallback(StateChangeCallback*);                

        // A vector to hold all our doors.
        std::vector<PACSDoor> doors;
    
    private:        
        PACSDoor* findDoorById(char*);
        PACSReader* findReaderById(char*, char*);
        PACSPeripheral* findPeripheralById(char*, char*);              
    };

#endif
