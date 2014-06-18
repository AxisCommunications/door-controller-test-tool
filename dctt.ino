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

// Serial baudrate of the Arduino.
#define SERIAL_BAUD 57600

// Websocket heartbeats.
#define HEARTBEAT_INTERVAL 5
#define HEARTBEAT_TIMEOUT 15

// Webserver fail message.
#define WEBDUINO_FAIL_MESSAGE ""

// Buffer for sending files to client.
#define FILE_TX_BUFFER_SIZE 64

// Reserved pins.
#define ETHERNET_SELECT_PIN 10
#define SD_CARD_SELECT_PIN 4
#define SS_HARDWARE_PIN 53
#define RESET_PIN 40

// Favicon
#define WEBDUINO_FAVICON_DATA ""

// Toggle bonjour/zeroconf functionality.
#undef BONJOUR_ENABLED

#include "SPI.h"
#include "avr/pgmspace.h"
#include "Ethernet.h"
#include "WebSocket.h"
#include "WebServer.h"

#include <SD.h>

#include "aJSON.h"
#include "SimpleTimer.h"

// STL stuff
#include <StandardCplusplus.h>
#include <vector>
#include <serstream>

// Door-specific files.
#include "PACSDoor.h"
#include "PACSReader.h"
#include "PACSPeripheral.h"
#include "PACSDoorManager.h"

// For freemem.
#include "System.h"

#ifdef BONJOUR_ENABLED
  #include "EthernetBonjour.h"
#endif

using namespace std;

// Cout pipes to serial.
namespace std {
  ohserialstream cout(Serial);
}

System sys;
WebServer* webserver;
WebSocket websocketServer;
PACSDoorManager doorManager;

// Pin mappings. Index is pin number.
char digitalPins[54][4];
char analogPins[16][4];

// Webserver filenames.
char* indexFilename = "index.htm";

// Configuration filenames.
const char* networkConfigFilename = "config/network.cfg";
const char* pinsConfigFilename = "config/pins.cfg";
const char* doorsConfigFilename = "config/doors.cfg";

// Network configuration structure.
struct config_t
{
  bool use_dhcp;
  uint8_t dhcp_refresh_minutes;
  uint8_t mac[6];
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;
  IPAddress dns;
  int httpPort;
  int websocketPort;
  } network_config;

unsigned long last_dhcp_renew;
int last_free_ram = 0;

// API commands
typedef enum Command
{
    SWIPECARD,
    ENTERPIN,
    OPENDOOR,
    CLOSEDOOR,
    PUSHREX,
    ACTIVATEINPUT,
    DEACTIVATEINPUT,
    GETPERIPHERALSTATE,
    GETNETWORKCONFIG,
    GETDOORCONFIG,
    GETPINCONFIG,
    SETCONFIG,
    UNDEFINED,
};

/*
* Mounts the SD card.
*/
void setupSDCard() {
  
  // Disable ethernet shield SPI while setting up SD
  pinMode(ETHERNET_SELECT_PIN, OUTPUT);
  digitalWrite(ETHERNET_SELECT_PIN, HIGH);
  cout << F("Mounting SD Card...");
  if(!SD.begin(SD_CARD_SELECT_PIN)) {
    cout << F(" failed.\n");
    while (true) delay(100); // Don't continue if we fail.
  }  
  else { 
    cout << F(" OK.\n");
  } // SD.begin() returns with its SPI disabled, so no need to do it ourselves.
}

/*
* Help function to print out free mem (to check for mem-leaks).
*/
void freeMem() {
  int freeRam = sys.ramFree();
  if (last_free_ram != freeRam) {
    cout << F("Free RAM: ") << freeRam << F(" bytes (") 
         << sys.ramSize() << F(" total).") << endl;
    last_free_ram = freeRam;
  }
}


/*
* Prints network configuration to serial port.
*/
void printNetworkConfiguration() {
  
  // MAC address takes a bit of work to format for output.
  char buff[3];
  cout << F("MAC: ");
  for(int i=0; i<6; i++) {
    sprintf(buff, "%02X", network_config.mac[i]);
    cout << buff;
    if (i != 5) cout << ":";
  }
  cout << endl;

  cout << F("DHCP ");
  cout << (network_config.use_dhcp ? F("enabled.") : F("disabled."));

  cout << F("\nIP: ");
  Ethernet.localIP().printTo(Serial);
  
  cout << F("\nSubnet Mask: ");
  Ethernet.subnetMask().printTo(Serial);
  
  cout << F("\nGateway: ");
  Ethernet.gatewayIP().printTo(Serial);
  
  cout << F("\nDNS Server: ");
  Ethernet.dnsServerIP().printTo(Serial);

  cout << F("\nHTTP Port: ") << (int)network_config.httpPort;
  cout << F("\nWebsocket Port: ") << (int)network_config.websocketPort;

  cout << F("\n");
}

/*
* Sends a file on the SD card to the client.
*/
void sendFile(WebServer &server, const char* type, const char* filename)
{
  
  byte txBuffer[FILE_TX_BUFFER_SIZE];
  int bytesRead = 0;
  P(could_not_open_file) = "Could not open file: ";

  File fileStream = SD.open(filename);
  if (!fileStream) {
    server.httpFail();
    server.printP(could_not_open_file);
    server.print(filename);
    return;
  }
  // Opening of file was successful, so send correct content 
  // type and start sending the file in "chunks".
  server.httpSuccess(type);
  while (fileStream.available()) 
  {
    txBuffer[bytesRead] = fileStream.read();
    bytesRead++;

    if(bytesRead == FILE_TX_BUFFER_SIZE)
    {
      server.write(txBuffer, FILE_TX_BUFFER_SIZE);
      bytesRead = 0;
    }                
  }    
  if(bytesRead > 0) {
    server.write(txBuffer, bytesRead); 
  }    
  server.printCRLF();        
  fileStream.close();  
}

/* *****************************************************************************************************
* 
* Configuration Section
* 
***************************************************************************************************** */

/*
* Used by the functions that load the pin and door configuration. Waits for
* data to appear in the stream or timeout to occur, then returns.
*/
void waitForData(Stream& stream, int timeout) {
  unsigned long i = millis() + timeout;
  while ((!stream.available()) && (millis() < i)) /* spin with a timeout*/;
}

