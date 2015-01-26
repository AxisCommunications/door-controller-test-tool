/*
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

// Uses Entropy library to generate random number for MAC address
// if there is no network.cfg file and the settings have not been 
// stored in EEPROM.

#include "Network.h"
#include "aJSON.h"
#include <StandardCplusplus.h>
#include <serstream>
#include <EEPROM.h>
#include <SD.h>

const char* Network::configFilename = "config/network.cfg";
uint8_t Network::mac_oui[3] = { 0x90, 0xA2, 0xDA };

Network::Network() {

  // The first three octets of the MAC address are the "Organizationally Unique Identifier" or
  // OUI. The UID from GHEO Sa, which is the correct one for Arduino Ethernet is 90:A2:DA
  // This is the signature that is used to store the settings in EEPROM if there is no 
  // network.cfg file.
    
  // Default settings
  use_dhcp = true;
  memcpy(mac, mac_oui, 3);
  memset(&mac[3], 0x81, 3);
  
  ip = IPAddress(192, 168, 1, 2);
  subnet = IPAddress(255, 255, 255, 0);
  gateway = IPAddress(192, 168, 1, 1);
  dns = IPAddress(0, 0, 0, 0);
      
  httpPort = 80;
  websocketPort = 8888;
  dhcp_refresh_minutes = 60;
}

/*
* Initializes the network settings for this Arduino by reading it from either the
* network.cfg file or EEPROM. If there is no network cfg file and the OUI is not
* stored in the first three bytes of EEPROM than generate a random MAC address 
* and save the default settings into EEPROM (including the MAC).
*/
void Network::Initialize()
{
  if (SD.exists((char*) configFilename))
  {
    File networkConfigFile;
    networkConfigFile = SD.open(configFilename);
    if (networkConfigFile)
    {
      parseNetworkConfiguration(networkConfigFile);
      networkConfigFile.close();
    }
    else
    {
      cout << F("Error opening ") << configFilename << endl;
    }
  }
  else
  {
    if (EEPROM.read(1) == mac_oui[0] && EEPROM.read(2) == mac_oui[1] && EEPROM.read(3) == mac_oui[2]) 
    {
      // The settings have been written in EEPROM  
      cout << F("read from EEPROM") << endl;;
      readSettings();
    }
    else
    {
      // Save the default settings to EEPROM  
      writeSettings();
      
      cout << F("stored to EEPROM") << endl;
    }
  }
}

/*
* Configures the ethernet shield with our specified network values.
*/
bool Network::setup() {  

  // Initialize the network settings before starting
  Initialize();
  
  // If we're not using DHCP-
  if (!use_dhcp) {
    Ethernet.begin(mac, ip, dns, gateway, subnet);
  } 
  // If we ARE using DHCP.
  else {
    if (Ethernet.begin(mac) == 0) {
      cout << F("Failed to configure Ethernet using DHCP.\n");
      return false;
    }
  }
  
  return true;
}

void Network::parseIPV4string(char* ipAddress, uint8_t* ipbytes) {
  sscanf(ipAddress, "%d.%d.%d.%d", &ipbytes[0], &ipbytes[1], &ipbytes[2], &ipbytes[3]);
}

