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

#include "PACSDoorManager.h"
#include <serstream>

using namespace std;

/*
* Constructor. 
*/
PACSDoorManager::PACSDoorManager() {}

/*
* Creates a new door and pushes it to the doors vector.
*/
PACSDoor* PACSDoorManager::createDoor(char* doorId) {
    
    doors.push_back(PACSDoor(doorId));
    return findDoorById(doorId);
}

/*
* Changes the id of a door.
*/
void PACSDoorManager::setDoorId(char* oldId, char* newId) {
    PACSDoor* d = findDoorById(oldId);
    if (d != NULL) {
        strcpy(d->id, newId);
    }
}

/*
* Initializes all the doors in the doors vector.
*/
void PACSDoorManager::initializeDoors() {
    for (unsigned i=0; i < doors.size(); i++) {
        doors[i].initialize();
    }    
}

/*
* Swipes a standard 26bit Wiegand card at the specified door and
*/
bool PACSDoorManager::swipeCard(char* doorId, char* readerId, unsigned long facilityCode, unsigned long cardNumber) {  
    
    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        if (d->swipeCard(readerId, facilityCode, cardNumber)) {
            cout << "[" << doorId << "|" << readerId << "]"<< F(": Card swiped. Facility code: ") 
                 << facilityCode << F(". Card number: ") << cardNumber << endl;        
             return true;
        }
        else {
            cout << "Reader not found: " << readerId << endl;
            return false;
        }
    }
    cout << "Door not found: " << doorId << endl;
    return false;
}

/*
* Enters a pin digit/sequence at the specified door and reader.
*/
bool PACSDoorManager::enterPIN(char* doorId, char* readerId, char* code) {
        
    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        if (d->enterPIN(readerId, code)) {
            cout << "[" << doorId << "|" << readerId << "]" << F(": Entered PIN digit(s): ") 
                 << code << endl;        
            return true;
        }
        else {
            cout << "Reader not found: " << readerId << endl;
            return false;
        }
    }
    cout << "Door not found: " << doorId << endl;
    return false;    
}

/*
* Opens the specified door monitor att the specified door.
*/
bool PACSDoorManager::openDoor(char* doorId, char* doorMonitorId) {        

    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        if (d->openDoor(doorMonitorId)) {
            cout << "[" << doorId << "|" << doorMonitorId << "]" << F(": Door opened.") << endl;        
            return true;
        }
        else {
            cout << "Peripheral not found: " << doorMonitorId << endl;
            return false;
        }        
    }
    cout << "Door not found: " << doorId << endl;
    return false;    
}

/*
* Closes the specified door monitor att the specified door.
*/
bool PACSDoorManager::closeDoor(char* doorId, char* doorMonitorId) {
    
    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        if (d->closeDoor(doorMonitorId)) {
            cout << "[" << doorId << "|" << doorMonitorId << "]" << F(": Door closed.") << endl;        
            return true;
        }
        else {
            cout << "Peripheral not found: " << doorMonitorId << endl;
            return false;
        }                
    }
    cout << "Door not found: " << doorId << endl;
    return false;    
}

/*
* Pushes the specified REX device att the specified door.
*/
bool PACSDoorManager::pushREX(char* doorId, char* rexId) {

    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        if (d->pushREX(rexId)) {
            cout << "[" << doorId << "|" << rexId << "]" << F(": REX pushed.") << endl;        
            return true;
        }
        else {
            cout << "Peripheral not found: " << rexId << endl;
            return false;
        }                
    }        
    cout << "Door not found: " << doorId << endl;
    return false;    
}


/*
* 
*/
bool PACSDoorManager::activateInput(char* doorId, char* inputId) {        

    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        if (d->activateInput(inputId)) {
            cout << "[" << doorId << "|" << inputId << "]" << F(": Input activated.") << endl;        
            return true;
        }
        else {
            cout << "Peripheral not found: " << inputId << endl;
            return false;
        }        
    }
    cout << "Door not found: " << doorId << endl;
    return false;    
}

/*
* 
*/
bool PACSDoorManager::deactivateInput(char* doorId, char* inputId) {
    
    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        if (d->deactivateInput(inputId)) {
            cout << "[" << doorId << "|" << inputId << "]" << F(": Input deactivated.") << endl;        
            return true;
        }
        else {
            cout << "Peripheral not found: " << inputId << endl;
            return false;
        }                
    }
    cout << "Door not found: " << doorId << endl;
    return false;    
}

/*
* Calls the updateLevels function for all the doors in the doors vector.
* This will read the current levels of all the peripherals connected to the Arduino, 
* and if they have changed, the registered callback will be called.
*/
void PACSDoorManager::updateLevels() {  
    for (unsigned i=0; i < doors.size(); i++) {
        doors[i].updateLevels();        
    }
}

/*
* Check if the specified peripheral is active (depends on the current state and
* the peripherals configured ActiveLevel in the doors.cfg file).
*/
int PACSDoorManager::isPeripheralActive(char* doorId, char* peripheralId) {
    PACSPeripheral* p = findPeripheralById(doorId, peripheralId);
    if (p != NULL) {
        return (p->isActive() ? 1 : 0);
    }    
    return -1;        
}

/*
* For the specified id, we search the doors vector and if we find a match,
* return a pointer to the door (and NULL if no match is found.)
*/
PACSDoor* PACSDoorManager::findDoorById(char* doorId) {
    for (unsigned i=0; i < doors.size(); i++) {
        if ((strcmp(doors[i].id, doorId) == 0))
            return &doors[i];
    }
    return NULL;
}

/*
* For the specified door id and reader id, we search the doors vector and its 
* associated readers and if we find a match, return a pointer to the reader
* (and NULL if no match is found.)
*/
PACSReader* PACSDoorManager::findReaderById(char* doorId, char* readerId) {
    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        return d->findReaderById(readerId);
    } 
    return NULL;
}

/*
* For the specified door id and peripheral id, we search the doors vector and its 
* associated peripherals and if we find a match, return a pointer to the peripheral
* (and NULL if no match is found.)
*/
PACSPeripheral* PACSDoorManager::findPeripheralById(char* doorId, char* peripheralId) {
    PACSDoor* d = findDoorById(doorId);
    if (d != NULL) {
        return d->findPeripheralById(peripheralId);
    } 
    return NULL;
}

/*
* Registers a function to be called when a pin changes state.
*/
void PACSDoorManager::registerStateChangeCallback(StateChangeCallback *callback) {
    for (unsigned i=0; i < doors.size(); i++) {
        doors[i].registerStateChangeCallback(callback);
    }      
}
