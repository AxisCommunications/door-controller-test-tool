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

#include "PACSReader.h"

/* 
* Constructors
*/
PACSReader::PACSReader() {}
PACSReader::PACSReader(char* rId, uint8_t rPin0, uint8_t rPin1) {
    strcpy(id, rId);
    pin0 = rPin0;
    pin1 = rPin1; 
}

/* 
* Initializes the reader.
*/
void PACSReader::initialize() {
    // Wiegand pins should be outputs and default HIGH. We ignore the 
    // active state for the reader pins, as they should always be high.    
    pinMode(pin0, OUTPUT);    
    pinMode(pin1, OUTPUT);   
    digitalWrite(pin0, HIGH); 
    digitalWrite(pin1, HIGH);    
}