/*
* Find the next quotation-mark, and read all bytes until quotation-mark 
* after that.
*/   
bool getNextToken(Stream& stream, char* tokenBuffer, uint8_t length) {

  stream.find("\"");
  uint8_t bytesRead = stream.readBytesUntil('"', tokenBuffer, length);
  tokenBuffer[bytesRead] = '\0';

  if (bytesRead > 0)
    return true;
  else
    return false;
}

// An enum to keep track of what we are parsing, when loading 
// the door configuration file.
// We put the enum in its oown namespace to prevent poluting
// the global one.
namespace Cfg {
  enum Pos {
            NONE, DOOR, READER, DOOR_MONITOR, REX, LOCK, DIGITAL_INPUT, DIGITAL_OUTPUT, //Container
            WIEGAND, GREEN_LED, BEEPER, //Subcontainer
            ID, PIN, PIN_ZERO, PIN_ONE, ACTIVE //Property
            };
  enum PinType {DIGITAL, ANALOG};
};

/*
* This function is used to load the pin mappings from the configuration-
* file on the SD card. Each pin has a 3 character long id, which is stored 
* in an array. This array is needed for lookup as the door configuration 
* file references these id:s, instead of the actual pin numbers.
*
* A custom parser is needed as there is not enough memory to load the entire 
* config file into memory.
*/
bool loadPinMappingsFromFile(const char* filename) {

  Cfg::PinType cfgPinType = Cfg::DIGITAL;
  const uint8_t tokenBufferLength = 4;
  char tokenBuffer[tokenBufferLength] = "";  
  bool parsingSucceeded = false;
  int i = 0;    
  uint8_t pin;

  // Open the file.
  File fileStream = SD.open(filename);    
  if (!fileStream) {
    cout << F("Error opening ") << filename << endl;
    return false;
  }  
  
  while (true) {             

    if (cfgPinType == Cfg::DIGITAL) {
      // Get the digital pin number.
      getNextToken(fileStream, tokenBuffer, tokenBufferLength); 
      pin = (uint8_t)atoi(tokenBuffer);

      // Make sure that the parsed pin is "correct", i.e. in numerical order.
      if (pin != i) {
        cout << F("Expected pin number ") << (int)i << ", got " << (int)pin << endl;
        break;
      }
      // Get the pin id and save to array.
      getNextToken(fileStream, tokenBuffer, tokenBufferLength);            
      strcpy(digitalPins[i], tokenBuffer);      
      digitalPins[i][3] = '\0';
    }   
    else if (cfgPinType == Cfg::ANALOG) { 
      // Get the analog pin number.
      getNextToken(fileStream, tokenBuffer, tokenBufferLength); 

      // Convert it to the actual analog pin number.
      uint8_t parsedPin;
      if (strcmp(tokenBuffer, "A0") == 0) parsedPin = A0;
      else if (strcmp(tokenBuffer, "A1") == 0) parsedPin = A1;
      else if (strcmp(tokenBuffer, "A2") == 0) parsedPin = A2;
      else if (strcmp(tokenBuffer, "A3") == 0) parsedPin = A3;      
      else if (strcmp(tokenBuffer, "A4") == 0) parsedPin = A4;
      else if (strcmp(tokenBuffer, "A5") == 0) parsedPin = A5;
      else if (strcmp(tokenBuffer, "A6") == 0) parsedPin = A6;
      else if (strcmp(tokenBuffer, "A7") == 0) parsedPin = A7;
      else if (strcmp(tokenBuffer, "A8") == 0) parsedPin = A8;
      else if (strcmp(tokenBuffer, "A9") == 0) parsedPin = A9;
      else if (strcmp(tokenBuffer, "A10") == 0) parsedPin = A10;
      else if (strcmp(tokenBuffer, "A11") == 0) parsedPin = A11;
      else if (strcmp(tokenBuffer, "A12") == 0) parsedPin = A12;
      else if (strcmp(tokenBuffer, "A13") == 0) parsedPin = A13;
      else if (strcmp(tokenBuffer, "A14") == 0) parsedPin = A14;
      else if (strcmp(tokenBuffer, "A15") == 0) parsedPin = A15;
      else {
        cout << F("Expected analog pin number, got ") << tokenBuffer << endl;
        break;
      }

      // Get the pin id and save to array.
      getNextToken(fileStream, tokenBuffer, tokenBufferLength);      
      strcpy(analogPins[i], tokenBuffer);
      analogPins[i][3] = '\0';
    } 
    
    // If we have parsed the last analog pin, we are finished!
    if ((cfgPinType == Cfg::ANALOG) && (i == 15))  {
      fileStream.close();
      parsingSucceeded = true;
      break;
    }      
    // If we have parsed the last digital pin, switch to parsing the analog ones.
    else if (i == 53) {
      cfgPinType = Cfg::ANALOG;
      i = 0;
    }
    // Otherwise, we are not finished. So just do the next one.
    else {
      i++;
    }
  }
  
  // Close the file and return if we succeeded or not.
  fileStream.close();
  return (parsingSucceeded ? true : false);
}

/*
* Returns a pin number given the passed pin id.
*/
uint8_t getPinNumber(char* pinId) {
  // Check for a match in the digital pins lookup table.
  for (int i=0;i<=53;i++) {
      if (strcmp(digitalPins[i], pinId) == 0) {
        return i;
      }      
  }
  
  // If we didn't find a match there, we check the analog
  // pins.
  for (int i=0;i<=15;i++) {
      if (strcmp(analogPins[i], pinId) == 0) {
        switch (i) {
          case 0: return A0;
          case 1: return A1;
          case 2: return A2;
          case 3: return A3;
          case 4: return A4;
          case 5: return A5;
          case 6: return A6;
          case 7: return A7;
          case 8: return A8;
          case 9: return A9;
          case 10: return A10;
          case 11: return A11;
          case 12: return A12;
          case 13: return A13;
          case 14: return A14;
          case 15: return A15;
        }
      }      
  }
  // If we find nothing...
  cout << F("No matching pin number found for pin id ") << pinId << endl;
  return 255;
}

/*
* Returns a pin number given the passed pin id.
*/
uint8_t isValidPin(int pinId) {
  return (pinId != 255 ? true : false);
}

