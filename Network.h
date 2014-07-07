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

#ifndef NETWORK_H_
#define NETWORK_H_

#include <Arduino.h>
#include <Ethernet.h>

class aJsonObject;

using namespace std;

class Network {
  public:
    Network();
    
    void Initialize();

    void printConfiguration();
    void settingsToJSON(aJsonObject *root);
    void settingsFromJSON(aJsonObject *root); 

    bool setup();
    
  private:
    void parseIPV4string(char* ipAddress, uint8_t* ipbytes);
    void IPAddressToString(char* buff, IPAddress ip);
    
    void readSettings();
    bool parseNetworkConfiguration(Stream& stream);
    
    void writeSettings();
    void saveNetworkConfiguration(Stream& stream);

    int readEEPROM(void* buf, int offset, int len);    
    int readEEPROM(IPAddress* address, int offset);
    int writeEEPROM(void* buf, int offset, int len);
    int writeEEPROM(IPAddress* address, int offset);
    
    void addArrayToObject(aJsonObject* object, const char *string, uint8_t* items, int count);
    void addArrayToObject(aJsonObject* object, const char *string, IPAddress ip);
    void objectToArray(aJsonObject* object, uint8_t* items);
    void objectToArray(aJsonObject* object, IPAddress* ip);
    
  public:
    static const char* configFilename;
    static uint8_t mac_oui[3];
    bool use_dhcp;
    uint8_t dhcp_refresh_minutes;
    uint8_t mac[6];
    IPAddress ip;
    IPAddress subnet;
    IPAddress gateway;
    IPAddress dns;
    int httpPort;
    int websocketPort;    
};

#endif
