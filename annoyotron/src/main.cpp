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

#define ACTIVE_SSID "annoyotron"
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
int last_analog;

bool debug;

int wasnt_found;

ESP8266WiFiScanClass scan;

bool should_become_inactive();

void emit_annoyance();

void active_loop();
void inactive_loop();

void become_active();
void become_inactive();

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
	pinMode(D4, OUTPUT);
	pinMode(A0, INPUT);
	pinMode(D5, INPUT_PULLUP);

	debug = (digitalRead(D5) == LOW);

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

	digitalWrite(D0, HIGH);
	digitalWrite(D4, LOW);

	wasnt_found = -1;

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

bool should_become_inactive() {
	//	return false;
	int was = last_analog;
	last_analog = analogRead(A0);
	return abs(was - last_analog) > (abs(was) + abs(last_analog)) / 10;
}

void emit_annoyance() {
	if(debug) {
		Serial.printf("Emitting annoyance\r\n");
	}
	flash_pin(D0, 75, 20, 20);
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
	last_analog = analogRead(A0);
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