/*
* Parses a door "chunk" and using DoorManager, adds the doors and peripherals.
* This method is pretty brutal. Could be done much nicer.
*/
int parseDoor(Stream& stream, char* startToken, char* stopToken) {

  // The passed stream points at the character after the door token,
  // e.g. "DOOR2": { "blah": "blah" }
  //             ^---- points here
  const uint8_t tokenLength = 16;
  char token[tokenLength] = "";
  uint8_t openBraces = 0;
  Cfg::Pos cfgPos = Cfg::DOOR;
  Cfg::Pos cfgParent = Cfg::NONE;
    
  // Create temporary PACS-objects with rubbish values. 
  // These will receive proper values during parsing.  
  PACSDoor* tempDoor = doorManager.createDoor("temp");  
  PACSReader tempReader("temprdr", 255, 255);
  PACSPeripheral tempPeripheral("tempper", GREENLED, 255, LOW);
  
  // Keep parsing while there are more tokens in stream.
  while(getNextToken(stream, token, tokenLength)) {
    
    // Check if we've parsed the entire door.
    if (strcmp(token, stopToken) == 0) {
      // We return false to specify that there are more doors.
      return 1;
    }
   
    // 
    // "CONTAINERS"
    //  
    else if (strcmp(token, "Reader") == 0) {
      cfgPos = Cfg::READER;
      cfgParent = Cfg::DOOR;  
      openBraces = 0;
    }     
    else if (strcmp(token, "REX") == 0) {         
      cfgPos = Cfg::REX;
      cfgParent = Cfg::DOOR;  
      openBraces = 0;
    }     
    else if (strcmp(token, "DoorMonitor") == 0) {   
      cfgPos = Cfg::DOOR_MONITOR;
      cfgParent = Cfg::DOOR;  
      openBraces = 0;
    }     
    else if (strcmp(token, "Lock") == 0) {   
      cfgPos = Cfg::LOCK;
      cfgParent = Cfg::DOOR;  
      openBraces = 0;
    }     
    else if (strcmp(token, "Input") == 0) {   
      cfgPos = Cfg::DIGITAL_INPUT;
      cfgParent = Cfg::DOOR;  
      openBraces = 0;
    }     
    else if (strcmp(token, "Output") == 0) {   
      cfgPos = Cfg::DIGITAL_OUTPUT;
      cfgParent = Cfg::DOOR;  
      openBraces = 0;
    }         
    //     
    //  "SUB-CONTAINERS"
    //
    else if (strcmp(token, "Wiegand") == 0) {   
      cfgPos = Cfg::WIEGAND;
      cfgParent = Cfg::READER;  
    }     
    else if (strcmp(token, "GreenLED") == 0) {   
      cfgPos = Cfg::GREEN_LED;
      cfgParent = Cfg::READER;  
    }     
    else if (strcmp(token, "Beeper") == 0) {   
      cfgPos = Cfg::BEEPER;
      cfgParent = Cfg::READER;  
    }     
    // 
    // STRING/INT OBJECTS
    //  
    else if (strcmp(token, "Id") == 0) {   
      getNextToken(stream, token, tokenLength);
      switch (cfgPos) {      
        case Cfg::DOOR:
          strcpy(tempDoor->id, token);
          break;
        case Cfg::WIEGAND:
          strcpy(tempReader.id, token);
          break;
        case Cfg::GREEN_LED:
        case Cfg::BEEPER:
        case Cfg::DOOR_MONITOR:
        case Cfg::REX:
        case Cfg::LOCK:
        case Cfg::DIGITAL_INPUT:
        case Cfg::DIGITAL_OUTPUT:
          strcpy(tempPeripheral.id, token);
      } 
    } 
    else if (strcmp(token, "Pin") == 0) {
      getNextToken(stream, token, tokenLength);          
      switch (cfgPos) {      
        case Cfg::GREEN_LED:
        case Cfg::BEEPER:
        case Cfg::DOOR_MONITOR:
        case Cfg::REX:
        case Cfg::LOCK:
        case Cfg::DIGITAL_INPUT:
        case Cfg::DIGITAL_OUTPUT:
          tempPeripheral.pin = getPinNumber(token);
          if (!isValidPin) {
            return -1;
          }
      } 
    }
    else if (strcmp(token, "Pin0") == 0) {
      getNextToken(stream, token, tokenLength);    
      switch (cfgPos) {          
        case Cfg::WIEGAND:                             
          tempReader.pin0 = getPinNumber(token);
          if (!isValidPin) {
            return -1;
          }
      } 
    }
    else if (strcmp(token, "Pin1") == 0) {
      getNextToken(stream, token, tokenLength);      
      switch (cfgPos) {      
        case Cfg::WIEGAND:                 
          tempReader.pin1 = getPinNumber(token);
          if (!isValidPin) {
            return -1;
          }
      } 
    }
    else if (strcmp(token, "ActiveLevel") == 0) {
      getNextToken(stream, token, tokenLength);
      switch (cfgPos) {      
        case Cfg::DOOR_MONITOR:
        case Cfg::REX:
        case Cfg::LOCK:
        case Cfg::DIGITAL_INPUT:
        case Cfg::DIGITAL_OUTPUT:
        case Cfg::GREEN_LED:
        case Cfg::BEEPER:
          if (strcmp(token, "HIGH") == 0) {
            tempPeripheral.activeLevel = HIGH;
          }
          else if (strcmp(token, "LOW") == 0) {
            tempPeripheral.activeLevel = LOW;
          }
      }
    }

    // Token is parsed. Now we need to traverse the container.
    while ((stream.available()) && (stream.peek() != '"')) {
      
      switch (stream.read()) {
        case ']':
          cfgPos = Cfg::DOOR;
          cfgParent = Cfg::NONE;
          break;
        case '{':
          openBraces++;
          break;
        case '}':
          openBraces--;

          // Closing Curly brace means an object has "ended", which
          // means we can save something.                  
          if (cfgParent == Cfg::READER) {
            switch (cfgPos) {              
              case Cfg::WIEGAND:
                tempDoor->addReader(tempReader.id, 
                                    tempReader.pin0, 
                                    tempReader.pin1);                                     
                break;
              case Cfg::GREEN_LED:
                tempDoor->addPeripheral(tempPeripheral.id, 
                                        GREENLED, 
                                        tempPeripheral.pin, 
                                        tempPeripheral.activeLevel);                

                break;
              case Cfg::BEEPER:
                tempDoor->addPeripheral(tempPeripheral.id, 
                                        BEEPER, 
                                        tempPeripheral.pin, 
                                        tempPeripheral.activeLevel);                   
                break;
            }
            // Move the parse position up a level.
            cfgPos = Cfg::READER;
            cfgParent = Cfg::DOOR;                        
          }
          else if (cfgParent == Cfg::DOOR) {
            switch (cfgPos) {
              case Cfg::DOOR_MONITOR:
                tempDoor->addPeripheral(tempPeripheral.id, 
                                        DOORMONITOR, 
                                        tempPeripheral.pin, 
                                        tempPeripheral.activeLevel);
                break;
              case Cfg::REX:
                tempDoor->addPeripheral(tempPeripheral.id, 
                                        REX, 
                                        tempPeripheral.pin, 
                                        tempPeripheral.activeLevel);

                break;
              case Cfg::LOCK:
                tempDoor->addPeripheral(tempPeripheral.id, 
                                        LOCK, 
                                        tempPeripheral.pin, 
                                        tempPeripheral.activeLevel);
                break;
              case Cfg::DIGITAL_INPUT:
                tempDoor->addPeripheral(tempPeripheral.id,
                                        DIGITAL_INPUT,
                                        tempPeripheral.pin,
                                        tempPeripheral.activeLevel);
                break;
              case Cfg::DIGITAL_OUTPUT:
                tempDoor->addPeripheral(tempPeripheral.id,
                                        DIGITAL_OUTPUT,
                                        tempPeripheral.pin,
                                        tempPeripheral.activeLevel);
                break;
            }

          }
          else if (cfgParent == Cfg::NONE) {
            //Do nothing
          }
      }
    }
  }
  // If we got to this point, it means there was no stop token found, i.e.
  // we have parsed the last door.
  return 0;
}

