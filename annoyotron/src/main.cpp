/*
 * main.cpp
 *
 *  Created on: May 8, 2016
 *      Author: robin
 */

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiScan.h>

#include <EEPROM.h>

#include "SparkFunTSL2561.h"

#define ACTIVE_SSID "annoyotron"
#define DEBUG_SSID "annoyotron_debug"
#define ACTIVE_BSSID_PREFIX "1A:FE:34:"

#define DEBUG_SHOULD_EMIT (random(100) == 0)
#define SHOULD_EMIT (random(3000) == 0)

#define ACTIVATION_WAIT_MIN 3000
#define ACTIVATION_WAIT_MAX 15000

#define WASNT_FOUND_MAX 3

#define ACTIVATION_WAIT_STEP_AVERAGE 750
#define ACTIVATION_WAIT_STEP_VARIANCE 250

bool active;
int32_t activation_wait;
double last_lux;

SFE_TSL2561 light;
boolean gain;     // Gain setting, 0 = X1, 1 = X16;
unsigned int ms;  // Integration ("shutter") time in milliseconds

bool debug;

int wasnt_found;

ESP8266WiFiScanClass scan;

bool should_become_inactive();

void emit_annoyance();

void active_loop();
void inactive_loop();

void become_active();
void become_inactive();

bool wifi_debug_scan();

void flash_pin(int pin, int n, int low_delay, int high_delay) {
	int was = digitalRead(pin);
	for(int i = 0; i < n; i++) {
		digitalWrite(pin, LOW);
		delay(low_delay);
		digitalWrite(pin, HIGH);
		delay(high_delay);
	}
	digitalWrite(pin, was);
}

void flash_pin(int pin) {
	flash_pin(pin, 10, 50, 50);
}

void setup() {
	pinMode(D0, OUTPUT);
	pinMode(D3, OUTPUT);
	pinMode(D4, OUTPUT);
	pinMode(A0, INPUT);
	pinMode(D5, INPUT_PULLUP);
	pinMode(D6, OUTPUT);

	digitalWrite(D0, HIGH);
	digitalWrite(D3, HIGH);

	digitalWrite(D6, LOW);
	debug = (digitalRead(D5) == LOW);
	if(wifi_debug_scan())
		debug = true;


	active = false;

	WiFi.mode(WIFI_STA);

	EEPROM.begin(2);
	int n = EEPROM.read(0) | (EEPROM.read(1) << 8);
	n += 1;
	n += analogRead(A0);
	randomSeed(n);
	EEPROM.write(0, n & 0xff);
	EEPROM.write(1, (n >> 8) & 0xff);
	EEPROM.commit();

	activation_wait = ACTIVATION_WAIT_MIN + random(ACTIVATION_WAIT_MAX - ACTIVATION_WAIT_MIN);

	digitalWrite(D4, LOW);

	wasnt_found = -1;

	light.begin();

	// The light sensor has a default integration time of 402ms,
	// and a default gain of low (1X).

	// If you would like to change either of these, you can
	// do so using the setTiming() command.

	// If gain = false (0), device is set to low gain (1X)
	// If gain = high (1), device is set to high gain (16X)

	gain = 1;

	// If time = 0, integration will be 13.7ms
	// If time = 1, integration will be 101ms
	// If time = 2, integration will be 402ms
	// If time = 3, use manual start / stop to perform your own integration

	unsigned char time = 2;

	// setTiming() will set the third parameter (ms) to the
	// requested integration time in ms (this will be useful later):

	Serial.println("Set timing...");
	light.setTiming(gain,time,ms);

	// To start taking measurements, power up the sensor:

	Serial.println("Powerup...");
	light.setPowerUp();

	if(debug) {
		delay(5000);
		Serial.begin(115200);
		Serial.printf("\r\nannoyotron startup\r\n");
	}
}

void loop() {
	if(active)
		active_loop();
	else
		inactive_loop();
}

double read_lux() {
	unsigned int data0, data1;

	if (light.getData(data0,data1)) {
	    double lux;    // Resulting lux value
	    boolean good;  // True if neither sensor is saturated

	    // Perform lux calculation:

	    good = light.getLux(gain,ms,data0,data1,lux);
	    if(good)
	    	return lux;
	    else
	    	return -1;
	} else
		return -1;

}

