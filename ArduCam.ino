/*
  ArduCam
  v1.0a
  
  Author:
       Chris Brunner <cyrusbuilt at gmail dot com>

  Copyright (c) 2013 CyrusBuilt

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/



#include <IniFile.h>
#include <SPI.h>
#include <util.h>
#include <EthernetUdp.h>
#include <EthernetServer.h>
#include <EthernetClient.h>
#include <Ethernet.h>
#include <Dns.h>
#include <Dhcp.h>
#include <SD.h>
#include <IPAddress.h>


#define VERSION "ArduCam V1.0a"

// Comment the following line to disable debug mode.
#define DEBUG 1

#ifdef DEBUG
#define DEBUG_BAUD_RATE 9600
#endif // DEBUG

#define SD_CS_PIN 4
#define STATUS_LED_PIN 6
#define DEFAULT_SERVER_PORT 80

// Global constants.
const byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const char *CONFIG_FILENAME = "/config.ini";
const size_t BUFFERLEN = 80;
const IPAddress DEFAULT_IP(192, 168, 1, 240);
const IPAddress DEFAULT_GW(192, 168, 1, 1);
const IPAddress DEFAULT_SN(255, 255, 255, 0);

// Global vars.
IPAddress ip;
IPAddress gateway;
IPAddress subnetmask;
IPAddress dns1;
IPAddress dns2;
EthernetServer server;


void printIniErrorMessage(uint8_t e, bool eol = true) {
#ifdef DEBUG
	switch (e) {
	case IniFile::errorNoError:
		Serial.print("no error");
		break;
	case IniFile::errorFileNotFound:
		Serial.print("file not found.");

		break;
	}

	if (eol) {
		Serial.println();
	}
#endif
}

/**
 * Flashes the status LED once per second to indicate an SD card error occurred.
 */
void statusLedSDErrorFlash() {
	while (1) {
		digitalWrite(STATUS_LED_PIN, HIGH);
		delay(1000);
		digitalWrite(STATUS_LED_PIN, LOW);
		delay(1000);
	}
}

void setNetworkDefaults() {
	ip = DEFAULT_IP;
	gateway = DEFAULT_GW;
	subnetmask = DEFAULT_SN;
}

/**
 * Reads the configuration file specified by the CONFIG_FILENAME
 * consstant and loads the values into memory.
 */
void readConfig() {
#ifdef DEBUG
	Serial.print("Reading configuration...");
#endif

	if (!SD.begin(SD_CS_PIN)) {
#ifdef DEBUG
		Serial.println("ERROR: SD.begin() failed.");
#endif
		statusLedSDErrorFlash();
	}

	char buffer[BUFFERLEN];
	IniFile ini(CONFIG_FILENAME);
	if (!ini.open()) {
		printIniErrorMessage(IniFile::errorFileNotOpen);

		setNetworkDefaults();
	}

}

void setup() {
	// Configure SPI select pin for SD card as outputs and make device inactive
	// to gaurantee init success.
	pinMode(SD_CS_PIN, OUTPUT);
	digitalWrite(SD_CS_PIN, HIGH);

	pinMode(STATUS_LED_PIN, OUTPUT);

#ifdef DEBUG
	// Open serial port and wait for connection.
	Serial.begin(DEBUG_BAUD_RATE);
	while (!Serial) {
		delay(10);
	}
#endif

	SPI.begin();
	readConfig();
}

void loop() {
	// Listen for incoming client connections.
	

}