/*
* Opens the door config file for reading and sends the doors it is comprised of
* for parsing, one at a time.
*/ 
bool parsDoorConfiguration(Stream& stream) {

  const uint8_t MAX_NO_OF_DOORS = 16;  
  uint8_t currentDoor = 1;    
  char conversionBuffer[3];
  bool doorsAvailable = true;
  
  cout << F("Parsing door: ");

  // Parse the door objects (as many as we can find, up
  // to the defined maximum).  
  while(doorsAvailable == 1) {  

    // Construct the start/stop door-number string, e.g. "DOOR3", "DOOR4"
    char currentDoorToken[10] = "DOOR";    
    char nextDoorToken[10] = "DOOR";    
    itoa(currentDoor, conversionBuffer, 10);
    strcat(currentDoorToken, conversionBuffer);
    itoa(currentDoor+1, conversionBuffer, 10);
    strcat(nextDoorToken, conversionBuffer);

    cout << (int)currentDoor << F(" ");

    // Check if there's a door config entry for this door number.
    // If there is, we create a door and start parsing it.
    waitForData(stream, 1000);              

    // Do the actual parsing of the door.
    doorsAvailable = parseDoor(stream, currentDoorToken, nextDoorToken);        
    
    // Check if there was an error while parsing.
    if (doorsAvailable == -1) {
      return false;
    }
    // Check if we have reached our door limit.
    else if (currentDoor == MAX_NO_OF_DOORS) {
      cout << F("Maximum number of doors reached.\n");
      return false;
    }
    else {
      currentDoor++;      
    }    
  }

  cout << endl;
  return true;

}

/*
* This function loads the door configuration, which should be in 
* JSON format. 
* 
* It must be structured in a certain way, specified in the documentation. 
* A custom parser is needed as there is not enough memory to load the entire 
* config file into memory.
*/
bool loadDoorConfigurationFromFile(const char* filename) {

  File doorCfgFile;
  bool parsingSucceeded;

  // Open the config file
  doorCfgFile = SD.open(filename);
  if (!doorCfgFile) {
    cout << F("Error opening ") << filename << endl;
    return false;
  }         

  parsingSucceeded = parsDoorConfiguration(doorCfgFile);
  doorCfgFile.close();

  return (parsingSucceeded ? true : false);
}

/*
* Print a sumamry of the configured doors and peripherals to serial.
*/
void printDoorConfiguration() {  
    for (unsigned i=0; i < doorManager.doors.size(); i++) {
        std::cout << "Door:" << std::endl
                  << "  Id: " << doorManager.doors[i].id << std::endl;
        
        std::cout << "Wiegand:\n";
        for (unsigned j=0; j < doorManager.doors[i].readers.size(); j++) {
          std::cout << "  Id: " << doorManager.doors[i].readers[j].id << 
                       " Pin0: " << (int)doorManager.doors[i].readers[j].pin0 <<
                       " Pin1: " << (int)doorManager.doors[i].readers[j].pin1 
                    << std::endl;
        } 

        std::cout << "Peripherals:\n";
        for (unsigned j=0; j < doorManager.doors[i].peripherals.size(); j++) {
          std::cout << "  Id: " << doorManager.doors[i].peripherals[j].id << 
                       " Pin: " << (int)doorManager.doors[i].peripherals[j].pin <<
                       " ActiveLevel: " << (int)doorManager.doors[i].peripherals[j].activeLevel 
                    << std::endl;
        }
        std::cout << std::endl;
    }
}

void parseIPV4string(char* ipAddress, uint8_t* ipbytes) {
  sscanf(ipAddress, "%d.%d.%d.%d", &ipbytes[0], &ipbytes[1], &ipbytes[2], &ipbytes[3]);
}