void Network::IPAddressToString(char* buff, IPAddress ip) {
  sprintf(buff, "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
}

/*
* Opens the network config file for parsing.
*
* The network config file is small enough fo us to be able to use aJSON (as
* opposed to creating our own parser, as with the door and pin configs.)
*/
bool Network::parseNetworkConfiguration(Stream& stream) {
  
  aJsonObject *root = aJson.parse(&aJsonStream(&stream));
  
  aJsonObject* dhcpEnabled = aJson.getObjectItem(root, "DHCPEnabled");
  aJsonObject* macAddress = aJson.getObjectItem(root, "MAC");
  aJsonObject* ipObject = aJson.getObjectItem(root, "IP");
  aJsonObject* gatewayObject = aJson.getObjectItem(root, "Gateway");
  aJsonObject* subnetObject = aJson.getObjectItem(root, "Subnet");
  aJsonObject* dnsObject = aJson.getObjectItem(root, "DNS");
  aJsonObject* httpPortObject = aJson.getObjectItem(root, "HTTPPort");
  aJsonObject* websocketPortObject = aJson.getObjectItem(root, "WebsocketPort");

  if (!dhcpEnabled) {
    cout << F("DHCPEnabled key not found in network config file.");
    return false;
  }
  else if (!macAddress) {
    cout << F("MAC key not found in network config file.");
    return false;
  }
  else if (!ipObject) {
    cout << F("IP key not found in network config file.");
    return false;
  }
  else if (!gatewayObject) {
    cout << F("Gateway key not found in network config file.");
    return false;
  }
  else if (!subnetObject) {
    cout << F("Subnet key not found in network config file.");
    return false;
  }
  else if (!dnsObject) {
    cout << F("DNS key not found in network config file.");
    return false;
  }
  else if (!httpPortObject) {
    cout << F("HTTPPort key not found in network config file.");
    return false;
  }
  else if (!websocketPortObject) {
    cout << F("WebsocketPort key not found in network config file.");
    return false;
  }  

  // DHCP related
  use_dhcp = dhcpEnabled->valuebool;
  
  // Parse the MAC address
  char macHexStr[2];
  int macAddressPos = -1;
  for (int i=0; i<6; i++) {
    macHexStr[0] = macAddress->valuestring[++macAddressPos];
    macHexStr[1] = macAddress->valuestring[++macAddressPos];
    macAddressPos++; //Skip the ":" separator.    
    mac[i] = (uint8_t)strtol(macHexStr, NULL, 16);
  }

  // IP related
  uint8_t ipbytes[4];
  parseIPV4string(ipObject->valuestring, ipbytes);
  ip = IPAddress(ipbytes);
  parseIPV4string(gatewayObject->valuestring, ipbytes);
  gateway = IPAddress(ipbytes);
  parseIPV4string(subnetObject->valuestring, ipbytes);
  subnet = IPAddress(ipbytes);
  parseIPV4string(dnsObject->valuestring, ipbytes);
  dns = IPAddress(ipbytes);

  // Ports
  httpPort = (int)httpPortObject->valueint;
  websocketPort = (int)websocketPortObject->valueint;
  
  // Free allocated memory.
  aJson.deleteItem(root);

  // We've successfully parsed the network file.
  return true;
}

void Network::saveNetworkConfiguration(Stream& stream) {
  aJsonObject *root = aJson.createObject();
  
  aJson.addBooleanToObject(root, "DHCPEnabled",  use_dhcp);

  char buff[32];
  sprintf(buff, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  aJson.addStringToObject(root, "MAC", buff);
  
  IPAddressToString(buff, ip);
  aJson.addStringToObject(root, "IP", buff);
  
  IPAddressToString(buff, gateway);
  aJson.addStringToObject(root, "Gateway", buff);
  
  IPAddressToString(buff, subnet);
  aJson.addStringToObject(root, "Subnet", buff);
  
  IPAddressToString(buff, dns);
  aJson.addStringToObject(root, "DNS", buff);

  aJson.addNumberToObject(root, "HTTPPort",     httpPort);
  aJson.addNumberToObject(root, "WebsocketPort", websocketPort);
  
  aJson.print(root, &aJsonStream(&stream));
  
  aJson.deleteItem(root);
}

/*
* Read the settings from EEPROM
*/
void Network::readSettings() {
  int j = 4;
  j+= readEEPROM(&mac[0], j, sizeof(mac) * sizeof(uint8_t));
  j+= readEEPROM(&use_dhcp, j, sizeof(bool));
  j+= readEEPROM(&ip, j);
  j+= readEEPROM(&subnet, j);
  j+= readEEPROM(&gateway, j);
  j+= readEEPROM(&dns, j);
  j+= readEEPROM(&httpPort, j, sizeof(httpPort));
  j+= readEEPROM(&websocketPort, j, sizeof(websocketPort));  
}

/*
* Save the settings in EEPROM
*/
void Network::writeSettings() {
  int j = 1;
  j+= writeEEPROM(mac_oui, j, sizeof(mac_oui) * sizeof(uint8_t));
  j+= writeEEPROM(mac, j, sizeof(mac) * sizeof(uint8_t));
  j+= writeEEPROM(&use_dhcp, j, sizeof(bool));
  j+= writeEEPROM(&ip, j);
  j+= writeEEPROM(&subnet, j);
  j+= writeEEPROM(&gateway, j);
  j+= writeEEPROM(&dns, j);
  j+= writeEEPROM(&httpPort, j, sizeof(httpPort));
  j+= writeEEPROM(&websocketPort, j,sizeof(websocketPort));  
}

int Network::readEEPROM(void* buf, int offset, int len) {
  uint8_t* p = (uint8_t*) buf;
  for (int i = 0; i < len; i++)
    p[i] = EEPROM.read(i + offset);
    
  return len;
}

int Network::readEEPROM(IPAddress* address, int offset) {
  uint32_t buf;
  int retVal = readEEPROM(&buf, offset, sizeof(uint32_t));
  *address = buf;
  return retVal;
}

int Network::writeEEPROM(void* buf, int offset, int len)
{
  uint8_t* p = (uint8_t*) buf;
  for (int i = 0; i < len; i++)    
    EEPROM.write(i + offset, p[i]);
    
  return len;
}

int Network::writeEEPROM(IPAddress* address, int offset) {
  uint32_t buf = (int32_t) *address;
  return writeEEPROM(&buf, offset, sizeof(uint32_t));
}

/*
* Prints network configuration to serial port.
*/
void Network::printConfiguration() {
  
  // MAC address takes a bit of work to format for output.
  char buff[32];
  cout << F("MAC: ");
  for(int i=0; i<6; i++) {
    sprintf(buff, "%02X", mac[i]);
    cout << buff;
    if (i != 5) cout << ":";
  }
  cout << endl;

  cout << F("DHCP ");
  cout << (use_dhcp ? F("enabled") : F("disabled"));

  cout << F("\nCONFIGURATION:\n\tIP:\t\t");
  IPAddressToString(buff, ip);
  cout << buff;
  
  cout << F("\n\tSubnet Mask:\t");
  IPAddressToString(buff, subnet);
  cout << buff;
  
  cout << F("\n\tGateway:\t");
  IPAddressToString(buff, gateway);
  cout << buff;
  
  cout << F("\n\tDNS Server:\t");
  IPAddressToString(buff, dns);
  cout << buff;

  if (use_dhcp) {  
    cout << F("\nETHERNET:\n\tIP:\t\t");
    Ethernet.localIP().printTo(Serial);
  
    cout << F("\n\tSubnet Mask:\t");
    Ethernet.subnetMask().printTo(Serial);
  
    cout << F("\n\tGateway:\t");
    Ethernet.gatewayIP().printTo(Serial);
  
    cout << F("\n\tDNS Server:\t");
    Ethernet.dnsServerIP().printTo(Serial);
  }

  cout << F("\nHTTP Port: ") << (int)httpPort;
  cout << F("\nWebsocket Port: ") << (int)websocketPort;

  cout << F("\n");
}

void Network::addArrayToObject(aJsonObject* object, const char *string, uint8_t* items, int count) {
  aJsonObject *array, *item;
  array = aJson.createArray();
  aJson.addItemToObject(object, string, array);
  for (int i = 0; i < count; i++) {
    item = aJson.createItem(items[i]);
    aJson.addItemToArray(array, item);
  }
}

void Network::addArrayToObject(aJsonObject* object, const char *string, IPAddress ip) {
  aJsonObject *array, *item;
  array = aJson.createArray();
  aJson.addItemToObject(object, string, array);
  for (int i = 0; i < 4; i++) {
    item = aJson.createItem(ip[i]);
    aJson.addItemToArray(array, item);
  }
}

void Network::objectToArray(aJsonObject* object, uint8_t* items) {
  if (object->type == aJson_Array) {
    for (int i = 0; i < aJson.getArraySize(object); i++) {
      aJsonObject* item = aJson.getArrayItem(object, i);
      if (item->type == aJson_Int)
        items[i] = item->valueint;
    }
  }
}

void Network::objectToArray(aJsonObject* object, IPAddress* ip) {
  uint8_t ipbytes[4];
  objectToArray(object, ipbytes);
  *ip = IPAddress(ipbytes);
}

/*
* Builds aJson object for the current Network Settings for editing
*/
void Network::settingsToJSON(aJsonObject *root)
{
  char buff[20];
  aJsonObject *ethernet;
  
  aJson.addBooleanToObject(root, "EEPROM", !SD.exists((char*) configFilename));
  aJson.addBooleanToObject(root, "DHCPEnabled",  use_dhcp);

  addArrayToObject(root, "MAC", mac, 6);
  addArrayToObject(root, "IP", ip);
  addArrayToObject(root, "Gateway", gateway);
  addArrayToObject(root, "Subnet", subnet);
  addArrayToObject(root, "DNS", dns);

  aJson.addNumberToObject(root, "HTTPPort",     httpPort);
  aJson.addNumberToObject(root, "WebsocketPort", websocketPort);

  aJson.addItemToObject(root, "Ethernet", ethernet = aJson.createObject());    
  
  IPAddressToString(buff, Ethernet.localIP());
  aJson.addStringToObject(ethernet, "IP",       buff);
  
  IPAddressToString(buff, Ethernet.gatewayIP());
  aJson.addStringToObject(ethernet, "Gateway",  buff);
  
  IPAddressToString(buff, Ethernet.subnetMask());
  aJson.addStringToObject(ethernet, "Subnet",   buff);
  
  IPAddressToString(buff, Ethernet.dnsServerIP());
  aJson.addStringToObject(ethernet, "DNS",      buff);
}

/*
* Retreives configurable settings from the aJson object and stores them to
* the configuration file or EEPROM
*/
void Network::settingsFromJSON(aJsonObject *root){

  aJsonObject* item = aJson.getObjectItem(root, "DHCPEnabled");
  if (item != NULL)
    use_dhcp = item->valuebool;

  item = aJson.getObjectItem(root, "MAC");
  if (item != NULL) 
    objectToArray(item, mac);
    
  item = aJson.getObjectItem(root, "IP");
  if (item != NULL) 
    objectToArray(item, &ip);
    
  item = aJson.getObjectItem(root, "Subnet");
  if (item != NULL) 
    objectToArray(item, &subnet);
    
  item = aJson.getObjectItem(root, "Gateway");
  if (item != NULL) 
    objectToArray(item, &gateway);
    
  item = aJson.getObjectItem(root, "DNS");
  if (item != NULL) 
    objectToArray(item, &dns);
    
  item = aJson.getObjectItem(root, "HTTPPort");
  if (item != NULL)
    httpPort = item->valueint;
  
  item = aJson.getObjectItem(root, "WebsocketPort");
  if (item != NULL)
    websocketPort = item->valueint;
  
  if (SD.exists((char*) configFilename))
  {
    File networkConfigFile = SD.open((char*) configFilename, FILE_WRITE);
    if (networkConfigFile)
    {
      saveNetworkConfiguration(networkConfigFile);
      networkConfigFile.close();
    }
    else
    {
      cout << F("Error opening ") << configFilename << endl;
    }
  }
  else
  {
    writeSettings();
  }
}