bool should_become_inactive() {
	//	return false;
	double lux = read_lux();
	return abs(lux - last_lux) > (abs(lux) + abs(last_lux)) / 10;
}

void emit_annoyance() {
	if(debug) {
		Serial.printf("Emitting annoyance\r\n");
		digitalWrite(D0, LOW);
	}
	digitalWrite(D3, LOW);
	delay(1000);
	digitalWrite(D3, HIGH);
	if(debug) {
		digitalWrite(D0, HIGH);
	}
}

void active_loop() {
	if(should_become_inactive()) {
		become_inactive();
		return;
	}
	if(debug) {
		if(DEBUG_SHOULD_EMIT)
			emit_annoyance();
	} else {
		if(SHOULD_EMIT)
			emit_annoyance();
	}
	delay(100);
}

bool wifi_debug_scan() {
	bool found = false;
	int scanned = scan.scanNetworks(false, true);
	for(int i = 0; i < scanned; i++) {
		if(scan.SSID(i) == DEBUG_SSID) {
			found = true;
			break;
		}
	}
	scan.scanDelete();
	return found;
}

bool wifi_scan() {
	bool found = false;
	int scanned = scan.scanNetworks(false, true);
	for(int i = 0; i < scanned; i++) {
		if(scan.SSID(i) == ACTIVE_SSID) {
			found = true;
			break;
		}
		String bssid = scan.BSSIDstr(i);
		if(bssid.startsWith(ACTIVE_BSSID_PREFIX)) {
			found = true;
			break;
		}
	}
	scan.scanDelete();
	return found;
}

void inactive_loop() {
	bool found = wifi_scan();

	if(digitalRead(D4) == LOW)
		digitalWrite(D4, HIGH);

	if(found) {
		if(debug && (wasnt_found <= (WASNT_FOUND_MAX - 2))) {
			Serial.printf("AP found, resetting activation wait\r\n");
		}
		activation_wait = ACTIVATION_WAIT_MIN + random(ACTIVATION_WAIT_MAX - ACTIVATION_WAIT_MIN);
		if(wasnt_found == -1) {
			if(debug) {
				Serial.printf("First loop found AP\r\n");
			}
			flash_pin(D4, 12, 20, 230);
		}
		wasnt_found = WASNT_FOUND_MAX;
		return;
	} else {
		if(wasnt_found == WASNT_FOUND_MAX - 2) {
			if(debug) {
				Serial.printf("Coordinated annoyance emission\r\n");
			}
			emit_annoyance();
		}
		if(wasnt_found > 0)
			wasnt_found--;
		else
			wasnt_found = 0;
		if(activation_wait <= 0) {
			become_active();
			return;
		} else {
			if(debug && (wasnt_found <= (WASNT_FOUND_MAX - 2))) {
				Serial.printf("No AP, remaining activation wait: %d\r\n", activation_wait);
			}
			int d = ACTIVATION_WAIT_STEP_AVERAGE - ACTIVATION_WAIT_STEP_VARIANCE + random(2 * ACTIVATION_WAIT_STEP_VARIANCE);
			activation_wait -= d;
			delay(d);
		}
	}
}

void become_active() {
	if(debug) {
		Serial.printf("Becoming active\r\n");
	}
	active = true;
	wasnt_found = false;
	WiFi.softAP(ACTIVE_SSID, NULL, 1 + random(12), 1);
	last_lux = read_lux();
	if(debug) {
		Serial.printf("Active AP BSSID: %s\r\n", WiFi.softAPmacAddress().c_str());
	}
	flash_pin(D4, 4, 150, 100);
	flash_pin(D4, 10, 20, 80);
	flash_pin(D4, 4, 150, 100);
	emit_annoyance();
}

void become_inactive() {
	if(debug) {
		Serial.printf("Becoming inactive\r\n");
	}
	active = false;
	WiFi.softAPdisconnect(true);
	wasnt_found = 0;
	activation_wait = ACTIVATION_WAIT_MAX + random(ACTIVATION_WAIT_MAX);
	if(debug) {
		flash_pin(D4, 12, 20, 230);
	}
}