/*
* Opens the network config file for parsing.
*
* The network config file is small enough for us to be able to use aJSON (as
* opposed to creating our own parser, as with the door and pin configs.)
*/
bool parseNetworkConfiguration(Stream& stream) {
       
  aJsonObject *root = aJson.parse(&aJsonStream(&stream));

  aJsonObject* dhcpEnabled = aJson.getObjectItem(root, "DHCPEnabled");
  aJsonObject* macAddress = aJson.getObjectItem(root, "MAC");
  aJsonObject* ip = aJson.getObjectItem(root, "IP");
  aJsonObject* gateway = aJson.getObjectItem(root, "Gateway");
  aJsonObject* subnet = aJson.getObjectItem(root, "Subnet");
  aJsonObject* dns = aJson.getObjectItem(root, "DNS");
  aJsonObject* httpPort = aJson.getObjectItem(root, "HTTPPort");
  aJsonObject* websocketPort = aJson.getObjectItem(root, "WebsocketPort");

  if (!dhcpEnabled) {
    cout << F("DHCPEnabled key not found in network config file.");
    return false;
  }
  else if (!macAddress) {
    cout << F("MAC key not found in network config file.");
    return false;
  }
  else if (!ip) {
    cout << F("IP key not found in network config file.");
    return false;
  }
  else if (!gateway) {
    cout << F("Gateway key not found in network config file.");
    return false;
  }
  else if (!subnet) {
    cout << F("Subnet key not found in network config file.");
    return false;
  }
  else if (!dns) {
    cout << F("DNS key not found in network config file.");
    return false;
  }
  else if (!httpPort) {
    cout << F("HTTPPort key not found in network config file.");
    return false;
  }
  else if (!websocketPort) {
    cout << F("WebsocketPort key not found in network config file.");
    return false;
  }  

  // DHCP related
  network_config.use_dhcp = dhcpEnabled->valuebool;
  network_config.dhcp_refresh_minutes = 60;
  
  // Parse the MAC address
  char macHexStr[2];
  int macAddressPos = -1;
  for (int i=0; i<6; i++) {
    macHexStr[0] = macAddress->valuestring[++macAddressPos];
    macHexStr[1] = macAddress->valuestring[++macAddressPos];
    macAddressPos++; //Skip the ":" separator.    
    network_config.mac[i] = (uint8_t)strtol(macHexStr, NULL, 16);
  }

  // IP related
  uint8_t ipbytes[4];
  parseIPV4string(ip->valuestring, ipbytes);
  network_config.ip = IPAddress(ipbytes);
  parseIPV4string(gateway->valuestring, ipbytes);
  network_config.gateway = IPAddress(ipbytes);
  parseIPV4string(subnet->valuestring, ipbytes);
  network_config.subnet = IPAddress(ipbytes);
  parseIPV4string(dns->valuestring, ipbytes);
  network_config.dns = IPAddress(ipbytes);

  // Ports
  network_config.httpPort = (int)httpPort->valueint;
  network_config.websocketPort = (int)websocketPort->valueint;
  
  // Free allocated memory.
  aJson.deleteItem(root);

  // We've successfully parsed the network file.
  return true;
}

/*
* Loads the network configuration, which should also be in 
* JSON format. 
*/
bool loadNetworkConfigurationFromFile(const char* filename) {

  File networkCfgFile;
  bool parsingSucceeded;
  
  // Open the config file
  networkCfgFile = SD.open(filename);
  if (!networkCfgFile) {
    cout << F("Error opening ") << filename << endl;
    return false;
  }           

  parsingSucceeded = parseNetworkConfiguration(networkCfgFile);
  networkCfgFile.close();

  return (parsingSucceeded ? true : false);
}

/* *****************************************************************************************************
* 
* Networking Section 
* 
***************************************************************************************************** */

/*
* Configures the ethernet shield with our specified network values.
*/
bool setupNetwork() {

  // If we're not using DHCP-
  if (!network_config.use_dhcp) {
    Ethernet.begin(network_config.mac, network_config.ip, network_config.dns, 
      network_config.gateway, network_config.subnet);
  } 
  // If we ARE using DHCP.
  else {
    if (Ethernet.begin(network_config.mac) == 0) {
      cout << F("Failed to configure Ethernet using DHCP.\n");
      return false;
    }
  }
  // Disable the SPI
  digitalWrite(ETHERNET_SELECT_PIN, HIGH);

  return true;
}

/*
* Renew the DHCP relase in a given interval.
*/
void renewDHCP() {
  if (network_config.use_dhcp) {
    Ethernet.maintain();
    cout << F("DHCP lease renewed.\n");
  }
}

/* *****************************************************************************************************
* 
* Webserver Section 
* 
***************************************************************************************************** */

/*
* Called whenever a non extisting page is called.
*/
void errorHTML(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  server.httpFail();

  if (type == WebServer::HEAD)
    return;
  
  server.print(F("<html><head><title>HTTP 400</title></head><body>\n"));
  server.print(F("<h2>HTTP 400 - Bad Request</h2>\n"));
  server.print(F("<p>The request cannot be fulfilled due to bad syntax.</p>\n"));
  server.print(F("</body></html>"));
}

/*
* Called for all the remaining cases. We need to check if the requested file is one we are servering,
* and if so, send it to the client.
*/
void webAppFile(WebServer &server, WebServer::ConnectionType type, char **url_path, char *url_tail, bool tail_complete)
{
  // For a HEAD request, we just stop after outputting headers.
  if (type == WebServer::HEAD)
    return;  

  cout << F("Client is requesting file: ") << *url_path << endl;      
  
  // Check if requested file is one we are serving on SD card.
  if (strcmp(*url_path, "index.htm") == 0 || strcmp(*url_path, "app.js") == 0 ||
    strcmp(*url_path, "keypad.mp3") == 0 || strcmp(*url_path, "favicon.ico") == 0) 
  {  
    
    // Create a full filename path. 32 characters should be 
    // enough for 8+3 filenames and the folder structure we have.
    char fullFilename[32];
    sprintf(fullFilename, "web/%s", *url_path);

    // It was successfully opened, so send the client a http 200 with correct
    // content type depending on the file.
    if (strcmp(*url_path, "app.js") == 0) {      
      sendFile(server, "text/javascript; charset=utf-8", fullFilename);
    }
    else if (strcmp(*url_path, "keypad.mp3") == 0) {    
      sendFile(server, "audio/mpeg", fullFilename);
    }
    else if (strcmp(*url_path, "favicon.ico") == 0) {    
      sendFile(server, "image/x-icon", fullFilename);
    }
    else {
      sendFile(server, "text/html; charset=utf-8", fullFilename);
    }    

  }
  
  // If we didn't get a match for any of the files we serve, we send the client
  // a http 4xx error.
  else {
    errorHTML(server, type, url_tail, tail_complete);
  }  

}

