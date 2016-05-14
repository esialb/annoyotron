# wifi annoyotron

*wifi-enabled practical joke noisemaker, not for use with spouse*

![assembled](assembled_1024.jpg)

## tl;dr

everyone's heard of the "annoyotron", a practical joke device that emits beeps
at random intervals, best used hidden behind a desk or under a refridgerator or
some other nefarious place.  enter the wifi-enabled annoyotron: these annoyotrons
form a network, with only one active at a time; they use their luminosity sensors
to detect when you have almost found the active one, then switch to another node
at random.

[source code](http://github.com/esialb/annoyotron/) for the microcontroller
program available on github.  parts are available on amazon, or aliexpress if
you want them in bulk.

## parts list

- nodemcu wifi microcontroller (esp8266 with arduino support)
- tsl2561 i2c luminosity sensor
- buzzer
- breadboard + wires
- battery holder
