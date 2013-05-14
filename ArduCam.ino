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

#include <Servo.h>
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

// Pin definitions. NOTE: Ethernet shield w/SD card reader attaches to pins
// 10, 11, 12, and 13.
#define SD_CS_PIN 4
#define STATUS_LED_PIN 6
#define DEFAULT_SERVER_PORT 80
#define SERVO_X_PIN 7
#define SERVO_Y_PIN 8

// Global constants.
const byte MAC[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const char *CONFIG_FILENAME = "/config.ini";
const char *INDEX_PAGE_PATH = "/web/index.htm";
const char *CONFIG_PAGE_PATH = "/web/config.htm";
const char *E404_PAGE_PATH = "/web/404.htm";
const size_t BUFFERLEN = 80;
const IPAddress DEFAULT_IP(192, 168, 1, 240);
const IPAddress DEFAULT_GW(192, 168, 1, 1);
const IPAddress DEFAULT_SN(255, 255, 255, 0);
const IPAddress DEFAULT_DNS = DEFAULT_GW;
const int XPOS_UPPER_LIMIT = 95;
const int XPOS_LOWER_LIMIT = 5;
const int YPOS_UPPER_LIMIT = 135;
const int YPOS_LOWER_LIMIT = 55;
const int XPOS_CENTER = 50;
const int YPOS_CENTER = 100;

// Config file constants
const char *CONFIG_SECTION_MAIN = "Main";
const char *CONFIG_KEY_IP = "IP";
const char *CONFIG_KEY_GW = "Gateway";
const char *CONFIG_KEY_SN = "SubnetMask";
const char *CONFIG_KEY_DNS = "DNS";
const char *CONFIG_KEY_PORT = "Port";

// Global vars.
IPAddress ip;
IPAddress gateway;
IPAddress subnetmask;
IPAddress dns;
EthernetServer server;
EthernetClient client;
Servo servoX;
Servo servoY;
int serverPort = DEFAULT_SERVER_PORT;
int requestnum = 0;
int xpos = XPOS_CENTER;
int ypos = YPOS_CENTER;

/**
 * A structure for holding X and Y-axis coordinates.
 * @member nXpos The X-axis position.
 * @member nYpos The Y-axis position.
 */
typedef struct
{
	int nCmd;
	int nXpos;
	int nYpos;
	bool operator ==(const Coordinates &rhs) const {
		return ((rhs.nXpos == nXpos) && (rhs.nYpos == nYpos) && (rhs.nCmd == nCmd));
	};
} Coordinates;

// Movement directions.
Coordinates up;
Coordinates down;
Coordinates center;
Coordinates left;
Coordinates right;

/**
 * Prints the description of the associated error.
 * @param e The error to get the description of.
 * @param eol Set true if end of line.
 */
void printSDErrorMessage(uint8_t e, bool eol = true) {
#ifdef DEBUG
	switch (e) {
	case IniFile::errorNoError:
		Serial.print("no error");
		break;
	case IniFile::errorFileNotFound:
		Serial.print("file not found.");
		break;
	case IniFile::errorFileNotOpen:
		Serial.print("file not open");
		break;
	case IniFile::errorBufferTooSmall:
		Serial.print("buffer too small");
		break;
	case IniFile::errorSeekError:
		Serial.print("seek error");
		break;
	case IniFile::errorSectionNotFound:
		Serial.print("section not found");
		break;
	case IniFile::errorKeyNotFound:
		Serial.print("key not found");
		break;
	case IniFile::errorEndOfFile:
		Serial.print("end of file");
		break;
	case IniFile::errorUnknownError:
		Serial.print("unknown error");
		break;
	default:
		Serial.print("unknown error value");
		break;
	}

	if (eol) {
		Serial.println();
	}
#endif
}

/**
 * Flashes the status LED once per second to indicate an SD card error occurred.
 * This method never returns as it runs in an endless loop.
 */
void statusLedSDErrorFlash() {
	while (1) {
		digitalWrite(STATUS_LED_PIN, HIGH);
		delay(1000);
		digitalWrite(STATUS_LED_PIN, LOW);
		delay(1000);
	}
}

/**
 * Flash 5 times and then stop to indicate default network settings used do to
 * config read failure.
 */
void statusLedNetworkWarnFlash() {
	for (int i = 1; i <= 5; i++) {
		digitalWrite(STATUS_LED_PIN, HIGH);
		delay(200);
		digitalWrite(STATUS_LED_PIN, LOW);
		delay(200);
	}
}

/**
 * Sets the default network settings.
 */
void setNetworkDefaults() {
#ifdef DEBUG
	Serial.println("Falling back to default network settings...");
#endif
	ip = DEFAULT_IP;
	gateway = DEFAULT_GW;
	subnetmask = DEFAULT_SN;
	dns = DEFAULT_DNS;
	statusLedNetworkWarnFlash();
}

/**
 * Reads the configuration file specified by the CONFIG_FILENAME
 * consstant and loads the values into memory.
 */
void readConfig() {
#ifdef DEBUG
	Serial.print("Reading configuration...");
#endif

	// Init SD card.
	if (!SD.begin(SD_CS_PIN)) {
#ifdef DEBUG
		Serial.println("ERROR: SD.begin() failed.");
#endif
		statusLedSDErrorFlash();
	}

	// Open config file.
	char buffer[BUFFERLEN];
	IniFile ini(CONFIG_FILENAME);
	if (!ini.open()) {
		printSDErrorMessage(IniFile::errorFileNotOpen);
		setNetworkDefaults();
		return;
	}

	// Check to see if the file is valid. This can be used to warn if lines are
	// longer than the buffer.
	if (!ini.validate(buffer, BUFFERLEN)) {
#ifdef DEBUG
		String fname = ini.getFilename();
		Serial.print("Config file '" + fname + "' not valid: ");
		printSDErrorMessage(ini.getError());
#endif
		setNetworkDefaults();
		ini.clearError();
		ini.close();
		return;
	}

	// Read in all the network settings.
	boolean fail = false;
	IPAddress temp;
	if (ini.getIPAddress(CONFIG_SECTION_MAIN, CONFIG_KEY_IP, buffer, BUFFERLEN, temp)) {
		ip = temp;
	}
	else {
#ifdef DEBUG
		Serial.print("ERROR: Failed to read key: " + String(CONFIG_KEY_IP) + ". Message: ");
		printSDErrorMessage(ini.getError());
#endif
		fail = true;
	}

	if (ini.getIPAddress(CONFIG_SECTION_MAIN, CONFIG_KEY_SN, buffer, BUFFERLEN, temp)) {
		subnetmask = temp;
	}
	else {
#if DEBUG
		Serial.print("ERROR: Failed to read key: " + String(CONFIG_KEY_SN) + ". Message: ");
		printSDErrorMessage(ini.getError());
#endif
		fail = true;
	}

	if (ini.getIPAddress(CONFIG_SECTION_MAIN, CONFIG_KEY_GW, buffer, BUFFERLEN, temp)) {
		gateway = temp;
	}
	else {
#ifdef DEBUG
		Serial.print("ERROR: Failed to ready key: " + String(CONFIG_KEY_GW) + ". Message: ");
		printSDErrorMessage(ini.getError());
#endif
		fail = true;
	}

	if (ini.getIPAddress(CONFIG_SECTION_MAIN, CONFIG_KEY_DNS, buffer, BUFFERLEN, temp)) {
		dns = temp;
	}
	else {
#ifdef DEBUG
		Serial.print("ERROR: Failed to read key: " + String(CONFIG_KEY_DNS) + ". Message: ");
		printSDErrorMessage(ini.getError());
#endif
		fail = true;
	}

	int port;
	if (ini.getValue(CONFIG_SECTION_MAIN, CONFIG_KEY_PORT, buffer, BUFFERLEN, port)) {
		serverPort = port;
	}
	else {
#ifdef DEBUG
		Serial.print("ERROR: Failed to read key: " + String(CONFIG_KEY_PORT) + ". Message: ");
		printSDErrorMessage(ini.getError());
#endif
		// On this one, we don't fail outright because the port is intially set to
		// its default anyway. If all the other settings were successful up to this
		// point, then we keep them and just use the default port. No sense in
		// defaulting *all* the settings over a bad port number.
	}

	// Revert to defaults if any network settings were wrong.
	if (fail) {
		setNetworkDefaults();
	}

	// Cleanup.
	if (ini.isOpen()) {
		ini.clearError();
		ini.close();
		ini.~IniFile();
	}
}

/**
 * Initializes the network interface.
 */
void initNetwork() {
	server = EthernetServer::EthernetServer(serverPort);
	byte m[sizeof(MAC)];
	memcpy(m, MAC, sizeof(MAC));
	Ethernet.begin(m, ip, dns, gateway, subnetmask);
	server.begin();
#ifdef DEBUG
	Serial.println();
	Serial.println("Network initialized as: ");
	Serial.println("IP: " + String(Ethernet.localIP()));
	Serial.println("Subnet: " + String(Ethernet.subnetMask()));
	Serial.println("Gateway: " + String(Ethernet.gatewayIP()));
	Serial.println("DNS: " + String(Ethernet.dnsServerIP()));
	Serial.println("Listening on port: " + String(serverPort));
#endif
}

/**
 * Initialize the X and Y-axis servos.
 */
void initServos() {
	servoX.attach(SERVO_X_PIN);
	servoY.attach(SERVO_Y_PIN);
#ifdef DEBUG
	Serial.println();
	Serial.println("X-axis servo attached to pin: " + String(SERVO_X_PIN));
	Serial.println("Y-axis servo attached to pin: " + String(SERVO_Y_PIN));
#endif
}

/**
 * Send a normal OK (200) response header to the client.
 */
void sendHttpOkHeader() {
	if (client) {
		String header = "HTTP/1.0 200 OK\nServer: arduino\nContent-Type: text/html\n\n";
		client.println(header);
	}
}

/**
 * Sends a 404 (page not found) response to the client. This sends the 404
 * response header, then attempts to read the 404 content page and send it
 * to the client. If the page does not exist, then constructs an html string
 * making up (very) basic content of a 404 response page and sends it instead.
 */
void respondWith404() {
	if (client) {
		// Send the 404 header.
		String header = "HTTP/1.1 404 Not Found\nServer: arduino\nContent-Type: text/html\n\n";
		client.println(header);

		// If the 404 page exists, then read it in and send that. Otherwise, send a
		// basic content string. NOTE: For some silly reason, the SD.open() method
		// defines the filename param as a const char*, but the exists() method defines
		// it as just a char*. So we use an ugly hack to cast a const char* to a char*
		// in order to do the check. Since the exists() method never modifies the
		// value of the param, I don't understand why it isn't a const just like the
		// other methods..... *sigh*
		String content = "";
		if (SD.exists(const_cast<char*>(E404_PAGE_PATH))) {
			File f = SD.open(E404_PAGE_PATH);
			if (f) {
				while (f.available()) {
					content.concat(f.read());
				}
				f.close();
			}
		}
		else {
			content = "<html><head><title>404</title></head><body><h1>404: Page not found.</h1></body></html>";
		}

		client.println(content);
		client.flush();
	}
}

/**
 * Reponds to the client by sending a 200 (ok) along with the specified page.
 * If the specified page does not exist, then responds with a 404 (page not
 * found) instead.
 * @param pagePath The path to the webpage to send to the client.
 */
void respondWithPage(const char *pagePath) {
	if (!SD.exists(const_cast<char*>(pagePath))) {
		respondWith404();
		return;
	}

	sendHttpOkHeader();
	if (client) {
		File f = SD.open(pagePath);
		if (f) {
			String content = "";
			while (f.available()) {
				content.concat(f.read());
			}

			f.close();
			client.println(content);
			client.flush();
		}
	}
}

/**
 * Moves servos in the specified direction (coordinates).
 * @param direction The direction to move in.
 */
void moveServos(Coordinates direction) {
	// Moving up.
	if (direction == up) {
		if ((ypos + up.nYpos) > YPOS_LOWER_LIMIT) {
			ypos += up.nYpos;
#ifdef DEBUG
			Serial.println("Panning up...");
#endif
		}
#ifdef DEBUG
		else {
			Serial.println("ERROR: Upper limit reached.");
		}
#endif
	}
	else if (direction == down) {
		// Moving down.
		if ((ypos + down.nYpos) < YPOS_UPPER_LIMIT) {
			ypos += down.nYpos;
#ifdef DEBUG
			Serial.println("Panning down...");
#endif
		}
#ifdef DEBUG
		else {
			Serial.println("ERROR: Down limit reached.");
		}
#endif
	}
	else if (direction == center) {
		// Moving to center pos.
		xpos = center.nXpos;
		ypos = center.nYpos;
#ifdef DEBUG
		Serial.println("Centering webcam...");
#endif
	}
	else if (direction == left) {
		// Moving left.
		if ((xpos + left.nXpos) < XPOS_UPPER_LIMIT) {
			xpos += left.nXpos;
#ifdef DEBUG
			Serial.println("Panning left...");
#endif
		}
#ifdef DEBUG
		else {
			Serial.println("ERROR: Left limit reached.");
		}
#endif
	}
	else if (direction == right) {
		// Moving right.
		if ((xpos + right.nXpos) > XPOS_LOWER_LIMIT) {
			xpos += right.nXpos;
#ifdef DEBUG
			Serial.println("Panning right...");
#endif
		}
#ifdef DEBUG
		else {
			Serial.println("ERROR: Right limit reached.");
		}
#endif
	}
	else {
		// Unknown direction.
#ifdef DEBUG
		Serial.print(String(direction.nCmd));
		Serial.println(" is not a command movement command.");
#endif
		return;
	}

	servoX.write(xpos);
	servoY.write(ypos);
#ifdef DEBUG
	Serial.println("New position X:" + String(xpos) + ",Y:" + String(ypos));
#endif
}

/**
 * Initialize host device and setup program.
 */
void setup() {
	// Initialize movement coordinates.
	up.nCmd = 1;
	up.nXpos = 0;
	up.nYpos = -10;
	down.nCmd = 2;
	down.nXpos = 0;
	down.nYpos = 10;
	center.nCmd = 3;
	center.nXpos = XPOS_CENTER;
	center.nYpos = YPOS_CENTER;
	left.nCmd = 4;
	left.nXpos = 10;
	left.nYpos = 0;
	right.nCmd = 5;
	right.nXpos = -10;
	right.nYpos = 0;

	// Configure SPI select pin for SD card as outputs and make device inactive
	// to gaurantee init success.
	pinMode(SD_CS_PIN, OUTPUT);
	digitalWrite(SD_CS_PIN, HIGH);

	// Indicate we are initializing.
	pinMode(STATUS_LED_PIN, OUTPUT);
	digitalWrite(STATUS_LED_PIN, HIGH);

#ifdef DEBUG
	// Open serial port and wait for connection.
	Serial.begin(DEBUG_BAUD_RATE);
	while (!Serial) {
		delay(10);
	}
#endif

	SPI.begin();
	readConfig();
	initNetwork();
	initServos();
	digitalWrite(STATUS_LED_PIN, LOW);
}

/**
 * Main program loop.
 */
void loop() {
	// Listen for incoming client connections.
	client = server.available();
	if (client) {
#ifdef DEBUG
		Serial.println("New client connected.");
#endif
		int sx = 0;
		int sy = 0;
		char c = NULL;
		String readString = "";
		while (client.connected()) {
			if (client.available()) {
				c = client.read();
				if (readString.length() < 100) {
					readString.concat(c);
				}

				if (c == '\n') {
					if (readString.length() > 0) {
						sx = readString.substring(7, 11).toInt();
						sy = readString.substring(12, 16).toInt();
						servoX.writeMicroseconds(sx);
						servoY.writeMicroseconds(sy);

						readString = "";
					}
				}
			}
		}
		client.println("HTTP/1.1 204 ArduCam");
		client.println();
		client.println();

		// Give the browser time to receive, then close the connection.
		delay(1);
		client.stop();
#ifdef DEBUG
		Serial.println("Client disconnected.");
#endif
	}
}