/*
* Called when client requests the root url. We just redirect to our url-path function
* which handles all our web files (including the index.htm).
*/
void defaultHTML(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{
  webAppFile(server, type, &indexFilename, url_tail, tail_complete);
}

/*
* Returns a http fail message to the user in case of failed API command.
*/
void apiResponse(bool requestSuccessful, const unsigned char* message) {
  if (requestSuccessful) {
    webserver->httpFail();
  }
  else {
   webserver->httpSuccess();
  }
  webserver->printP(message);
  webserver->printCRLF();           
}

/*
 * This is the api route for sending http commands. 
 * Three post parameters need to be specified: 
 *    cmd (the action to be performed, must come first!)
 *    doorId (id of the target door)
 *    id (id of the target peripheral)
 * Alternatively, you can send in a json structure.
 */
void apiCMD(WebServer &server, WebServer::ConnectionType type, char *url_tail, bool tail_complete)
{

  URLPARAM_RESULT urlParamResult;
  Command cmd = UNDEFINED;  
  char name[32], value[32];
  bool postParamsAvailable;
  char id[16] = {'\0'};
  char doorId[16] = {'\0'};
  int facilityCode = -1;
  long cardNumber = -1;
  char pin[16] = {'\0'};

   P(out_of_bounds) = "Card or facility-code is out of bounds.\n";
   P(card_not_specified) = "Card or facility-code not specified.\n";
   P(id_not_found) = "Request failed. No door and/or reader/peripheral found with specified id(s).\n";
   P(unknown_command) = "Unknown command.\n";
   P(could_not_open_door_config_file) = "Could not open door config file for reading.";
   P(peripheral_is_active) = "Peripheral is ACTIVE.";
   P(peripheral_is_inactive) = "Peripheral is INACTIVE.";
   P(ok) = "OK";

  if (type == WebServer::HEAD)
    return;

  // If we receive a http-post, we assume the request is sent with json-data.
  if (type == WebServer::POST) {}
  
  // If we receive a http-get, we assume the request was issued with URL parameters.
  if (type == WebServer::GET) {    
    
    // Parse the URL parameters.
    do {
        urlParamResult = server.nextURLparam(&url_tail, name, 32, value, 32);      
        if ((urlParamResult == URLPARAM_OK) && name && value) {
        
        // The cmd parameter's value tells us what action to perform.
        if (strcmp(name, "cmd") == 0) {                                      
          if (strcmp(value, "swipecard") == 0) cmd = SWIPECARD;
          else if (strcmp(value, "enterpin") == 0) cmd = ENTERPIN;
          else if (strcmp(value, "opendoor") == 0) cmd = OPENDOOR;
          else if (strcmp(value, "closedoor") == 0) cmd = CLOSEDOOR;
          else if (strcmp(value, "pushrex") == 0) cmd = PUSHREX;
          else if (strcmp(value, "activateinput") == 0) cmd = ACTIVATEINPUT;
          else if (strcmp(value, "deactivateinput") == 0) cmd = DEACTIVATEINPUT;          
          else if (strcmp(value, "getperipheralstate") == 0) cmd = GETPERIPHERALSTATE;
          else if (strcmp(value, "getnetworkconfig") == 0) cmd = GETNETWORKCONFIG;
          else if (strcmp(value, "getdoorconfig") == 0) cmd = GETDOORCONFIG;
          else if (strcmp(value, "getpinconfig") == 0) cmd = GETPINCONFIG;
          else cmd = UNDEFINED;
        }
        // 
        else if (strcmp(name, "facilitycode") == 0) {
          if (value && (cmd == SWIPECARD)) {
            facilityCode = atoi(value);
          }
        }
        else if (strcmp(name, "cardnumber") == 0) {
          if (value && (cmd == SWIPECARD)) {
            cardNumber = atol(value);           
          }  
        }
        else if (strcmp(name, "pin") == 0) {
          if (value && (cmd == ENTERPIN)) {
            strcpy(pin, value);             
          }
        }
        else if (strcmp(name, "doorid") == 0) {
          if (value) {
            strcpy(doorId, value);
          }
        }
        else if (strcmp(name, "id") == 0) {
          if (value) {
            strcpy(id, value);
          }
        }
      }
    } while (urlParamResult != URLPARAM_EOS);
      
    // Now we see what command was issued, find the door and 
    // reader/peripheral and perform it.
    switch (cmd) {
      // SwipeCard Command
      case SWIPECARD:
          // First check that the input parameters are there.
          if ((facilityCode == -1) || (cardNumber == -1)) {            
            apiResponse(false, card_not_specified);
            return;
          }          
          // Then check that the input parameters are valid.
          else if (!((facilityCode>=0) && (facilityCode<=255) && (cardNumber>=0) && (cardNumber<=65535))) {            
            apiResponse(false, out_of_bounds);
            return;
          }
          // And if so, execute the command and tell user if it successful or not.
          else if(!doorManager.swipeCard(doorId, id, facilityCode, cardNumber)) {        
            apiResponse(false, id_not_found);
            return;
          }
        break;
     
      // Enter pin command
      case ENTERPIN:
        if (!doorManager.enterPIN(doorId, id, pin)) {        
          apiResponse(false, id_not_found);
          return;
        } 
        break;
     
      // Open door command
      case OPENDOOR:
        if (!doorManager.openDoor(doorId, id)) {        
          apiResponse(false, id_not_found);
          return;        
        } 
        break;
     
     // Close door command
      case CLOSEDOOR:
        if (!doorManager.closeDoor(doorId, id)) {        
          apiResponse(false, id_not_found);
          return;        
        }
        break;
     
      // Push REX command
      case PUSHREX:
        if (!doorManager.pushREX(doorId, id)) {        
          apiResponse(false, id_not_found);
          return;
        }        
        break;      
      
      // Activate Input command
      case ACTIVATEINPUT:
        if (!doorManager.activateInput(doorId, id)) {        
          apiResponse(false, id_not_found);
          return;
        }        
        break;      
      
      // Deactivate Input command
      case DEACTIVATEINPUT:
        if (!doorManager.deactivateInput(doorId, id)) {        
          apiResponse(false, id_not_found);
          return;
        }        
        break;      
      
      // Get peripheral state command
      case GETPERIPHERALSTATE:
        {
          int isActive = doorManager.isPeripheralActive(doorId, id);
          if (isActive == -1) {        
            apiResponse(false, id_not_found);          
          }
          else if (isActive) {
            apiResponse(true, peripheral_is_active);          
          }
          else {
            apiResponse(true, peripheral_is_inactive);          
          }
          return;  
        }

      // Get door config command
      case GETNETWORKCONFIG:
        {                  
          sendFile(server, "application/json", networkConfigFilename);
          return;    
        }
        break;


      // Get door config command
      case GETDOORCONFIG:
        {                  
          sendFile(server, "application/json", doorsConfigFilename);
          return;    
        }
        break;

      // Get pin config command
      case GETPINCONFIG:
        {                  
          sendFile(server, "application/json", pinsConfigFilename);
          return;    
        }
        break;
      

      case UNDEFINED:
      default:
        server.httpFail();
        server.printP(unknown_command);
        return;
    }

    server.httpSuccess("text/html", NULL);
    server.printP(ok);
    server.printCRLF();

  }
}

/*
* Must be called periodically to see if new announcements need to be sent.
*/
#ifdef BONJOUR_ENABLED
  void updateBonjour() {
    EthernetBonjour.run();
    delay(1);
  }
#endif


/* *****************************************************************************************************
* 
* PACS Section
* 
***************************************************************************************************** */

/*
* onStateChange()
* Called whenever a peripheral has changed pin levels.
*/
void onStateChange(PACSDoor &door, PACSPeripheral &p) {
  
  aJsonObject *root, *update;

  root = aJson.createObject();  
  aJson.addItemToObject(root, "Update", update = aJson.createObject());    
  aJson.addStringToObject(update, "DoorId", door.id);
  aJson.addStringToObject(update, "Id", p.id);

  switch (p.type) {        
    case GREENLED:
    case BEEPER:
    case DOORMONITOR:
    case REX:
    case LOCK:    
    case DIGITAL_INPUT:
    case DIGITAL_OUTPUT:  
  
      cout << "[" << door.id << "|" << p.id << "]: ";

      if (p.isActive()) {
        cout << F("is ACTIVE\n");
        aJson.addTrueToObject(update, "IsActive");
      }
      else {
        cout << F("is INACTIVE\n");
        aJson.addFalseToObject(update, "IsActive");  
      }
  
      break;
  
    default:
      cout << "[" << door.id << "|" << p.id << "]: Unknown periperhal";
      break;
  }  
  
  // Render the JSON string. 
  char *json_string = aJson.print(root);

  // Send the update over websocket connection.  
  if (websocketServer.isConnected()) { 
    websocketServer.sendMessage(json_string, strlen(json_string));
  }

  // Free allocated memory.
  free(json_string);
  aJson.deleteItem(root);

}

/* ****************************************************************************************************
* 
* WebSocket Section
* 
***************************************************************************************************** */

// Global timer for timed events.
SimpleTimer timer;

int bonjourTimer = -1;
int dhcpRenewalTimer = -1;
int sendHeartbeatTimer = -1;
int heartbeatTimeoutTimer = -1;
bool heartbeatAcknowledged = true;

/*
* onHeartbeatTimeout()
* Is called if we have not received a heartbeat PONG reply from the client in the defined
* amount of time.
*/
void onHeartbeatTimeout() {
  if (websocketServer.isConnected()) {
    cout << F("Heartbeat timeout. Closing connection.\n");
    websocketServer.gracelessClose(WS_POLICY_VIOLATION, "Heartbeat timeout.");
  }
  timer.deleteTimer(heartbeatTimeoutTimer);
  heartbeatTimeoutTimer = -1;
}

/*
* onHeartbeat()
* Is called whenever there is a reply to the hearbeat PING frame sent out by our websocket server.
*/
void onHeartbeatResponse(WebSocket &socket) {
  // Reset the timeout timer.
  timer.restartTimer(heartbeatTimeoutTimer);
  heartbeatAcknowledged = true;
}

/*
* sendHeartbeat()
* Check that the connected client is still alive by sending a heartbeat (websocket ping) and
* wait for a pong reply. 
*/
void sendHeartbeat() {

  if (websocketServer.isConnected()) {     
    if (!heartbeatAcknowledged) {
      cout << F("No heartbeat acknowledgedment received.\n");
    }
    websocketServer.sendHeartbeat();      
    // Start the timeout timer, if not started. 
    if (heartbeatTimeoutTimer == -1) {
      heartbeatTimeoutTimer = timer.setTimeout(HEARTBEAT_TIMEOUT*1000, onHeartbeatTimeout);
    }
    heartbeatAcknowledged = false;
  }
}


/*
* onConnect()
* Is called whenever there is a new websocket connection (there can be only 
* one connection at any given time.)
*/
void onConnect(WebSocket &socket) {  
  cout << F("Websocket connection.\n");
  
  if (sendHeartbeatTimer != -1) {
   timer.deleteTimer(sendHeartbeatTimer);
   sendHeartbeatTimer = -1;
 } 
 sendHeartbeatTimer = timer.setInterval(HEARTBEAT_INTERVAL*1000, sendHeartbeat);
}

/*
* onDisconnect()
* Is called when the websocket connection is disconnected. 
*/
void onDisconnect(WebSocket &socket) {
  timer.deleteTimer(heartbeatTimeoutTimer);
  timer.deleteTimer(sendHeartbeatTimer);
  sendHeartbeatTimer = -1;
  heartbeatTimeoutTimer = -1;
  cout << F("Websocket was disconnected.\n");
}

/*
* onData()
* Is called whenever there is a new data available in the websocket pipe. 
*/
void onData(WebSocket &socket, char* dataString, unsigned short frameLength) {

  // Parse the JSON data into an object tree.
  aJsonObject* root = aJson.parse(dataString);  

  if (root == NULL) {
    cout << F("Data is not valid JSON.\n");
    return;
  }

  // Get the command
  aJsonObject* cmd = root->child;

  //
  // RequestUpdate command
  //
  if (strcmp(cmd->name, "RequestUpdate") == 0) {
    for (unsigned i=0; i < doorManager.doors.size(); i++) {        
      for (unsigned j=0; j < doorManager.doors[i].peripherals.size(); j++) {
        onStateChange(doorManager.doors[i], doorManager.doors[i].peripherals[j]);
      }
    }
    aJson.deleteItem(root);
    return; 
  }

  // The rest of the commands require a door- and peripheral id.
  aJsonObject* doorId = aJson.getObjectItem(cmd, "DoorId");
  aJsonObject* id = aJson.getObjectItem(cmd, "Id");
  if (cmd == NULL || doorId == NULL || id == NULL) {
    cout << F("Command, door-id and/or id not present in JSON structure.") << endl;
    aJson.deleteItem(root);
    return;
  }        

  //
  // SwipeCard command
  //
  if (strcmp(cmd->name, "SwipeCard") == 0) {
    aJsonObject* facilityCode = aJson.getObjectItem(cmd, "FacilityCode");
    aJsonObject* cardNumber = aJson.getObjectItem(cmd, "CardNumber");  
    
    if (facilityCode == NULL || cardNumber == NULL) {
      cout << F("Facility Code and/or cardNumber not present in JSON structure.") << endl;
      aJson.deleteItem(root);
      return;
    }        
    doorManager.swipeCard(doorId->valuestring, id->valuestring, 
                          atoi(facilityCode->valuestring), atoi(cardNumber->valuestring));
  }
  
  //
  // EnterPIN command
  //
  else if (strcmp(cmd->name, "EnterPIN") == 0) {
    aJsonObject* pin = aJson.getObjectItem(cmd, "PIN");  
    if (pin == NULL) {
      cout << F("PIN not present in JSON structure.") << endl;
      aJson.deleteItem(root);
      return;
    }        
    doorManager.enterPIN(doorId->valuestring, id->valuestring, pin->valuestring);
  }  

  //
  // OpenDoor command
  //
  else if (strcmp(cmd->name, "OpenDoor") == 0) {
    doorManager.openDoor(doorId->valuestring, id->valuestring);
  }

  //
  // CloseDoor command
  //
  else if (strcmp(cmd->name, "CloseDoor") == 0) {
    doorManager.closeDoor(doorId->valuestring, id->valuestring);
  }

  //
  // PushREX command
  //
  else if (strcmp(cmd->name, "PushREX") == 0) {
    doorManager.pushREX(doorId->valuestring, id->valuestring);
  }

  //
  // ActivateInput command
  //
  else if (strcmp(cmd->name, "ActivateInput") == 0) {
    doorManager.activateInput(doorId->valuestring, id->valuestring);
  }

  //
  // DeactivateInput command
  //
  else if (strcmp(cmd->name, "DeactivateInput") == 0) {
    doorManager.deactivateInput(doorId->valuestring, id->valuestring);
  }

  //
  // Not a recognized command.
  //
  else {    
    cout << F("Unkown command.") << cmd->name << endl;
  }

  aJson.deleteItem(root);
}

/* 
* Setup() is where the Arduino starts executing code.
* We setup our network, doors, servers and other stuff here.
*/
void setup()
{
  Serial.begin(SERIAL_BAUD);
  
  cout << F("\n*************************************\n");
  cout << F("*  SETUP\n");
  cout << F("*************************************\n\n");
 
  // Setup SD card and initialize the Ethernet adapter 
  // with the settings from eeprom.
  pinMode(SS_HARDWARE_PIN, OUTPUT);
  setupSDCard();  
  delay(200);

  //a
  // Load network, pin and door config from SD card.
  //
  cout << F("Loading network configuration.\n");
  if (!loadNetworkConfigurationFromFile(networkConfigFilename))
    while (true) delay(100); 
  cout << F("Loading pin configuration.\n");
  if (!loadPinMappingsFromFile(pinsConfigFilename))
    while (true) delay(100); 
  cout << F("Loading door configuration.\n");
  if (!loadDoorConfigurationFromFile(doorsConfigFilename))
    while (true) delay(100); 
  
  // Door configuration is loaded! Now initialize all the doors and their
  // peripherals/readers. This sets correct pinmode, active-level etc.
  doorManager.initializeDoors();
  doorManager.registerStateChangeCallback(&onStateChange);  
  
  cout << F("\n*************************************\n");
  cout << F("*  DOOR CONFIGURATION\n");
  cout << F("*************************************\n\n");

  printDoorConfiguration();

  //
  // Setup the network.
  //
  cout << F("Setting up the network.\n");
  setupNetwork();
  
  cout << F("\n*************************************\n");
  cout << F("*  NETWORK CONFIGURATION\n");
  cout << F("*************************************\n\n");
  printNetworkConfiguration();

#ifdef BONJOUR_ENABLED
  // Start the bonjour/zeroconf service.  
  char* bonjour_hostname = "pacsis";
  EthernetBonjour.begin(bonjour_hostname);
  EthernetBonjour.addServiceRecord("Pacsis._ws", network_config.websocketPort, MDNSServiceTCP);
  cout << F("Bonjour/ZeroConf name: ") << bonjour_hostname << ".local" << endl;
#endif

  // Setup the server and the routes and begin listening for incoming connections.
  webserver = new WebServer("", network_config.httpPort);  
  webserver->setDefaultCommand(&defaultHTML); // Root url.
  webserver->setUrlPathCommand(&webAppFile); // All web files on SD card.
  webserver->setFailureCommand(&errorHTML); // HTTP 400.
  webserver->addCommand("api", &apiCMD); // API route.
  webserver->begin();

  // Setup the websocket server and start listening for incoming connections.
  websocketServer = WebSocket("/", 8888);
  websocketServer.registerConnectCallback(&onConnect);
  websocketServer.registerDisconnectCallback(&onDisconnect);
  websocketServer.registerDataCallback(&onData);
  websocketServer.registerHeartbeatResponseCallback(&onHeartbeatResponse);
  websocketServer.begin();

  // Register timed events.
  dhcpRenewalTimer = timer.setInterval(network_config.dhcp_refresh_minutes*60000, renewDHCP);    
  int freememTimer = timer.setInterval(5000, freeMem);
  #ifdef BONJOUR_ENABLED
    bonjourTimer = timer.setInterval(500, updateBonjour);
  #endif  

  cout << F("\n*************************************\n");
  cout << F("*  MAIN LOOP\n");
  cout << F("*************************************\n\n");
}

/*
* Main loop
*/
void loop() {

  // Poll the timer
  timer.run();
  
  // Process incoming web-server connections.
  char buff[200];
  int len = 200;
  webserver->processConnection(buff, &len);

  // Listen for data on websocket connection.
  websocketServer.listen();

  // Checks if any pins have altered states, and notifies 
  // the registered callbacks.
  doorManager.updateLevels();
}
