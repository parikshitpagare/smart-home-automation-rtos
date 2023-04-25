<p align="center">
  <img src="https://user-images.githubusercontent.com/80714882/234116124-5bbd7e92-5432-42f2-bb0e-574ed005aee8.png" width="75%" height="75%">
</p>


<p align="center">
   <img src="https://img.shields.io/badge/ESPRESSIF-ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white" >
   <img src="https://img.shields.io/badge/LICENSE-MIT-green?style=for-the-badge" >
</p>

## Smart Home Automation

- A complete home automation system developed on ESP32 microcontroller using freeRTOS. 
- The system is controlled wirelessly via Bluetooth with an android app developed using MIT App Inventor.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234113165-50b23e8c-def4-42bf-a306-8ba56830c085.jpg" width="75%" height="75%">
</p>

## Demo

**YouTube** - https://youtu.be/aT6Lj6hBVUk

## Features

- Wireless control with Bluetooth
- Dedicated android app
- Activity monitoring on OLED display and android app
- Temperature sensing using DHT11 sensor
- Light intensity sensing using LDR
- Fan/Light control in 3 modes - manual, automatic (based on sensors) and off
- Person detection at door using ultrasonic sensor
- Touch detection on door with alarm using ESP32 inbuilt touch sensor
- Smoke detection with alarm using MQ2 sensor

## System Overview

- The entire system is custom made with wiring and placements planned according to requirements.
- It mimics a room in a house.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234113332-74fb8622-caf8-4488-b6c7-f4801b7c3afe.jpg" width="75%" height="75%">
</p>

### Hardware

#### Microcontroller

- ESP32

#### Sensors

- DHT11 Temperature and Humidity sensor
- LDR module
- MQ2 Gas/Smoke Sensor
- Ultrasonic Sensor

#### Miscellaneous

- OLED display
- Relay
- Buzzers
- Leds
- Bulb
- DC fan


<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234113520-049bbf32-09a2-4669-a82a-f895dc9fe02e.png" width="70%" height="70%">
</p>

### Programming  

#### Software

- Arduino IDE

#### Libraries

- [Bluetooth Serial](https://github.com/espressif/arduino-esp32/tree/master/libraries/BluetoothSerial)
- [Wire](https://github.com/esp8266/Arduino/blob/master/libraries/Wire/)
- [DHT](https://github.com/adafruit/DHT-sensor-library)
- [Adafruit_SSD1306](https://github.com/adafruit/Adafruit_SSD1306)
- [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Ticker](https://github.com/espressif/arduino-esp32/tree/master/libraries/Ticker)

## App Overview

- The android app is developed on MIT App Inventor.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234113882-f83c0412-1feb-4512-9034-2f692feeefb5.png" width="80%" height="80%">
</p>

## Working

### Connecting to App

- Connect/Disconnect button is provided in the app to connect with the system via Bluetooth.
- There is no external bluetooth module used as the ESP32 microcontroller has one builtin.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234114055-145417d4-2d16-4848-a3a1-4b87104e7108.png" width="80%" height="80%">
</p> 

### Activity Monitoring

- All the sensor activity and switch controls are monitored on OLED display that is interfaced using I2C communication protocol.
- The same is monitored on the android app.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234114238-41d6ee21-1c28-46a9-9c0d-b48742afa090.png" width="80%" height="80%">
</p> 

### Temperature and Light Sensing

- DHT11 digital sensor is used to sense temperature in the room and it is displayed on App/OLED.
- Light intensity in the room is sensed using a LDR (Light Dependent Resistor) module.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234114566-7230f107-29dd-4cd1-966a-6301c06e53cb.png" width="80%" height="80%">
</p> 

### Safety System

- For safety purpose, a smoke detection unit is implemented with MQ2 sensor which activates a buzzer and Led in presence of smoke. 
- The buzzer turns off automatically once the smoke disappears.


<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234114873-c41521b8-81c2-43db-bd48-f31e5adf0f3e.png" width="80%" height="80%">
</p> 

### Security System 

- If a person comes in the range of ultrasonic sensor mounted on the door, the App/OLED displays presence of the person and Led turns on to notify the same.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234115167-8e785810-1221-41b8-b0b5-788d5d4c64e7.png" width="80%" height="80%">
</p> 

- If a person touches the door handle a buzzer is activated and App/OLED displays presence of touch along with an Led indicator.It can be deactivated by pressing the turn off button on app.
- Touch sensing is implemented using ESP32 inbuilt touch sensor.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234115176-9462aff6-b6b9-430a-b0e1-804a2cdbfc69.png" width="80%" height="80%">
</p> 

### Fan/Light Control

The fan/bulb is operated in 3 modes using relays,

- Manual Mode : Utilizes the buttons on the App for manual switching.
- Automatic Mode : Automated switching of fan and light based on temperature monitored by DHT11 sensor and light intensity measured by LDR (Light Dependent Resistor)respectively.
- Off Mode : Everything can be switched off at once when not required.

<p align="center">
	<img src="https://user-images.githubusercontent.com/80714882/234115221-09de22eb-a5c0-47e3-819a-6b6720899b08.png" width="80%" height="80%">
</p> 

## Creator

**Parikshit Pagare**

<a href="https://linkedin.com/in/parikshitpagare"><img src="https://img.shields.io/badge/Linkedin-0A66C2?style=for-the-badge&logo=linkedin&logoColor=white.svg"/></a>
<a href="https://youtube.com/@parikshitpagare"><img src="https://img.shields.io/badge/YouTube-FF0000?style=for-the-badge&logo=YouTube&logoColor=white.svg"/></a>
