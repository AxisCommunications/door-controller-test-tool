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

#ifndef PACSREADER_H_
#define PACSREADER_H_

#include <Arduino.h>

#define READER_ID_MAX_LENGTH 16 // The max number of characters for the ID.

class PACSReader {    
    public:
        PACSReader();
        PACSReader(char*, uint8_t, uint8_t);
        
        void initialize(); // Initialize the reader.

        char id[READER_ID_MAX_LENGTH + 1]; // Id of the reader
        uint8_t pin0; // Wiegand data0 hardware-pin.
        uint8_t pin1; // Wiegand data1 hardware-pin.       
};

#endif